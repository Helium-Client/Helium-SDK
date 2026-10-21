use core::ffi::c_void;
use core::ptr;
use core::sync::atomic::{AtomicPtr, Ordering};
use helium_sdk::{HostV1, PluginDescriptorV1, StringView, ABI_VERSION, INCOMPATIBLE_ABI, LOG_INFO, OK};

static HOST: AtomicPtr<HostV1> = AtomicPtr::new(ptr::null_mut());

unsafe extern "C" fn on_load(host: *const HostV1, out_plugin_context: *mut *mut c_void) -> i32 {
    if host.is_null() || out_plugin_context.is_null() {
        return helium_sdk::INVALID_ARGUMENT;
    }

    unsafe { *out_plugin_context = ptr::null_mut() };
    let host_ref = unsafe { &*host };
    if (host_ref.struct_size as usize) < core::mem::size_of::<HostV1>()
        || host_ref.abi_version != ABI_VERSION
        || host_ref.log.is_none()
    {
        return INCOMPATIBLE_ABI;
    }

    HOST.store(host.cast_mut(), Ordering::Release);
    let message = StringView::from_static_str("Hello from the Helium Rust SDK example");
    if let Some(log) = host_ref.log {
        unsafe { log(host_ref.context, LOG_INFO, message) };
    }
    OK
}

unsafe extern "C" fn on_unload(_plugin_context: *mut c_void) {
    let host = HOST.swap(ptr::null_mut(), Ordering::AcqRel);
    if host.is_null() {
        return;
    }
    let host_ref = unsafe { &*host };
    if let Some(log) = host_ref.log {
        unsafe {
            log(
                host_ref.context,
                LOG_INFO,
                StringView::from_static_str("Helium Rust SDK example is shutting down"),
            )
        };
    }
}

static DESCRIPTOR: PluginDescriptorV1 = PluginDescriptorV1 {
    struct_size: core::mem::size_of::<PluginDescriptorV1>() as u32,
    abi_version: ABI_VERSION,
    id: StringView::from_static_str("dev.helium.rust-example"),
    name: StringView::from_static_str("Helium Rust Example"),
    version: StringView::from_static_str("1.0.0"),
    on_load: Some(on_load),
    on_unload: Some(on_unload),
};

#[no_mangle]
pub unsafe extern "C" fn helium_plugin_query_v1(host_abi_version: u32) -> *const PluginDescriptorV1 {
    if host_abi_version == ABI_VERSION {
        ptr::addr_of!(DESCRIPTOR)
    } else {
        ptr::null()
    }
}
