// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanPipelineState.h: Vulkan pipeline state definitions.
=============================================================================*/

#pragma once

#include "VulkanConfiguration.h"
#include "VulkanMemory.h"
#include "VulkanCommandBuffer.h"
#include "VulkanDescriptorSets.h"
#include "VulkanPipeline.h"
#include "VulkanRHIPrivate.h"
#include "Containers/ArrayView.h"

class FVulkanComputePipeline;
extern TAutoConsoleVariable<int32> GDynamicGlobalUBs;


// Common Pipeline state
class FVulkanCommonPipelineDescriptorState : public VulkanRHI::FDeviceChild
{
public:
	FVulkanCommonPipelineDescriptorState(FVulkanDevice* InDevice)
		: VulkanRHI::FDeviceChild(InDevice)
		, bUseBindless(InDevice->SupportsBindless())
	{
	}

	virtual ~FVulkanCommonPipelineDescriptorState() {}

	const FVulkanDSetsKey& GetDSetsKey() const
	{
		check(UseVulkanDescriptorCache());
		if (bIsDSetsKeyDirty)
		{
			DSetsKey.GenerateFromData(DSWriteContainer.HashableDescriptorInfo.GetData(),
				sizeof(FVulkanHashableDescriptorInfo) * DSWriteContainer.HashableDescriptorInfo.Num());
			bIsDSetsKeyDirty = false;
		}
		return DSetsKey;
	}

	bool HasVolatileResources() const
	{
		for (const FVulkanDescriptorSetWriter& Writer : DSWriter)
		{
			if (Writer.bHasVolatileResources)
			{
				return true;
			}
		}
		return false;
	}

	inline void MarkDirty(bool bDirty)
	{
		bIsResourcesDirty |= bDirty;
		bIsDSetsKeyDirty |= bDirty;
	}

	void SetSRV(FVulkanCmdBuffer* CmdBuffer, bool bCompute, uint8 DescriptorSet, uint32 BindingIndex, FVulkanShaderResourceView* SRV);
	void SetUAV(FVulkanCmdBuffer* CmdBuffer, bool bCompute, uint8 DescriptorSet, uint32 BindingIndex, FVulkanUnorderedAccessView* UAV);

	inline void SetTexture(uint8 DescriptorSet, uint32 BindingIndex, const FVulkanTexture* Texture, VkImageLayout Layout)
	{
		check(!bUseBindless);
		check(Texture && Texture->PartialView);

		// If the texture doesn't support sampling, then we read it through a UAV
		if (Texture->SupportsSampling())
		{
			MarkDirty(DSWriter[DescriptorSet].WriteImage(BindingIndex, Texture->PartialView->GetTextureView(), Layout));
		}
		else
		{
			MarkDirty(DSWriter[DescriptorSet].WriteStorageImage(BindingIndex, Texture->PartialView->GetTextureView(), Layout));
		}
	}

	inline void SetSamplerState(uint8 DescriptorSet, uint32 BindingIndex, const FVulkanSamplerState* Sampler)
	{
		check(!bUseBindless);
		check(Sampler && Sampler->Sampler != VK_NULL_HANDLE);
		MarkDirty(DSWriter[DescriptorSet].WriteSampler(BindingIndex, *Sampler));
	}

	inline void SetInputAttachment(uint8 DescriptorSet, uint32 BindingIndex, const FVulkanView::FTextureView& TextureView, VkImageLayout Layout)
	{
		check(!bUseBindless);
		MarkDirty(DSWriter[DescriptorSet].WriteInputAttachment(BindingIndex, TextureView, Layout));
	}

	template<bool bDynamic>
	inline void SetUniformBuffer(uint8 DescriptorSet, uint32 BindingIndex, const FVulkanUniformBuffer* UniformBuffer)
	{
		const VulkanRHI::FVulkanAllocation& Allocation = UniformBuffer->Allocation;
		VkDeviceSize Range = UniformBuffer->bUniformView ? PLATFORM_MAX_UNIFORM_BUFFER_RANGE : UniformBuffer->GetSize();

		if (bDynamic)
		{
			MarkDirty(DSWriter[DescriptorSet].WriteDynamicUniformBuffer(BindingIndex, Allocation.GetBufferHandle(), Allocation.HandleId, 0, Range, UniformBuffer->GetOffset()));
		}
		else
		{
			MarkDirty(DSWriter[DescriptorSet].WriteUniformBuffer(BindingIndex, Allocation.GetBufferHandle(), Allocation.HandleId, UniformBuffer->GetOffset(), Range));
		}
	}

	inline void SetUniformBufferDynamicOffset(uint8 DescriptorSet, uint32 BindingIndex, uint32 DynamicOffset)
	{
		const uint8 DynamicOffsetIndex = DSWriter[DescriptorSet].BindingToDynamicOffsetMap[BindingIndex];
		DSWriter[DescriptorSet].DynamicOffsets[DynamicOffsetIndex] = DynamicOffset;
	}

protected:
	void Reset()
	{
		for(FVulkanDescriptorSetWriter& Writer : DSWriter)
		{
			Writer.Reset();
		}
	}
	inline void Bind(VkCommandBuffer CmdBuffer, VkPipelineLayout PipelineLayout, VkPipelineBindPoint BindPoint)
	{
		// Bindless will replace with global sets
		if (!bUseBindless)
		{
			VulkanRHI::vkCmdBindDescriptorSets(CmdBuffer,
				BindPoint,
				PipelineLayout,
				0, DescriptorSetHandles.Num(), DescriptorSetHandles.GetData(),
				(uint32)DynamicOffsets.Num(), DynamicOffsets.GetData());
		}
	}

	void CreateDescriptorWriteInfos();

	//#todo-rco: Won't work multithreaded!
	FVulkanDescriptorSetWriteContainer DSWriteContainer;
	const FVulkanDescriptorSetsLayout* DescriptorSetsLayout = nullptr;

	//#todo-rco: Won't work multithreaded!
	TArray<VkDescriptorSet> DescriptorSetHandles;

	// Bitmask of sets that exist in this pipeline
	//#todo-rco: Won't work multithreaded!
	uint32			UsedSetsMask = 0;

	//#todo-rco: Won't work multithreaded!
	TArray<uint32> DynamicOffsets;

	bool bIsResourcesDirty = true;

	TArray<FVulkanDescriptorSetWriter> DSWriter;
	
	mutable FVulkanDSetsKey DSetsKey;
	mutable bool bIsDSetsKeyDirty = true;

	const bool bUseBindless;
};


class FVulkanComputePipelineDescriptorState : public FVulkanCommonPipelineDescriptorState
{
public:
	FVulkanComputePipelineDescriptorState(FVulkanDevice* InDevice, FVulkanComputePipeline* InComputePipeline);
	virtual ~FVulkanComputePipelineDescriptorState()
	{
		ComputePipeline->Release();
	}

	void Reset()
	{
		FVulkanCommonPipelineDescriptorState::Reset();
		PackedUniformBuffersDirty = PackedUniformBuffersMask;
	}

	inline void SetPackedGlobalShaderParameter(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
	{
		PackedUniformBuffers.SetPackedGlobalParameter(BufferIndex, ByteOffset, NumBytes, NewValue, PackedUniformBuffersDirty);
	}

	inline void SetUniformBufferConstantData(uint32 BindingIndex, const TArray<uint8>& ConstantData)
	{
		PackedUniformBuffers.SetEmulatedUniformBufferIntoPacked(BindingIndex, ConstantData, PackedUniformBuffersDirty);
	}

	bool UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
	{
		check(!bUseBindless);

		const bool bUseDynamicGlobalUBs = (GDynamicGlobalUBs->GetInt() > 0);
		if (bUseDynamicGlobalUBs)
		{
			return InternalUpdateDescriptorSets<true>(CmdListContext, CmdBuffer);
		}
		else
		{
			return InternalUpdateDescriptorSets<false>(CmdListContext, CmdBuffer);
		}
	}

	void UpdateBindlessDescriptors(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);

	inline void BindDescriptorSets(VkCommandBuffer CmdBuffer)
	{
		Bind(CmdBuffer, ComputePipeline->GetLayout().GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
	}

	inline const FVulkanComputePipelineDescriptorInfo& GetComputePipelineDescriptorInfo() const
	{
		return *PipelineDescriptorInfo;
		//return GfxPipeline->Pipeline->GetGfxLayout().GetGfxPipelineDescriptorInfo();
	}

protected:
	const FVulkanComputePipelineDescriptorInfo* PipelineDescriptorInfo;

	FPackedUniformBuffers PackedUniformBuffers;
	uint64 PackedUniformBuffersMask;
	uint64 PackedUniformBuffersDirty;

	FVulkanComputePipeline* ComputePipeline;

	template<bool bUseDynamicGlobalUBs>
	bool InternalUpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);

	friend class FVulkanPendingComputeState;
	friend class FVulkanCommandListContext;
};

class FVulkanGraphicsPipelineDescriptorState : public FVulkanCommonPipelineDescriptorState
{
public:
	FVulkanGraphicsPipelineDescriptorState(FVulkanDevice* InDevice, FVulkanRHIGraphicsPipelineState* InGfxPipeline);
	virtual ~FVulkanGraphicsPipelineDescriptorState()
	{
		GfxPipeline->Release();
	}

	inline void SetPackedGlobalShaderParameter(uint8 Stage, uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValue)
	{
		PackedUniformBuffers[Stage].SetPackedGlobalParameter(BufferIndex, ByteOffset, NumBytes, NewValue, PackedUniformBuffersDirty[Stage]);
	}

	inline void SetUniformBufferConstantData(uint8 Stage, uint32 BindingIndex, const TArray<uint8>& ConstantData)
	{
		PackedUniformBuffers[Stage].SetEmulatedUniformBufferIntoPacked(BindingIndex, ConstantData, PackedUniformBuffersDirty[Stage]);
	}

	bool UpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer)
	{
		check(!bUseBindless);

		const bool bUseDynamicGlobalUBs = (GDynamicGlobalUBs->GetInt() > 0);
		if (bUseDynamicGlobalUBs)
		{
			return InternalUpdateDescriptorSets<true>(CmdListContext, CmdBuffer);
		}
		else
		{
			return InternalUpdateDescriptorSets<false>(CmdListContext, CmdBuffer);
		}
	}

	void UpdateBindlessDescriptors(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);

	inline void BindDescriptorSets(VkCommandBuffer CmdBuffer)
	{
		Bind(CmdBuffer, GfxPipeline->GetLayout().GetPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS);
	}

	void Reset()
	{
		FMemory::Memcpy(PackedUniformBuffersDirty, PackedUniformBuffersMask);
		FVulkanCommonPipelineDescriptorState::Reset();
		bIsResourcesDirty = true;
	}

	inline const FVulkanGfxPipelineDescriptorInfo& GetGfxPipelineDescriptorInfo() const
	{
		return *PipelineDescriptorInfo;
	}

protected:
	const FVulkanGfxPipelineDescriptorInfo* PipelineDescriptorInfo;

	TStaticArray<FPackedUniformBuffers, ShaderStage::NumStages> PackedUniformBuffers;
	TStaticArray<uint64, ShaderStage::NumStages> PackedUniformBuffersMask;
	TStaticArray<uint64, ShaderStage::NumStages> PackedUniformBuffersDirty;

	FVulkanRHIGraphicsPipelineState* GfxPipeline;

	template<bool bUseDynamicGlobalUBs>
	bool InternalUpdateDescriptorSets(FVulkanCommandListContext* CmdListContext, FVulkanCmdBuffer* CmdBuffer);

	friend class FVulkanPendingGfxState;
	friend class FVulkanCommandListContext;
};

template <bool bIsDynamic>
static inline bool UpdatePackedUniformBuffers(VkDeviceSize UBOffsetAlignment, const uint16* RESTRICT PackedUBBindingIndices, const FPackedUniformBuffers& PackedUniformBuffers,
	FVulkanDescriptorSetWriter& DescriptorWriteSet, FVulkanUniformBufferUploader* UniformBufferUploader, uint8* RESTRICT CPURingBufferBase, uint64 RemainingPackedUniformsMask,
	FVulkanCmdBuffer* InCmdBuffer)
{
	bool bAnyUBDirty = false;
	int32 PackedUBIndex = 0;
	while (RemainingPackedUniformsMask)
	{
		if (RemainingPackedUniformsMask & 1)
		{
			const FPackedUniformBuffers::FPackedBuffer& StagedUniformBuffer = PackedUniformBuffers.GetBuffer(PackedUBIndex);
			int32 BindingIndex = PackedUBBindingIndices[PackedUBIndex];

			const int32 UBSize = StagedUniformBuffer.Num();

			// get offset into the RingBufferBase pointer
			uint64 RingBufferOffset = UniformBufferUploader->AllocateMemory(UBSize, UBOffsetAlignment, InCmdBuffer);

			// get location in the ring buffer to use
			FMemory::Memcpy(CPURingBufferBase + RingBufferOffset, StagedUniformBuffer.GetData(), UBSize);

			const VulkanRHI::FVulkanAllocation& Allocation = UniformBufferUploader->GetCPUBufferAllocation();
			if (bIsDynamic)
			{
				const bool bDirty = DescriptorWriteSet.WriteDynamicUniformBuffer(BindingIndex, Allocation.GetBufferHandle(), Allocation.HandleId, UniformBufferUploader->GetCPUBufferOffset(), UBSize, RingBufferOffset);
				bAnyUBDirty = bAnyUBDirty || bDirty;

			}
			else
			{
				const bool bDirty = DescriptorWriteSet.WriteUniformBuffer(BindingIndex, Allocation.GetBufferHandle(), Allocation.HandleId, RingBufferOffset + UniformBufferUploader->GetCPUBufferOffset(), UBSize);
				bAnyUBDirty = bAnyUBDirty || bDirty;

			}
		}
		RemainingPackedUniformsMask = RemainingPackedUniformsMask >> 1;
		++PackedUBIndex;
	}

	return bAnyUBDirty;
}
