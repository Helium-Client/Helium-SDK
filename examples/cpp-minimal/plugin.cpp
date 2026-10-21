#include <helium/helium.h>

namespace {
const HeliumHostV1 *g_host = nullptr;
const HeliumDinitServiceV1 *g_dinit = nullptr;

void write_log(uint32_t level, const char *text, uint32_t length) {
    if (g_host == nullptr || g_host->log == nullptr || text == nullptr) {
        return;
    }
    g_host->log(g_host->context, level, HeliumStringView{text, length});
}

HeliumStatus on_tick_post(void *, HeliumDinitEventV1 *event) {
    if (event == nullptr || event->struct_size < sizeof(HeliumDinitEventV1) ||
        event->event_type != HELIUM_DINIT_TICK_POST) {
        return HELIUM_INVALID_ARGUMENT;
    }
    return HELIUM_OK;
}

HeliumStatus on_load(const HeliumHostV1 *host, void **out_plugin_context) {
    if (out_plugin_context == nullptr) {
        return HELIUM_INVALID_ARGUMENT;
    }
    *out_plugin_context = nullptr;

    if (host == nullptr ||
        host->struct_size < sizeof(HeliumHostV1) ||
        host->abi_version != HELIUM_PLUGIN_ABI_VERSION ||
        host->log == nullptr || host->get_service == nullptr) {
        return HELIUM_INCOMPATIBLE_ABI;
    }

    g_host = host;
    static constexpr char message[] = "Hello from the Helium C++ SDK example";
    write_log(HELIUM_LOG_INFO, message, static_cast<uint32_t>(sizeof(message) - 1));

    static constexpr char service_name[] = HELIUM_DINIT_SERVICE_ID;
    const HeliumStringView service_id = {
        service_name,
        static_cast<uint32_t>(sizeof(service_name) - 1)
    };
    const HeliumServiceHeaderV1 *service_header = nullptr;
    const HeliumStatus status = host->get_service(
        host->context, service_id, HELIUM_DINIT_SERVICE_VERSION, &service_header);
    if (status == HELIUM_OK && service_header != nullptr &&
        service_header->struct_size >= sizeof(HeliumDinitServiceV1) &&
        service_header->version >= HELIUM_DINIT_SERVICE_VERSION) {
        g_dinit = reinterpret_cast<const HeliumDinitServiceV1 *>(service_header);
        if (g_dinit->subscribe != nullptr) {
            uint64_t subscription_id = 0;
            const HeliumStatus subscribe_status = g_dinit->subscribe(
                g_dinit->context,
                HELIUM_DINIT_TICK_POST,
                0,
                on_tick_post,
                nullptr,
                &subscription_id);
            if (subscribe_status != HELIUM_OK) {
                g_dinit = nullptr;
            }
        }
    }

    return HELIUM_OK;
}

void on_unload(void *) {
    g_dinit = nullptr;
    static constexpr char message[] = "Helium C++ SDK example is shutting down";
    write_log(HELIUM_LOG_INFO, message, static_cast<uint32_t>(sizeof(message) - 1));
    g_host = nullptr;
}

static constexpr char kId[] = "dev.helium.cpp-example";
static constexpr char kName[] = "Helium C++ Example";
static constexpr char kVersion[] = "1.0.0";

const HeliumPluginDescriptorV1 kDescriptor = {
    sizeof(HeliumPluginDescriptorV1),
    HELIUM_PLUGIN_ABI_VERSION,
    {kId, static_cast<uint32_t>(sizeof(kId) - 1)},
    {kName, static_cast<uint32_t>(sizeof(kName) - 1)},
    {kVersion, static_cast<uint32_t>(sizeof(kVersion) - 1)},
    on_load,
    on_unload
};
}

extern "C" HELIUM_PLUGIN_EXPORT
const HeliumPluginDescriptorV1 *helium_plugin_query_v1(uint32_t host_abi_version) {
    if (host_abi_version != HELIUM_PLUGIN_ABI_VERSION) {
        return nullptr;
    }
    return &kDescriptor;
}
