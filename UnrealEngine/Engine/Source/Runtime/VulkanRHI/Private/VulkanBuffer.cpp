// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanIndexBuffer.cpp: Vulkan Index buffer RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanDevice.h"
#include "VulkanContext.h"
#include "Containers/ResourceArray.h"
#include "VulkanLLM.h"
#include "VulkanRayTracing.h"
#include "VulkanTransientResourceAllocator.h"

static TMap<FVulkanResourceMultiBuffer*, VulkanRHI::FPendingBufferLock> GPendingLockIBs;
static FCriticalSection GPendingLockIBsMutex;

static FORCEINLINE VulkanRHI::FPendingBufferLock GetPendingBufferLock(FVulkanResourceMultiBuffer* Buffer)
{
	VulkanRHI::FPendingBufferLock PendingLock;

	// Found only if it was created for Write
	FScopeLock ScopeLock(&GPendingLockIBsMutex);
	const bool bFound = GPendingLockIBs.RemoveAndCopyValue(Buffer, PendingLock);

	checkf(bFound, TEXT("Mismatched Buffer Lock/Unlock!"));
	return PendingLock;
}

static FORCEINLINE void UpdateVulkanBufferStats(uint64_t Size, VkBufferUsageFlags Usage, bool Allocating)
{
	const bool bUniformBuffer = !!(Usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	const bool bIndexBuffer = !!(Usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	const bool bVertexBuffer = !!(Usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	const bool bAccelerationStructure = !!(Usage & VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);

	if (Allocating)
	{
		if (bUniformBuffer)
		{
			INC_MEMORY_STAT_BY(STAT_UniformBufferMemory, Size);
		}
		else if (bIndexBuffer)
		{
			INC_MEMORY_STAT_BY(STAT_IndexBufferMemory, Size);
		}
		else if (bVertexBuffer)
		{
			INC_MEMORY_STAT_BY(STAT_VertexBufferMemory, Size);
		}
		else if (bAccelerationStructure)
		{
			INC_MEMORY_STAT_BY(STAT_RTAccelerationStructureMemory, Size);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_StructuredBufferMemory, Size);
		}
	}
	else
	{
		if (bUniformBuffer)
		{
			DEC_MEMORY_STAT_BY(STAT_UniformBufferMemory, Size);
		}
		else if (bIndexBuffer)
		{
			DEC_MEMORY_STAT_BY(STAT_IndexBufferMemory, Size);
		}
		else if (bVertexBuffer)
		{
			DEC_MEMORY_STAT_BY(STAT_VertexBufferMemory, Size);
		}
		else if (bAccelerationStructure)
		{
			INC_MEMORY_STAT_BY(STAT_RTAccelerationStructureMemory, Size);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_StructuredBufferMemory, Size);
		}
	}
}


VkBufferUsageFlags FVulkanResourceMultiBuffer::UEToVKBufferUsageFlags(FVulkanDevice* InDevice, EBufferUsageFlags InUEUsage, bool bZeroSize)
{
	// Always include TRANSFER_SRC since hardware vendors confirmed it wouldn't have any performance cost and we need it for some debug functionalities.
	VkBufferUsageFlags OutVkUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	auto TranslateFlag = [&OutVkUsage, &InUEUsage](EBufferUsageFlags SearchUEFlag, VkBufferUsageFlags AddedIfFound, VkBufferUsageFlags AddedIfNotFound = 0)
	{
		const bool HasFlag = EnumHasAnyFlags(InUEUsage, SearchUEFlag);
		OutVkUsage |= HasFlag ? AddedIfFound : AddedIfNotFound;
	};

	TranslateFlag(BUF_VertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	TranslateFlag(BUF_IndexBuffer, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	TranslateFlag(BUF_StructuredBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

#if VULKAN_RHI_RAYTRACING
	TranslateFlag(BUF_AccelerationStructure, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
#endif

	if (!bZeroSize)
	{
		TranslateFlag(BUF_UnorderedAccess, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT);
		TranslateFlag(BUF_DrawIndirect, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
		TranslateFlag(BUF_KeepCPUAccessible, (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT));
		TranslateFlag(BUF_ShaderResource, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT);

		TranslateFlag(BUF_Volatile, 0, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

#if VULKAN_RHI_RAYTRACING
		if (InDevice->GetOptionalExtensions().HasRaytracingExtensions())
		{
			OutVkUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

			TranslateFlag(BUF_AccelerationStructure, 0, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
		}
#endif
	}

	return OutVkUsage;
}

FVulkanResourceMultiBuffer::FVulkanResourceMultiBuffer(FVulkanDevice* InDevice, uint32 InSize, EBufferUsageFlags InUEUsage, uint32 InStride, FRHIResourceCreateInfo& CreateInfo, FRHICommandListBase* InRHICmdList, const FRHITransientHeapAllocation* InTransientHeapAllocation)
	: FRHIBuffer(InSize, InUEUsage, InStride)
	, VulkanRHI::FDeviceChild(InDevice)
	, BufferUsageFlags(0)
	, NumBuffers(0)
	, DynamicBufferIndex(0)
	, LockStatus(ELockStatus::Unlocked)
{
	VULKAN_TRACK_OBJECT_CREATE(FVulkanResourceMultiBuffer, this);

	const bool bZeroSize = (InSize == 0);
	BufferUsageFlags = UEToVKBufferUsageFlags(InDevice, InUEUsage, bZeroSize);
	
	if (!bZeroSize)
	{
		check(InDevice);

		const bool bVolatile = EnumHasAnyFlags(InUEUsage, BUF_Volatile);
		if (bVolatile)
		{
			check(InRHICmdList);

			// Get a dummy buffer as sometimes the high-level misbehaves and tries to use SRVs off volatile buffers before filling them in...
			void* Data = Lock(*InRHICmdList, RLM_WriteOnly, InSize, 0);

			if (CreateInfo.ResourceArray)
			{
				uint32 CopyDataSize = FMath::Min(InSize, CreateInfo.ResourceArray->GetResourceDataSize());
				FMemory::Memcpy(Data, CreateInfo.ResourceArray->GetResourceData(), CopyDataSize);
			}
			else
			{
				FMemory::Memzero(Data, InSize);
			}

			Unlock(*InRHICmdList);
		}
		else
		{
			NumBuffers = GetNumBuffersFromUsage(InUEUsage);
			check(NumBuffers <= UE_ARRAY_COUNT(Buffers));

			const bool bUnifiedMem = InDevice->HasUnifiedMemory();
			const uint32 BufferAlignment = FMemoryManager::CalculateBufferAlignment(*InDevice, InUEUsage, bZeroSize);

			if (InTransientHeapAllocation != nullptr)
			{
				const uint32 AlignedSize = Align(InSize, BufferAlignment);

				Buffers[0] = FVulkanTransientHeap::GetVulkanAllocation(*InTransientHeapAllocation);
				Buffers[0].Size = InSize;
				check(Buffers[0].Offset % BufferAlignment == 0);
				for (uint32 Index = 1; Index < NumBuffers; ++Index)
				{
					Buffers[Index] = Buffers[Index - 1];
					Buffers[Index].Offset += AlignedSize;
					Buffers[Index].Size = InSize;
					check((Buffers[Index].Offset + InSize) <= InTransientHeapAllocation->Size);
				}
				
				VULKAN_SET_DEBUG_NAME((*InDevice), VK_OBJECT_TYPE_BUFFER, Buffers[0].VulkanHandle, TEXT("%s:(FVulkanResourceMultiBuffer*)0x%p:Buffer[1,2,3]"), CreateInfo.DebugName ? CreateInfo.DebugName : TEXT("?"), this);
			}
			else
			{
				VkMemoryPropertyFlags BufferMemFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
				if (bUnifiedMem)
				{
					BufferMemFlags |= (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				}

				for (uint32 Index = 0; Index < NumBuffers; ++Index)
				{
					if (!InDevice->GetMemoryManager().AllocateBufferPooled(Buffers[Index], nullptr, InSize, BufferAlignment, BufferUsageFlags, BufferMemFlags, EVulkanAllocationMetaMultiBuffer, __FILE__, __LINE__))
					{
						InDevice->GetMemoryManager().HandleOOM();
					}
					VULKAN_SET_DEBUG_NAME((*InDevice), VK_OBJECT_TYPE_BUFFER, Buffers[Index].VulkanHandle, TEXT("%s:(FVulkanResourceMultiBuffer*)0x%p:Buffer[%d]"), CreateInfo.DebugName ? CreateInfo.DebugName : TEXT("?"), this, Index);
				}
			}

			Current.Alloc.Reference(Buffers[DynamicBufferIndex]);
			Current.Handle = (VkBuffer)Current.Alloc.VulkanHandle;
			Current.Offset = Current.Alloc.Offset;
			Current.Size = InSize;

			if (CreateInfo.ResourceArray)
			{
				check(InRHICmdList);

				uint32 CopyDataSize = FMath::Min(InSize, CreateInfo.ResourceArray->GetResourceDataSize());
				// We know this buffer is not in use by GPU atm. If we do have a direct access initialize it without extra copies
				if (bUnifiedMem)
				{
					void* Data = (uint8*)Buffers[DynamicBufferIndex].GetMappedPointer(Device);
					FMemory::Memcpy(Data, CreateInfo.ResourceArray->GetResourceData(), CopyDataSize);
				}
				else
				{
					void* Data = Lock(*InRHICmdList, RLM_WriteOnly, CopyDataSize, 0);
					FMemory::Memcpy(Data, CreateInfo.ResourceArray->GetResourceData(), CopyDataSize);
					Unlock(*InRHICmdList);
				}

				CreateInfo.ResourceArray->Discard();
			}

			UpdateVulkanBufferStats(InSize * NumBuffers, BufferUsageFlags, true);
		}
	}
}

FVulkanResourceMultiBuffer::~FVulkanResourceMultiBuffer()
{
	VULKAN_TRACK_OBJECT_DELETE(FVulkanResourceMultiBuffer, this);
	uint64_t TotalSize = 0;
	for (uint32 Index = 0; Index < NumBuffers; ++Index)
	{
		TotalSize += Buffers[Index].Size;
		Device->GetMemoryManager().FreeVulkanAllocation(Buffers[Index]);
	}
	UpdateVulkanBufferStats(TotalSize, BufferUsageFlags, false);
}

void* FVulkanResourceMultiBuffer::Lock(FRHICommandListBase& RHICmdList, EResourceLockMode LockMode, uint32 LockSize, uint32 Offset)
{
	// Use the immediate context for write operations, since we are only accessing allocators.
	FVulkanCommandListContext& Context = FVulkanCommandListContext::GetVulkanContext(LockMode == RLM_WriteOnly ? *RHIGetDefaultContext() : RHICmdList.GetContext());

	return Lock(Context, LockMode, LockSize, Offset);
}

void* FVulkanResourceMultiBuffer::Lock(FVulkanCommandListContext& Context, EResourceLockMode LockMode, uint32 LockSize, uint32 Offset)
{
	void* Data = nullptr;
	uint32 DataOffset = 0;

	const bool bDynamic = EnumHasAnyFlags(GetUsage(), BUF_Dynamic);
	const bool bVolatile = EnumHasAnyFlags(GetUsage(), BUF_Volatile);
	const bool bStatic = EnumHasAnyFlags(GetUsage(), BUF_Static) || !(bVolatile || bDynamic);
	const bool bUAV = EnumHasAnyFlags(GetUsage(), BUF_UnorderedAccess);
	const bool bSR = EnumHasAnyFlags(GetUsage(), BUF_ShaderResource);

	LockStatus = ELockStatus::Locked;

	if (bVolatile)
	{
		check(NumBuffers == 0);
		if (LockMode == RLM_ReadOnly)
		{
			checkf(0, TEXT("Volatile buffers can't be locked for read."));
		}
		else
		{
			Context.GetTempFrameAllocationBuffer().Alloc(LockSize + Offset, 256, VolatileLockInfo);
			Data = VolatileLockInfo.Data;
			++VolatileLockInfo.LockCounter;
			check(!VolatileLockInfo.Allocation.HasAllocation());
			Current.Alloc = VolatileLockInfo.Allocation;
			Current.Handle = Current.Alloc.GetBufferHandle();
			Current.Offset = VolatileLockInfo.CurrentOffset + Current.Alloc.Offset;
			Current.Size = LockSize;
		}
	}
	else
	{
		check(bStatic || bDynamic || bUAV || bSR);

		if (LockMode == RLM_ReadOnly)
		{
			check(IsInRenderingThread() && Context.IsImmediate());

			const bool bUnifiedMem = Device->HasUnifiedMemory();
			if (bUnifiedMem)
			{
				Data = Buffers[DynamicBufferIndex].GetMappedPointer(Device);
				LockStatus = ELockStatus::PersistentMapping;
				DataOffset = Offset;
			}
			else 
			{
				Device->PrepareForCPURead();
				FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetUploadCmdBuffer();
				
				// Make sure any previous tasks have finished on the source buffer.
				VkMemoryBarrier BarrierBefore = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT };
				VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &BarrierBefore, 0, nullptr, 0, nullptr);

				// Create a staging buffer we can use to copy data from device to cpu.
				VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(LockSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

				// Fill the staging buffer with the data on the device.
				VkBufferCopy Regions;
				Regions.size = LockSize;
				Regions.srcOffset = Offset + Buffers[DynamicBufferIndex].Offset;
				Regions.dstOffset = 0;
				
				VulkanRHI::vkCmdCopyBuffer(CmdBuffer->GetHandle(), Buffers[DynamicBufferIndex].GetBufferHandle(), StagingBuffer->GetHandle(), 1, &Regions);

				// Setup barrier.
				VkMemoryBarrier BarrierAfter = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_HOST_READ_BIT };
				VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &BarrierAfter, 0, nullptr, 0, nullptr);
				
				// Force upload.
				Context.GetCommandBufferManager()->SubmitUploadCmdBuffer();
				Device->WaitUntilIdle();

				// Flush.
				StagingBuffer->FlushMappedMemory();

				// Get mapped pointer. 
				Data = StagingBuffer->GetMappedPointer();

				// Release temp staging buffer during unlock.
				VulkanRHI::FPendingBufferLock PendingLock;
				PendingLock.Offset = 0;
				PendingLock.Size = LockSize;
				PendingLock.LockMode = LockMode;
				PendingLock.StagingBuffer = StagingBuffer;

				{
					FScopeLock ScopeLock(&GPendingLockIBsMutex);
					check(!GPendingLockIBs.Contains(this));
					GPendingLockIBs.Add(this, PendingLock);
				}

				Context.GetCommandBufferManager()->PrepareForNewActiveCommandBuffer();
			}
		}
		else
		{
			check(LockMode == RLM_WriteOnly);
			DynamicBufferIndex = (DynamicBufferIndex + 1) % NumBuffers;
			Current.Alloc.Reference(Buffers[DynamicBufferIndex]);		
			Current.Handle = (VkBuffer)Current.Alloc.VulkanHandle;
			Current.Offset = Current.Alloc.Offset;
			Current.Size = LockSize;

			// Always use staging buffers to update 'Static' buffers since they maybe be in use by GPU atm
			const bool bUseStagingBuffer = (bStatic || !Device->HasUnifiedMemory());
			if (bUseStagingBuffer)
			{
				VulkanRHI::FPendingBufferLock PendingLock;
				PendingLock.Offset = Offset;
				PendingLock.Size = LockSize;
				PendingLock.LockMode = LockMode;

				VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(LockSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
				PendingLock.StagingBuffer = StagingBuffer;
				Data = StagingBuffer->GetMappedPointer();

				{
					FScopeLock ScopeLock(&GPendingLockIBsMutex);
					check(!GPendingLockIBs.Contains(this));
					GPendingLockIBs.Add(this, PendingLock);
				}
			}
			else
			{
				Data = Buffers[DynamicBufferIndex].GetMappedPointer(Device);
				DataOffset = Offset;
				LockStatus = ELockStatus::PersistentMapping;
			}
		}
	}

	check(Data);
	return (uint8*)Data + DataOffset;
}

inline void FVulkanResourceMultiBuffer::InternalUnlock(FVulkanCommandListContext& Context, VulkanRHI::FPendingBufferLock& PendingLock, FVulkanResourceMultiBuffer* MultiBuffer, int32 InDynamicBufferIndex)
{
	uint32 LockSize = PendingLock.Size;
	uint32 LockOffset = PendingLock.Offset;
	VulkanRHI::FStagingBuffer* StagingBuffer = PendingLock.StagingBuffer;
	PendingLock.StagingBuffer = nullptr;

	// We need to do this on the active command buffer instead of using an upload command buffer. The high level code sometimes reuses the same
	// buffer in sequences of upload / dispatch, upload / dispatch, so we need to order the copy commands correctly with respect to the dispatches.
	FVulkanCmdBuffer* Cmd = Context.GetCommandBufferManager()->GetActiveCmdBuffer();
	check(Cmd && Cmd->IsOutsideRenderPass());
	VkCommandBuffer CmdBuffer = Cmd->GetHandle();

	VulkanRHI::DebugHeavyWeightBarrier(CmdBuffer, 16);

	VkBufferCopy Region;
	FMemory::Memzero(Region);
	Region.size = LockSize;
	//Region.srcOffset = 0;
	Region.dstOffset = LockOffset + MultiBuffer->Buffers[InDynamicBufferIndex].Offset;
	VulkanRHI::vkCmdCopyBuffer(CmdBuffer, StagingBuffer->GetHandle(), MultiBuffer->Buffers[InDynamicBufferIndex].GetBufferHandle(), 1, &Region);

	// High level code expects the data in MultiBuffer to be ready to read
	VkMemoryBarrier BarrierAfter = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT };
	VulkanRHI::vkCmdPipelineBarrier(CmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &BarrierAfter, 0, nullptr, 0, nullptr);

	MultiBuffer->GetParent()->GetStagingManager().ReleaseBuffer(Cmd, StagingBuffer);
}

struct FRHICommandMultiBufferUnlock final : public FRHICommand<FRHICommandMultiBufferUnlock>
{
	VulkanRHI::FPendingBufferLock PendingLock;
	FVulkanResourceMultiBuffer* MultiBuffer;
	FVulkanDevice* Device;
	int32 DynamicBufferIndex;

	FRHICommandMultiBufferUnlock(FVulkanDevice* InDevice, const VulkanRHI::FPendingBufferLock& InPendingLock, FVulkanResourceMultiBuffer* InMultiBuffer, int32 InDynamicBufferIndex)
		: PendingLock(InPendingLock)
		, MultiBuffer(InMultiBuffer)
		, Device(InDevice)
		, DynamicBufferIndex(InDynamicBufferIndex)
	{
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		FVulkanResourceMultiBuffer::InternalUnlock(FVulkanCommandListContext::GetVulkanContext(CmdList.GetContext()), PendingLock, MultiBuffer, DynamicBufferIndex);
	}
};

void FVulkanResourceMultiBuffer::Unlock(FRHICommandListBase* RHICmdList, FVulkanCommandListContext* Context)
{
	check(RHICmdList || Context);

	const bool bDynamic = EnumHasAnyFlags(GetUsage(), BUF_Dynamic);
	const bool bVolatile = EnumHasAnyFlags(GetUsage(), BUF_Volatile);
	const bool bStatic = EnumHasAnyFlags(GetUsage(), BUF_Static) || !(bVolatile || bDynamic);
	const bool bSR = EnumHasAnyFlags(GetUsage(), BUF_ShaderResource);

	check(LockStatus != ELockStatus::Unlocked);

	if (bVolatile || LockStatus == ELockStatus::PersistentMapping)
	{
		// Nothing to do here...
	}
	else
	{
		check(bStatic || bDynamic || bSR);

		VulkanRHI::FPendingBufferLock PendingLock = GetPendingBufferLock(this);

		PendingLock.StagingBuffer->FlushMappedMemory();

		if (PendingLock.LockMode == RLM_ReadOnly)
		{
			// Just remove the staging buffer here.
			Device->GetStagingManager().ReleaseBuffer(0, PendingLock.StagingBuffer);
		}
		else if (PendingLock.LockMode == RLM_WriteOnly)
		{
			if (Context || (RHICmdList && RHICmdList->IsBottomOfPipe()))
			{
				if (!Context)
				{
					Context = &FVulkanCommandListContext::GetVulkanContext(RHICmdList->GetContext());
				}

				FVulkanResourceMultiBuffer::InternalUnlock(*Context, PendingLock, this, DynamicBufferIndex);
			}
			else
			{
				ALLOC_COMMAND_CL(*RHICmdList, FRHICommandMultiBufferUnlock)(Device, PendingLock, this, DynamicBufferIndex);
			}
		}
	}

	LockStatus = ELockStatus::Unlocked;
}

void FVulkanResourceMultiBuffer::Swap(FVulkanResourceMultiBuffer& Other)
{
	FRHIBuffer::Swap(Other);

	check(LockStatus == ELockStatus::Unlocked);

	// FDeviceChild
	::Swap(Device, Other.Device);
	
	::Swap(BufferUsageFlags, Other.BufferUsageFlags);
	::Swap(NumBuffers, Other.NumBuffers);
	::Swap(DynamicBufferIndex, Other.DynamicBufferIndex);
	::Swap(Buffers, Other.Buffers);
	::Swap(Current, Other.Current);
	::Swap(VolatileLockInfo, Other.VolatileLockInfo);
}

FBufferRHIRef FVulkanDynamicRHI::RHICreateBuffer(FRHICommandListBase& RHICmdList, uint32 Size, EBufferUsageFlags Usage, uint32 Stride, ERHIAccess ResourceState, FRHIResourceCreateInfo& ResourceCreateInfo)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanBuffers);

	if (ResourceCreateInfo.bWithoutNativeResource)
	{
		return new FVulkanResourceMultiBuffer(Device, 0, BUF_None, 0, ResourceCreateInfo, &RHICmdList);
	}
	return new FVulkanResourceMultiBuffer(Device, Size, Usage, Stride, ResourceCreateInfo, &RHICmdList);
}

void* FVulkanDynamicRHI::LockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* BufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanBuffers);
	FVulkanResourceMultiBuffer* Buffer = ResourceCast(BufferRHI);
	return Buffer->Lock(RHICmdList, LockMode, Size, Offset);
}

void FVulkanDynamicRHI::UnlockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* BufferRHI)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanBuffers);
	FVulkanResourceMultiBuffer* Buffer = ResourceCast(BufferRHI);
	Buffer->Unlock(RHICmdList);
}

void FVulkanDynamicRHI::RHICopyBuffer(FRHIBuffer* SourceBufferRHI, FRHIBuffer* DestBufferRHI)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanDynamicRHI::RHITransferBufferUnderlyingResource(FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer)
{
	check(DestBuffer);
	FVulkanResourceMultiBuffer* Dest = ResourceCast(DestBuffer);
	if (!SrcBuffer)
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("RHITransferBufferUnderlyingResource"));
		TRefCountPtr<FVulkanResourceMultiBuffer> DeletionProxy = new FVulkanResourceMultiBuffer(Dest->GetParent(), 0, BUF_None, 0, CreateInfo, nullptr);
		Dest->Swap(*DeletionProxy);
	}
	else
	{
		FVulkanResourceMultiBuffer* Src = ResourceCast(SrcBuffer);
		Dest->Swap(*Src);
	}
}

void FVulkanDynamicRHI::RHIUnlockBuffer(FRHICommandListBase& RHICmdList, FRHIBuffer* BufferRHI)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_UnlockBuffer_RenderThread);

	// We might need to queue a copy, make sure we have an active pipeline
	if (RHICmdList.IsTopOfPipe() && (RHICmdList.GetPipeline() == ERHIPipeline::None))
	{
		RHICmdList.SwitchPipeline(ERHIPipeline::Graphics);
	}
	FDynamicRHI::RHIUnlockBuffer(RHICmdList, BufferRHI);
}