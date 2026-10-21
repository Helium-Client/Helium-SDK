#![deny(unsafe_op_in_unsafe_fn)]

use core::ffi::{c_char, c_void};

pub use dinit::{ServiceHeaderV1, Status, ERROR, INCOMPATIBLE_ABI, INVALID_ARGUMENT, NOT_FOUND, OK};

pub const ABI_VERSION: u32 = 1;
pub const QUERY_SYMBOL: &str = "helium_plugin_query_v1";

pub const LOG_TRACE: u32 = 0;
pub const LOG_DEBUG: u32 = 1;
pub const LOG_INFO: u32 = 2;
pub const LOG_WARN: u32 = 3;
pub const LOG_ERROR: u32 = 4;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct StringView {
    pub data: *const c_char,
    pub byte_len: u32,
}

impl StringView {
    pub const fn from_static_str(value: &'static str) -> Self {
        Self {
            data: value.as_ptr() as *const c_char,
            byte_len: value.len() as u32,
        }
    }
}

unsafe impl Sync for StringView {}

pub type LogFn = Option<unsafe extern "C" fn(context: *mut c_void, level: u32, message: StringView)>;
pub type GetServiceFn = Option<
    unsafe extern "C" fn(
        context: *mut c_void,
        service_id: StringView,
        min_version: u32,
        out_service: *mut *const ServiceHeaderV1,
    ) -> Status,
>;
pub type PluginLoadFn =
    Option<unsafe extern "C" fn(host: *const HostV1, out_plugin_context: *mut *mut c_void) -> Status>;
pub type PluginUnloadFn = Option<unsafe extern "C" fn(plugin_context: *mut c_void)>;
pub type PluginQueryV1 = unsafe extern "C" fn(host_abi_version: u32) -> *const PluginDescriptorV1;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct HostV1 {
    pub struct_size: u32,
    pub abi_version: u32,
    pub context: *mut c_void,
    pub log: LogFn,
    pub get_service: GetServiceFn,
}

#[repr(C)]
pub struct PluginDescriptorV1 {
    pub struct_size: u32,
    pub abi_version: u32,
    pub id: StringView,
    pub name: StringView,
    pub version: StringView,
    pub on_load: PluginLoadFn,
    pub on_unload: PluginUnloadFn,
}

unsafe impl Sync for PluginDescriptorV1 {}

pub const fn host_v1_size() -> u32 {
    core::mem::size_of::<HostV1>() as u32
}

pub const fn plugin_descriptor_v1_size() -> u32 {
    core::mem::size_of::<PluginDescriptorV1>() as u32
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn stable_values_and_layout_are_defined() {
        assert_eq!(ABI_VERSION, 1);
        assert_eq!(OK, 0);
        assert_eq!(core::mem::size_of::<Status>(), 4);
        assert_eq!(host_v1_size() as usize, core::mem::size_of::<HostV1>());
        assert_eq!(
            plugin_descriptor_v1_size() as usize,
            core::mem::size_of::<PluginDescriptorV1>()
        );
    }

    #[test]
    fn string_view_preserves_utf8_byte_length() {
        let view = StringView::from_static_str("Helium café");
        assert_eq!(view.byte_len as usize, "Helium café".len());
        assert!(!view.data.is_null());
    }
}
