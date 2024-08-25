// Copyright Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanCommandBuffer.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "VulkanConfiguration.h"
#include "VulkanBarriers.h"

extern int32 GVulkanProfileCmdBuffers;
extern int32 GVulkanUseCmdBufferTimingForGPUTime;

class FVulkanDevice;
class FVulkanCommandBufferPool;
class FVulkanCommandBufferManager;
class FVulkanRenderTargetLayout;
class FVulkanQueue;
class FVulkanDescriptorPoolSetContainer;
class FVulkanGPUTiming;

namespace VulkanRHI
{
	class FFence;
	class FSemaphore;
}

class FVulkanCmdBuffer
{
protected:
	friend class FVulkanCommandBufferManager;
	friend class FVulkanCommandBufferPool;
	friend class FVulkanQueue;

	FVulkanCmdBuffer(FVulkanDevice* InDevice, FVulkanCommandBufferPool* InCommandBufferPool, bool bInIsUploadOnly);
	~FVulkanCmdBuffer();

public:
	FVulkanCommandBufferPool* GetOwner()
	{
		return CommandBufferPool;
	}

	FVulkanDevice* GetDevice()
	{
		return Device;
	}

	bool IsUniformBufferBarrierAdded() const
	{
		return bIsUniformBufferBarrierAdded;
	}

	inline bool IsInsideRenderPass() const
	{
		return State == EState::IsInsideRenderPass;
	}

	inline bool IsOutsideRenderPass() const
	{
		return State == EState::IsInsideBegin;
	}

	inline bool HasBegun() const
	{
		return State == EState::IsInsideBegin || State == EState::IsInsideRenderPass;
	}

	inline bool HasEnded() const
	{
		return State == EState::HasEnded;
	}

	inline bool IsSubmitted() const
	{
		return State == EState::Submitted;
	}

	inline bool IsAllocated() const
	{
		return State != EState::NotAllocated;
	}

	inline VkCommandBuffer GetHandle()
	{
		return CommandBufferHandle;
	}

	inline volatile uint64 GetFenceSignaledCounter() const
	{
		return FenceSignaledCounter;
	}

	//#todo-rco: Temp to help find out where the crash is coming from!
	inline volatile uint64 GetFenceSignaledCounterA() const
	{
		return FenceSignaledCounter;
	}

	inline volatile uint64 GetFenceSignaledCounterB() const
	{
		return FenceSignaledCounter;
	}

	inline volatile uint64 GetFenceSignaledCounterC() const
	{
		return FenceSignaledCounter;
	}

	inline volatile uint64 GetFenceSignaledCounterD() const
	{
		return FenceSignaledCounter;
	}

	inline volatile uint64 GetFenceSignaledCounterE() const
	{
		return FenceSignaledCounter;
	}

	inline volatile uint64 GetFenceSignaledCounterF() const
	{
		return FenceSignaledCounter;
	}

	inline volatile uint64 GetFenceSignaledCounterG() const
	{
		return FenceSignaledCounter;
	}

	inline volatile uint64 GetFenceSignaledCounterH() const
	{
		return FenceSignaledCounter;
	}

	inline volatile uint64 GetFenceSignaledCounterI() const
	{
		return FenceSignaledCounter;
	}

	inline volatile uint64 GetSubmittedFenceCounter() const
	{
		return SubmittedFenceCounter;
	}

	inline FVulkanLayoutManager& GetLayoutManager()
	{
		return LayoutManager;
	}

	inline bool HasValidTiming() const
	{
		return (Timing != nullptr) && (FMath::Abs((int64)FenceSignaledCounter - (int64)LastValidTiming) < 3);
	}

	void VULKANRHI_API AddWaitSemaphore(VkPipelineStageFlags InWaitFlags, VulkanRHI::FSemaphore* InWaitSemaphore)
	{
		AddWaitSemaphore(InWaitFlags, MakeArrayView<VulkanRHI::FSemaphore*>(&InWaitSemaphore, 1));
	}

	void VULKANRHI_API AddWaitSemaphore(VkPipelineStageFlags InWaitFlags, TArrayView<VulkanRHI::FSemaphore*> InWaitSemaphores);

	void Begin();
	void End();

	enum class EState : uint8
	{
		ReadyForBegin,
		IsInsideBegin,
		IsInsideRenderPass,
		HasEnded,
		Submitted,
		NotAllocated,
		NeedReset,
	};

	TArray<VkViewport, TInlineAllocator<2>> CurrentViewports;
	TArray<VkRect2D, TInlineAllocator<2>> CurrentScissors;
	uint32 CurrentStencilRef;
	EState State;
	uint8 bNeedsDynamicStateSet			: 1;
	uint8 bHasPipeline					: 1;
	uint8 bHasViewport					: 1;
	uint8 bHasScissor					: 1;
	uint8 bHasStencilRef				: 1;
	uint8 bIsUploadOnly					: 1;
	uint8 bIsUniformBufferBarrierAdded	: 1;

	// You never want to call Begin/EndRenderPass directly as it will mess up the layout manager.
	void BeginRenderPass(const FVulkanRenderTargetLayout& Layout, class FVulkanRenderPass* RenderPass, class FVulkanFramebuffer* Framebuffer, const VkClearValue* AttachmentClearValues);
	void EndRenderPass();


	void BeginUniformUpdateBarrier();
	void EndUniformUpdateBarrier();
	//#todo-rco: Hide this
	FVulkanDescriptorPoolSetContainer* CurrentDescriptorPoolSetContainer = nullptr;

	bool AcquirePoolSetAndDescriptorsIfNeeded(const class FVulkanDescriptorSetsLayout& Layout, bool bNeedDescriptors, VkDescriptorSet* OutDescriptors);


	struct PendingQuery
	{
		uint64 Index;
		uint64 Count;
		VkBuffer BufferHandle;
		VkQueryPool PoolHandle;
		bool bBlocking;
	};
	void AddPendingTimestampQuery(uint64 Index, uint64 Count, VkQueryPool PoolHandle, VkBuffer BufferHandle, bool bBlocking);

private:
	FVulkanDevice* Device;
	VkCommandBuffer CommandBufferHandle;
	double SubmittedTime = 0.0f;

	TArray<VkPipelineStageFlags> WaitFlags;
	TArray<VulkanRHI::FSemaphore*> WaitSemaphores;
	TArray<VulkanRHI::FSemaphore*> SubmittedWaitSemaphores;
	TArray<PendingQuery> PendingTimestampQueries;

	void MarkSemaphoresAsSubmitted()
	{
		WaitFlags.Reset();
		// Move to pending delete list
		SubmittedWaitSemaphores = WaitSemaphores;
		WaitSemaphores.Reset();
	}

	// Do not cache this pointer as it might change depending on VULKAN_REUSE_FENCES
	VulkanRHI::FFence* Fence;

	// Last value passed after the fence got signaled
	volatile uint64 FenceSignaledCounter;
	// Last value when we submitted the cmd buffer; useful to track down if something waiting for the fence has actually been submitted
	volatile uint64 SubmittedFenceCounter;

	void RefreshFenceStatus();
	void InitializeTimings(FVulkanCommandListContext* InContext);

	FVulkanCommandBufferPool* CommandBufferPool;

	FVulkanGPUTiming* Timing;
	uint64 LastValidTiming;

	void AcquirePoolSetContainer();

	void AllocMemory();
	void FreeMemory();

	FVulkanLayoutManager LayoutManager;

public:
	//#todo-rco: Hide this
	TMap<uint32, class FVulkanTypedDescriptorPoolSet*> TypedDescriptorPoolSets;

	friend class FVulkanDynamicRHI;
	friend class FVulkanLayoutManager;
};

class FVulkanCommandBufferPool
{
public:
	FVulkanCommandBufferPool(FVulkanDevice* InDevice, FVulkanCommandBufferManager& InMgr);
	~FVulkanCommandBufferPool();

	void RefreshFenceStatus(FVulkanCmdBuffer* SkipCmdBuffer = nullptr);

	inline VkCommandPool GetHandle() const
	{
		return Handle;
	}

	inline FCriticalSection* GetCS()
	{
		return &CS;
	}

	void FreeUnusedCmdBuffers(FVulkanQueue* Queue, bool bTrimMemory);

	inline FVulkanCommandBufferManager& GetMgr()
	{
		return Mgr;
	}

private:
	VkCommandPool Handle;

	TArray<FVulkanCmdBuffer*> CmdBuffers;
	TArray<FVulkanCmdBuffer*> FreeCmdBuffers;

	FCriticalSection CS;
	FVulkanDevice* Device;

	FVulkanCommandBufferManager& Mgr;

	FVulkanCmdBuffer* Create(bool bIsUploadOnly);


	void Create(uint32 QueueFamilyIndex);
	friend class FVulkanCommandBufferManager;
};

class FVulkanCommandBufferManager
{
public:
	FVulkanCommandBufferManager(FVulkanDevice* InDevice, FVulkanCommandListContext* InContext);
	~FVulkanCommandBufferManager();

	void Init(FVulkanCommandListContext* InContext);

	inline FVulkanCmdBuffer* GetActiveCmdBuffer()
	{
		if (UploadCmdBuffer)
		{
			SubmitUploadCmdBuffer();
		}

		return ActiveCmdBuffer;
	}

	inline FVulkanCmdBuffer* GetActiveCmdBufferDirect()
	{
		return ActiveCmdBuffer;
	}


	inline bool HasPendingUploadCmdBuffer() const
	{
		return UploadCmdBuffer != nullptr;
	}

	inline bool HasPendingActiveCmdBuffer() const
	{
		return ActiveCmdBuffer != nullptr;
	}

	VULKANRHI_API FVulkanCmdBuffer* GetUploadCmdBuffer();

	VULKANRHI_API void SubmitUploadCmdBuffer(uint32 NumSignalSemaphores = 0, VkSemaphore* SignalSemaphores = nullptr);

	void SubmitActiveCmdBuffer(TArrayView<VulkanRHI::FSemaphore*> SignalSemaphores);

	void SubmitActiveCmdBuffer()
	{
		SubmitActiveCmdBuffer(MakeArrayView<VulkanRHI::FSemaphore*>(nullptr, 0));
	}

	void SubmitActiveCmdBuffer(VulkanRHI::FSemaphore* SignalSemaphore)
	{
		SubmitActiveCmdBuffer(MakeArrayView<VulkanRHI::FSemaphore*>(&SignalSemaphore, 1));
	}

	/** Regular SACB() expects not-ended and would rotate the command buffer immediately, but Present has a special logic */
	void SubmitActiveCmdBufferFromPresent(VulkanRHI::FSemaphore* SignalSemaphore = nullptr);

	void WaitForCmdBuffer(FVulkanCmdBuffer* CmdBuffer, float TimeInSecondsToWait = 10.0f);


	void AddQueryPoolForReset(VkQueryPool Pool, uint32 Size);
	void FlushResetQueryPools();

	// Update the fences of all cmd buffers except SkipCmdBuffer
	void RefreshFenceStatus(FVulkanCmdBuffer* SkipCmdBuffer = nullptr)
	{
		Pool.RefreshFenceStatus(SkipCmdBuffer);
	}

	void PrepareForNewActiveCommandBuffer();

	inline VkCommandPool GetHandle() const
	{
		return Pool.GetHandle();
	}

	uint32 CalculateGPUTime();

	void FreeUnusedCmdBuffers(bool bTrimMemory);

	inline FVulkanCommandListContext* GetCommandListContext()
	{
		return Context;
	}

	inline FVulkanQueue* GetQueue()
	{
		return Queue;
	}

	inline void NotifyDeletedImage(VkImage Image)
	{
		if (UploadCmdBuffer)
		{
			UploadCmdBuffer->GetLayoutManager().NotifyDeletedImage(Image);
		}
		if (ActiveCmdBuffer)
		{
			ActiveCmdBuffer->GetLayoutManager().NotifyDeletedImage(Image);
		}
	}

private:
	struct FQueryPoolReset
	{
		VkQueryPool Pool; 
		uint32 Size;
	};
	
	FVulkanDevice* Device;
	FVulkanCommandListContext* Context;
	FVulkanCommandBufferPool Pool;
	FVulkanQueue* Queue;
	FVulkanCmdBuffer* ActiveCmdBuffer;
	FVulkanCmdBuffer* UploadCmdBuffer;
	TArray<FQueryPoolReset> PoolResets;

	/** This semaphore is used to prevent overlaps between the (current) graphics cmdbuf and next upload cmdbuf. */
	VulkanRHI::FSemaphore* ActiveCmdBufferSemaphore;

	/** Holds semaphores associated with the recent upload cmdbuf(s) - waiting to be added to the next graphics cmdbuf as WaitSemaphores. */
	TArray<VulkanRHI::FSemaphore*> RenderingCompletedSemaphores;
	
	/** This semaphore is used to prevent overlaps between (current) upload cmdbuf and next graphics cmdbuf. */
	VulkanRHI::FSemaphore* UploadCmdBufferSemaphore;
	
	/** Holds semaphores associated with the recent upload cmdbuf(s) - waiting to be added to the next graphics cmdbuf as WaitSemaphores. */
	TArray<VulkanRHI::FSemaphore*> UploadCompletedSemaphores;
};
