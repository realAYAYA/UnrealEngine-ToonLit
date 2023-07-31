// Copyright Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanPendingState.cpp: Private VulkanPendingState function definitions.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPendingState.h"
#include "VulkanPipeline.h"
#include "VulkanContext.h"

FVulkanDescriptorPool::FVulkanDescriptorPool(FVulkanDevice* InDevice, const FVulkanDescriptorSetsLayout& InLayout, uint32 MaxSetsAllocations)
	: Device(InDevice)
	, MaxDescriptorSets(0)
	, NumAllocatedDescriptorSets(0)
	, PeakAllocatedDescriptorSets(0)
	, Layout(InLayout)
	, DescriptorPool(VK_NULL_HANDLE)
{
	INC_DWORD_STAT(STAT_VulkanNumDescPools);

	// Descriptor sets number required to allocate the max number of descriptor sets layout.
	// When we're hashing pools with types usage ID the descriptor pool can be used for different layouts so the initial layout does not make much sense.
	// In the latter case we'll be probably overallocating the descriptor types but given the relatively small number of max allocations this should not have
	// a serious impact.
	MaxDescriptorSets = MaxSetsAllocations*(VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID ? 1 : Layout.GetLayouts().Num());
	TArray<VkDescriptorPoolSize, TFixedAllocator<VK_DESCRIPTOR_TYPE_RANGE_SIZE>> Types;
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex <= VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		VkDescriptorType DescriptorType =(VkDescriptorType)TypeIndex;
		uint32 NumTypesUsed = Layout.GetTypesUsed(DescriptorType);
		if (NumTypesUsed > 0)
		{
			VkDescriptorPoolSize* Type = new(Types) VkDescriptorPoolSize;
			FMemory::Memzero(*Type);
			Type->type = DescriptorType;
			Type->descriptorCount = NumTypesUsed * MaxSetsAllocations;
		}
	}

	VkDescriptorPoolCreateInfo PoolInfo;
	ZeroVulkanStruct(PoolInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
	// you don't need this flag because pool reset feature. Also this flag increase pool size in memory and vkResetDescriptorPool time.
	//PoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	PoolInfo.poolSizeCount = Types.Num();
	PoolInfo.pPoolSizes = Types.GetData();
	PoolInfo.maxSets = MaxDescriptorSets;

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanVkCreateDescriptorPool);
#endif
	VERIFYVULKANRESULT(VulkanRHI::vkCreateDescriptorPool(Device->GetInstanceHandle(), &PoolInfo, VULKAN_CPU_ALLOCATOR, &DescriptorPool));

	INC_DWORD_STAT_BY(STAT_VulkanNumDescSetsTotal, MaxDescriptorSets);
}

FVulkanDescriptorPool::~FVulkanDescriptorPool()
{
	DEC_DWORD_STAT_BY(STAT_VulkanNumDescSetsTotal, MaxDescriptorSets);
	DEC_DWORD_STAT(STAT_VulkanNumDescPools);

	if (DescriptorPool != VK_NULL_HANDLE)
	{
		VulkanRHI::vkDestroyDescriptorPool(Device->GetInstanceHandle(), DescriptorPool, VULKAN_CPU_ALLOCATOR);
		DescriptorPool = VK_NULL_HANDLE;
	}
}

void FVulkanDescriptorPool::TrackAddUsage(const FVulkanDescriptorSetsLayout& InLayout)
{
	// Check and increment our current type usage
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex <= VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		ensure(Layout.GetTypesUsed((VkDescriptorType)TypeIndex) == InLayout.GetTypesUsed((VkDescriptorType)TypeIndex));
	}

	NumAllocatedDescriptorSets += InLayout.GetLayouts().Num();
	PeakAllocatedDescriptorSets = FMath::Max(NumAllocatedDescriptorSets, PeakAllocatedDescriptorSets);
}

void FVulkanDescriptorPool::TrackRemoveUsage(const FVulkanDescriptorSetsLayout& InLayout)
{
	for (uint32 TypeIndex = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; TypeIndex <= VK_DESCRIPTOR_TYPE_END_RANGE; ++TypeIndex)
	{
		check(Layout.GetTypesUsed((VkDescriptorType)TypeIndex) == InLayout.GetTypesUsed((VkDescriptorType)TypeIndex));
	}

	NumAllocatedDescriptorSets -= InLayout.GetLayouts().Num();
}

void FVulkanDescriptorPool::Reset()
{
	if (DescriptorPool != VK_NULL_HANDLE)
	{
		VERIFYVULKANRESULT(VulkanRHI::vkResetDescriptorPool(Device->GetInstanceHandle(), DescriptorPool, 0));
	}

	NumAllocatedDescriptorSets = 0;
}

bool FVulkanDescriptorPool::AllocateDescriptorSets(const VkDescriptorSetAllocateInfo& InDescriptorSetAllocateInfo, VkDescriptorSet* OutSets)
{
	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = InDescriptorSetAllocateInfo;
	DescriptorSetAllocateInfo.descriptorPool = DescriptorPool;

	return VK_SUCCESS == VulkanRHI::vkAllocateDescriptorSets(Device->GetInstanceHandle(), &DescriptorSetAllocateInfo, OutSets);
}

FVulkanTypedDescriptorPoolSet::~FVulkanTypedDescriptorPoolSet()
{
	for (FPoolList* Pool = PoolListHead; Pool;)
	{
		FPoolList* Next = Pool->Next;

		delete Pool->Element;
		delete Pool;

		Pool = Next;
	}
	PoolsCount = 0;
}

FVulkanDescriptorPool* FVulkanTypedDescriptorPoolSet::PushNewPool()
{
	// Max number of descriptor sets layout allocations
	const uint32 MaxSetsAllocationsBase = 32;
	// Allow max 128 setS per pool (32 << 2)
	const uint32 MaxSetsAllocations = MaxSetsAllocationsBase << FMath::Min(PoolsCount, 2u);

	auto* NewPool = new FVulkanDescriptorPool(Device, Layout, MaxSetsAllocations);

	if (PoolListCurrent)
	{
		PoolListCurrent->Next = new FPoolList(NewPool);
		PoolListCurrent = PoolListCurrent->Next;
	}
	else
	{
		PoolListCurrent = PoolListHead = new FPoolList(NewPool);
	}
	++PoolsCount;

	return NewPool;
}

FVulkanDescriptorPool* FVulkanTypedDescriptorPoolSet::GetFreePool(bool bForceNewPool)
{
	// Likely this
	if (!bForceNewPool)
	{
		return PoolListCurrent->Element;
	}

	if (PoolListCurrent->Next)
	{
		PoolListCurrent = PoolListCurrent->Next;
		return PoolListCurrent->Element;
	}

	return PushNewPool();
}

bool FVulkanTypedDescriptorPoolSet::AllocateDescriptorSets(const FVulkanDescriptorSetsLayout& InLayout, VkDescriptorSet* OutSets)
{
	const TArray<VkDescriptorSetLayout>& LayoutHandles = InLayout.GetHandles();

	if (LayoutHandles.Num() > 0)
	{
		auto* Pool = PoolListCurrent->Element;
		while (!Pool->AllocateDescriptorSets(InLayout.GetAllocateInfo(), OutSets))
		{
			Pool = GetFreePool(true);
		}

#if VULKAN_ENABLE_AGGRESSIVE_STATS
		//INC_DWORD_STAT_BY(STAT_VulkanNumDescSetsTotal, LayoutHandles.Num());
		Pool->TrackAddUsage(InLayout);
#endif

		return true;
	}

	return true;
}

void FVulkanTypedDescriptorPoolSet::Reset()
{
	for (FPoolList* Pool = PoolListHead; Pool; Pool = Pool->Next)
	{
		Pool->Element->Reset();
	}

	PoolListCurrent = PoolListHead;
}

FVulkanDescriptorPoolSetContainer::~FVulkanDescriptorPoolSetContainer()
{
	for (auto& Pair : TypedDescriptorPools)
	{
		FVulkanTypedDescriptorPoolSet* TypedPool = Pair.Value;
		delete TypedPool;
	}

	TypedDescriptorPools.Reset();
}

FVulkanTypedDescriptorPoolSet* FVulkanDescriptorPoolSetContainer::AcquireTypedPoolSet(const FVulkanDescriptorSetsLayout& Layout)
{
	const uint32 Hash = VULKAN_HASH_POOLS_WITH_TYPES_USAGE_ID ? Layout.GetTypesUsageID() : GetTypeHash(Layout);

	FVulkanTypedDescriptorPoolSet* TypedPool = TypedDescriptorPools.FindRef(Hash);

	if (!TypedPool)
	{
		TypedPool = new FVulkanTypedDescriptorPoolSet(Device, Layout);
		TypedDescriptorPools.Add(Hash, TypedPool);
	}

	return TypedPool;
}

void FVulkanDescriptorPoolSetContainer::Reset()
{
	for (auto& Pair : TypedDescriptorPools)
	{
		FVulkanTypedDescriptorPoolSet* TypedPool = Pair.Value;
		TypedPool->Reset();
	}
}

FVulkanDescriptorPoolsManager::~FVulkanDescriptorPoolsManager()
{
	for (auto* PoolSet : PoolSets)
	{
		delete PoolSet;
	}

	PoolSets.Reset();
}

FVulkanDescriptorPoolSetContainer& FVulkanDescriptorPoolsManager::AcquirePoolSetContainer()
{
	FScopeLock ScopeLock(&CS);

	for (auto* PoolSet : PoolSets)
	{
		if (PoolSet->IsUnused())
		{
			PoolSet->SetUsed(true);
			return *PoolSet;
		}
	}

	FVulkanDescriptorPoolSetContainer* PoolSet = new FVulkanDescriptorPoolSetContainer(Device);
	PoolSets.Add(PoolSet);

	return *PoolSet;
}

void FVulkanDescriptorPoolsManager::ReleasePoolSet(FVulkanDescriptorPoolSetContainer& PoolSet)
{
	PoolSet.Reset();
	PoolSet.SetUsed(false);
}

void FVulkanDescriptorPoolsManager::GC()
{
	FScopeLock ScopeLock(&CS);

	// Pool sets are forward allocated - iterate from the back to increase the chance of finding an unused one
	for (int32 Index = PoolSets.Num() - 1; Index >= 0; Index--)
	{
		auto* PoolSet = PoolSets[Index];
		if (PoolSet->IsUnused() && GFrameNumberRenderThread - PoolSet->GetLastFrameUsed() > NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS)
		{
			PoolSets.RemoveAtSwap(Index, 1, true);

			if (AsyncDeletionTask)
			{
				if (!AsyncDeletionTask->IsDone())
				{
					AsyncDeletionTask->EnsureCompletion();
				}

				AsyncDeletionTask->GetTask().SetPoolSet(PoolSet);
			}
			else
			{
				AsyncDeletionTask = new FAsyncTask<FVulkanAsyncPoolSetDeletionWorker>(PoolSet);
			}

			AsyncDeletionTask->StartBackgroundTask();

			break;
		}
	}
}


FVulkanPendingComputeState::~FVulkanPendingComputeState()
{
	for (auto It = PipelineStates.CreateIterator(); It; ++It)
	{
		FVulkanCommonPipelineDescriptorState* State = It->Value;
		delete State;
	}
}


void FVulkanPendingComputeState::SetSRVForUBResource(uint32 DescriptorSet, uint32 BindingIndex, FVulkanShaderResourceView* SRV)
{
	if (SRV)
	{
		// make sure any dynamically backed SRV points to current memory
		SRV->UpdateView();
		if (SRV->BufferViews.Num() != 0)
		{
			FVulkanBufferView* BufferView = SRV->GetBufferView();
			checkf(BufferView->View != VK_NULL_HANDLE, TEXT("Empty SRV"));
			CurrentState->SetSRVBufferViewState(DescriptorSet, BindingIndex, BufferView);
		}
		else if (SRV->SourceStructuredBuffer)
		{
			CurrentState->SetStorageBuffer(DescriptorSet, BindingIndex, SRV->SourceStructuredBuffer);
		}
#if VULKAN_RHI_RAYTRACING
		else if (SRV->AccelerationStructureHandle)
		{
			CurrentState->SetAccelerationStructure(DescriptorSet, BindingIndex, SRV->AccelerationStructureHandle);
		}
#endif // VULKAN_RHI_RAYTRACING
		else
		{
			checkf(SRV->TextureView.View != VK_NULL_HANDLE, TEXT("Empty SRV"));
			const FVulkanImageLayout& Layout = Context.GetLayoutManager().GetFullLayoutChecked(SRV->TextureView.Image);
			CurrentState->SetSRVTextureView(DescriptorSet, BindingIndex, SRV->TextureView, Layout.GetSubresLayout(SRV->FirstArraySlice, SRV->MipLevel));
		}
	}
	else
	{
		//CurrentState->SetSRVBufferViewState(BindIndex, nullptr);
	}
}

void FVulkanPendingComputeState::SetUAVForUBResource(uint32 DescriptorSet, uint32 BindingIndex, FVulkanUnorderedAccessView* UAV)
{
	if (UAV)
	{
		// make sure any dynamically backed UAV points to current memory
		UAV->UpdateView();
		if (UAV->SourceBuffer && UAV->BufferViewFormat == PF_Unknown)
		{
			CurrentState->SetStorageBuffer(DescriptorSet, BindingIndex, UAV->SourceBuffer);
		}
		else if (UAV->BufferView)
		{
			CurrentState->SetUAVTexelBufferViewState(DescriptorSet, BindingIndex, UAV->BufferView);
		}
		else if (UAV->SourceTexture)
		{
			CurrentState->SetUAVTextureView(DescriptorSet, BindingIndex, UAV->TextureView, VK_IMAGE_LAYOUT_GENERAL);
		}
		else
		{
			ensure(0);
		}
	}
}


void FVulkanPendingComputeState::PrepareForDispatch(FVulkanCmdBuffer* InCmdBuffer)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDispatchCallPrepareTime);
#endif

	check(CurrentState);

	const bool bHasDescriptorSets = CurrentState->UpdateDescriptorSets(&Context, InCmdBuffer);

	VkCommandBuffer CmdBuffer = InCmdBuffer->GetHandle();

	{
		//#todo-rco: Move this to SetComputePipeline()
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanPipelineBind);
#endif
		CurrentPipeline->Bind(CmdBuffer);
		if (bHasDescriptorSets)
		{
			CurrentState->BindDescriptorSets(CmdBuffer);
		}
	}
}

FVulkanPendingGfxState::~FVulkanPendingGfxState()
{
	TMap<FVulkanRHIGraphicsPipelineState*, FVulkanGraphicsPipelineDescriptorState*> Temp;
	Swap(Temp, PipelineStates);
	for (auto& Pair : Temp)
	{
		FVulkanGraphicsPipelineDescriptorState* State = Pair.Value;
		delete State;
	}
}

void FVulkanPendingGfxState::PrepareForDraw(FVulkanCmdBuffer* CmdBuffer)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanDrawCallPrepareTime);
#endif

	check(CmdBuffer->bHasPipeline);

	// TODO: Add 'dirty' flag? Need to rebind only on PSO change
	if (CurrentPipeline->bHasInputAttachments)
	{
		FVulkanFramebuffer* CurrentFramebuffer = Context.GetLayoutManager().CurrentFramebuffer;
		UpdateInputAttachments(CurrentFramebuffer);
	}
	
	bool bHasDescriptorSets = CurrentState->UpdateDescriptorSets(&Context, CmdBuffer);

	UpdateDynamicStates(CmdBuffer);

	if (bHasDescriptorSets)
	{
		CurrentState->BindDescriptorSets(CmdBuffer->GetHandle());
	}

	if (bDirtyVertexStreams)
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanBindVertexStreamsTime);
#endif
		// Its possible to have no vertex buffers
		const FVulkanVertexInputStateInfo& VertexInputStateInfo = CurrentPipeline->GetVertexInputState();
		if (VertexInputStateInfo.AttributesNum == 0)
		{
			// However, we need to verify that there are also no bindings
			check(VertexInputStateInfo.BindingsNum == 0);
			return;
		}

		struct FTemporaryIA
		{
			VkBuffer VertexBuffers[MaxVertexElementCount];
			VkDeviceSize VertexOffsets[MaxVertexElementCount];
			int32 NumUsed = 0;

			void Add(VkBuffer InBuffer, VkDeviceSize InSize)
			{
				check(NumUsed < MaxVertexElementCount);
				VertexBuffers[NumUsed] = InBuffer;
				VertexOffsets[NumUsed] = InSize;
				++NumUsed;
			}
		} TemporaryIA;

		const VkVertexInputAttributeDescription* CurrAttribute = nullptr;
		for (uint32 BindingIndex = 0; BindingIndex < VertexInputStateInfo.BindingsNum; BindingIndex++)
		{
			const VkVertexInputBindingDescription& CurrBinding = VertexInputStateInfo.Bindings[BindingIndex];

			uint32 StreamIndex = VertexInputStateInfo.BindingToStream.FindChecked(BindingIndex);
			FVulkanPendingGfxState::FVertexStream& CurrStream = PendingStreams[StreamIndex];

			// Verify the vertex buffer is set
			if (CurrStream.Stream == VK_NULL_HANDLE)
			{
				// The attribute in stream index is probably compiled out
#if UE_BUILD_DEBUG
				// Lets verify
				for (uint32 AttributeIndex = 0; AttributeIndex < VertexInputStateInfo.AttributesNum; AttributeIndex++)
				{
					if (VertexInputStateInfo.Attributes[AttributeIndex].binding == CurrBinding.binding)
					{
						uint64 VertexShaderKey = GetCurrentShaderKey(ShaderStage::Vertex);
						FVulkanVertexShader* VertexShader = Device->GetShaderFactory().LookupShader<FVulkanVertexShader>(VertexShaderKey);
						UE_LOG(LogVulkanRHI, Warning, TEXT("Missing input assembly binding on location %d in Vertex shader '%s'"),
							CurrBinding.binding,
							VertexShader ? *VertexShader->GetDebugName() : TEXT("Null"));
						ensure(0);
					}
				}
#endif
				continue;
			}

			TemporaryIA.Add(CurrStream.Stream, CurrStream.BufferOffset);
		}

		if (TemporaryIA.NumUsed > 0)
		{
			// Bindings are expected to be in ascending order with no index gaps in between:
			// Correct:		0, 1, 2, 3
			// Incorrect:	1, 0, 2, 3
			// Incorrect:	0, 2, 3, 5
			// Reordering and creation of stream binding index is done in "GenerateVertexInputStateInfo()"
			VulkanRHI::vkCmdBindVertexBuffers(CmdBuffer->GetHandle(), 0, TemporaryIA.NumUsed, TemporaryIA.VertexBuffers, TemporaryIA.VertexOffsets);
		}

		bDirtyVertexStreams = false;
	}
}

void FVulkanPendingGfxState::InternalUpdateDynamicStates(FVulkanCmdBuffer* Cmd)
{
	const bool bNeedsUpdateViewport = !Cmd->bHasViewport || Viewports.Num() != Cmd->CurrentViewports.Num() || (FMemory::Memcmp((const void*)Cmd->CurrentViewports.GetData(), (const void*)Viewports.GetData(), Viewports.Num() * sizeof(VkViewport)) != 0);
	// Validate and update Viewport
	if (bNeedsUpdateViewport)
	{
		ensure(Viewports[0].width > 0 || Viewports[0].height > 0);

		// Flip viewport on Y-axis to be uniform between DXC generated SPIR-V shaders (requires VK_KHR_maintenance1 extension)
		TArray<VkViewport, TInlineAllocator<2>> FlippedViewports = Viewports;
		for (VkViewport& FlippedViewport : FlippedViewports)
		{
			FlippedViewport.y += FlippedViewport.height;
			FlippedViewport.height = -FlippedViewport.height;
		}
		VulkanRHI::vkCmdSetViewport(Cmd->GetHandle(), 0, FlippedViewports.Num(), FlippedViewports.GetData());

		Cmd->CurrentViewports = Viewports;
		Cmd->bHasViewport = true;
	}

	const bool bNeedsUpdateScissor = !Cmd->bHasScissor || Scissors.Num() != Cmd->CurrentScissors.Num() || (FMemory::Memcmp((const void*)Cmd->CurrentScissors.GetData(), (const void*)Scissors.GetData(), Scissors.Num() * sizeof(VkRect2D)) != 0);
	if (bNeedsUpdateScissor)
	{
		VulkanRHI::vkCmdSetScissor(Cmd->GetHandle(), 0, Scissors.Num(), Scissors.GetData());
		Cmd->CurrentScissors = Scissors;
		Cmd->bHasScissor = true;
	}

	const bool bNeedsUpdateStencil = !Cmd->bHasStencilRef || (Cmd->CurrentStencilRef != StencilRef);
	if (bNeedsUpdateStencil)
	{
		VulkanRHI::vkCmdSetStencilReference(Cmd->GetHandle(), VK_STENCIL_FRONT_AND_BACK, StencilRef);
		Cmd->CurrentStencilRef = StencilRef;
		Cmd->bHasStencilRef = true;
	}

	Cmd->bNeedsDynamicStateSet = false;
}

void FVulkanPendingGfxState::UpdateInputAttachments(FVulkanFramebuffer* Framebuffer)
{
	const FVulkanGfxPipelineDescriptorInfo& GfxDescriptorInfo = CurrentState->GetGfxPipelineDescriptorInfo();
	const TArray<FInputAttachmentData>& InputAttachmentData = GfxDescriptorInfo.GetInputAttachmentData();

	for (int32 Index = 0; Index < InputAttachmentData.Num(); ++Index)
	{
		const FInputAttachmentData& AttachmentData = InputAttachmentData[Index];
		const uint32 ColorIndex = static_cast<uint32>(AttachmentData.Type);
		
		switch (AttachmentData.Type)
		{
		case FVulkanShaderHeader::EAttachmentType::Color0:
			CurrentState->SetInputAttachment(AttachmentData.DescriptorSet, AttachmentData.BindingIndex, Framebuffer->AttachmentTextureViews[0], VK_IMAGE_LAYOUT_GENERAL);
			break;
		case FVulkanShaderHeader::EAttachmentType::Color1:
		case FVulkanShaderHeader::EAttachmentType::Color2:
		case FVulkanShaderHeader::EAttachmentType::Color3:
		case FVulkanShaderHeader::EAttachmentType::Color4:
		case FVulkanShaderHeader::EAttachmentType::Color5:
		case FVulkanShaderHeader::EAttachmentType::Color6:
		case FVulkanShaderHeader::EAttachmentType::Color7:
			check(ColorIndex < Framebuffer->GetNumColorAttachments());
			CurrentState->SetInputAttachment(AttachmentData.DescriptorSet, AttachmentData.BindingIndex, Framebuffer->AttachmentTextureViews[ColorIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			break;
		case FVulkanShaderHeader::EAttachmentType::Depth:
			CurrentState->SetInputAttachment(AttachmentData.DescriptorSet, AttachmentData.BindingIndex, Framebuffer->GetPartialDepthTextureView(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
			break;
		default:
			check(0);
		}
	}
}

void FVulkanPendingGfxState::SetSRVForUBResource(uint8 DescriptorSet, uint32 BindingIndex, FVulkanShaderResourceView* SRV)
{
	if (SRV)
	{
		// make sure any dynamically backed SRV points to current memory
		SRV->UpdateView();
		if (SRV->BufferViews.Num() != 0)
		{
			FVulkanBufferView* BufferView = SRV->GetBufferView();
			checkf(BufferView->View != VK_NULL_HANDLE, TEXT("Empty SRV"));

			CurrentState->SetSRVBufferViewState(DescriptorSet, BindingIndex, BufferView);
		}
		else if (SRV->SourceStructuredBuffer)
		{
			CurrentState->SetStorageBuffer(DescriptorSet, BindingIndex, SRV->SourceStructuredBuffer);
		}
		else
		{
			checkf(SRV->TextureView.View != VK_NULL_HANDLE, TEXT("Empty SRV"));
			const FVulkanImageLayout& Layout = Context.GetLayoutManager().GetFullLayoutChecked(SRV->TextureView.Image);
			CurrentState->SetSRVTextureView(DescriptorSet, BindingIndex, SRV->TextureView, Layout.GetSubresLayout(SRV->FirstArraySlice, SRV->MipLevel));
		}
	}
	else
	{
		//CurrentState->SetSRVBufferViewState(Stage, BindIndex, nullptr);
	}
}

void FVulkanPendingGfxState::SetUAVForUBResource(uint8 DescriptorSet, uint32 BindingIndex, FVulkanUnorderedAccessView* UAV)
{
	if (UAV)
	{
		// make sure any dynamically backed UAV points to current memory
		UAV->UpdateView();
		if (UAV->SourceBuffer && UAV->BufferViewFormat == PF_Unknown)
		{
			CurrentState->SetStorageBuffer(DescriptorSet, BindingIndex, UAV->SourceBuffer);
		}
		else if (UAV->BufferView)
		{
			CurrentState->SetUAVTexelBufferViewState(DescriptorSet, BindingIndex, UAV->BufferView);
		}
		else if (UAV->SourceTexture)
		{
			const FVulkanImageLayout& Layout = Context.GetLayoutManager().GetFullLayoutChecked(UAV->TextureView.Image);
			CurrentState->SetUAVTextureView(DescriptorSet, BindingIndex, UAV->TextureView, Layout.GetSubresLayout(UAV->FirstArraySlice, UAV->MipLevel));
		}
		else
		{
			ensure(0);
		}
	}
}

int32 GDSetCacheTargetSetsPerPool = 4096;
FAutoConsoleVariableRef CVarDSetCacheTargetSetsPerPool(
	TEXT("r.Vulkan.DSetCacheTargetSetsPerPool"),
	GDSetCacheTargetSetsPerPool,
	TEXT("Target number of descriptor set allocations per single pool.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

int32 GDSetCacheMaxPoolLookups = 2;
FAutoConsoleVariableRef CVarDSetCacheMaxPoolLookups(
	TEXT("r.Vulkan.DSetCacheMaxPoolLookups"),
	GDSetCacheMaxPoolLookups,
	TEXT("Maximum count of pool's caches to lookup before allocating new descriptor.\n"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

const float DefaultPoolSizes[VK_DESCRIPTOR_TYPE_RANGE_SIZE] = 
{
	2,		// VK_DESCRIPTOR_TYPE_SAMPLER
	2,		// VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
	2,		// VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
	1/8.0,	// VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
	1/2.0,	// VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
	1/8.0,	// VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
	1/4.0,	// VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
	1/8.0,	// VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
	4,		// VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
	1/8.0,	// VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
	1/8.0	// VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
};

FVulkanGenericDescriptorPool::FVulkanGenericDescriptorPool(FVulkanDevice* InDevice, uint32 InMaxDescriptorSets, const float PoolSizesRatio[VK_DESCRIPTOR_TYPE_RANGE_SIZE])
	: Device(InDevice)
	, MaxDescriptorSets(InMaxDescriptorSets)
	, DescriptorPool(VK_NULL_HANDLE)
{
	VkDescriptorPoolSize Types[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
	FMemory::Memzero(Types);

	for (uint32 i = 0; i < VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++i)
	{
		VkDescriptorType DescriptorType = static_cast<VkDescriptorType>(VK_DESCRIPTOR_TYPE_BEGIN_RANGE + i);
		
		float MinSize = FMath::Max(DefaultPoolSizes[i]*InMaxDescriptorSets, 4.0f);
		PoolSizes[i] = (uint32)FMath::Max(PoolSizesRatio[i]*InMaxDescriptorSets, MinSize);
		
		Types[i].type = DescriptorType;
		Types[i].descriptorCount = PoolSizes[i];
	}

	VkDescriptorPoolCreateInfo PoolInfo;
	ZeroVulkanStruct(PoolInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
	PoolInfo.poolSizeCount = VK_DESCRIPTOR_TYPE_RANGE_SIZE;
	PoolInfo.pPoolSizes = Types;
	PoolInfo.maxSets = MaxDescriptorSets;

#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanVkCreateDescriptorPool);
#endif
	VERIFYVULKANRESULT(VulkanRHI::vkCreateDescriptorPool(Device->GetInstanceHandle(), &PoolInfo, VULKAN_CPU_ALLOCATOR, &DescriptorPool));

	INC_DWORD_STAT_BY(STAT_VulkanNumDescSetsTotal, MaxDescriptorSets);
	INC_DWORD_STAT(STAT_VulkanNumDescPools);
}

FVulkanGenericDescriptorPool::~FVulkanGenericDescriptorPool()
{
	if (DescriptorPool != VK_NULL_HANDLE)
	{
		DEC_DWORD_STAT_BY(STAT_VulkanNumDescSetsTotal, MaxDescriptorSets);
		DEC_DWORD_STAT(STAT_VulkanNumDescPools);

		VulkanRHI::vkDestroyDescriptorPool(Device->GetInstanceHandle(), DescriptorPool, VULKAN_CPU_ALLOCATOR);
	}
}

void FVulkanGenericDescriptorPool::Reset()
{
	check(DescriptorPool != VK_NULL_HANDLE);
	VERIFYVULKANRESULT(VulkanRHI::vkResetDescriptorPool(Device->GetInstanceHandle(), DescriptorPool, 0));
}

bool FVulkanGenericDescriptorPool::AllocateDescriptorSet(VkDescriptorSetLayout Layout, VkDescriptorSet& OutSet)
{
	check(DescriptorPool != VK_NULL_HANDLE);

	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo;
	ZeroVulkanStruct(DescriptorSetAllocateInfo, VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
	DescriptorSetAllocateInfo.descriptorPool = DescriptorPool;
	DescriptorSetAllocateInfo.descriptorSetCount = 1;
	DescriptorSetAllocateInfo.pSetLayouts = &Layout;

	return (VK_SUCCESS == VulkanRHI::vkAllocateDescriptorSets(Device->GetInstanceHandle(), &DescriptorSetAllocateInfo, &OutSet));
}

FVulkanDescriptorSetCache::FVulkanDescriptorSetCache(FVulkanDevice* InDevice)
	: Device(InDevice)
	, PoolAllocRatio(0.0f)
{
	constexpr uint32 ProbePoolMaxNumSets = 128; // Used for initial estimation of the Allocation Ratio
	CachedPools.Add(MakeUnique<FCachedPool>(Device, ProbePoolMaxNumSets, DefaultPoolSizes));
}

FVulkanDescriptorSetCache::~FVulkanDescriptorSetCache()
{
}

void FVulkanDescriptorSetCache::UpdateAllocRatio()
{
	const float FilterParam = ((PoolAllocRatio > 0.0f) ? 2.0f : 0.0f);
	PoolAllocRatio = (PoolAllocRatio * FilterParam + CachedPools[0]->CalcAllocRatio()) / (FilterParam + 1.0f);
}

void FVulkanDescriptorSetCache::AddCachedPool()
{
	check(PoolAllocRatio > 0.0f);
	const uint32 MaxDescriptorSets = FMath::RoundFromZero(GDSetCacheTargetSetsPerPool / PoolAllocRatio);
	if (FreePool)
	{
		constexpr float MinErrorTolerance = -0.10f;
		constexpr float MaxErrorTolerance = 0.50f;
		const float Error = ((static_cast<float>(FreePool->GetMaxDescriptorSets()) - static_cast<float>(MaxDescriptorSets)) / static_cast<float>(MaxDescriptorSets));
		if ((Error >= MinErrorTolerance) && (Error <= MaxErrorTolerance))
		{
			FreePool->Reset();
			CachedPools.EmplaceAt(0, MoveTemp(FreePool));
			return;
		}

		// Don't write 'error' as it confuses reporting; it's a perf warning more than an actual error
		UE_LOG(LogVulkanRHI, Display, TEXT("FVulkanDescriptorSetCache::AddCachedPool() MaxDescriptorSets Delta/Err: %f. Tolerance: [%f..%f]."),	Error, MinErrorTolerance, MaxErrorTolerance);
		FreePool.Reset();
	}
	
	// use current pool sizes statistic for a new pool
	float PoolSizesRatio[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
	CachedPools[0]->CalcPoolSizesRatio(PoolSizesRatio);

	CachedPools.EmplaceAt(0, MakeUnique<FCachedPool>(Device, MaxDescriptorSets, PoolSizesRatio));
}

void FVulkanDescriptorSetCache::GetDescriptorSets(const FVulkanDSetsKey& DSetsKey, const FVulkanDescriptorSetsLayout& SetsLayout,
	TArray<FVulkanDescriptorSetWriter>& DSWriters, VkDescriptorSet* OutSets)
{
	check(CachedPools.Num() > 0);

	for (int32 Index = 0; (Index < GDSetCacheMaxPoolLookups) && (Index < CachedPools.Num()); ++Index)
	{
		if (CachedPools[Index]->FindDescriptorSets(DSetsKey, OutSets))
		{
			return;
		}
	}

	bool bFirstTime = true;
	while (!CachedPools[0]->CreateDescriptorSets(DSetsKey, SetsLayout, DSWriters, OutSets))
	{
		checkf(bFirstTime, TEXT("FATAL! Failed to create descriptor sets from new pool!"));
		bFirstTime = false;
		UpdateAllocRatio();
		AddCachedPool();
	}
}

void FVulkanDescriptorSetCache::GC()
{
	// Loop is for OOM safety. Normally there would be at most 1 loop.
	while ((CachedPools.Num() > GDSetCacheMaxPoolLookups) && CachedPools.Last()->CanGC())
	{
		const uint32 RemoveIndex = (CachedPools.Num() - 1);
		if (FreePool)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("FVulkanDescriptorSetCache::GC() Free Pool is not empty! Too small r.Vulkan.DSetCacheTargetSetsPerPool?"));
		}
		FreePool = MoveTemp(CachedPools[RemoveIndex]);
		CachedPools.RemoveAt(RemoveIndex, 1, false);
	}
}

const float FVulkanDescriptorSetCache::FCachedPool::MinAllocRatio = 0.5f;
const float FVulkanDescriptorSetCache::FCachedPool::MaxAllocRatio = 16.0f;

FVulkanDescriptorSetCache::FCachedPool::FCachedPool(FVulkanDevice* InDevice, uint32 InMaxDescriptorSets, const float PoolSizes[VK_DESCRIPTOR_TYPE_RANGE_SIZE])
	: SetCapacity(FMath::RoundToZero(InMaxDescriptorSets * MaxAllocRatio))
	, Pool(InDevice, InMaxDescriptorSets, PoolSizes)
	, RecentFrame(0)
{
	FMemory::Memzero(PoolSizesStatistic, sizeof(PoolSizesStatistic));
}

bool FVulkanDescriptorSetCache::FCachedPool::FindDescriptorSets(const FVulkanDSetsKey& DSetsKey, VkDescriptorSet* OutSets)
{
	FSetsEntry* SetsEntry = SetsCache.Find(DSetsKey);
	if (!SetsEntry)
	{
		return false;
	}

	FMemory::Memcpy(OutSets, &SetsEntry->Sets, sizeof(VkDescriptorSet) * SetsEntry->NumSets);
	RecentFrame = GFrameNumberRenderThread;

	return true;
}

bool FVulkanDescriptorSetCache::FCachedPool::CreateDescriptorSets(
	const FVulkanDSetsKey& DSetsKey, 
	const FVulkanDescriptorSetsLayout& SetsLayout,
	TArray<FVulkanDescriptorSetWriter>& DSWriters, 
	VkDescriptorSet* OutSets)
{
	FSetsEntry NewSetEntry{};

	NewSetEntry.NumSets = DSWriters.Num();
	check(NewSetEntry.NumSets <= NewSetEntry.Sets.Num());
	check(NewSetEntry.NumSets == SetsLayout.GetHandles().Num());

	for (int32 Index = 0; Index < NewSetEntry.NumSets; ++Index)
	{
		FVulkanDescriptorSetWriter& DSWriter = DSWriters[Index];
		if (DSWriter.GetNumWrites() == 0) // Should not normally happen
		{
			NewSetEntry.Sets[Index] = VK_NULL_HANDLE;
			continue;
		}
		if (VkDescriptorSet* FoundSet = SetCache.Find(DSWriter.GetKey()))
		{
			NewSetEntry.Sets[Index] = *FoundSet;
			continue;
		}

		if ((SetCache.Num() == SetCapacity) ||
			!Pool.AllocateDescriptorSet(SetsLayout.GetHandles()[Index], NewSetEntry.Sets[Index]))
		{
			return false;
		}
		SetCache.Emplace(DSWriter.GetKey().CopyDeep(), NewSetEntry.Sets[Index]);

		DSWriter.SetDescriptorSet(NewSetEntry.Sets[Index]);

		for (int32 i = 0; i < VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++i)
		{
			VkDescriptorType DescriptorType = static_cast<VkDescriptorType>(VK_DESCRIPTOR_TYPE_BEGIN_RANGE + i);
			PoolSizesStatistic[i] += SetsLayout.GetTypesUsed(DescriptorType);
		}

#if VULKAN_ENABLE_AGGRESSIVE_STATS
		INC_DWORD_STAT_BY(STAT_VulkanNumUpdateDescriptors, DSWriter.GetNumWrites());
		INC_DWORD_STAT(STAT_VulkanNumDescSets);
		SCOPE_CYCLE_COUNTER(STAT_VulkanVkUpdateDS);
#endif
		VulkanRHI::vkUpdateDescriptorSets(Pool.GetDevice()->GetInstanceHandle(),
			DSWriter.GetNumWrites(), DSWriter.GetWriteDescriptors(), 0, nullptr);
	}

	SetsCache.Emplace(DSetsKey.CopyDeep(), MoveTemp(NewSetEntry));

	FMemory::Memcpy(OutSets, &NewSetEntry.Sets, sizeof(VkDescriptorSet) * NewSetEntry.NumSets);
	RecentFrame = GFrameNumberRenderThread;

	return true;
}

void FVulkanDescriptorSetCache::FCachedPool::Reset()
{
	Pool.Reset();
	SetsCache.Reset();
	SetCache.Reset();
	FMemory::Memzero(PoolSizesStatistic, sizeof(PoolSizesStatistic));
}

bool FVulkanDescriptorSetCache::FCachedPool::CanGC() const
{
	constexpr uint32 FramesBeforeGC = NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS;
	return ((GFrameNumberRenderThread - RecentFrame) > FramesBeforeGC);
}

float FVulkanDescriptorSetCache::FCachedPool::CalcAllocRatio() const
{
	float AllocRatio = (static_cast<float>(SetCache.Num()) / static_cast<float>(Pool.GetMaxDescriptorSets()));
	if (AllocRatio < MinAllocRatio)
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("FVulkanDescriptorSetCache::FCachedPool::CalcAllocRatio() Pool Allocation Ratio is too low: %f. Using: %f."), AllocRatio, MinAllocRatio);
		AllocRatio = MinAllocRatio;
	}
	return AllocRatio;
}

void FVulkanDescriptorSetCache::FCachedPool::CalcPoolSizesRatio(float PoolSizesRatio[VK_DESCRIPTOR_TYPE_RANGE_SIZE])
{
	int32 NumSets = FMath::Max(SetCache.Num(), 1);
	for (uint32 i = 0; i < VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++i)
	{
		PoolSizesRatio[i] = PoolSizesStatistic[i]/(float)NumSets;
	}
}

bool FVulkanPendingGfxState::SetGfxPipeline(FVulkanRHIGraphicsPipelineState* InGfxPipeline, bool bForceReset)
{
	bool bChanged = bForceReset;

	if (InGfxPipeline != CurrentPipeline)
	{
		CurrentPipeline = InGfxPipeline;
		FVulkanGraphicsPipelineDescriptorState** Found = PipelineStates.Find(InGfxPipeline);
		if (Found)
		{
			CurrentState = *Found;
			check(CurrentState->GfxPipeline == InGfxPipeline);
		}
		else
		{
			CurrentState = new FVulkanGraphicsPipelineDescriptorState(Device, InGfxPipeline);
			PipelineStates.Add(CurrentPipeline, CurrentState);
		}

		PrimitiveType = InGfxPipeline->PrimitiveType;
		bChanged = true;
	}

	if (bChanged || bForceReset)
	{
		CurrentState->Reset();
	}

	return bChanged;
}
