// Copyright Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanQueue.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanConfiguration.h"

class FVulkanDevice;
class FVulkanCmdBuffer;
class FVulkanCommandListContext;

namespace VulkanRHI
{
	class FFence;
	class FSemaphore;
}

class FVulkanQueue
{
public:
	FVulkanQueue(FVulkanDevice* InDevice, uint32 InFamilyIndex);
	~FVulkanQueue();

	inline uint32 GetFamilyIndex() const
	{
		return FamilyIndex;
	}

	inline uint32 GetQueueIndex() const
	{
		return QueueIndex;
	}

	void Submit(FVulkanCmdBuffer* CmdBuffer, uint32 NumSignalSemaphores = 0, VkSemaphore* SignalSemaphores = nullptr);

	inline void Submit(FVulkanCmdBuffer* CmdBuffer, VkSemaphore SignalSemaphore)
	{
		Submit(CmdBuffer, 1, &SignalSemaphore);
	}

	inline VkQueue GetHandle() const
	{
		return Queue;
	}

	void GetLastSubmittedInfo(FVulkanCmdBuffer*& OutCmdBuffer, uint64& OutFenceCounter) const
	{
		FScopeLock ScopeLock(&CS);
		OutCmdBuffer = LastSubmittedCmdBuffer;
		OutFenceCounter = LastSubmittedCmdBufferFenceCounter;
	}

	inline uint64 GetSubmitCount() const
	{
		return SubmitCounter;
	}

	inline VkPipelineStageFlags GetSupportedStageBits()
	{
		return SupportedStages;
	}

private:
	VkQueue Queue;
	uint32 FamilyIndex;
	uint32 QueueIndex;
	FVulkanDevice* Device;

	mutable FCriticalSection CS;
	FVulkanCmdBuffer* LastSubmittedCmdBuffer;
	uint64 LastSubmittedCmdBufferFenceCounter;
	uint64 SubmitCounter;
	VkPipelineStageFlags SupportedStages;

	void UpdateLastSubmittedCommandBuffer(FVulkanCmdBuffer* CmdBuffer);
	void FillSupportedStageBits();
};
