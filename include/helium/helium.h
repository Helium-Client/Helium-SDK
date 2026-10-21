#ifndef HELIUM_SDK_HELIUM_H
#define HELIUM_SDK_HELIUM_H

#include <stdint.h>
#include <dinit/dinit.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HELIUM_PLUGIN_ABI_VERSION 1u
#define HELIUM_PLUGIN_QUERY_SYMBOL "helium_plugin_query_v1"

typedef enum HeliumLogLevel {
    HELIUM_LOG_TRACE = 0,
    HELIUM_LOG_DEBUG = 1,
    HELIUM_LOG_INFO = 2,
    HELIUM_LOG_WARN = 3,
    HELIUM_LOG_ERROR = 4
} HeliumLogLevel;

typedef struct HeliumStringView {
    const char *data;
    uint32_t byte_len;
} HeliumStringView;

typedef struct HeliumHostV1 HeliumHostV1;
typedef struct HeliumPluginDescriptorV1 HeliumPluginDescriptorV1;

typedef void (*HeliumLogFn)(void *context, uint32_t level, HeliumStringView message);
typedef HeliumStatus (*HeliumGetServiceFn)(
    void *context,
    HeliumStringView service_id,
    uint32_t min_version,
    const HeliumServiceHeaderV1 **out_service
);
typedef HeliumStatus (*HeliumPluginLoadFn)(
    const HeliumHostV1 *host,
    void **out_plugin_context
);
typedef void (*HeliumPluginUnloadFn)(void *plugin_context);
typedef const HeliumPluginDescriptorV1 *(*HeliumPluginQueryV1)(uint32_t host_abi_version);

struct HeliumHostV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    void *context;
    HeliumLogFn log;
    HeliumGetServiceFn get_service;
};

struct HeliumPluginDescriptorV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    HeliumStringView id;
    HeliumStringView name;
    HeliumStringView version;
    HeliumPluginLoadFn on_load;
    HeliumPluginUnloadFn on_unload;
};

#if defined(_WIN32)
#  if defined(HELIUM_PLUGIN_BUILD)
#    define HELIUM_PLUGIN_EXPORT __declspec(dllexport)
#  else
#    define HELIUM_PLUGIN_EXPORT
#  endif
#else
#  define HELIUM_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

HELIUM_PLUGIN_EXPORT const HeliumPluginDescriptorV1 *
helium_plugin_query_v1(uint32_t host_abi_version);

#ifdef __cplusplus
}
#endif

#endif
