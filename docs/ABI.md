# Helium Native Plugin ABI v1

This document explains how a native Helium plugin communicates with its host. It is intended for plugin authors who need to understand the package format, shared-library entry point, lifecycle, host functions, and Dinit events.

The public C and C++ declarations are in `include/helium/helium.h`. Rust plugins use the matching definitions in the `helium-sdk` crate. Dinit's service declarations are in the separate `Dinit` repository at `include/dinit/dinit.h`, and the Rust event-service implementation is in the standalone `dinit` crate.

## 1. The pieces of a plugin

A plugin has two files in its package:

1. A `helium-plugin.json` manifest, which supplies the plugin's identity and names its library.
2. A native shared library (`.dll`, `.so`, or `.dylib`) that exports `helium_plugin_query_v1`.

The manifest is used by the host to identify and locate the library. The ABI descriptor inside the library is the host's source of truth for the callbacks and metadata the binary actually provides. Hosts should validate both before loading a plugin.

Example manifest:

```json
{
  "id": "dev.helium.example",
  "name": "Example Plugin",
  "version": "1.0.0",
  "api_version": 1,
  "library": "libexample_plugin.so"
}
```

The `library` value is a filename in the package, not a path. Do not assume a platform-specific suffix; name the library produced by the build. The JSON Schema at `manifest/helium-plugin.schema.json` is the authoritative definition of valid fields and constraints. In particular, all five fields are required, additional fields are rejected, and `api_version` must be `1`.

## 2. Entry point and descriptor

Every ABI v1 library exports this C-compatible function:

```c
const HeliumPluginDescriptorV1 *
helium_plugin_query_v1(uint32_t host_abi_version);
```

The host passes the ABI version it supports. The plugin returns a pointer to a static, immutable `HeliumPluginDescriptorV1` when it supports that version, or null otherwise. The query function should only report compatibility: do not allocate plugin resources, start threads, subscribe to events, or otherwise initialize the plugin here. The host may query the library before deciding whether it will load the plugin.

The descriptor contains:

| Field | Meaning |
| --- | --- |
| `struct_size` | Size in bytes of the descriptor layout supplied by the plugin |
| `abi_version` | Plugin ABI version; `1` for this contract |
| `id` | Stable machine-readable plugin identifier |
| `name` | Human-readable plugin name |
| `version` | Plugin release version |
| `on_load` | Callback that initializes the plugin |
| `on_unload` | Callback that releases plugin resources |

The host checks `struct_size` before reading fields beyond the size it recognizes and checks `abi_version` before calling plugin callbacks. Plugin IDs in a package manifest and descriptor should agree.

### String representation

`HeliumStringView` is a pointer and a byte length. Text is UTF-8, and the byte length is **not** a character count. The data is not required to end in a null byte. A host or plugin must use the supplied length when reading or writing a string and must not assume null termination.

The descriptor and all strings it points to must remain valid and unchanged until the host unloads the library. The host must likewise keep host-owned data passed to the plugin valid for the documented lifetime of that data.

## 3. Host functions and status values

During `on_load`, the host provides a `HeliumHostV1` table:

| Field | Purpose |
| --- | --- |
| `struct_size` | Size of the host table, in bytes |
| `abi_version` | Host's plugin ABI version |
| `context` | Opaque host-owned value passed back to host functions |
| `log` | Optional callback for sending a UTF-8 message to the host log |
| `get_service` | Optional callback for requesting a versioned host service |

Check the table's size and version before using it. Check optional function pointers before calling them. The `context` value is owned by the host: do not free it, modify it, or retain it past plugin unload.

The shared status type is a signed 32-bit integer. Its defined values are:

| Status | Value | Meaning |
| --- | ---: | --- |
| `HELIUM_OK` | `0` | Operation succeeded |
| `HELIUM_ERROR` | `-1` | General failure |
| `HELIUM_INCOMPATIBLE_ABI` | `-2` | Requested or supplied ABI is incompatible |
| `HELIUM_NOT_FOUND` | `-3` | Requested service or item is unavailable |
| `HELIUM_INVALID_ARGUMENT` | `-4` | An argument is invalid |

When `get_service` succeeds, it writes a service header pointer to the output. A service header begins with `struct_size` and `version`; check both before reading the rest of the service table. A missing service returns `HELIUM_NOT_FOUND`, and the host clears the output pointer. A plugin should treat optional services as optional and decide whether it can operate without them.

Log levels are `TRACE` (0), `DEBUG` (1), `INFO` (2), `WARN` (3), and `ERROR` (4). Keep the message buffer valid for the duration of the logging call.

## 4. Plugin lifecycle and ownership

The expected host lifecycle is:

1. Validate the package manifest and load the native library.
2. Find and call `helium_plugin_query_v1` with the host ABI version.
3. Validate the returned descriptor and required callbacks.
4. Call `on_load(host, &plugin_context)`.
5. While the plugin is active, allow it to use the host functions and services.
6. Before unloading, stop event delivery, wait for in-flight event callbacks, call `on_unload(plugin_context)`, and unload the library.

The host initializes the output plugin context to null before calling `on_load`. Use it to return an opaque handle for plugin-owned state when necessary. The plugin defines what that handle means and is responsible for releasing its own allocations. Never ask the host to free plugin-owned data, or the plugin to free host-owned data, unless a specific service explicitly defines that ownership transfer.

If initialization fails after acquiring resources, release those resources before returning an error. The host may call `on_unload` if a valid context was returned, so the unload callback should tolerate the context value the plugin documented, including null if appropriate.

Do not let C++ exceptions escape an ABI callback. Do not let Rust panics unwind across the C ABI; callbacks that can panic should contain that panic and return an appropriate status instead. A callback must return according to its declared ABI and not retain pointers beyond their documented lifetime.

Stop and join plugin-created threads and release plugin-owned callbacks before `on_unload` returns. Do not keep the host table, host context, service-table pointers, event user contexts, or callback function pointers for use after unload.

## 5. Dinit event service

The host can expose Dinit through versioned service discovery:

| Property | Value |
| --- | --- |
| Service ID | `helium.dinit.events` |
| Minimum service version | `1` |
| Service table | `HeliumDinitServiceV1` |

The service table provides a `subscribe` function and an `unsubscribe` function. A subscription names an event type, an integer priority, a callback, and an opaque user context. On success, `subscribe` writes a subscription ID. Check the service header size and version and check both function pointers before calling them.

### Events

ABI v1 defines these event IDs:

| Event | ID | Meaning | Can be cancelled? |
| --- | ---: | --- | --- |
| `CLIENT_STARTED` | `1` | Client startup notification | No |
| `CLIENT_STOPPING` | `2` | Client shutdown notification | No |
| `TICK_PRE` | `10` | Before Helium's regular module tick | Yes |
| `TICK_POST` | `11` | After Helium's regular module tick | No |
| `RENDER_FRAME` | `20` | Before overlay rendering for a frame | Yes |

The callback receives a mutable `HeliumDinitEventV1`. Its fields include:

| Field | Meaning |
| --- | --- |
| `struct_size` | Size of the event structure |
| `event_type` | One of the event IDs above |
| `flags` | Event flags, including the cancellation flag |
| `reserved` | Reserved for ABI use |
| `sequence` | Event sequence value |
| `tick` | Tick value when applicable |
| `delta_seconds` | Frame delta in seconds when applicable |

Fields that do not apply to a given event are zero. Check `struct_size` and `event_type` before using the event. For a cancellable event, set `HELIUM_DINIT_EVENT_CANCELLED` in `flags` to request cancellation. A `TICK_PRE` callback can skip Helium's regular module tick. A `RENDER_FRAME` callback can skip that overlay draw. Other currently defined events are notifications; cancellation does not apply to them.

`TICK_PRE` and `TICK_POST` wrap Helium's module tick detected by the client's frame hook. They are not injection points for Minecraft's vanilla tick. `RENDER_FRAME` occurs after the client's rotation update and before overlay rendering. Cancelling it does not undo or skip the rotation update.

### Subscription lifetime

Subscriptions belong to the plugin that obtained the service. Calling `unsubscribe` removes a subscription from future dispatch snapshots, but an event callback that has already begun may still finish. Keep its user context valid until that callback has returned.

Before calling a plugin's `on_unload`, the host removes its remaining subscriptions and waits for in-flight Dinit callbacks to finish. The host cancels those subscriptions before unload, so their IDs are no longer valid during `on_unload`. This does not stop worker threads created by the plugin: stop and join those threads yourself. Do not retain the service table or host context after unload.

Event callbacks must not throw a C++ exception or unwind a Rust panic across the C ABI. Keep callbacks short and avoid blocking operations that could stall client event dispatch.

## 6. Writing a plugin safely

When implementing a plugin:

1. Export exactly the expected entry point and return a static descriptor only for ABI versions you support.
2. Use stable, package-unique IDs and make the manifest metadata agree with the descriptor.
3. Validate sizes, versions, pointers, and status codes before using ABI data.
4. Treat service discovery as optional unless your plugin cannot operate without a particular service.
5. Keep string buffers valid for the complete call or descriptor lifetime required by the ABI.
6. Make initialization reversible; clean up partial setup before reporting failure.
7. Unsubscribe, stop threads, and release resources before unload.
8. Keep exceptions and panics from crossing ABI boundaries.

Native plugins execute in the host process with the host's privileges. ABI validation is not a security sandbox. Load only trusted plugin libraries.

## 7. Compatibility and scope

ABI versioning is independent of Minecraft versions and mappings. Existing ABI v1 fields and status values must not be reordered or reinterpreted. Additive service capabilities should use versioned service tables. Any incompatible change to the plugin contract requires a new versioned entry point and descriptor.

A stable ABI does not guarantee that a host supports a specific Minecraft version, operating system, or game capability. The Helium Client project owns runtime integration and plugin loading; this SDK does not inject into Minecraft and does not patch, build, or depend on that client.

Minecraft handles, general-purpose render services, packet events, and C# bindings are not part of this SDK's current ABI. Future game, render, and packet APIs should be introduced as independently versioned service tables rather than by changing existing `HeliumHostV1` fields.
