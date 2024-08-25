// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanStatePipeline.cpp: Vulkan pipeline state implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanPipelineState.h"
#include "VulkanResources.h"
#include "VulkanPipeline.h"
#include "VulkanContext.h"
#include "VulkanPendingState.h"
#include "VulkanPipeline.h"
#include "VulkanLLM.h"
#include "RHICoreShader.h"

enum
{
	NumAllocationsPerPool = 8,
};

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
static TAutoConsoleVariable<int32> GAlwaysWriteDS(
	TEXT("r.Vulkan.AlwaysWriteDS"),
	0,
	TEXT(""),
	ECVF_RenderThreadSafe
);
#endif

static bool ShouldAlwaysWriteDescriptors()
{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	return (GAlwaysWriteDS.GetValueOnAnyThread() != 0);
#else
	return false;
#endif
}

FVulkanComputePipelineDescriptorState::FVulkanComputePipelineDescriptorState(FVulkanDevice* InDevice, FVulkanComputePipeline* InComputePipeline)
	: FVulkanCommonPipelineDescriptorState(InDevice)
	, PackedUniformBuffersMask(0)
	, PackedUniformBuffersDirty(0)
	, ComputePipeline(InComputePipeline)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanShaders);
	check(InComputePipeline);
	const FVulkanShaderHeader& CodeHeader = InComputePipeline->GetShaderCodeHeader();
	PackedUniformBuffers.Init(CodeHeader, PackedUniformBuffersMask);

	DescriptorSetsLayout = &InComputePipeline->GetLayout().GetDescriptorSetsLayout();
	PipelineDescriptorInfo = &InComputePipeline->GetComputeLayout().GetComputePipelineDescriptorInfo();

	UsedSetsMask = PipelineDescriptorInfo->HasDescriptorsInSetMask;

	CreateDescriptorWriteInfos();
	InComputePipeline->AddRef();

	ensure(DSWriter.Num() == 0 || DSWriter.Num() == 1);
}

void FVulkanCommonPipelineDescriptorState::CreateDescriptorWriteInfos()
{
	check(DSWriteContainer.DescriptorWrites.Num() == 0);

	const int32 NumSets = DescriptorSetsLayout->RemappingInfo.SetInfos.Num();
	check(UsedSetsMask <= (uint32)(((uint32)1 << NumSets) - 1));

	for (int32 Set = 0; Set < NumSets; ++Set)
	{
		const FDescriptorSetRemappingInfo::FSetInfo& SetInfo = DescriptorSetsLayout->RemappingInfo.SetInfos[Set];
		
		if (UseVulkanDescriptorCache())
		{
			DSWriteContainer.HashableDescriptorInfo.AddZeroed(SetInfo.Types.Num() + 1); // Add 1 for the Layout
		}
		DSWriteContainer.DescriptorWrites.AddZeroed(SetInfo.Types.Num());
		DSWriteContainer.DescriptorImageInfo.AddZeroed(SetInfo.NumImageInfos);
		DSWriteContainer.DescriptorBufferInfo.AddZeroed(SetInfo.NumBufferInfos);

#if VULKAN_RHI_RAYTRACING
		DSWriteContainer.AccelerationStructureWrites.AddZeroed(SetInfo.NumAccelerationStructures);
		DSWriteContainer.AccelerationStructures.AddZeroed(SetInfo.NumAccelerationStructures);
#endif // VULKAN_RHI_RAYTRACING

		checkf(SetInfo.Types.Num() < 255, TEXT("Need more bits for BindingToDynamicOffsetMap (currently 8)! Requires %d descriptor bindings in a set!"), SetInfo.Types.Num());
		DSWriteContainer.BindingToDynamicOffsetMap.AddUninitialized(SetInfo.Types.Num());
	}

	FMemory::Memset(DSWriteContainer.BindingToDynamicOffsetMap.GetData(), 255, DSWriteContainer.BindingToDynamicOffsetMap.Num());

	check(DSWriter.Num() == 0);
	DSWriter.AddDefaulted(NumSets);

	const FVulkanSamplerState& DefaultSampler = Device->GetDefaultSampler();
	const FVulkanView::FTextureView& DefaultImageView = Device->GetDefaultImageView();

	FVulkanHashableDescriptorInfo* CurrentHashableDescriptorInfo = nullptr;
	if (UseVulkanDescriptorCache())
	{
		CurrentHashableDescriptorInfo = DSWriteContainer.HashableDescriptorInfo.GetData();
	}
	VkWriteDescriptorSet* CurrentDescriptorWrite = DSWriteContainer.DescriptorWrites.GetData();
	VkDescriptorImageInfo* CurrentImageInfo = DSWriteContainer.DescriptorImageInfo.GetData();
	VkDescriptorBufferInfo* CurrentBufferInfo = DSWriteContainer.DescriptorBufferInfo.GetData();

#if VULKAN_RHI_RAYTRACING
	VkWriteDescriptorSetAccelerationStructureKHR* CurrentAccelerationStructuresWriteDescriptors = DSWriteContainer.AccelerationStructureWrites.GetData();
	VkAccelerationStructureKHR* CurrentAccelerationStructures = DSWriteContainer.AccelerationStructures.GetData();
#endif // VULKAN_RHI_RAYTRACING

	uint8* CurrentBindingToDynamicOffsetMap = DSWriteContainer.BindingToDynamicOffsetMap.GetData();
	TArray<uint32> DynamicOffsetsStart;
	DynamicOffsetsStart.AddZeroed(NumSets);
	uint32 TotalNumDynamicOffsets = 0;

	for (int32 Set = 0; Set < NumSets; ++Set)
	{
		const FDescriptorSetRemappingInfo::FSetInfo& SetInfo = DescriptorSetsLayout->RemappingInfo.SetInfos[Set];

		DynamicOffsetsStart[Set] = TotalNumDynamicOffsets;

		uint32 NumDynamicOffsets = DSWriter[Set].SetupDescriptorWrites(
			SetInfo.Types, CurrentHashableDescriptorInfo,
			CurrentDescriptorWrite, CurrentImageInfo, CurrentBufferInfo, CurrentBindingToDynamicOffsetMap,
#if VULKAN_RHI_RAYTRACING
			CurrentAccelerationStructuresWriteDescriptors,
			CurrentAccelerationStructures,
#endif // VULKAN_RHI_RAYTRACING
			DefaultSampler, DefaultImageView);

		TotalNumDynamicOffsets += NumDynamicOffsets;

		if (CurrentHashableDescriptorInfo) // UseVulkanDescriptorCache()
		{
			CurrentHashableDescriptorInfo += SetInfo.Types.Num();
			CurrentHashableDescriptorInfo->Layout.Max0 = UINT32_MAX;
			CurrentHashableDescriptorInfo->Layout.Max1 = UINT32_MAX;
			CurrentHashableDescriptorInfo->Layout.LayoutId = DescriptorSetsLayout->GetHandleIds()[Set];
			++CurrentHashableDescriptorInfo;
		}

		CurrentDescriptorWrite += SetInfo.Types.Num();
		CurrentImageInfo += SetInfo.NumImageInfos;
		CurrentBufferInfo += SetInfo.NumBufferInfos;

#if VULKAN_RHI_RAYTRACING
		CurrentAccelerationStructuresWriteDescriptors += SetInfo.NumAccelerationStructures;
		CurrentAccelerationStructures += SetInfo.NumAccelerationStructures;
#endif // VULKAN_RHI_RAYTRACING

		CurrentBindingToDynamicOffsetMap += SetInfo.Types.Num();
	}

	DynamicOffsets.AddZeroed(TotalNumDynamicOffsets);
	for (int32 Set = 0; Set < NumSets; ++Set)
	{
		DSWriter[Set].DynamicOffsets = DynamicOffsetsStart[Set] + DynamicOffsets.GetData();
	}

	DescriptorSetHandles.AddZeroed(NumSets);
}

template<bool bUseDynamicGlobalUBs>
bool FVulkanComputePipelineDescriptorState::InternalUpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateDescriptorSets);
#endif

	// Early exit
	if (!UsedSetsMask)
	{
		return false;
	}

	FVulkanUniformBufferUploader* UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8* CPURingBufferBase = (uint8*)UniformBufferUploader->GetCPUMappedPointer();
	const VkDeviceSize UBOffsetAlignment = Device->GetLimits().minUniformBufferOffsetAlignment;

	if (PackedUniformBuffersDirty != 0)
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanApplyPackedUniformBuffers);
#endif
		UpdatePackedUniformBuffers<bUseDynamicGlobalUBs>(UBOffsetAlignment, PipelineDescriptorInfo->RemappingInfo->StageInfos[0].PackedUBBindingIndices.GetData(), PackedUniformBuffers, DSWriter[0], UniformBufferUploader, CPURingBufferBase, PackedUniformBuffersDirty, CmdBuffer);
		PackedUniformBuffersDirty = 0;
	}

	// We are not using UseVulkanDescriptorCache() for compute pipelines
	// Compute tend to use volatile resources that polute descriptor cache

	if (!CmdBuffer->AcquirePoolSetAndDescriptorsIfNeeded(*DescriptorSetsLayout, true, DescriptorSetHandles.GetData()))
	{
		return false;
	}

	const VkDescriptorSet DescriptorSet = DescriptorSetHandles[0];
	DSWriter[0].SetDescriptorSet(DescriptorSet);
#if VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
	for(FVulkanDescriptorSetWriter& Writer : DSWriter)
	{
		Writer.CheckAllWritten();
	}
#endif

	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		INC_DWORD_STAT_BY(STAT_VulkanNumUpdateDescriptors, DSWriteContainer.DescriptorWrites.Num());
		INC_DWORD_STAT(STAT_VulkanNumDescSets);
		SCOPE_CYCLE_COUNTER(STAT_VulkanVkUpdateDS);
#endif
		VulkanRHI::vkUpdateDescriptorSets(Device->GetInstanceHandle(), DSWriteContainer.DescriptorWrites.Num(), DSWriteContainer.DescriptorWrites.GetData(), 0, nullptr);
	}

	return true;
}

void FVulkanComputePipelineDescriptorState::UpdateBindlessDescriptors(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
{
	check(bUseBindless);

	// We should only have uniform buffers at this point
	check(DSWriteContainer.DescriptorBufferInfo.Num() == DSWriteContainer.DescriptorWrites.Num());
	check(DSWriteContainer.DescriptorImageInfo.Num() == 0);

	FVulkanUniformBufferUploader* UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8* CPURingBufferBase = (uint8*)UniformBufferUploader->GetCPUMappedPointer();
	const VkDeviceSize UBOffsetAlignment = Device->GetLimits().minUniformBufferOffsetAlignment;

	FVulkanBindlessDescriptorManager::FUniformBufferDescriptorArrays StageUBs;

	const int32 Stage = (int32)ShaderStage::EStage::Compute;
	const FDescriptorSetRemappingInfo* RESTRICT RemappingInfo = PipelineDescriptorInfo->RemappingInfo;
	const FDescriptorSetRemappingInfo::FStageInfo& StageInfo = RemappingInfo->StageInfos[Stage];

	TArray<VkDescriptorAddressInfoEXT>& DescriptorAddressInfos = StageUBs[Stage];
	DescriptorAddressInfos.SetNumZeroed(StageInfo.PackedUBBindingIndices.Num() + StageInfo.UniformBuffers.Num());

	// PackedUniformBuffersDirty ?
	check((PackedUniformBuffersMask == 0) || (PackedUniformBuffersMask == 1));
	if (PackedUniformBuffersMask != 0)
	{
		check((StageInfo.PackedUBDescriptorSet != UINT16_MAX) && (StageInfo.PackedUBDescriptorSet <= (uint16)Stage));
		uint64 RemainingPackedUniformsMask = PackedUniformBuffersMask;
		int32 PackedUBIndex = 0;
		while (RemainingPackedUniformsMask)
		{
			if (RemainingPackedUniformsMask & 1)
			{
				const FPackedUniformBuffers::FPackedBuffer& StagedUniformBuffer = PackedUniformBuffers.GetBuffer(PackedUBIndex);
				const int32 UBSize = StagedUniformBuffer.Num();
				const int32 BindingIndex = StageInfo.PackedUBBindingIndices[PackedUBIndex];

				const uint64 RingBufferOffset = UniformBufferUploader->AllocateMemory(UBSize, UBOffsetAlignment, CmdBuffer);

				// Make sure it wasn't written to already
				VkDescriptorAddressInfoEXT& DescriptorAddressInfo = DescriptorAddressInfos[BindingIndex];
				check(DescriptorAddressInfo.sType == 0);
				DescriptorAddressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
				DescriptorAddressInfo.address = UniformBufferUploader->GetCPUBufferAddress() + RingBufferOffset;
				DescriptorAddressInfo.range = UBSize;

				// get location in the ring buffer to use
				FMemory::Memcpy(CPURingBufferBase + RingBufferOffset, StagedUniformBuffer.GetData(), UBSize);
			}
			RemainingPackedUniformsMask = RemainingPackedUniformsMask >> 1;
			++PackedUBIndex;
		}

		PackedUniformBuffersDirty = 0;
	}

	for (int32 UBIndex = 0; UBIndex < StageInfo.UniformBuffers.Num(); ++UBIndex)
	{
		const FDescriptorSetRemappingInfo::FUBRemappingInfo& UBRemappingInfo = StageInfo.UniformBuffers[UBIndex];
		check((StageInfo.PackedUBDescriptorSet == UINT16_MAX) || UBRemappingInfo.Remapping.NewDescriptorSet == StageInfo.PackedUBDescriptorSet);
		check(UBRemappingInfo.bHasConstantData);

		VkDescriptorAddressInfoEXT& DescriptorAddressInfo = DescriptorAddressInfos[UBRemappingInfo.Remapping.NewBindingIndex];
		check(DescriptorAddressInfo.sType == 0);

		VkWriteDescriptorSet& WriteDescriptorSet = DSWriter[UBRemappingInfo.Remapping.NewDescriptorSet].WriteDescriptors[UBRemappingInfo.Remapping.NewBindingIndex];
		check(WriteDescriptorSet.dstBinding == UBRemappingInfo.Remapping.NewBindingIndex);
		check(WriteDescriptorSet.dstArrayElement == 0);
		check(WriteDescriptorSet.descriptorCount == 1);
		check(WriteDescriptorSet.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		checkSlow(WriteDescriptorSet.pBufferInfo);

		VkBufferDeviceAddressInfo BufferInfo;
		ZeroVulkanStruct(BufferInfo, VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
		BufferInfo.buffer = WriteDescriptorSet.pBufferInfo->buffer;
		VkDeviceAddress BufferAddress = VulkanRHI::vkGetBufferDeviceAddressKHR(Device->GetInstanceHandle(), &BufferInfo);

		DescriptorAddressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
		DescriptorAddressInfo.address = BufferAddress + WriteDescriptorSet.pBufferInfo->offset;
		DescriptorAddressInfo.range = WriteDescriptorSet.pBufferInfo->range;
	}

	// Send to descriptor manager
	Device->GetBindlessDescriptorManager()->RegisterUniformBuffers(CmdBuffer->GetHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, StageUBs);
}

FVulkanGraphicsPipelineDescriptorState::FVulkanGraphicsPipelineDescriptorState(FVulkanDevice* InDevice, FVulkanRHIGraphicsPipelineState* InGfxPipeline)
	: FVulkanCommonPipelineDescriptorState(InDevice)
	, GfxPipeline(InGfxPipeline)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanShaders);
	FMemory::Memzero(PackedUniformBuffersMask);
	FMemory::Memzero(PackedUniformBuffersDirty);

	check(InGfxPipeline);
	{
		
		check(InGfxPipeline->Layout);
		DescriptorSetsLayout = &InGfxPipeline->Layout->GetDescriptorSetsLayout();
		FVulkanGfxLayout& GfxLayout  = *(FVulkanGfxLayout*)InGfxPipeline->Layout;
		PipelineDescriptorInfo = &GfxLayout.GetGfxPipelineDescriptorInfo();

		UsedSetsMask = PipelineDescriptorInfo->HasDescriptorsInSetMask;
		const FVulkanShaderFactory& ShaderFactory = Device->GetShaderFactory();

		const FVulkanVertexShader* VertexShader = ShaderFactory.LookupShader<FVulkanVertexShader>(InGfxPipeline->GetShaderKey(SF_Vertex));
		check(VertexShader);
		PackedUniformBuffers[ShaderStage::Vertex].Init(VertexShader->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Vertex]);

		uint64 PixelShaderKey = InGfxPipeline->GetShaderKey(SF_Pixel);
		if (PixelShaderKey)
		{
			const FVulkanPixelShader* PixelShader = ShaderFactory.LookupShader<FVulkanPixelShader>(PixelShaderKey);
			check(PixelShader);

			PackedUniformBuffers[ShaderStage::Pixel].Init(PixelShader->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Pixel]);
		}

#if VULKAN_SUPPORTS_GEOMETRY_SHADERS
		uint64 GeometryShaderKey = InGfxPipeline->GetShaderKey(SF_Geometry);
		if (GeometryShaderKey)
		{
			const FVulkanGeometryShader* GeometryShader = ShaderFactory.LookupShader<FVulkanGeometryShader>(GeometryShaderKey);
			check(GeometryShader);

			PackedUniformBuffers[ShaderStage::Geometry].Init(GeometryShader->GetCodeHeader(), PackedUniformBuffersMask[ShaderStage::Geometry]);
		}
#endif

		CreateDescriptorWriteInfos();

		//UE_LOG(LogVulkanRHI, Warning, TEXT("GfxPSOState %p For PSO %p Writes:%d"), this, InGfxPipeline, DSWriteContainer.DescriptorWrites.Num());

		InGfxPipeline->AddRef();
	}
}

template<bool bUseDynamicGlobalUBs>
bool FVulkanGraphicsPipelineDescriptorState::InternalUpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateDescriptorSets);
#endif

	// Early exit
	if (!UsedSetsMask)
	{
		return false;
	}

	FVulkanUniformBufferUploader* UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8* CPURingBufferBase = (uint8*)UniformBufferUploader->GetCPUMappedPointer();
	const VkDeviceSize UBOffsetAlignment = Device->GetLimits().minUniformBufferOffsetAlignment;

	const FDescriptorSetRemappingInfo* RESTRICT RemappingInfo = PipelineDescriptorInfo->RemappingInfo;

	// Process updates
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanApplyPackedUniformBuffers);
#endif
		for (int32 Stage = 0; Stage < ShaderStage::NumStages; ++Stage)
		{
			if (PackedUniformBuffersDirty[Stage] != 0)
			{
				const uint32 DescriptorSet = RemappingInfo->StageInfos[Stage].PackedUBDescriptorSet;
				MarkDirty(UpdatePackedUniformBuffers<bUseDynamicGlobalUBs>(UBOffsetAlignment, RemappingInfo->StageInfos[Stage].PackedUBBindingIndices.GetData(), PackedUniformBuffers[Stage], DSWriter[DescriptorSet], UniformBufferUploader, CPURingBufferBase, PackedUniformBuffersDirty[Stage], CmdBuffer));
				PackedUniformBuffersDirty[Stage] = 0;
			}
		}
	}

	if (UseVulkanDescriptorCache() && !HasVolatileResources())
	{
		if (bIsResourcesDirty)
		{
			Device->GetDescriptorSetCache().GetDescriptorSets(GetDSetsKey(), *DescriptorSetsLayout, DSWriter, DescriptorSetHandles.GetData());
			bIsResourcesDirty = false;
		}
	}
	else
	{
		const bool bNeedsWrite = (bIsResourcesDirty || ShouldAlwaysWriteDescriptors());

		// Allocate sets based on what changed
		if (CmdBuffer->AcquirePoolSetAndDescriptorsIfNeeded(*DescriptorSetsLayout, bNeedsWrite, DescriptorSetHandles.GetData()))
		{
			uint32 RemainingSetsMask = UsedSetsMask;
			uint32 Set = 0;
			uint32 NumSets = 0;
			while (RemainingSetsMask)
			{
				if (RemainingSetsMask & 1)
				{
					const VkDescriptorSet DescriptorSet = DescriptorSetHandles[Set];
					DSWriter[Set].SetDescriptorSet(DescriptorSet);
#if VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
					DSWriter[Set].CheckAllWritten();
#endif
					++NumSets;
				}

				++Set;
				RemainingSetsMask >>= 1;
			}

	#if VULKAN_ENABLE_AGGRESSIVE_STATS
			INC_DWORD_STAT_BY(STAT_VulkanNumUpdateDescriptors, DSWriteContainer.DescriptorWrites.Num());
			INC_DWORD_STAT_BY(STAT_VulkanNumDescSets, NumSets);
			SCOPE_CYCLE_COUNTER(STAT_VulkanVkUpdateDS);
	#endif
			VulkanRHI::vkUpdateDescriptorSets(Device->GetInstanceHandle(), DSWriteContainer.DescriptorWrites.Num(), DSWriteContainer.DescriptorWrites.GetData(), 0, nullptr);

			bIsResourcesDirty = false;
		}
	}

	return true;
}

void FVulkanGraphicsPipelineDescriptorState::UpdateBindlessDescriptors(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
{
	check(bUseBindless);

	// We should only have uniform buffers at this point
	check(DSWriteContainer.DescriptorBufferInfo.Num() == DSWriteContainer.DescriptorWrites.Num());
	check(DSWriteContainer.DescriptorImageInfo.Num() == 0);

	FVulkanUniformBufferUploader* UniformBufferUploader = CmdListContext->GetUniformBufferUploader();
	uint8* CPURingBufferBase = (uint8*)UniformBufferUploader->GetCPUMappedPointer();
	const VkDeviceSize UBOffsetAlignment = Device->GetLimits().minUniformBufferOffsetAlignment;

	const FDescriptorSetRemappingInfo* RESTRICT RemappingInfo = PipelineDescriptorInfo->RemappingInfo;

	FVulkanBindlessDescriptorManager::FUniformBufferDescriptorArrays StageUBs;

	// Process updates
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanApplyPackedUniformBuffers);
#endif
		for (int32 Stage = 0; Stage < ShaderStage::NumStages; ++Stage)
		{
			const FDescriptorSetRemappingInfo::FStageInfo& StageInfo = RemappingInfo->StageInfos[Stage];

			TArray<VkDescriptorAddressInfoEXT>& DescriptorAddressInfos = StageUBs[Stage];
			DescriptorAddressInfos.SetNumZeroed(StageInfo.PackedUBBindingIndices.Num() + StageInfo.UniformBuffers.Num());

			// PackedUniformBuffersDirty ?
			check((PackedUniformBuffersMask[Stage] == 0) || (PackedUniformBuffersMask[Stage] == 1));
			if (PackedUniformBuffersMask[Stage] != 0)
			{
				check((StageInfo.PackedUBDescriptorSet != UINT16_MAX) && (StageInfo.PackedUBDescriptorSet <= (uint16)Stage));
				uint64 RemainingPackedUniformsMask = PackedUniformBuffersMask[Stage];
				int32 PackedUBIndex = 0;
				while (RemainingPackedUniformsMask)
				{
					if (RemainingPackedUniformsMask & 1)
					{
						const FPackedUniformBuffers::FPackedBuffer& StagedUniformBuffer = PackedUniformBuffers[Stage].GetBuffer(PackedUBIndex);
						const int32 UBSize = StagedUniformBuffer.Num();
						const int32 BindingIndex = StageInfo.PackedUBBindingIndices[PackedUBIndex];

						const uint64 RingBufferOffset = UniformBufferUploader->AllocateMemory(UBSize, UBOffsetAlignment, CmdBuffer);

						// Make sure it wasn't written to already
						VkDescriptorAddressInfoEXT& DescriptorAddressInfo = DescriptorAddressInfos[BindingIndex];
						check(DescriptorAddressInfo.sType == 0);
						DescriptorAddressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
						DescriptorAddressInfo.address = UniformBufferUploader->GetCPUBufferAddress() + RingBufferOffset;
						DescriptorAddressInfo.range = UBSize;

						// get location in the ring buffer to use
						FMemory::Memcpy(CPURingBufferBase + RingBufferOffset, StagedUniformBuffer.GetData(), UBSize);
					}
					RemainingPackedUniformsMask = RemainingPackedUniformsMask >> 1;
					++PackedUBIndex;
				}

				PackedUniformBuffersDirty[Stage] = 0;
			}

			for (int32 UBIndex = 0; UBIndex < StageInfo.UniformBuffers.Num(); ++UBIndex)
			{
				const FDescriptorSetRemappingInfo::FUBRemappingInfo& UBRemappingInfo = StageInfo.UniformBuffers[UBIndex];
				check((StageInfo.PackedUBDescriptorSet == UINT16_MAX) || UBRemappingInfo.Remapping.NewDescriptorSet == StageInfo.PackedUBDescriptorSet);
				check(UBRemappingInfo.bHasConstantData);

				VkDescriptorAddressInfoEXT& DescriptorAddressInfo = DescriptorAddressInfos[UBRemappingInfo.Remapping.NewBindingIndex];
				check(DescriptorAddressInfo.sType == 0);

				VkWriteDescriptorSet& WriteDescriptorSet = DSWriter[UBRemappingInfo.Remapping.NewDescriptorSet].WriteDescriptors[UBRemappingInfo.Remapping.NewBindingIndex];
				check(WriteDescriptorSet.dstBinding == UBRemappingInfo.Remapping.NewBindingIndex);
				check(WriteDescriptorSet.dstArrayElement == 0);
				check(WriteDescriptorSet.descriptorCount == 1);
				check(WriteDescriptorSet.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
				checkSlow(WriteDescriptorSet.pBufferInfo);

				VkBufferDeviceAddressInfo BufferInfo;
				ZeroVulkanStruct(BufferInfo, VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
				BufferInfo.buffer = WriteDescriptorSet.pBufferInfo->buffer;
				VkDeviceAddress BufferAddress = VulkanRHI::vkGetBufferDeviceAddressKHR(Device->GetInstanceHandle(), &BufferInfo);

				DescriptorAddressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
				DescriptorAddressInfo.address = BufferAddress + WriteDescriptorSet.pBufferInfo->offset;
				DescriptorAddressInfo.range = WriteDescriptorSet.pBufferInfo->range;
			}
		}
	}

	// Send to descriptor manager
	Device->GetBindlessDescriptorManager()->RegisterUniformBuffers(CmdBuffer->GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, StageUBs);
}


template <typename TRHIShader>
void FVulkanCommandListContext::ApplyStaticUniformBuffers(TRHIShader* Shader)
{
	if (Shader)
	{
		const auto& StaticSlots = Shader->StaticSlots;
		const auto& UBInfos = Shader->GetCodeHeader().UniformBuffers;

		for (int32 BufferIndex = 0; BufferIndex < StaticSlots.Num(); ++BufferIndex)
		{
			const FUniformBufferStaticSlot Slot = StaticSlots[BufferIndex];

			if (IsUniformBufferStaticSlotValid(Slot))
			{
				FRHIUniformBuffer* Buffer = GlobalUniformBuffers[Slot];
				UE::RHICore::ValidateStaticUniformBuffer(Buffer, Slot, UBInfos[BufferIndex].LayoutHash);

				if (Buffer)
				{
					RHISetShaderUniformBuffer(Shader, BufferIndex, Buffer);
				}
			}
		}
	}
}

void FVulkanCommandListContext::RHISetGraphicsPipelineState(FRHIGraphicsPipelineState* GraphicsState, uint32 StencilRef, bool bApplyAdditionalState)
{
	FVulkanRHIGraphicsPipelineState* Pipeline = ResourceCast(GraphicsState);
	
	FVulkanPipelineStateCacheManager* PipelineStateCache = Device->GetPipelineStateCache();
	PipelineStateCache->LRUTouch(Pipeline);

	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	bool bForceResetPipeline = !CmdBuffer->bHasPipeline;

	if (PendingGfxState->SetGfxPipeline(Pipeline, bForceResetPipeline))
	{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
		SCOPE_CYCLE_COUNTER(STAT_VulkanPipelineBind);
#endif
		PendingGfxState->Bind(CmdBuffer->GetHandle());
		CmdBuffer->bHasPipeline = true;
		PendingGfxState->MarkNeedsDynamicStates();
	}

	PendingGfxState->SetStencilRef(StencilRef);

	if (bApplyAdditionalState)
	{
		ApplyStaticUniformBuffers(static_cast<FVulkanVertexShader*>(Pipeline->VulkanShaders[ShaderStage::Vertex]));
#if PLATFORM_SUPPORTS_GEOMETRY_SHADERS
		ApplyStaticUniformBuffers(static_cast<FVulkanGeometryShader*>(Pipeline->VulkanShaders[ShaderStage::Geometry]));
#endif
		ApplyStaticUniformBuffers(static_cast<FVulkanPixelShader*>(Pipeline->VulkanShaders[ShaderStage::Pixel]));
	}
}

void FVulkanCommandListContext::RHISetComputePipelineState(FRHIComputePipelineState* ComputePipelineState)
{
	FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
	if (CmdBuffer->IsInsideRenderPass())
	{
		if (GVulkanSubmitAfterEveryEndRenderPass)
		{
			CommandBufferManager->SubmitActiveCmdBuffer();
			CommandBufferManager->PrepareForNewActiveCommandBuffer();
			CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		}
	}

	if (CmdBuffer->CurrentDescriptorPoolSetContainer == nullptr)
	{
		CmdBuffer->CurrentDescriptorPoolSetContainer = &Device->GetDescriptorPoolsManager().AcquirePoolSetContainer();
	}

	//#todo-rco: Set PendingGfx to null
	FVulkanComputePipeline* ComputePipeline = ResourceCast(ComputePipelineState);
	PendingComputeState->SetComputePipeline(ComputePipeline);

	ApplyStaticUniformBuffers(const_cast<FVulkanComputeShader*>(ComputePipeline->GetShader()));
}


template bool FVulkanGraphicsPipelineDescriptorState::InternalUpdateDescriptorSets<true>(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);
template bool FVulkanGraphicsPipelineDescriptorState::InternalUpdateDescriptorSets<false>(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);
template bool FVulkanComputePipelineDescriptorState::InternalUpdateDescriptorSets<true>(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);
template bool FVulkanComputePipelineDescriptorState::InternalUpdateDescriptorSets<false>(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);


void FVulkanDescriptorSetWriter::CheckAllWritten()
{
#if VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
	auto GetVkDescriptorTypeString = [](VkDescriptorType Type)
	{
		switch (Type)
		{
			// + 19 to skip "VK_DESCRIPTOR_TYPE_"
	#define VKSWITCHCASE(x)	case x: return FString(&TEXT(#x)[19]);
			VKSWITCHCASE(VK_DESCRIPTOR_TYPE_SAMPLER)
				VKSWITCHCASE(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
				VKSWITCHCASE(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
				VKSWITCHCASE(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				VKSWITCHCASE(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
				VKSWITCHCASE(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
				VKSWITCHCASE(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
				VKSWITCHCASE(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
				VKSWITCHCASE(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
				VKSWITCHCASE(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
				VKSWITCHCASE(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT)
	#undef VKSWITCHCASE
		default:
			break;
		}

		return FString::Printf(TEXT("Unknown VkDescriptorType %d"), (int32)Type);
	};

	const uint32 Writes = NumWrites;
	if (Writes == 0)
		return;

	bool bFail = false;
	if(Writes <= 32) //early out for the most common case.
	{
		bFail = WrittenMask[0] != ((1llu << Writes)-1);
	}
	else
	{
		const int32 Last = int32(WrittenMask.Num()-1);
		for(int32 i = 0; !bFail && i < Last; ++i)
		{
			uint64 Mask = WrittenMask[i];
			bFail = bFail || Mask != 0xffffffff; 
		}

		const uint32 TailCount = Writes - (Last * 32);
		check(TailCount != 0);
		const uint32 TailMask = (1llu << TailCount)-1;
		bFail = bFail || TailMask != WrittenMask[Last];
	}

	if(bFail)
	{
		FString Descriptors;
		for (uint32 i = 0; i < Writes; ++i)
		{
			uint32 Index = i / 32;
			uint32 Mask = i % 32;
			if(0 == (WrittenMask[Index] & (1llu << Mask)))
			{
				FString TypeString = GetVkDescriptorTypeString(WriteDescriptors[i].descriptorType);
				Descriptors += FString::Printf(TEXT("\t\tDescriptorWrite %d/%d Was not written(Type %s)\n"), i, NumWrites, *TypeString);
			}
		}
		UE_LOG(LogVulkanRHI, Warning, TEXT("Not All descriptors where filled out. this can/will cause a driver crash\n%s\n"), *Descriptors);
		ensureMsgf(false, TEXT("Not All descriptors where filled out. this can/will cause a driver crash\n%s\n"), *Descriptors);
	}
#endif
}

void FVulkanDescriptorSetWriter::Reset()
{
	bHasVolatileResources = false;

#if VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
	WrittenMask = BaseWrittenMask;
#endif
}
void FVulkanDescriptorSetWriter::SetWritten(uint32 DescriptorIndex)
{
#if VULKAN_VALIDATE_DESCRIPTORS_WRITTEN
	uint32 Index = DescriptorIndex / 32;
	uint32 Mask = DescriptorIndex % 32;
	WrittenMask[Index] |= (1<<Mask);
#endif
}
void FVulkanDescriptorSetWriter::SetWrittenBase(uint32 DescriptorIndex)
{
#if VULKAN_VALIDATE_DESCRIPTORS_WRITTEN	
	uint32 Index = DescriptorIndex / 32;
	uint32 Mask = DescriptorIndex % 32;
	BaseWrittenMask[Index] |= (1<<Mask);
#endif
}

void FVulkanDescriptorSetWriter::InitWrittenMasks(uint32 NumDescriptorWrites)
{
#if VULKAN_VALIDATE_DESCRIPTORS_WRITTEN	
	uint32 Size = (NumDescriptorWrites + 31) / 32;
	WrittenMask.Empty(Size);
	WrittenMask.SetNumZeroed(Size);
	BaseWrittenMask.Empty(Size);
	BaseWrittenMask.SetNumZeroed(Size);
#endif
}

void FVulkanCommonPipelineDescriptorState::SetSRV(FVulkanCmdBuffer* CmdBuffer, bool bCompute, uint8 DescriptorSet, uint32 BindingIndex, FVulkanShaderResourceView* SRV)
{
	check(!bUseBindless);

	ERHIAccess Access = bCompute
		? ERHIAccess::SRVCompute
		: ERHIAccess::SRVGraphics;

	switch (SRV->GetViewType())
	{
	case FVulkanView::EType::Null:
		checkf(false, TEXT("Attempt to bind a null SRV."));
		break;
		
	case FVulkanView::EType::TypedBuffer:
		MarkDirty(DSWriter[DescriptorSet].WriteUniformTexelBuffer(BindingIndex, SRV->GetTypedBufferView()));
		break;

	case FVulkanView::EType::Texture:
		{
			const FVulkanTexture* VulkanTexture = ResourceCast(SRV->GetTexture());
			const VkImageLayout Layout = FVulkanLayoutManager::GetDefaultLayout(CmdBuffer, *VulkanTexture, Access);
			MarkDirty(DSWriter[DescriptorSet].WriteImage(BindingIndex, SRV->GetTextureView(), Layout));
		}
		break;

	case FVulkanView::EType::StructuredBuffer:
		check((ResourceCast(SRV->GetBuffer())->GetBufferUsageFlags() & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) == VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		MarkDirty(DSWriter[DescriptorSet].WriteStorageBuffer(BindingIndex, SRV->GetStructuredBufferView()));
		break;

#if VULKAN_RHI_RAYTRACING
	case FVulkanView::EType::AccelerationStructure:
		MarkDirty(DSWriter[DescriptorSet].WriteAccelerationStructure(BindingIndex, SRV->GetAccelerationStructureView().Handle));
		break;
#endif
	}
}

void FVulkanCommonPipelineDescriptorState::SetUAV(FVulkanCmdBuffer* CmdBuffer, bool bCompute, uint8 DescriptorSet, uint32 BindingIndex, FVulkanUnorderedAccessView* UAV)
{
	check(!bUseBindless);

	ERHIAccess Access = bCompute
		? ERHIAccess::UAVCompute
		: ERHIAccess::UAVGraphics;

	switch (UAV->GetViewType())
	{
	case FVulkanView::EType::Null:
		checkf(false, TEXT("Attempt to bind a null UAV."));
		break;

	case FVulkanView::EType::TypedBuffer:
		MarkDirty(DSWriter[DescriptorSet].WriteStorageTexelBuffer(BindingIndex, UAV->GetTypedBufferView()));
		break;

	case FVulkanView::EType::Texture:
		{
			const FVulkanTexture* VulkanTexture = ResourceCast(UAV->GetTexture());
			const VkImageLayout ExpectedLayout = FVulkanLayoutManager::GetDefaultLayout(CmdBuffer, *VulkanTexture, Access);
			MarkDirty(DSWriter[DescriptorSet].WriteStorageImage(BindingIndex, UAV->GetTextureView(), ExpectedLayout));
		}
		break;


	case FVulkanView::EType::StructuredBuffer:
		check((ResourceCast(UAV->GetBuffer())->GetBufferUsageFlags() & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) == VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		MarkDirty(DSWriter[DescriptorSet].WriteStorageBuffer(BindingIndex, UAV->GetStructuredBufferView()));
		break;

#if VULKAN_RHI_RAYTRACING
	case FVulkanView::EType::AccelerationStructure:
		MarkDirty(DSWriter[DescriptorSet].WriteAccelerationStructure(BindingIndex, UAV->GetAccelerationStructureView().Handle));
		break;
#endif
	}
}
