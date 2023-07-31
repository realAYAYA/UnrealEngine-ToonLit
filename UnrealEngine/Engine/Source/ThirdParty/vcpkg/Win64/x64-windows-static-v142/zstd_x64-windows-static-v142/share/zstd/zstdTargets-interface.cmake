
add_library(zstd::libzstd_shared IMPORTED INTERFACE)
set_target_properties(zstd::libzstd_shared PROPERTIES INTERFACE_LINK_LIBRARIES zstd::libzstd_static)
