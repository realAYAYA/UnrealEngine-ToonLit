
<!-- markdownlint-disable MD041 -->
<p align="left"><img src="https://vulkan.lunarg.com/img/NewLunarGLogoBlack.png" alt="LunarG" width=263 height=113 /></p>
<p align="left">Copyright (c) 2021-2022 LunarG, Inc.</p>

<p align="center"><img src="./images/logo.png" width=400 /></p>

[![Creative Commons][3]][4]

[3]: https://i.creativecommons.org/l/by-nd/4.0/88x31.png "Creative Commons License"
[4]: https://creativecommons.org/licenses/by-nd/4.0/

# Vulkan Profiles Definitions

## Vulkan Profiles List

| Profiles | VP_UE_Vulkan_ES3_1_Android | VP_UE_Vulkan_SM5 | VP_UE_Vulkan_SM5_Android | VP_UE_Vulkan_SM5_Android_RT | VP_UE_Vulkan_SM5_RT | VP_UE_Vulkan_SM6 | VP_UE_Vulkan_SM6_RT |
|----------|----------------------------|------------------|--------------------------|-----------------------------|---------------------|------------------|---------------------|
| Label | Epic Games - Unreal Engine - Android Vulkan - ES 3.1 | Epic Games - Unreal Engine - Desktop Vulkan SM5 | Epic Games - Unreal Engine - Android Vulkan SM5 | Epic Games - Unreal Engine - Android Vulkan SM5 with RayTracing | Epic Games - Unreal Engine - Desktop Vulkan RT | Epic Games - Unreal Engine - Desktop Vulkan SM6 | Epic Games - Unreal Engine - Desktop Vulkan RT |
| Description | A profile that describes the minimum requirements of the engine for using ES 3.1 feature level with Vulkan on Android. | A profile that describes the minimum requirements of the engine for using Vulkan. | A profile that describes the minimum requirements of the engine for using Vulkan on Android. | A profile that describes the RayTracing requirements of the engine with Vulkan on Android. | A profile that describes the RayTracing requirements of the engine with Vulkan. | A profile that describes the requirements of the engine for using Vulkan in SM6. | A profile that describes the RayTracing requirements of the engine with Vulkan. |
| Version | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| Required API version | 1.1.0 | 1.1.0 | 1.1.0 | 1.2.0 | 1.2.0 | 1.3.0 | 1.3.0 |
| Fallback profiles | - | - | - | - | - | - | - |

## Vulkan Profiles Extensions

* :heavy_check_mark: indicates that the extension is defined in the profile
* "X.X Core" indicates that the extension is not defined in the profile but the extension is promoted to the specified core API version that is smaller than or equal to the minimum required API version of the profile
* :x: indicates that the extension is neither defined in the profile nor it is promoted to a core API version that is smaller than or equal to the minimum required API version of the profile

| Profiles | VP_UE_Vulkan_ES3_1_Android | VP_UE_Vulkan_SM5 | VP_UE_Vulkan_SM5_Android | VP_UE_Vulkan_SM5_Android_RT | VP_UE_Vulkan_SM5_RT | VP_UE_Vulkan_SM6 | VP_UE_Vulkan_SM6_RT |
|----------|----------------------------|------------------|--------------------------|-----------------------------|---------------------|------------------|---------------------|
| **Instance extensions** |
| [VK_KHR_device_group_creation](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_device_group_creation.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_external_fence_capabilities](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_external_fence_capabilities.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_external_memory_capabilities](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_external_memory_capabilities.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_external_semaphore_capabilities](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_external_semaphore_capabilities.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_get_physical_device_properties2](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_get_physical_device_properties2.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| **Device extensions** |
| [VK_KHR_16bit_storage](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_16bit_storage.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_8bit_storage](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_8bit_storage.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_acceleration_structure](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_acceleration_structure.html) | :x: | :x: | :x: | :heavy_check_mark: | :heavy_check_mark: | :x: | :heavy_check_mark: |
| [VK_KHR_bind_memory2](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_bind_memory2.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_buffer_device_address](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_buffer_device_address.html) | :x: | :x: | :x: | :heavy_check_mark: | :heavy_check_mark: | 1.2 Core | :heavy_check_mark: |
| [VK_KHR_copy_commands2](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_copy_commands2.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_KHR_create_renderpass2](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_create_renderpass2.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_dedicated_allocation](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_dedicated_allocation.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_deferred_host_operations](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_deferred_host_operations.html) | :x: | :x: | :x: | :heavy_check_mark: | :heavy_check_mark: | :x: | :heavy_check_mark: |
| [VK_KHR_depth_stencil_resolve](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_depth_stencil_resolve.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_descriptor_update_template](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_descriptor_update_template.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_device_group](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_device_group.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_draw_indirect_count](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_draw_indirect_count.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_driver_properties](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_driver_properties.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_dynamic_rendering](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_dynamic_rendering.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_KHR_external_fence](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_external_fence.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_external_memory](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_external_memory.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_external_semaphore](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_external_semaphore.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_format_feature_flags2](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_format_feature_flags2.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_KHR_get_memory_requirements2](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_get_memory_requirements2.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_image_format_list](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_image_format_list.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_imageless_framebuffer](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_imageless_framebuffer.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_maintenance1](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_maintenance1.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_maintenance2](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_maintenance2.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_maintenance3](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_maintenance3.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_maintenance4](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_maintenance4.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_KHR_multiview](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_multiview.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_ray_query](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_ray_query.html) | :x: | :x: | :x: | :heavy_check_mark: | :heavy_check_mark: | :x: | :heavy_check_mark: |
| [VK_KHR_ray_tracing_pipeline](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_ray_tracing_pipeline.html) | :x: | :x: | :x: | :x: | :heavy_check_mark: | :x: | :heavy_check_mark: |
| [VK_KHR_relaxed_block_layout](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_relaxed_block_layout.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_sampler_mirror_clamp_to_edge](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_sampler_mirror_clamp_to_edge.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_sampler_ycbcr_conversion](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_sampler_ycbcr_conversion.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_separate_depth_stencil_layouts](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_separate_depth_stencil_layouts.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_shader_atomic_int64](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_shader_atomic_int64.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_shader_draw_parameters](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_shader_draw_parameters.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_shader_float16_int8](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_shader_float16_int8.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_shader_float_controls](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_shader_float_controls.html) | :x: | :x: | :x: | :heavy_check_mark: | :heavy_check_mark: | 1.2 Core | :heavy_check_mark: |
| [VK_KHR_shader_integer_dot_product](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_shader_integer_dot_product.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_KHR_shader_non_semantic_info](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_shader_non_semantic_info.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_KHR_shader_subgroup_extended_types](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_shader_subgroup_extended_types.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_shader_terminate_invocation](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_shader_terminate_invocation.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_KHR_spirv_1_4](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_spirv_1_4.html) | :x: | :x: | :x: | :heavy_check_mark: | :heavy_check_mark: | 1.2 Core | :heavy_check_mark: |
| [VK_KHR_storage_buffer_storage_class](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_storage_buffer_storage_class.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_synchronization2](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_synchronization2.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_KHR_timeline_semaphore](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_timeline_semaphore.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_uniform_buffer_standard_layout](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_uniform_buffer_standard_layout.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_variable_pointers](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_variable_pointers.html) | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core | 1.1 Core |
| [VK_KHR_vulkan_memory_model](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_vulkan_memory_model.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_KHR_zero_initialize_workgroup_memory](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_KHR_zero_initialize_workgroup_memory.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_4444_formats](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_4444_formats.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_descriptor_indexing](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_descriptor_indexing.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | :heavy_check_mark: | :heavy_check_mark: |
| [VK_EXT_extended_dynamic_state](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_extended_dynamic_state.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_extended_dynamic_state2](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_extended_dynamic_state2.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_host_query_reset](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_host_query_reset.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_EXT_image_robustness](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_image_robustness.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_inline_uniform_block](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_inline_uniform_block.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_pipeline_creation_cache_control](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_pipeline_creation_cache_control.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_pipeline_creation_feedback](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_pipeline_creation_feedback.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_private_data](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_private_data.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_sampler_filter_minmax](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_sampler_filter_minmax.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_EXT_scalar_block_layout](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_scalar_block_layout.html) | :x: | :x: | :x: | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| [VK_EXT_separate_stencil_usage](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_separate_stencil_usage.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_EXT_shader_demote_to_helper_invocation](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_shader_demote_to_helper_invocation.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_shader_image_atomic_int64](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_shader_image_atomic_int64.html) | :x: | :x: | :x: | :x: | :x: | :heavy_check_mark: | :heavy_check_mark: |
| [VK_EXT_shader_viewport_index_layer](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_shader_viewport_index_layer.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |
| [VK_EXT_subgroup_size_control](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_subgroup_size_control.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_texel_buffer_alignment](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_texel_buffer_alignment.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_texture_compression_astc_hdr](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_texture_compression_astc_hdr.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_tooling_info](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_tooling_info.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_EXT_ycbcr_2plane_444_formats](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_EXT_ycbcr_2plane_444_formats.html) | :x: | :x: | :x: | :x: | :x: | 1.3 Core | 1.3 Core |
| [VK_AMD_draw_indirect_count](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VK_AMD_draw_indirect_count.html) | :x: | :x: | :x: | 1.2 Core | 1.2 Core | 1.2 Core | 1.2 Core |

## Vulkan Profile Features

> **NOTE**: The table below only contains features explicitly defined by the corresponding profile. Further features may be supported by the profiles in accordance to the requirements defined in the "Feature Requirements" section of the appropriate version of the Vulkan API Specification.

* :heavy_check_mark: indicates that the feature is defined in the profile (hover over the symbol to view the structure and corresponding extension or core API version where the feature is defined in the profile)
* :warning: indicates that the feature is not defined in the profile but an equivalent feature is (hover over the symbol to view the structure and corresponding extension or core API version where the feature is defined in the profile)
* :x: indicates that neither the feature nor an equivalent feature is defined in the profile

| Profiles | VP_UE_Vulkan_ES3_1_Android | VP_UE_Vulkan_SM5 | VP_UE_Vulkan_SM5_Android | VP_UE_Vulkan_SM5_Android_RT | VP_UE_Vulkan_SM5_RT | VP_UE_Vulkan_SM6 | VP_UE_Vulkan_SM6_RT |
|----------|----------------------------|------------------|--------------------------|-----------------------------|---------------------|------------------|---------------------|
| **Vulkan 1.0** |
| [fragmentStoresAndAtomics](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceFeatures.html) | :x: | <span title="defined in VkPhysicalDeviceFeatures (Vulkan 1.0)">:heavy_check_mark:</span> | :x: | <span title="defined in VkPhysicalDeviceFeatures (Vulkan 1.0)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceFeatures (Vulkan 1.0)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceFeatures (Vulkan 1.0)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceFeatures (Vulkan 1.0)">:heavy_check_mark:</span> |
| [shaderInt64](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceFeatures.html) | :x: | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceFeatures (Vulkan 1.0)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceFeatures (Vulkan 1.0)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceFeatures (Vulkan 1.0)">:heavy_check_mark:</span> |
| **Vulkan 1.2** |
| [bufferDeviceAddress](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceBufferDeviceAddressFeatures.html) | :x: | :x: | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceBufferDeviceAddressFeatures (Vulkan 1.2)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceBufferDeviceAddressFeatures (Vulkan 1.2)">:heavy_check_mark:</span> |
| [descriptorBindingPartiallyBound](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceDescriptorIndexingFeatures.html) | :x: | :x: | :x: | :x: | :x: | <span title="equivalent defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:warning:</span> | <span title="equivalent defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:warning:</span> |
| [descriptorBindingUpdateUnusedWhilePending](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceDescriptorIndexingFeatures.html) | :x: | :x: | :x: | :x: | :x: | <span title="equivalent defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:warning:</span> | <span title="equivalent defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:warning:</span> |
| [descriptorBindingVariableDescriptorCount](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceDescriptorIndexingFeatures.html) | :x: | :x: | :x: | :x: | :x: | <span title="equivalent defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:warning:</span> | <span title="equivalent defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:warning:</span> |
| [runtimeDescriptorArray](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceDescriptorIndexingFeatures.html) | :x: | :x: | :x: | :x: | :x: | <span title="equivalent defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:warning:</span> | <span title="equivalent defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:warning:</span> |
| [scalarBlockLayout](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceScalarBlockLayoutFeatures.html) | :x: | :x: | :x: | <span title="equivalent defined in VkPhysicalDeviceScalarBlockLayoutFeaturesEXT (VK_EXT_scalar_block_layout)">:warning:</span> | <span title="equivalent defined in VkPhysicalDeviceScalarBlockLayoutFeaturesEXT (VK_EXT_scalar_block_layout)">:warning:</span> | <span title="equivalent defined in VkPhysicalDeviceScalarBlockLayoutFeaturesEXT (VK_EXT_scalar_block_layout)">:warning:</span> | <span title="equivalent defined in VkPhysicalDeviceScalarBlockLayoutFeaturesEXT (VK_EXT_scalar_block_layout)">:warning:</span> |
| [separateDepthStencilLayouts](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures.html) | :x: | :x: | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures (Vulkan 1.2)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures (Vulkan 1.2)">:heavy_check_mark:</span> |
| [shaderBufferInt64Atomics](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceShaderAtomicInt64Features.html) | :x: | :x: | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceShaderAtomicInt64Features (Vulkan 1.2)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceShaderAtomicInt64Features (Vulkan 1.2)">:heavy_check_mark:</span> |
| **Vulkan 1.3** |
| [maintenance4](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceMaintenance4Features.html) | :x: | :x: | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceMaintenance4Features (Vulkan 1.3)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceMaintenance4Features (Vulkan 1.3)">:heavy_check_mark:</span> |
| [synchronization2](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceSynchronization2Features.html) | :x: | :x: | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceSynchronization2Features (Vulkan 1.3)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceSynchronization2Features (Vulkan 1.3)">:heavy_check_mark:</span> |
| **VK_KHR_acceleration_structure** |
| [accelerationStructure](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceAccelerationStructureFeaturesKHR.html) | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceAccelerationStructureFeaturesKHR (VK_KHR_acceleration_structure)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceAccelerationStructureFeaturesKHR (VK_KHR_acceleration_structure)">:heavy_check_mark:</span> | :x: | <span title="defined in VkPhysicalDeviceAccelerationStructureFeaturesKHR (VK_KHR_acceleration_structure)">:heavy_check_mark:</span> |
| [descriptorBindingAccelerationStructureUpdateAfterBind](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceAccelerationStructureFeaturesKHR.html) | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceAccelerationStructureFeaturesKHR (VK_KHR_acceleration_structure)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceAccelerationStructureFeaturesKHR (VK_KHR_acceleration_structure)">:heavy_check_mark:</span> | :x: | <span title="defined in VkPhysicalDeviceAccelerationStructureFeaturesKHR (VK_KHR_acceleration_structure)">:heavy_check_mark:</span> |
| **VK_KHR_buffer_device_address** |
| [bufferDeviceAddress](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceBufferDeviceAddressFeatures.html) | :x: | :x: | :x: | :x: | :x: | <span title="equivalent defined in VkPhysicalDeviceBufferDeviceAddressFeatures (Vulkan 1.2)">:warning:</span> | <span title="equivalent defined in VkPhysicalDeviceBufferDeviceAddressFeatures (Vulkan 1.2)">:warning:</span> |
| **VK_KHR_maintenance4** |
| [maintenance4](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceMaintenance4Features.html) | :x: | :x: | :x: | :x: | :x: | <span title="equivalent defined in VkPhysicalDeviceMaintenance4Features (Vulkan 1.3)">:warning:</span> | <span title="equivalent defined in VkPhysicalDeviceMaintenance4Features (Vulkan 1.3)">:warning:</span> |
| **VK_KHR_ray_query** |
| [rayQuery](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceRayQueryFeaturesKHR.html) | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceRayQueryFeaturesKHR (VK_KHR_ray_query)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceRayQueryFeaturesKHR (VK_KHR_ray_query)">:heavy_check_mark:</span> | :x: | <span title="defined in VkPhysicalDeviceRayQueryFeaturesKHR (VK_KHR_ray_query)">:heavy_check_mark:</span> |
| **VK_KHR_ray_tracing_pipeline** |
| [rayTracingPipeline](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceRayTracingPipelineFeaturesKHR.html) | :x: | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceRayTracingPipelineFeaturesKHR (VK_KHR_ray_tracing_pipeline)">:heavy_check_mark:</span> | :x: | <span title="defined in VkPhysicalDeviceRayTracingPipelineFeaturesKHR (VK_KHR_ray_tracing_pipeline)">:heavy_check_mark:</span> |
| [rayTraversalPrimitiveCulling](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceRayTracingPipelineFeaturesKHR.html) | :x: | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceRayTracingPipelineFeaturesKHR (VK_KHR_ray_tracing_pipeline)">:heavy_check_mark:</span> | :x: | <span title="defined in VkPhysicalDeviceRayTracingPipelineFeaturesKHR (VK_KHR_ray_tracing_pipeline)">:heavy_check_mark:</span> |
| **VK_KHR_separate_depth_stencil_layouts** |
| [separateDepthStencilLayouts](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures.html) | :x: | :x: | :x: | :x: | :x: | <span title="equivalent defined in VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures (Vulkan 1.2)">:warning:</span> | <span title="equivalent defined in VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures (Vulkan 1.2)">:warning:</span> |
| **VK_KHR_shader_atomic_int64** |
| [shaderBufferInt64Atomics](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceShaderAtomicInt64Features.html) | :x: | :x: | :x: | :x: | :x: | <span title="equivalent defined in VkPhysicalDeviceShaderAtomicInt64Features (Vulkan 1.2)">:warning:</span> | <span title="equivalent defined in VkPhysicalDeviceShaderAtomicInt64Features (Vulkan 1.2)">:warning:</span> |
| **VK_KHR_synchronization2** |
| [synchronization2](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceSynchronization2Features.html) | :x: | :x: | :x: | :x: | :x: | <span title="equivalent defined in VkPhysicalDeviceSynchronization2Features (Vulkan 1.3)">:warning:</span> | <span title="equivalent defined in VkPhysicalDeviceSynchronization2Features (Vulkan 1.3)">:warning:</span> |
| **VK_EXT_descriptor_indexing** |
| [descriptorBindingPartiallyBound](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceDescriptorIndexingFeatures.html) | :x: | :x: | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:heavy_check_mark:</span> |
| [descriptorBindingUpdateUnusedWhilePending](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceDescriptorIndexingFeatures.html) | :x: | :x: | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:heavy_check_mark:</span> |
| [descriptorBindingVariableDescriptorCount](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceDescriptorIndexingFeatures.html) | :x: | :x: | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:heavy_check_mark:</span> |
| [runtimeDescriptorArray](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceDescriptorIndexingFeatures.html) | :x: | :x: | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceDescriptorIndexingFeaturesEXT (VK_EXT_descriptor_indexing)">:heavy_check_mark:</span> |
| **VK_EXT_scalar_block_layout** |
| [scalarBlockLayout](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceScalarBlockLayoutFeatures.html) | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceScalarBlockLayoutFeaturesEXT (VK_EXT_scalar_block_layout)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceScalarBlockLayoutFeaturesEXT (VK_EXT_scalar_block_layout)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceScalarBlockLayoutFeaturesEXT (VK_EXT_scalar_block_layout)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceScalarBlockLayoutFeaturesEXT (VK_EXT_scalar_block_layout)">:heavy_check_mark:</span> |
| **VK_EXT_shader_image_atomic_int64** |
| [shaderImageInt64Atomics](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT.html) | :x: | :x: | :x: | :x: | :x: | <span title="defined in VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT (VK_EXT_shader_image_atomic_int64)">:heavy_check_mark:</span> | <span title="defined in VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT (VK_EXT_shader_image_atomic_int64)">:heavy_check_mark:</span> |

## Vulkan Profile Limits (Properties)

> **NOTE**: The table below only contains properties/limits explicitly defined by the corresponding profile. Further properties/limits may be supported by the profiles in accordance to the requirements defined in the "Limit Requirements" section of the appropriate version of the Vulkan API Specification.

* "valueWithRegularFont" indicates that the limit/property is defined in the profile (hover over the value to view the structure and corresponding extension or core API version where the limit/property is defined in the profile)
* "_valueWithItalicFont_" indicates that the limit/property is not defined in the profile but an equivalent limit/property is (hover over the symbol to view the structure and corresponding extension or core API version where the limit/property is defined in the profile)
* "-" indicates that neither the limit/property nor an equivalent limit/property is defined in the profile

| Profiles | VP_UE_Vulkan_ES3_1_Android | VP_UE_Vulkan_SM5 | VP_UE_Vulkan_SM5_Android | VP_UE_Vulkan_SM5_Android_RT | VP_UE_Vulkan_SM5_RT | VP_UE_Vulkan_SM6 | VP_UE_Vulkan_SM6_RT |
|----------|----------------------------|------------------|--------------------------|-----------------------------|---------------------|------------------|---------------------|
| **Vulkan 1.0** |
| [maxBoundDescriptorSets (max)](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPhysicalDeviceLimits.html) | - | <span title="defined in VkPhysicalDeviceProperties (Vulkan 1.0)">4</span> | - | <span title="defined in VkPhysicalDeviceProperties (Vulkan 1.0)">7</span> | <span title="defined in VkPhysicalDeviceProperties (Vulkan 1.0)">8</span> | - | <span title="defined in VkPhysicalDeviceProperties (Vulkan 1.0)">8</span> |

## Vulkan Profile Queue Families

* "valueWithRegularFont" indicates that the queue family property is defined in the profile (hover over the value to view the structure and corresponding extension or core API version where the queue family property is defined in the profile)
* "_valueWithItalicFont_" indicates that the queue family property is not defined in the profile but an equivalent queue family property is (hover over the symbol to view the structure and corresponding extension or core API version where the queue family property is defined in the profile)
* "-" indicates that neither the queue family property nor an equivalent queue family property is defined in the profile
* Empty cells next to the properties of a particular queue family definition section indicate that the profile does not have a corresponding queue family definition

| Profiles | VP_UE_Vulkan_ES3_1_Android | VP_UE_Vulkan_SM5 | VP_UE_Vulkan_SM5_Android | VP_UE_Vulkan_SM5_Android_RT | VP_UE_Vulkan_SM5_RT | VP_UE_Vulkan_SM6 | VP_UE_Vulkan_SM6_RT |
|----------|----------------------------|------------------|--------------------------|-----------------------------|---------------------|------------------|---------------------|

## Vulkan Profile Formats

> **NOTE**: The table below only contains formats and properties explicitly defined by the corresponding profile. Further formats and properties may be supported by the profiles in accordance to the requirements defined in the "Required Format Support" section of the appropriate version of the Vulkan API Specification.

* "valueWithRegularFont" indicates that the format property is defined in the profile (hover over the value to view the structure and corresponding extension or core API version where the format property is defined in the profile)
* "_valueWithItalicFont_" indicates that the format property is not defined in the profile but an equivalent format property is (hover over the symbol to view the structure and corresponding extension or core API version where the format property is defined in the profile)
* "-" indicates that neither the format property nor an equivalent format property is defined in the profile
* Empty cells next to the properties of a particular format definition section indicate that the profile does not have a corresponding format definition

| Profiles | VP_UE_Vulkan_ES3_1_Android | VP_UE_Vulkan_SM5 | VP_UE_Vulkan_SM5_Android | VP_UE_Vulkan_SM5_Android_RT | VP_UE_Vulkan_SM5_RT | VP_UE_Vulkan_SM6 | VP_UE_Vulkan_SM6_RT |
|----------|----------------------------|------------------|--------------------------|-----------------------------|---------------------|------------------|---------------------|
| **VK_FORMAT_R64_UINT** |
| [optimalTilingFeatures](https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkFormatProperties3.html) |  |  |  |  |  | <span title="defined in VkFormatProperties (Vulkan 1.0)">(VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT \| VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT)</span> | <span title="defined in VkFormatProperties (Vulkan 1.0)">(VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT \| VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT)</span> |
