// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanQueue.cpp: Vulkan Queue implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanQueue.h"
#include "VulkanMemory.h"
#include "VulkanContext.h"

int32 GWaitForIdleOnSubmit = 0;
FAutoConsoleVariableRef CVarVulkanWaitForIdleOnSubmit(
	TEXT("r.Vulkan.WaitForIdleOnSubmit"),
	GWaitForIdleOnSubmit,
	TEXT("Waits for the GPU to be idle after submitting a command buffer. Useful for tracking GPU hangs.\n")
	TEXT(" 0: Do not wait(default)\n")
	TEXT(" 1: Wait on every submit\n")
	TEXT(" 2: Wait when submitting an upload buffer\n")
	TEXT(" 3: Wait when submitting an active buffer (one that has gfx commands)\n"),
	ECVF_Default
	);

FVulkanQueue::FVulkanQueue(FVulkanDevice* InDevice, uint32 InFamilyIndex)
	: Queue(VK_NULL_HANDLE)
	, FamilyIndex(InFamilyIndex)
	, QueueIndex(0)
	, Device(InDevice)
	, LastSubmittedCmdBuffer(nullptr)
	, LastSubmittedCmdBufferFenceCounter(0)
	, SubmitCounter(0)
	, LayoutManager(false, nullptr)
{
	VulkanRHI::vkGetDeviceQueue(Device->GetInstanceHandle(), FamilyIndex, QueueIndex, &Queue);

	FillSupportedStageBits();
}

FVulkanQueue::~FVulkanQueue()
{
	check(Device);
}

void FVulkanQueue::Submit(FVulkanCmdBuffer* CmdBuffer, uint32 NumSignalSemaphores, VkSemaphore* SignalSemaphores)
{
	check(CmdBuffer->HasEnded());

	VulkanRHI::FFence* Fence = CmdBuffer->Fence;
	check(!Fence->IsSignaled());

	const VkCommandBuffer CmdBuffers[] = { CmdBuffer->GetHandle() };

	VkSubmitInfo SubmitInfo;
	ZeroVulkanStruct(SubmitInfo, VK_STRUCTURE_TYPE_SUBMIT_INFO);
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = CmdBuffers;
	SubmitInfo.signalSemaphoreCount = NumSignalSemaphores;
	SubmitInfo.pSignalSemaphores = SignalSemaphores;

	TArray<VkSemaphore> WaitSemaphores;
	if (CmdBuffer->WaitSemaphores.Num() > 0)
	{
		WaitSemaphores.Empty((uint32)CmdBuffer->WaitSemaphores.Num());
		for (VulkanRHI::FSemaphore* Semaphore : CmdBuffer->WaitSemaphores)
		{
			WaitSemaphores.Add(Semaphore->GetHandle());
		}
		SubmitInfo.waitSemaphoreCount = (uint32)CmdBuffer->WaitSemaphores.Num();
		SubmitInfo.pWaitSemaphores = WaitSemaphores.GetData();
		SubmitInfo.pWaitDstStageMask = CmdBuffer->WaitFlags.GetData();
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanQueueSubmit);
		VERIFYVULKANRESULT(VulkanRHI::vkQueueSubmit(Queue, 1, &SubmitInfo, Fence->GetHandle()));
	}

	CmdBuffer->State = FVulkanCmdBuffer::EState::Submitted;
	CmdBuffer->MarkSemaphoresAsSubmitted();
	CmdBuffer->SubmittedFenceCounter = CmdBuffer->FenceSignaledCounter;

	bool bShouldStall = false;

	if (GWaitForIdleOnSubmit != 0)
	{
		FVulkanCommandBufferManager* CmdBufferMgr = &CmdBuffer->GetOwner()->GetMgr();

		switch(GWaitForIdleOnSubmit)
		{
			default:
				// intentional fall-through
			case 1:
				bShouldStall = true;
				break;

			case 2:
				bShouldStall = (CmdBufferMgr->HasPendingUploadCmdBuffer() && CmdBufferMgr->GetUploadCmdBuffer() == CmdBuffer);
				break;

			case 3:
				bShouldStall = (CmdBufferMgr->HasPendingActiveCmdBuffer() && CmdBufferMgr->GetActiveCmdBufferDirect() == CmdBuffer);
				break;
		}
	}

	if (bShouldStall)
	{
		// 200 ms timeout
		bool bSuccess = Device->GetFenceManager().WaitForFence(CmdBuffer->Fence, 200 * 1000 * 1000);
		ensure(bSuccess);
		ensure(Device->GetFenceManager().IsFenceSignaled(CmdBuffer->Fence));
		CmdBuffer->GetOwner()->RefreshFenceStatus();
	}

	UpdateLastSubmittedCommandBuffer(CmdBuffer);

	CmdBuffer->GetOwner()->RefreshFenceStatus(CmdBuffer);

	Device->GetStagingManager().ProcessPendingFree(false, false);

	// If we're tracking layouts for the queue, merge in the changes recorded in this command buffer's context
	CmdBuffer->GetLayoutManager().TransferTo(LayoutManager);
}

void FVulkanQueue::UpdateLastSubmittedCommandBuffer(FVulkanCmdBuffer* CmdBuffer)
{
	FScopeLock ScopeLock(&CS);
	LastSubmittedCmdBuffer = CmdBuffer;
	LastSubmittedCmdBufferFenceCounter = CmdBuffer->GetFenceSignaledCounterH();
	++SubmitCounter;
}

void FVulkanQueue::FillSupportedStageBits()
{
	check(Device);
	check((int32)FamilyIndex < Device->GetQueueFamilyProps().Num());

	const VkQueueFamilyProperties& QueueProps = Device->GetQueueFamilyProps()[FamilyIndex];

	SupportedStages = 
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | 
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT |
		VK_PIPELINE_STAGE_HOST_BIT |
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

	if (VKHasAnyFlags(QueueProps.queueFlags, VK_QUEUE_GRAPHICS_BIT))
	{
		SupportedStages |=
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
			VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
			VK_PIPELINE_STAGE_TRANSFER_BIT |
			VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;

		if (Device->GetPhysicalDeviceFeatures().Core_1_0.geometryShader)
		{
			SupportedStages |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
		}
		if (Device->GetOptionalExtensions().HasKHRFragmentShadingRate)
		{
			SupportedStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
		}
		if (Device->GetOptionalExtensions().HasEXTFragmentDensityMap)
		{
			SupportedStages |= VK_PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT_EXT;
		}
	}

	if (VKHasAnyFlags(QueueProps.queueFlags, VK_QUEUE_COMPUTE_BIT))
	{
		SupportedStages |=
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT |
			VK_PIPELINE_STAGE_TRANSFER_BIT;

#if VULKAN_RHI_RAYTRACING
		SupportedStages |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
#endif
	}

	if (VKHasAnyFlags(QueueProps.queueFlags, VK_QUEUE_TRANSFER_BIT))
	{
		SupportedStages |= VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
}

void FVulkanQueue::NotifyDeletedImage(VkImage Image)
{
	LayoutManager.NotifyDeletedImage(Image);
}
