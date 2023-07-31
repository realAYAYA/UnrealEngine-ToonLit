// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanLLM.h: Vulkan LLM definitions.
=============================================================================*/


#pragma once

#include "HAL/LowLevelMemTracker.h"

#if VULKAN_USE_LLM

#define LLM_SCOPE_VULKAN(Tag)			LLM_SCOPE((ELLMTag)Tag)
#define LLM_PLATFORM_SCOPE_VULKAN(Tag)	LLM_PLATFORM_SCOPE((ELLMTag)Tag)
extern uint64 GVulkanLLMAllocationID;
#define LLM_TRACK_VULKAN_HIGH_LEVEL_ALLOC(AllocObj, Size) { AllocObj.SetLLMTrackerID(0xDEAD | (++GVulkanLLMAllocationID << 16)); FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, (void*)AllocObj.GetLLMTrackerID(), Size, (ELLMTag)ELLMTagVulkan::VulkanMisc); }
#define LLM_TRACK_VULKAN_HIGH_LEVEL_FREE(AllocObj) { FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Default, (void*)AllocObj.GetLLMTrackerID()); }
#define LLM_TRACK_VULKAN_SPARE_MEMORY_GPU(Size)	{ LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT((ELLMTag)ELLMTagVulkan::VulkanSpareMemoryGPU, Size, ELLMTracker::Default, ELLMAllocType::None); }

#define LLM_TRACK_VULKAN_HIGH_LEVEL_ALLOCATION
enum class ELLMTagVulkan : LLM_TAG_TYPE
{
	VulkanMisc = (LLM_TAG_TYPE)ELLMTag::PlatformTagStart,
	VulkanUniformBuffers,
	VulkanBuffers,
	VulkanTextures,
	VulkanRenderTargets,
	VulkanSpareMemoryGPU,
	VulkanShaders,
	VulkanFrameTemp,
	VulkanStagingBuffers,
	VulkanDriverMemoryCPU,
	VulkanDriverMemoryGPU,
	Count,
};

static_assert((int32)ELLMTagVulkan::Count <= (int32)ELLMTag::PlatformTagEnd, "too many ELLMTagVulkan tags");

namespace VulkanLLM
{
	void Initialize();
}

#else

#define LLM_SCOPE_VULKAN(...)
#define LLM_PLATFORM_SCOPE_VULKAN(...)
#define LLM_TRACK_VULKAN_HIGH_LEVEL_ALLOC(...)
#define LLM_TRACK_VULKAN_HIGH_LEVEL_FREE(...)
#define LLM_TRACK_VULKAN_SPARE_MEMORY_GPU(...)

#endif		// #if VULKAN_USE_LLM
