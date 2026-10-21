#include <helium/helium.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace {
struct HostState {
    int log_count = 0;
    std::string last_log;
    bool callback_error = false;
};

void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void host_log(void *context, uint32_t level, HeliumStringView message) {
    if (context == nullptr) {
        return;
    }
    auto *state = static_cast<HostState *>(context);
    if (level > HELIUM_LOG_ERROR || (message.data == nullptr && message.byte_len != 0)) {
        state->callback_error = true;
        return;
    }
    state->last_log.assign(message.data == nullptr ? "" : message.data, message.byte_len);
    ++state->log_count;
}

HeliumStatus host_get_service(
    void *context,
    HeliumStringView service_id,
    uint32_t min_version,
    const HeliumServiceHeaderV1 **out_service) {
    if (context == nullptr || out_service == nullptr ||
        (service_id.data == nullptr && service_id.byte_len != 0) || min_version < 1) {
        if (context != nullptr) {
            static_cast<HostState *>(context)->callback_error = true;
        }
        return HELIUM_INVALID_ARGUMENT;
    }
    *out_service = nullptr;
    return HELIUM_NOT_FOUND;
}
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: helium_host_contract_smoke <plugin-library>\n";
        return 2;
    }

#if defined(_WIN32)
    HMODULE library = LoadLibraryA(argv[1]);
    if (library == nullptr) {
        std::cerr << "LoadLibraryA failed with error " << GetLastError() << '\n';
        return 2;
    }
    auto query = reinterpret_cast<HeliumPluginQueryV1>(
        GetProcAddress(library, HELIUM_PLUGIN_QUERY_SYMBOL));
#else
    void *library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (library == nullptr) {
        std::cerr << "dlopen failed: " << dlerror() << '\n';
        return 2;
    }
    auto query = reinterpret_cast<HeliumPluginQueryV1>(
        dlsym(library, HELIUM_PLUGIN_QUERY_SYMBOL));
#endif

    int result = 0;
    try {
        require(query != nullptr, "query export is missing");
        require(query(HELIUM_PLUGIN_ABI_VERSION + 1) == nullptr, "unsupported ABI must be rejected");

        const auto *descriptor = query(HELIUM_PLUGIN_ABI_VERSION);
        require(descriptor != nullptr, "descriptor is null");
        require(descriptor->struct_size >= sizeof(HeliumPluginDescriptorV1), "descriptor is too small");
        require(descriptor->abi_version == HELIUM_PLUGIN_ABI_VERSION, "descriptor ABI mismatch");
        require(descriptor->on_load != nullptr, "on_load callback is required");
        require(descriptor->on_unload != nullptr, "sample must expose on_unload");
        require(std::string(descriptor->id.data, descriptor->id.byte_len) == "dev.helium.cpp-example", "plugin id mismatch");
        require(std::string(descriptor->name.data, descriptor->name.byte_len) == "Helium C++ Example", "plugin name mismatch");
        require(std::string(descriptor->version.data, descriptor->version.byte_len) == "1.0.0", "plugin version mismatch");

        HostState state;
        const HeliumHostV1 host = {
            sizeof(HeliumHostV1),
            HELIUM_PLUGIN_ABI_VERSION,
            &state,
            host_log,
            host_get_service
        };

        void *plugin_context = reinterpret_cast<void *>(static_cast<uintptr_t>(1));
        require(descriptor->on_load(&host, &plugin_context) == HELIUM_OK, "on_load failed");
        require(plugin_context == nullptr, "sample context should be null");
        require(!state.callback_error, "host callback reported an invalid argument");
        require(state.log_count == 1, "expected one startup log");
        require(state.last_log == "Hello from the Helium C++ SDK example", "startup log mismatch");

        const char service_name[] = "org.helium.events";
        const HeliumStringView service_id = {
            service_name,
            static_cast<uint32_t>(sizeof(service_name) - 1)
        };
        const HeliumServiceHeaderV1 *service = reinterpret_cast<const HeliumServiceHeaderV1 *>(1);
        require(host.get_service(host.context, service_id, 1, &service) == HELIUM_NOT_FOUND, "unknown service status mismatch");
        require(service == nullptr, "unknown service output must be null");

        descriptor->on_unload(plugin_context);
        require(!state.callback_error, "host callback reported an invalid argument");
        require(state.log_count == 2, "expected shutdown log");
        require(state.last_log == "Helium C++ SDK example is shutting down", "shutdown log mismatch");
    } catch (const std::exception &error) {
        std::cerr << "contract test failed: " << error.what() << '\n';
        result = 1;
    }

#if defined(_WIN32)
    FreeLibrary(library);
#else
    dlclose(library);
#endif
    if (result == 0) {
        std::cout << "ABI query, metadata, logging, service lookup and unload checks passed.\n";
    }
    return result;
}
