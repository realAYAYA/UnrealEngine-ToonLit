// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanCommandBuffer.cpp: Vulkan device RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanContext.h"
#include "VulkanDescriptorSets.h"

static int32 GUseSingleQueue = 0;
static FAutoConsoleVariableRef CVarVulkanUseSingleQueue(
	TEXT("r.Vulkan.UseSingleQueue"),
	GUseSingleQueue,
	TEXT("Forces using the same queue for uploads and graphics.\n")
	TEXT(" 0: Uses multiple queues(default)\n")
	TEXT(" 1: Always uses the gfx queue for submissions"),
	ECVF_Default
);

int32 GVulkanProfileCmdBuffers = 0;
static FAutoConsoleVariableRef CVarVulkanProfileCmdBuffers(
	TEXT("r.Vulkan.ProfileCmdBuffers"),
	GVulkanProfileCmdBuffers,
	TEXT("Insert GPU timing queries in every cmd buffer\n"),
	ECVF_Default
);

int32 GVulkanUseCmdBufferTimingForGPUTime = 0;
static FAutoConsoleVariableRef CVarVulkanUseCmdBufferTimingForGPUTime(
	TEXT("r.Vulkan.UseCmdBufferTimingForGPUTime"),
	GVulkanUseCmdBufferTimingForGPUTime,
	TEXT("Use the profile command buffers for GPU time\n"),
	ECVF_Default
);

static int32 GVulkanUploadCmdBufferSemaphore = 0;	// despite this behavior being more correct, default to off to avoid changing the existing (as of UE 4.24) behavior
static FAutoConsoleVariableRef CVarVulkanPreventOverlapWithUpload(
	TEXT("r.Vulkan.UploadCmdBufferSemaphore"),
	GVulkanUploadCmdBufferSemaphore,
	TEXT("Whether command buffers for uploads and graphics can be executed simultaneously.\n")
	TEXT(" 0: The buffers are submitted without any synch(default)\n")
	TEXT(" 1: Graphics buffers will not overlap with the upload buffers"),
	ECVF_ReadOnly
);

#define CMD_BUFFER_TIME_TO_WAIT_BEFORE_DELETING		10

const uint32 GNumberOfFramesBeforeDeletingDescriptorPool = 300;
extern int32 GVulkanAutoCorrectUnknownLayouts;

FVulkanCmdBuffer::FVulkanCmdBuffer(FVulkanDevice* InDevice, FVulkanCommandBufferPool* InCommandBufferPool, bool bInIsUploadOnly)
	: CurrentStencilRef(0)
	, State(EState::NotAllocated)
	, bNeedsDynamicStateSet(1)
	, bHasPipeline(0)
	, bHasViewport(0)
	, bHasScissor(0)
	, bHasStencilRef(0)
	, bIsUploadOnly(bInIsUploadOnly ? 1 : 0)
	, bIsUniformBufferBarrierAdded(0)
	, Device(InDevice)
	, CommandBufferHandle(VK_NULL_HANDLE)
	, Fence(nullptr)
	, FenceSignaledCounter(0)
	, SubmittedFenceCounter(0)
	, CommandBufferPool(InCommandBufferPool)
	, Timing(nullptr)
	, LastValidTiming(0)
	, LayoutManager(InDevice->SupportsParallelRendering() && !GVulkanAutoCorrectUnknownLayouts,
		&InCommandBufferPool->GetMgr().GetCommandListContext()->GetQueue()->GetLayoutManager())
{
	{
		FScopeLock ScopeLock(CommandBufferPool->GetCS());
		AllocMemory();
	}

	Fence = Device->GetFenceManager().AllocateFence();
}

void FVulkanCmdBuffer::AllocMemory()
{
	// Assumes we are inside a lock for the pool
	check(State == EState::NotAllocated);
	CurrentViewports.Empty();
	CurrentScissors.Empty();

	VkCommandBufferAllocateInfo CreateCmdBufInfo;
	ZeroVulkanStruct(CreateCmdBufInfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
	CreateCmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	CreateCmdBufInfo.commandBufferCount = 1;
	CreateCmdBufInfo.commandPool = CommandBufferPool->GetHandle();

	VERIFYVULKANRESULT(VulkanRHI::vkAllocateCommandBuffers(Device->GetInstanceHandle(), &CreateCmdBufInfo, &CommandBufferHandle));

	bNeedsDynamicStateSet = 1;
	bHasPipeline = 0;
	bHasViewport = 0;
	bHasScissor = 0;
	bHasStencilRef = 0;
	State = EState::ReadyForBegin;

	INC_DWORD_STAT(STAT_VulkanNumCmdBuffers);
}

FVulkanCmdBuffer::~FVulkanCmdBuffer()
{
	VulkanRHI::FFenceManager& FenceManager = Device->GetFenceManager();
	if (State == EState::Submitted)
	{
		// Wait 33ms
		uint64 WaitForCmdBufferInNanoSeconds = 33 * 1000 * 1000LL;
		FenceManager.WaitAndReleaseFence(Fence, WaitForCmdBufferInNanoSeconds);
	}
	else
	{
		// Just free the fence, CmdBuffer was not submitted
		FenceManager.ReleaseFence(Fence);
	}

	if (State != EState::NotAllocated)
	{
		FreeMemory();
	}

	if (Timing)
	{
		Timing->Release();
		delete Timing;
		Timing = nullptr;
	}
}

void FVulkanCmdBuffer::FreeMemory()
{
	// Assumes we are inside a lock for the pool
	check(State != EState::NotAllocated);
	check(CommandBufferHandle != VK_NULL_HANDLE);
	VulkanRHI::vkFreeCommandBuffers(Device->GetInstanceHandle(), CommandBufferPool->GetHandle(), 1, &CommandBufferHandle);
	CommandBufferHandle = VK_NULL_HANDLE;

	DEC_DWORD_STAT(STAT_VulkanNumCmdBuffers);
	State = EState::NotAllocated;
}

void FVulkanCmdBuffer::BeginUniformUpdateBarrier()
{
	if(!bIsUniformBufferBarrierAdded)
	{
		VkMemoryBarrier Barrier;
		ZeroVulkanStruct(Barrier, VK_STRUCTURE_TYPE_MEMORY_BARRIER);
		Barrier.srcAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
		Barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		VulkanRHI::vkCmdPipelineBarrier(GetHandle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &Barrier, 0, nullptr, 0, nullptr);
		bIsUniformBufferBarrierAdded = true;
	}
}

void FVulkanCmdBuffer::EndUniformUpdateBarrier()
{
	if(bIsUniformBufferBarrierAdded)
	{
		VkMemoryBarrier Barrier;
		ZeroVulkanStruct(Barrier, VK_STRUCTURE_TYPE_MEMORY_BARRIER);
		Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		Barrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
		VulkanRHI::vkCmdPipelineBarrier(GetHandle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &Barrier, 0, nullptr, 0, nullptr);
		bIsUniformBufferBarrierAdded = false;
	}
}
void FVulkanCmdBuffer::EndRenderPass()
{
	checkf(IsInsideRenderPass(), TEXT("Can't EndRP as we're NOT inside one! CmdBuffer 0x%p State=%d"), CommandBufferHandle, (int32)State);
	VulkanRHI::vkCmdEndRenderPass(CommandBufferHandle);
	State = EState::IsInsideBegin;
}

void FVulkanCmdBuffer::BeginRenderPass(const FVulkanRenderTargetLayout& Layout, FVulkanRenderPass* RenderPass, FVulkanFramebuffer* Framebuffer, const VkClearValue* AttachmentClearValues)
{
	if (bIsUniformBufferBarrierAdded)
	{
		EndUniformUpdateBarrier();
	}
	checkf(IsOutsideRenderPass(), TEXT("Can't BeginRP as already inside one! CmdBuffer 0x%p State=%d"), CommandBufferHandle, (int32)State);

	VkRenderPassBeginInfo Info;
	ZeroVulkanStruct(Info, VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
	Info.renderPass = RenderPass->GetHandle();
	Info.framebuffer = Framebuffer->GetHandle();
	Info.renderArea = Framebuffer->GetRenderArea();
	Info.clearValueCount = Layout.GetNumUsedClearValues();
	Info.pClearValues = AttachmentClearValues;

#if VULKAN_SUPPORTS_QCOM_RENDERPASS_TRANSFORM
	VkRenderPassTransformBeginInfoQCOM RPTransformBeginInfoQCOM;
	VkSurfaceTransformFlagBitsKHR QCOMTransform = Layout.GetQCOMRenderPassTransform();

	if (QCOMTransform != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		ZeroVulkanStruct(RPTransformBeginInfoQCOM, (VkStructureType)VK_STRUCTURE_TYPE_RENDER_PASS_TRANSFORM_BEGIN_INFO_QCOM);

		RPTransformBeginInfoQCOM.transform = QCOMTransform;
		Info.pNext = &RPTransformBeginInfoQCOM;
	}
#endif

	if (Device->GetOptionalExtensions().HasKHRRenderPass2)
	{
		VkSubpassBeginInfo SubpassInfo;
		ZeroVulkanStruct(SubpassInfo, VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO);
		SubpassInfo.contents = VK_SUBPASS_CONTENTS_INLINE;
		VulkanRHI::vkCmdBeginRenderPass2KHR(CommandBufferHandle, &Info, &SubpassInfo);
	}
	else
	{
		VulkanRHI::vkCmdBeginRenderPass(CommandBufferHandle, &Info, VK_SUBPASS_CONTENTS_INLINE);
	}

	State = EState::IsInsideRenderPass;

	// Acquire a descriptor pool set on a first render pass
	if (CurrentDescriptorPoolSetContainer == nullptr)
	{
		AcquirePoolSetContainer();
	}
}


void FVulkanCmdBuffer::AddPendingTimestampQuery(uint64 Index, uint64 Count, VkQueryPool PoolHandle, VkBuffer BufferHandle, bool bBlocking)
{
	PendingQuery PQ;
	PQ.Index = Index;
	PQ.Count = Count;
	PQ.PoolHandle = PoolHandle;
	PQ.BufferHandle = BufferHandle;
	PQ.bBlocking = bBlocking;
	PendingTimestampQueries.Add(PQ);
}

void FVulkanCmdBuffer::End()
{
	checkf(IsOutsideRenderPass(), TEXT("Can't End as we're inside a render pass! CmdBuffer 0x%p State=%d"), CommandBufferHandle, (int32)State);

	if (GVulkanProfileCmdBuffers || GVulkanUseCmdBufferTimingForGPUTime)
	{
		if (Timing)
		{
			Timing->EndTiming(this);
			LastValidTiming = FenceSignaledCounter;
		}
	}

	for (PendingQuery& Query : PendingTimestampQueries)
	{
		const uint64 Index = Query.Index;
		const VkBuffer BufferHandle = Query.BufferHandle;
		const VkQueryPool PoolHandle = Query.PoolHandle;
		const VkQueryResultFlags BlockingFlags = Query.bBlocking ?  VK_QUERY_RESULT_WAIT_BIT : VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
		const uint32 Width = (Query.bBlocking ? 1 : 2);
		const uint32 Stride = sizeof(uint64) * Width;

		VulkanRHI::vkCmdCopyQueryPoolResults(GetHandle(), PoolHandle, Index, Query.Count, BufferHandle, Stride * Index, Stride, VK_QUERY_RESULT_64_BIT | BlockingFlags);
		if (Query.bBlocking)
		{
			VulkanRHI::vkCmdResetQueryPool(GetHandle(), PoolHandle, Index, Query.Count);
		}
	}

	PendingTimestampQueries.Reset();

	VERIFYVULKANRESULT(VulkanRHI::vkEndCommandBuffer(GetHandle()));
	State = EState::HasEnded;
}

inline void FVulkanCmdBuffer::InitializeTimings(FVulkanCommandListContext* InContext)
{
	if ((GVulkanProfileCmdBuffers || GVulkanUseCmdBufferTimingForGPUTime) && !Timing)
	{
		if (InContext)
		{
			Timing = new FVulkanGPUTiming(InContext, Device);

			// Upload cb's can be submitted multiple times in a single frame, so we use an expanded pool to catch timings
			// Any overflow will wrap
			const uint32 PoolSize = bIsUploadOnly ? 256 : 32;
			Timing->Initialize(PoolSize);
		}
	}
}

void FVulkanCmdBuffer::AddWaitSemaphore(VkPipelineStageFlags InWaitFlags, TArrayView<VulkanRHI::FSemaphore*> InWaitSemaphores)
{
	WaitFlags.Reserve(WaitFlags.Num() + InWaitSemaphores.Num());

	for (VulkanRHI::FSemaphore* Sema : InWaitSemaphores)
	{
		WaitFlags.Add(InWaitFlags);
		Sema->AddRef();
		check(!WaitSemaphores.Contains(Sema));
	}

	WaitSemaphores.Append(InWaitSemaphores);
}

void FVulkanCmdBuffer::Begin()
{
	{
		FScopeLock ScopeLock(CommandBufferPool->GetCS());
		if(State == EState::NeedReset)
		{
			VulkanRHI::vkResetCommandBuffer(CommandBufferHandle, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
		}
		else
		{
			checkf(State == EState::ReadyForBegin, TEXT("Can't Begin as we're NOT ready! CmdBuffer 0x%p State=%d"), CommandBufferHandle, (int32)State);
		}
		State = EState::IsInsideBegin;
	}

	VkCommandBufferBeginInfo CmdBufBeginInfo;
	ZeroVulkanStruct(CmdBufBeginInfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
	CmdBufBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VERIFYVULKANRESULT(VulkanRHI::vkBeginCommandBuffer(CommandBufferHandle, &CmdBufBeginInfo));

	if (GVulkanProfileCmdBuffers || GVulkanUseCmdBufferTimingForGPUTime)
	{
		InitializeTimings(CommandBufferPool->GetMgr().GetCommandListContext());
		if (Timing)
		{
			Timing->StartTiming(this);
		}
	}
	check(!CurrentDescriptorPoolSetContainer);

	if (!bIsUploadOnly && Device->SupportsBindless())
	{
		FVulkanBindlessDescriptorManager* BindlessDescriptorManager = Device->GetBindlessDescriptorManager();
		FVulkanQueue* Queue = GetOwner()->GetMgr().GetQueue();
		const VkPipelineStageFlags SupportedStages = Queue->GetSupportedStageBits();
		BindlessDescriptorManager->BindDescriptorBuffers(CommandBufferHandle, SupportedStages);
	}

	bNeedsDynamicStateSet = true;
}

void FVulkanCmdBuffer::AcquirePoolSetContainer()
{
	check(!CurrentDescriptorPoolSetContainer);
	CurrentDescriptorPoolSetContainer = &Device->GetDescriptorPoolsManager().AcquirePoolSetContainer();
	ensure(TypedDescriptorPoolSets.Num() == 0);
}

bool FVulkanCmdBuffer::AcquirePoolSetAndDescriptorsIfNeeded(const class FVulkanDescriptorSetsLayout& Layout, bool bNeedDescriptors, VkDescriptorSet* OutDescriptors)
{
	//#todo-rco: This only happens when we call draws outside a render pass...
	if (!CurrentDescriptorPoolSetContainer)
	{
		AcquirePoolSetContainer();
	}

	const uint32 Hash = VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID ? Layout.GetTypesUsageID() : GetTypeHash(Layout);
	FVulkanTypedDescriptorPoolSet*& FoundTypedSet = TypedDescriptorPoolSets.FindOrAdd(Hash);

	if (!FoundTypedSet)
	{
		FoundTypedSet = CurrentDescriptorPoolSetContainer->AcquireTypedPoolSet(Layout);
		bNeedDescriptors = true;
	}

	if (bNeedDescriptors)
	{
		return FoundTypedSet->AllocateDescriptorSets(Layout, OutDescriptors);
	}

	return false;
}

void FVulkanCmdBuffer::RefreshFenceStatus()
{
	if (State == EState::Submitted)
	{
		VulkanRHI::FFenceManager* FenceMgr = Fence->GetOwner();
		if (FenceMgr->IsFenceSignaled(Fence))
		{
			bHasPipeline = false;
			bHasViewport = false;
			bHasScissor = false;
			bHasStencilRef = false;

			for (VulkanRHI::FSemaphore* Semaphore : SubmittedWaitSemaphores)
			{
				Semaphore->Release();
			}
			SubmittedWaitSemaphores.Reset();

			CurrentViewports.Empty();
			CurrentScissors.Empty();
			CurrentStencilRef = 0;
#if VULKAN_REUSE_FENCES
			Fence->GetOwner()->ResetFence(Fence);
#else
			VulkanRHI::FFence* PrevFence = Fence;
			Fence = FenceMgr->AllocateFence();
			FenceMgr->ReleaseFence(PrevFence);
#endif
			++FenceSignaledCounter;

			if (CurrentDescriptorPoolSetContainer)
			{
				//#todo-rco: Reset here?
				TypedDescriptorPoolSets.Reset();
				Device->GetDescriptorPoolsManager().ReleasePoolSet(*CurrentDescriptorPoolSetContainer);
				CurrentDescriptorPoolSetContainer = nullptr;
			}
			else
			{
				check(TypedDescriptorPoolSets.Num() == 0);
			}

			// Change state at the end to be safe
			State = EState::NeedReset;
		}
	}
	else
	{
		check(!Fence->IsSignaled());
	}
}

FVulkanCommandBufferPool::FVulkanCommandBufferPool(FVulkanDevice* InDevice, FVulkanCommandBufferManager& InMgr)
	: Handle(VK_NULL_HANDLE)
	, Device(InDevice)
	, Mgr(InMgr)
{
}

FVulkanCommandBufferPool::~FVulkanCommandBufferPool()
{
	for (int32 Index = 0; Index < CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = CmdBuffers[Index];
		delete CmdBuffer;
	}

	for (int32 Index = 0; Index < FreeCmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = FreeCmdBuffers[Index];
		delete CmdBuffer;
	}

	VulkanRHI::vkDestroyCommandPool(Device->GetInstanceHandle(), Handle, VULKAN_CPU_ALLOCATOR);
	Handle = VK_NULL_HANDLE;
}

void FVulkanCommandBufferPool::Create(uint32 QueueFamilyIndex)
{
	VkCommandPoolCreateInfo CmdPoolInfo;
	ZeroVulkanStruct(CmdPoolInfo, VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
	CmdPoolInfo.queueFamilyIndex =  QueueFamilyIndex;
	//#todo-rco: Should we use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT?
	CmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VERIFYVULKANRESULT(VulkanRHI::vkCreateCommandPool(Device->GetInstanceHandle(), &CmdPoolInfo, VULKAN_CPU_ALLOCATOR, &Handle));
}

FVulkanCommandBufferManager::FVulkanCommandBufferManager(FVulkanDevice* InDevice, FVulkanCommandListContext* InContext)
	: Device(InDevice)
	, Context(InContext)
	, Pool(InDevice, *this)
	, Queue(InContext->GetQueue())
	, ActiveCmdBuffer(nullptr)
	, UploadCmdBuffer(nullptr)
	, ActiveCmdBufferSemaphore(nullptr)
	, UploadCmdBufferSemaphore(nullptr)
{
	check(Device);

	Pool.Create(Queue->GetFamilyIndex());

	if (GVulkanUploadCmdBufferSemaphore)
	{
		ActiveCmdBufferSemaphore = new VulkanRHI::FSemaphore(*Device);
	}

	ActiveCmdBuffer = Pool.Create(false);
}

void FVulkanCommandBufferManager::Init(FVulkanCommandListContext* InContext)
{
	ActiveCmdBuffer->InitializeTimings(InContext);
	ActiveCmdBuffer->Begin();
}

FVulkanCommandBufferManager::~FVulkanCommandBufferManager()
{
}

void FVulkanCommandBufferManager::WaitForCmdBuffer(FVulkanCmdBuffer* CmdBuffer, float TimeInSecondsToWait)
{
	FScopeLock ScopeLock(&Pool.CS);
	check(CmdBuffer->IsSubmitted());
	bool bSuccess = Device->GetFenceManager().WaitForFence(CmdBuffer->Fence, (uint64)(TimeInSecondsToWait * 1e9));
	check(bSuccess);
	CmdBuffer->RefreshFenceStatus();
}

void FVulkanCommandBufferManager::AddQueryPoolForReset(VkQueryPool QueryPool, uint32 Size)
{
	FScopeLock ScopeLock(&Pool.CS);
	FQueryPoolReset QPR = { QueryPool, Size};
	PoolResets.Add(QPR);
}
void FVulkanCommandBufferManager::FlushResetQueryPools()
{
	FScopeLock ScopeLock(&Pool.CS);
	if(PoolResets.Num())
	{
		FVulkanCmdBuffer* PoolResetCommandBuffer = nullptr;
		for (int32 Index = 0; Index < Pool.CmdBuffers.Num(); ++Index)
		{
			FVulkanCmdBuffer* CmdBuffer = Pool.CmdBuffers[Index];
			CmdBuffer->RefreshFenceStatus();
	#if VULKAN_USE_DIFFERENT_POOL_CMDBUFFERS
			if (!CmdBuffer->bIsUploadOnly)
	#endif
			{
				if (CmdBuffer->State == FVulkanCmdBuffer::EState::ReadyForBegin)
				{
					PoolResetCommandBuffer = CmdBuffer;
				}
			}
		}
		if (!PoolResetCommandBuffer)
		{
			PoolResetCommandBuffer = Pool.Create(false);
		}
		PoolResetCommandBuffer->Begin();

		VkCommandBuffer CommandBuffer = PoolResetCommandBuffer->GetHandle();
		for (FQueryPoolReset& QPR : PoolResets)
		{
			VulkanRHI::vkCmdResetQueryPool(CommandBuffer, QPR.Pool, 0, QPR.Size);
		}
		PoolResetCommandBuffer->End();
		Queue->Submit(PoolResetCommandBuffer);
		PoolResets.Empty();
	}
}

void FVulkanCommandBufferManager::SubmitUploadCmdBuffer(uint32 NumSignalSemaphores, VkSemaphore* SignalSemaphores)
{
	FScopeLock ScopeLock(&Pool.CS);
	check(UploadCmdBuffer);
	check(UploadCmdBuffer->CurrentDescriptorPoolSetContainer == nullptr);
	if (!UploadCmdBuffer->IsSubmitted() && UploadCmdBuffer->HasBegun())
	{
		check(UploadCmdBuffer->IsOutsideRenderPass());

		VulkanRHI::DebugHeavyWeightBarrier(UploadCmdBuffer->GetHandle(), 4);

		UploadCmdBuffer->End();

		if (GVulkanUploadCmdBufferSemaphore)
		{
			// Add semaphores associated with the recent active cmdbuf(s), if any. That will prevent
			// the overlap, delaying execution of this cmdbuf until the graphics one(s) is complete.
			for (FSemaphore* WaitForThis : RenderingCompletedSemaphores)
			{
				UploadCmdBuffer->AddWaitSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, WaitForThis);
			}

			if (NumSignalSemaphores == 0)
			{
				checkf(UploadCmdBufferSemaphore != nullptr, TEXT("FVulkanCommandBufferManager::SubmitUploadCmdBuffer: upload command buffer does not have an associated completion semaphore."));
				VkSemaphore Sema = UploadCmdBufferSemaphore->GetHandle();
				Queue->Submit(UploadCmdBuffer, 1, &Sema);
				UploadCompletedSemaphores.Add(UploadCmdBufferSemaphore);
				UploadCmdBufferSemaphore = nullptr;
			}
			else
			{
				TArray<VkSemaphore, TInlineAllocator<16>> CombinedSemaphores;
				CombinedSemaphores.Add(UploadCmdBufferSemaphore->GetHandle());
				CombinedSemaphores.Append(SignalSemaphores, NumSignalSemaphores);
				Queue->Submit(UploadCmdBuffer, CombinedSemaphores.Num(), CombinedSemaphores.GetData());
			}
			// the buffer will now hold on to the wait semaphores, so we can clear them here
			RenderingCompletedSemaphores.Empty();
		}
		else
		{
			Queue->Submit(UploadCmdBuffer, NumSignalSemaphores, SignalSemaphores);
		}

		UploadCmdBuffer->SubmittedTime = FPlatformTime::Seconds();
	}

	UploadCmdBuffer = nullptr;
}

void FVulkanCommandBufferManager::SubmitActiveCmdBuffer(TArrayView<VulkanRHI::FSemaphore*> SignalSemaphores)
{
	FScopeLock ScopeLock(&Pool.CS);
	FlushResetQueryPools();
	check(!UploadCmdBuffer);
	check(ActiveCmdBuffer);

	TArray<VkSemaphore, FConcurrentLinearArrayAllocator> SemaphoreHandles;
	SemaphoreHandles.Reserve(SignalSemaphores.Num() + 1);
	for (VulkanRHI::FSemaphore* Semaphore : SignalSemaphores)
	{
		SemaphoreHandles.Add(Semaphore->GetHandle());
	}

	if (!ActiveCmdBuffer->IsSubmitted() && ActiveCmdBuffer->HasBegun())
	{
		if (!ActiveCmdBuffer->IsOutsideRenderPass())
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Forcing EndRenderPass() for submission"));
			ActiveCmdBuffer->EndRenderPass();
		}

		VulkanRHI::DebugHeavyWeightBarrier(ActiveCmdBuffer->GetHandle(), 8);

		ActiveCmdBuffer->End();

		if (GVulkanUploadCmdBufferSemaphore)
		{
			checkf(ActiveCmdBufferSemaphore != nullptr, TEXT("FVulkanCommandBufferManager::SubmitUploadCmdBuffer: graphics command buffer does not have an associated completion semaphore."));

			// Add semaphores associated with the recent upload cmdbuf(s), if any. That will prevent
			// the overlap, delaying execution of this cmdbuf until upload one(s) are complete.
			for (FSemaphore* UploadCompleteSema : UploadCompletedSemaphores)
			{
				ActiveCmdBuffer->AddWaitSemaphore(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, UploadCompleteSema);
			}

			SemaphoreHandles.Add(ActiveCmdBufferSemaphore->GetHandle());
			Queue->Submit(ActiveCmdBuffer, SemaphoreHandles.Num(), SemaphoreHandles.GetData());

			RenderingCompletedSemaphores.Add(ActiveCmdBufferSemaphore);
			ActiveCmdBufferSemaphore = nullptr;

			// the buffer will now hold on to the wait semaphores, so we can clear them here
			UploadCompletedSemaphores.Empty();
		}
		else
		{
			Queue->Submit(ActiveCmdBuffer, SemaphoreHandles.Num(), SemaphoreHandles.GetData());
		}
		ActiveCmdBuffer->SubmittedTime = FPlatformTime::Seconds();
	}

	ActiveCmdBuffer = nullptr;
	if (GVulkanUploadCmdBufferSemaphore && ActiveCmdBufferSemaphore != nullptr)
	{
		// most likely we didn't submit as it wasn't begun
		check(ActiveCmdBufferSemaphore->GetRefCount() == 0);
		delete ActiveCmdBufferSemaphore;
		ActiveCmdBufferSemaphore = nullptr;
	}
}

void FVulkanCommandBufferManager::SubmitActiveCmdBufferFromPresent(VulkanRHI::FSemaphore* SignalSemaphore)
{
	if (GVulkanUploadCmdBufferSemaphore)
	{
		// unlike more advanced regular SACB(), this is just a wrapper
		// around Queue->Submit() to avoid rewriting the logic in Present
		if (SignalSemaphore)
		{
			VkSemaphore SignalThis[2] =
			{
				SignalSemaphore->GetHandle(),
				ActiveCmdBufferSemaphore->GetHandle()
			};

			Queue->Submit(ActiveCmdBuffer, UE_ARRAY_COUNT(SignalThis), SignalThis);
		}
		else
		{
			VkSemaphore SignalThis = ActiveCmdBufferSemaphore->GetHandle();
			Queue->Submit(ActiveCmdBuffer, 1, &SignalThis);
		}

		RenderingCompletedSemaphores.Add(ActiveCmdBufferSemaphore);
		ActiveCmdBufferSemaphore = nullptr;
	}
	else
	{
		if (SignalSemaphore)
		{
			Queue->Submit(ActiveCmdBuffer, SignalSemaphore->GetHandle());
		}
		else
		{
			Queue->Submit(ActiveCmdBuffer);
		}
	}
}

FVulkanCmdBuffer* FVulkanCommandBufferPool::Create(bool bIsUploadOnly)
{
	// Assumes we are inside a lock for the pool
	for (int32 Index = FreeCmdBuffers.Num() - 1; Index >= 0; --Index)
	{
		FVulkanCmdBuffer* CmdBuffer = FreeCmdBuffers[Index];
#if VULKAN_USE_DIFFERENT_POOL_CMDBUFFERS
		if (CmdBuffer->bIsUploadOnly == bIsUploadOnly)
#endif
		{
			FreeCmdBuffers.RemoveAtSwap(Index);
			CmdBuffer->AllocMemory();
			CmdBuffers.Add(CmdBuffer);
			return CmdBuffer;
		}
	}

	FVulkanCmdBuffer* CmdBuffer = new FVulkanCmdBuffer(Device, this, bIsUploadOnly);
	CmdBuffers.Add(CmdBuffer);
	check(CmdBuffer);
	return CmdBuffer;
}

void FVulkanCommandBufferPool::RefreshFenceStatus(FVulkanCmdBuffer* SkipCmdBuffer)
{
	FScopeLock ScopeLock(&CS);
	for (int32 Index = 0; Index < CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = CmdBuffers[Index];
		if (CmdBuffer != SkipCmdBuffer)
		{
			CmdBuffer->RefreshFenceStatus();
		}
	}
}

void FVulkanCommandBufferManager::PrepareForNewActiveCommandBuffer()
{
	FScopeLock ScopeLock(&Pool.CS);
	check(!UploadCmdBuffer);

	if (GVulkanUploadCmdBufferSemaphore)
	{
		// create a completion semaphore
		checkf(ActiveCmdBufferSemaphore == nullptr, TEXT("FVulkanCommandBufferManager::PrepareForNewActiveCommandBuffer: a semaphore from the previous active buffer has been leaked."));
		ActiveCmdBufferSemaphore = new VulkanRHI::FSemaphore(*Device);
	}

	for (int32 Index = 0; Index < Pool.CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = Pool.CmdBuffers[Index];
		CmdBuffer->RefreshFenceStatus();
#if VULKAN_USE_DIFFERENT_POOL_CMDBUFFERS
		if (!CmdBuffer->bIsUploadOnly)
#endif
		{
			if (CmdBuffer->State == FVulkanCmdBuffer::EState::ReadyForBegin || CmdBuffer->State == FVulkanCmdBuffer::EState::NeedReset)
			{
				ActiveCmdBuffer = CmdBuffer;
				ActiveCmdBuffer->Begin();
				return;
			}
			else
			{
				check(CmdBuffer->State == FVulkanCmdBuffer::EState::Submitted);
			}
		}
	}

	// All cmd buffers are being executed still
	ActiveCmdBuffer = Pool.Create(false);
	ActiveCmdBuffer->Begin();
}

uint32 FVulkanCommandBufferManager::CalculateGPUTime()
{
	uint32 Time = 0;
	for (int32 Index = 0; Index < Pool.CmdBuffers.Num(); ++Index)
	{
		FVulkanCmdBuffer* CmdBuffer = Pool.CmdBuffers[Index];
		if (CmdBuffer->HasValidTiming())
		{
			Time += CmdBuffer->Timing->GetTiming(false);
		}
	}
	return Time;
}

FVulkanCmdBuffer* FVulkanCommandBufferManager::GetUploadCmdBuffer()
{
	FScopeLock ScopeLock(&Pool.CS);
	if (!UploadCmdBuffer)
	{
		if (GVulkanUploadCmdBufferSemaphore)
		{
			// create a completion semaphore
			checkf(UploadCmdBufferSemaphore == nullptr, TEXT("FVulkanCommandBufferManager::GetUploadCmdBuffer: a semaphore from the previous upload buffer has been leaked."));
			UploadCmdBufferSemaphore = new VulkanRHI::FSemaphore(*Device);
		}

		for (int32 Index = 0; Index < Pool.CmdBuffers.Num(); ++Index)
		{
			FVulkanCmdBuffer* CmdBuffer = Pool.CmdBuffers[Index];
			CmdBuffer->RefreshFenceStatus();
#if VULKAN_USE_DIFFERENT_POOL_CMDBUFFERS
			if (CmdBuffer->bIsUploadOnly)
#endif
			{
				if (CmdBuffer->State == FVulkanCmdBuffer::EState::ReadyForBegin || CmdBuffer->State == FVulkanCmdBuffer::EState::NeedReset)
				{
					UploadCmdBuffer = CmdBuffer;
					UploadCmdBuffer->Begin();
					return UploadCmdBuffer;
				}
			}
		}

		// All cmd buffers are being executed still
		UploadCmdBuffer = Pool.Create(true);
		UploadCmdBuffer->Begin();
	}

	return UploadCmdBuffer;
}

#if VULKAN_DELETE_STALE_CMDBUFFERS
struct FRHICommandFreeUnusedCmdBuffers final : public FRHICommand<FRHICommandFreeUnusedCmdBuffers>
{
	FVulkanCommandBufferPool* Pool;
	FVulkanQueue* Queue;
	bool bTrimMemory;

	FRHICommandFreeUnusedCmdBuffers(FVulkanCommandBufferPool* InPool, FVulkanQueue* InQueue, bool bInTrimMemory)
		: Pool(InPool)
		, Queue(InQueue)
		, bTrimMemory(bInTrimMemory)
	{
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		Pool->FreeUnusedCmdBuffers(Queue, bTrimMemory);
	}
};
#endif


void FVulkanCommandBufferPool::FreeUnusedCmdBuffers(FVulkanQueue* InQueue, bool bTrimMemory)
{
#if VULKAN_DELETE_STALE_CMDBUFFERS
	FScopeLock ScopeLock(&CS);
	
	if (bTrimMemory)
	{
		VulkanRHI::vkTrimCommandPool(Device->GetInstanceHandle(), Handle, 0);
		return;
	}

	const double CurrentTime = FPlatformTime::Seconds();

	// In case Queue stores pointer to a cmdbuffer, do not delete it
	FVulkanCmdBuffer* LastSubmittedCmdBuffer = nullptr;
	uint64 LastSubmittedFenceCounter = 0;
	InQueue->GetLastSubmittedInfo(LastSubmittedCmdBuffer, LastSubmittedFenceCounter);

	// Deferred deletion queue caches pointers to cmdbuffers
	FDeferredDeletionQueue2& DeferredDeletionQueue = Device->GetDeferredDeletionQueue();

	for (int32 Index = CmdBuffers.Num() - 1; Index >= 0; --Index)
	{
		FVulkanCmdBuffer* CmdBuffer = CmdBuffers[Index];
		if (CmdBuffer != LastSubmittedCmdBuffer &&
			(CmdBuffer->State == FVulkanCmdBuffer::EState::ReadyForBegin || CmdBuffer->State == FVulkanCmdBuffer::EState::NeedReset) &&
			(CurrentTime - CmdBuffer->SubmittedTime) > CMD_BUFFER_TIME_TO_WAIT_BEFORE_DELETING)
		{
			DeferredDeletionQueue.OnCmdBufferDeleted(CmdBuffer);

			CmdBuffer->FreeMemory();
			CmdBuffers.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			FreeCmdBuffers.Add(CmdBuffer);
		}
	}
#endif
}

void FVulkanCommandBufferManager::FreeUnusedCmdBuffers(bool bTrimMemory)
{
#if VULKAN_DELETE_STALE_CMDBUFFERS
	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (!IsInRenderingThread() || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
	{
		Pool.FreeUnusedCmdBuffers(Queue, bTrimMemory);
	}
	else
	{
		check(IsInRenderingThread());
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandFreeUnusedCmdBuffers)(&Pool, Queue, bTrimMemory);
	}
#endif
}
