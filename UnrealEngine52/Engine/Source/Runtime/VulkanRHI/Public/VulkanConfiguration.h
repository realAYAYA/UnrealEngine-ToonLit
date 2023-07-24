// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanConfiguration.h: Control compilation of the runtime RHI.
=============================================================================*/

// Compiled with 1.2.141.2

#pragma once

#include "HAL/LowLevelMemTracker.h"
#include "VulkanCommon.h"

struct VkAllocationCallbacks;

// API version we want to target.
#ifndef UE_VK_API_VERSION
	#define UE_VK_API_VERSION									VK_API_VERSION_1_0
#endif

// by default, we enable debugging in Development builds, unless the platform says not to
#ifndef VULKAN_SHOULD_DEBUG_IN_DEVELOPMENT
	#define VULKAN_SHOULD_DEBUG_IN_DEVELOPMENT 1
#endif

#define VULKAN_HAS_DEBUGGING_ENABLED							(UE_BUILD_DEBUG || (UE_BUILD_DEVELOPMENT && VULKAN_SHOULD_DEBUG_IN_DEVELOPMENT))

#ifndef VULKAN_SHOULD_ENABLE_DRAW_MARKERS
	#define VULKAN_SHOULD_ENABLE_DRAW_MARKERS					0
#endif

// Enables logging wrappers per Vulkan call
#ifndef VULKAN_ENABLE_DUMP_LAYER
	#define VULKAN_ENABLE_DUMP_LAYER							0
#endif

#define VULKAN_ENABLE_DRAW_MARKERS								VULKAN_SHOULD_ENABLE_DRAW_MARKERS

#ifndef VULKAN_ENABLE_IMAGE_TRACKING_LAYER
	#define VULKAN_ENABLE_IMAGE_TRACKING_LAYER					0
#endif

#ifndef VULKAN_ENABLE_BUFFER_TRACKING_LAYER
	#define VULKAN_ENABLE_BUFFER_TRACKING_LAYER					0
#endif

#define VULKAN_ENABLE_TRACKING_LAYER							(VULKAN_ENABLE_BUFFER_TRACKING_LAYER || VULKAN_ENABLE_IMAGE_TRACKING_LAYER)
#define VULKAN_ENABLE_WRAP_LAYER								(VULKAN_ENABLE_DUMP_LAYER || VULKAN_ENABLE_TRACKING_LAYER)

#define VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID					1

#define VULKAN_SINGLE_ALLOCATION_PER_RESOURCE					0

#ifndef VULKAN_SHOULD_USE_LLM
	#define VULKAN_SHOULD_USE_LLM								0
#endif

#ifndef VULKAN_USE_LLM
	#define VULKAN_USE_LLM										((ENABLE_LOW_LEVEL_MEM_TRACKER) && VULKAN_SHOULD_USE_LLM)
#endif

#ifndef VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED
	#define VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED				VULKAN_USE_LLM
#endif

#ifndef VULKAN_SHOULD_USE_COMMANDWRAPPERS
	#define VULKAN_SHOULD_USE_COMMANDWRAPPERS					VULKAN_ENABLE_WRAP_LAYER
#endif

#ifndef VULKAN_COMMANDWRAPPERS_ENABLE
	#define VULKAN_COMMANDWRAPPERS_ENABLE						VULKAN_SHOULD_USE_COMMANDWRAPPERS
#endif

#ifndef VULKAN_USE_IMAGE_ACQUIRE_FENCES
	#define VULKAN_USE_IMAGE_ACQUIRE_FENCES						1
#endif

#define VULKAN_ENABLE_AGGRESSIVE_STATS							0

#define VULKAN_REUSE_FENCES										1

#ifndef VULKAN_QUERY_CALLSTACK
	#define VULKAN_QUERY_CALLSTACK								0
#endif

#ifndef VULKAN_ENABLE_DESKTOP_HMD_SUPPORT
	#define VULKAN_ENABLE_DESKTOP_HMD_SUPPORT					0
#endif

#ifndef VULKAN_SIGNAL_UNIMPLEMENTED
	#define VULKAN_SIGNAL_UNIMPLEMENTED()
#endif

#ifndef VULKAN_ENABLE_LRU_CACHE
	#define VULKAN_ENABLE_LRU_CACHE								0
#endif

#ifdef VK_EXT_validation_cache
	#define VULKAN_SUPPORTS_VALIDATION_CACHE					1
#else
	#define VULKAN_SUPPORTS_VALIDATION_CACHE					0
#endif

#ifdef VK_EXT_validation_features
	#define VULKAN_HAS_VALIDATION_FEATURES						1
#else
	#define VULKAN_HAS_VALIDATION_FEATURES						0
#endif

#ifndef VULKAN_SUPPORTS_DEDICATED_ALLOCATION
	#ifdef VK_KHR_dedicated_allocation
		#define VULKAN_SUPPORTS_DEDICATED_ALLOCATION			1
	#else
		#define VULKAN_SUPPORTS_DEDICATED_ALLOCATION			0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING
	#define VULKAN_SUPPORTS_GOOGLE_DISPLAY_TIMING				0
#endif

#ifndef VULKAN_USE_CREATE_ANDROID_SURFACE
	#define VULKAN_USE_CREATE_ANDROID_SURFACE					0
#endif

#ifndef VULKAN_USE_CREATE_WIN32_SURFACE
	#define VULKAN_USE_CREATE_WIN32_SURFACE						0
#endif

#ifndef VULKAN_USE_REAL_RENDERPASS_COMPATIBILITY
	#define VULKAN_USE_REAL_RENDERPASS_COMPATIBILITY			1
#endif

#ifndef VULKAN_USE_DIFFERENT_POOL_CMDBUFFERS
	#define VULKAN_USE_DIFFERENT_POOL_CMDBUFFERS				1
#endif

#ifndef VULKAN_DELETE_STALE_CMDBUFFERS
	#define VULKAN_DELETE_STALE_CMDBUFFERS						1
#endif

#ifndef VULKAN_SUPPORTS_AMD_BUFFER_MARKER
	#define VULKAN_SUPPORTS_AMD_BUFFER_MARKER					0
#endif

#ifndef VULKAN_SUPPORTS_NV_DIAGNOSTIC_CHECKPOINT
	#ifdef VK_NV_device_diagnostic_checkpoints
		#define VULKAN_SUPPORTS_NV_DIAGNOSTIC_CHECKPOINT		1
	#else
		#define VULKAN_SUPPORTS_NV_DIAGNOSTIC_CHECKPOINT		0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_NV_DIAGNOSTIC_CONFIG
	#ifdef VK_NV_device_diagnostics_config
		#define VULKAN_SUPPORTS_NV_DIAGNOSTIC_CONFIG			1
	#else
		#define VULKAN_SUPPORTS_NV_DIAGNOSTIC_CONFIG			0
	#endif
#endif

#define VULKAN_SUPPORTS_NV_DIAGNOSTICS							(VULKAN_SUPPORTS_NV_DIAGNOSTIC_CHECKPOINT && VULKAN_SUPPORTS_NV_DIAGNOSTIC_CONFIG)

#ifndef VULKAN_SUPPORTS_GPU_CRASH_DUMPS
	#define VULKAN_SUPPORTS_GPU_CRASH_DUMPS						(VULKAN_SUPPORTS_AMD_BUFFER_MARKER || VULKAN_SUPPORTS_NV_DIAGNOSTIC_CONFIG)
#endif

#ifndef VULKAN_SUPPORTS_DEBUG_UTILS
	#ifdef VK_EXT_debug_utils
		#define VULKAN_SUPPORTS_DEBUG_UTILS						1
	#else
		#define VULKAN_SUPPORTS_DEBUG_UTILS						0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT
	#ifdef VK_EXT_scalar_block_layout
		#define VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT				1
	#else
		#define VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT				0
	#endif
#endif


#ifndef VULKAN_SUPPORTS_MEMORY_BUDGET
	#ifdef VK_EXT_memory_budget
		#define VULKAN_SUPPORTS_MEMORY_BUDGET					1
	#else
		#define VULKAN_SUPPORTS_MEMORY_BUDGET					0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_MEMORY_PRIORITY
	#ifdef VK_EXT_memory_priority
		#define VULKAN_SUPPORTS_MEMORY_PRIORITY					1
	#else
		#define VULKAN_SUPPORTS_MEMORY_PRIORITY					0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_PHYSICAL_DEVICE_PROPERTIES2
	#ifdef VK_KHR_get_physical_device_properties2
		#define VULKAN_SUPPORTS_PHYSICAL_DEVICE_PROPERTIES2		1
	#else
		#define VULKAN_SUPPORTS_PHYSICAL_DEVICE_PROPERTIES2		0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_EXTERNAL_MEMORY
	#ifdef VK_KHR_external_memory_capabilities
		#define VULKAN_SUPPORTS_EXTERNAL_MEMORY					(VULKAN_SUPPORTS_PHYSICAL_DEVICE_PROPERTIES2)	// Requirement
	#else
		#define VULKAN_SUPPORTS_EXTERNAL_MEMORY					0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_DRIVER_PROPERTIES
	#ifdef VK_KHR_driver_properties
		#define VULKAN_SUPPORTS_DRIVER_PROPERTIES				1
	#else
		#define VULKAN_SUPPORTS_DRIVER_PROPERTIES				0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_QCOM_RENDERPASS_TRANSFORM
	#define VULKAN_SUPPORTS_QCOM_RENDERPASS_TRANSFORM			0
#endif

#ifndef VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE
	#ifdef VK_EXT_full_screen_exclusive
		#define VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE			1
	#else
		#define VULKAN_SUPPORTS_FULLSCREEN_EXCLUSIVE			0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_TEXTURE_COMPRESSION_ASTC_HDR
#ifdef VK_EXT_texture_compression_astc_hdr
#define VULKAN_SUPPORTS_TEXTURE_COMPRESSION_ASTC_HDR			1
#else
#define VULKAN_SUPPORTS_TEXTURE_COMPRESSION_ASTC_HDR			0
#endif
#endif

#ifndef VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP
	#ifdef VK_EXT_fragment_density_map
		#define VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP			1
	#else
		#define VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP			0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP2
	#ifdef VK_EXT_fragment_density_map2
		#define VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP2			1
	#else
		#define VULKAN_SUPPORTS_FRAGMENT_DENSITY_MAP2			0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_FRAGMENT_SHADING_RATE
	#ifdef VK_KHR_fragment_shading_rate
		#define VULKAN_SUPPORTS_FRAGMENT_SHADING_RATE 1
	#else
		#define VULKAN_SUPPORTS_FRAGMENT_SHADING_RATE 0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_RENDERPASS2
	#ifdef VK_KHR_create_renderpass2
		#define VULKAN_SUPPORTS_RENDERPASS2 1
	#else
		#define VULKAN_SUPPORTS_RENDERPASS2 0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_MULTIVIEW
	#ifdef VK_KHR_multiview
		#define VULKAN_SUPPORTS_MULTIVIEW						1
	#else
		#define VULKAN_SUPPORTS_MULTIVIEW						0
	#endif
#endif

#ifndef VULKAN_SUPPORTS_ASTC_DECODE_MODE
	#ifdef VK_EXT_astc_decode_mode
		#define VULKAN_SUPPORTS_ASTC_DECODE_MODE				(VULKAN_SUPPORTS_PHYSICAL_DEVICE_PROPERTIES2)	// Requirement
	#else
		#define VULKAN_SUPPORTS_ASTC_DECODE_MODE				0
	#endif
#endif

#ifdef VK_KHR_shader_atomic_int64
	#define VULKAN_SUPPORTS_BUFFER_64BIT_ATOMICS	(VULKAN_SUPPORTS_PHYSICAL_DEVICE_PROPERTIES2)	// Requirement
#else
	#define VULKAN_SUPPORTS_BUFFER_64BIT_ATOMICS	0
#endif

#ifdef VK_EXT_shader_image_atomic_int64
	#define VULKAN_SUPPORTS_IMAGE_64BIT_ATOMICS		(VULKAN_SUPPORTS_PHYSICAL_DEVICE_PROPERTIES2)	// Requirement
#else
	#define VULKAN_SUPPORTS_IMAGE_64BIT_ATOMICS		0
#endif

#ifdef VK_EXT_shader_viewport_index_layer
	#define VULKAN_SUPPORTS_SHADER_VIEWPORT_INDEX_LAYER	1
#else
	#define VULKAN_SUPPORTS_SHADER_VIEWPORT_INDEX_LAYER	0
#endif

#ifndef VULKAN_SUPPORTS_DESCRIPTOR_INDEXING
	#ifdef VK_EXT_descriptor_indexing
		#define VULKAN_SUPPORTS_DESCRIPTOR_INDEXING	(VULKAN_SUPPORTS_PHYSICAL_DEVICE_PROPERTIES2)	// Requirement
	#else
		#define VULKAN_SUPPORTS_DESCRIPTOR_INDEXING	0
	#endif
#endif

#ifndef VULKAN_OBJECT_TRACKING 
#define VULKAN_OBJECT_TRACKING 0 //Track objects created and memory used. use r.vulkan.dumpmemory to dump to console
#endif

VULKANRHI_API DECLARE_LOG_CATEGORY_EXTERN(LogVulkanRHI, Log, All);

#if VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED
	#define VULKAN_CPU_ALLOCATOR								VulkanRHI::GetMemoryAllocator(nullptr)
#else
	#define VULKAN_CPU_ALLOCATOR								nullptr
#endif

#ifndef VULKAN_PURGE_SHADER_MODULES
	#define VULKAN_PURGE_SHADER_MODULES							0
#endif

#ifndef VULKAN_SUPPORTS_TRANSIENT_RESOURCE_ALLOCATOR
	#define VULKAN_SUPPORTS_TRANSIENT_RESOURCE_ALLOCATOR		1
#endif


#if !defined(NV_AFTERMATH)
	#define NV_AFTERMATH 0
#endif


#ifndef VK_TYPE_TO_STRING
#	define VK_TYPE_TO_STRING(Type, Value) *FString::Printf(TEXT("%u"), (uint32)Value)
#endif
#ifndef VK_FLAGS_TO_STRING
#	define VK_FLAGS_TO_STRING(Type, Value) *FString::Printf(TEXT("%u"), (uint32)Value)
#endif


namespace VulkanRHI
{
	static FORCEINLINE const VkAllocationCallbacks* GetMemoryAllocator(const VkAllocationCallbacks* Allocator)
	{
#if VULKAN_CUSTOM_MEMORY_MANAGER_ENABLED
		extern VkAllocationCallbacks GAllocationCallbacks;
		return Allocator ? Allocator : &GAllocationCallbacks;
#else
		return Allocator;
#endif
	}
}
