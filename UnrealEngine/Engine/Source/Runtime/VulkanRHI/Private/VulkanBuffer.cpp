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
#include "RHICoreStats.h"

static TMap<FVulkanResourceMultiBuffer*, VulkanRHI::FPendingBufferLock> GPendingLockIBs;
static FCriticalSection GPendingLockIBsMutex;

int32 GVulkanForceStagingBufferOnLock = 0;
static FAutoConsoleVariableRef CVarVulkanForceStagingBufferOnLock(
	TEXT("r.Vulkan.ForceStagingBufferOnLock"),
	GVulkanForceStagingBufferOnLock,
	TEXT("When nonzero, non-volatile buffer locks will always use staging buffers. Useful for debugging.\n")
	TEXT("default: 0"),
	ECVF_RenderThreadSafe
);

static FORCEINLINE VulkanRHI::FPendingBufferLock GetPendingBufferLock(FVulkanResourceMultiBuffer* Buffer)
{
	VulkanRHI::FPendingBufferLock PendingLock;

	// Found only if it was created for Write
	FScopeLock ScopeLock(&GPendingLockIBsMutex);
	const bool bFound = GPendingLockIBs.RemoveAndCopyValue(Buffer, PendingLock);

	checkf(bFound, TEXT("Mismatched Buffer Lock/Unlock!"));
	return PendingLock;
}

static void UpdateVulkanBufferStats(const FRHIBufferDesc& BufferDesc, int64 BufferSize, bool bAllocating)
{
	UE::RHICore::UpdateGlobalBufferStats(BufferDesc, BufferSize, bAllocating);
}

static VkDeviceAddress GetBufferDeviceAddress(FVulkanDevice* Device, VkBuffer Buffer)
{
	if (Device->GetOptionalExtensions().HasBufferDeviceAddress)
	{
		VkBufferDeviceAddressInfoKHR DeviceAddressInfo;
		ZeroVulkanStruct(DeviceAddressInfo, VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
		DeviceAddressInfo.buffer = Buffer;
		return VulkanRHI::vkGetBufferDeviceAddressKHR(Device->GetInstanceHandle(), &DeviceAddressInfo);
	}
	return 0;
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
	TranslateFlag(BUF_UniformBuffer, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

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
		// For descriptors buffers
		if (InDevice->GetOptionalExtensions().HasBufferDeviceAddress)
		{
			OutVkUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		}
	}

	return OutVkUsage;
}

FVulkanResourceMultiBuffer::FVulkanResourceMultiBuffer(FVulkanDevice* InDevice, FRHIBufferDesc const& InBufferDesc, FRHIResourceCreateInfo& CreateInfo, FRHICommandListBase* InRHICmdList, const FRHITransientHeapAllocation* InTransientHeapAllocation)
	: FRHIBuffer(InBufferDesc)
	, VulkanRHI::FDeviceChild(InDevice)
{
	VULKAN_TRACK_OBJECT_CREATE(FVulkanResourceMultiBuffer, this);

	const bool bZeroSize = (InBufferDesc.Size == 0);
	BufferUsageFlags = UEToVKBufferUsageFlags(InDevice, InBufferDesc.Usage, bZeroSize);
	
	if (!bZeroSize)
	{
		check(InDevice);

		const bool bVolatile = EnumHasAnyFlags(InBufferDesc.Usage, BUF_Volatile);
		if (bVolatile)
		{
			check(InRHICmdList);

			// Volatile buffers always work out of the same first slot
			CurrentBufferIndex = BufferAllocs.Emplace();

			// Get a dummy buffer as sometimes the high-level misbehaves and tries to use SRVs off volatile buffers before filling them in...
			void* Data = Lock(*InRHICmdList, RLM_WriteOnly, InBufferDesc.Size, 0);

			if (CreateInfo.ResourceArray)
			{
				uint32 CopyDataSize = FMath::Min(InBufferDesc.Size, CreateInfo.ResourceArray->GetResourceDataSize());
				FMemory::Memcpy(Data, CreateInfo.ResourceArray->GetResourceData(), CopyDataSize);
			}
			else
			{
				FMemory::Memzero(Data, InBufferDesc.Size);
			}

			Unlock(*InRHICmdList);
		}
		else
		{
			const bool bUnifiedMem = InDevice->HasUnifiedMemory();
			const uint32 BufferAlignment = FMemoryManager::CalculateBufferAlignment(*InDevice, InBufferDesc.Usage, bZeroSize);

			if (InTransientHeapAllocation != nullptr)
			{
				FBufferAlloc NewBufferAlloc;
				NewBufferAlloc.Alloc = FVulkanTransientHeap::GetVulkanAllocation(*InTransientHeapAllocation);
				NewBufferAlloc.HostPtr = bUnifiedMem ? NewBufferAlloc.Alloc.GetMappedPointer(Device) : nullptr;
				NewBufferAlloc.DeviceAddress = GetBufferDeviceAddress(InDevice, NewBufferAlloc.Alloc.GetBufferHandle()) + NewBufferAlloc.Alloc.Offset;
				check(NewBufferAlloc.Alloc.Offset % BufferAlignment == 0);
				check(NewBufferAlloc.Alloc.Size >= InBufferDesc.Size);
				CurrentBufferIndex = BufferAllocs.Add(NewBufferAlloc);
			}
			else
			{
				AdvanceBufferIndex();
			}

			VULKAN_SET_DEBUG_NAME((*InDevice), VK_OBJECT_TYPE_BUFFER, BufferAllocs[CurrentBufferIndex].Alloc.GetBufferHandle(), TEXT("%s"), CreateInfo.DebugName ? CreateInfo.DebugName : TEXT("UnknownBuffer"));

			if (CreateInfo.ResourceArray)
			{
				check(InRHICmdList);

				uint32 CopyDataSize = FMath::Min(InBufferDesc.Size, CreateInfo.ResourceArray->GetResourceDataSize());
				// We know this buffer is not in use by GPU atm. If we do have a direct access initialize it without extra copies
				if (bUnifiedMem)
				{
					FMemory::Memcpy(BufferAllocs[CurrentBufferIndex].HostPtr, CreateInfo.ResourceArray->GetResourceData(), CopyDataSize);
				}
				else
				{
					void* Data = Lock(*InRHICmdList, RLM_WriteOnly, CopyDataSize, 0);
					FMemory::Memcpy(Data, CreateInfo.ResourceArray->GetResourceData(), CopyDataSize);
					Unlock(*InRHICmdList);
				}

				CreateInfo.ResourceArray->Discard();
			}
		}
	}
}

FVulkanResourceMultiBuffer::~FVulkanResourceMultiBuffer()
{
	VULKAN_TRACK_OBJECT_DELETE(FVulkanResourceMultiBuffer, this);
	ReleaseOwnership();
}


void FVulkanResourceMultiBuffer::AdvanceBufferIndex()
{
	auto FreePreviousAlloc = [&]() {
		if (CurrentBufferIndex >= 0)
		{
			FBufferAlloc& CurrentBufferAlloc = BufferAllocs[CurrentBufferIndex];
			CurrentBufferAlloc.AllocStatus = FBufferAlloc::EAllocStatus::NeedsFence;
			CurrentBufferAlloc.Fence->Clear();
		}
	};

	// Try to see if one of the buffers in our pool can be reused
	if (BufferAllocs.Num() > 1)
	{
		for (int32 BufferIndex = 0; BufferIndex < BufferAllocs.Num(); ++BufferIndex)
		{
			if (CurrentBufferIndex == BufferIndex)
			{
				continue;
			}

			// Fences are only written on Unlock(), but are polled on lock/unlock
			FBufferAlloc& BufferAlloc = BufferAllocs[BufferIndex];
			if ((BufferAlloc.AllocStatus == FBufferAlloc::EAllocStatus::Pending) && BufferAlloc.Fence->Poll())
			{
				BufferAlloc.AllocStatus = FBufferAlloc::EAllocStatus::Available;
				BufferAlloc.Fence->Clear();
			}

			if (BufferAlloc.AllocStatus == FBufferAlloc::EAllocStatus::Available)
			{
				FreePreviousAlloc();

				CurrentBufferIndex = BufferIndex;
				BufferAllocs[CurrentBufferIndex].AllocStatus = FBufferAlloc::EAllocStatus::InUse;
				return;
			}
		}
	}

	// Allocate a new buffer
	{
		FreePreviousAlloc();

		const bool bUnifiedMem = Device->HasUnifiedMemory();
		const VkMemoryPropertyFlags BufferMemFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | (bUnifiedMem ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) : 0);
		const uint32 BufferSize = GetSize();
		const uint32 BufferAlignment = FMemoryManager::CalculateBufferAlignment(*Device, GetUsage(), (BufferSize == 0));

		CurrentBufferIndex = BufferAllocs.Emplace();
		FBufferAlloc& NewBufferAlloc = BufferAllocs[CurrentBufferIndex];
		if (!Device->GetMemoryManager().AllocateBufferPooled(NewBufferAlloc.Alloc, nullptr, BufferSize, BufferAlignment, BufferUsageFlags, BufferMemFlags, EVulkanAllocationMetaMultiBuffer, __FILE__, __LINE__))
		{
			Device->GetMemoryManager().HandleOOM();
		}
		NewBufferAlloc.HostPtr = bUnifiedMem ? NewBufferAlloc.Alloc.GetMappedPointer(Device) : nullptr;
		NewBufferAlloc.Fence = new FVulkanGPUFence(TEXT("VulkanDynamicBuffer"));
		NewBufferAlloc.AllocStatus = FBufferAlloc::EAllocStatus::InUse;
		NewBufferAlloc.DeviceAddress = GetBufferDeviceAddress(Device, NewBufferAlloc.Alloc.GetBufferHandle()) + NewBufferAlloc.Alloc.Offset;

		UpdateVulkanBufferStats(GetDesc(), BufferSize, true);
	}
}

void FVulkanResourceMultiBuffer::UpdateBufferAllocStates(FVulkanCommandListContext& Context)
{
	for (int32 BufferIndex = 0; BufferIndex < BufferAllocs.Num(); ++BufferIndex)
	{
		if (CurrentBufferIndex == BufferIndex)
		{
			continue;
		}

		FBufferAlloc& BufferAlloc = BufferAllocs[BufferIndex];
		if (BufferAlloc.AllocStatus == FBufferAlloc::EAllocStatus::Pending)
		{
			if (BufferAlloc.Fence->Poll())
			{
				BufferAlloc.AllocStatus = FBufferAlloc::EAllocStatus::Available;
				BufferAlloc.Fence->Clear();
			}
		}
		else if (BufferAlloc.AllocStatus == FBufferAlloc::EAllocStatus::NeedsFence)
		{
			Context.RHIWriteGPUFence(BufferAlloc.Fence);
			BufferAlloc.AllocStatus = FBufferAlloc::EAllocStatus::Pending;
		}
	}
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

	const bool bVolatile = EnumHasAnyFlags(GetUsage(), BUF_Volatile);
	check(LockStatus == ELockStatus::Unlocked);

	LockStatus = ELockStatus::Locked;
	++LockCounter;

	if (bVolatile)
	{
		if (LockMode == RLM_ReadOnly)
		{
			checkf(0, TEXT("Volatile buffers can't be locked for read."));
		}
		else
		{
			FBufferAlloc& BufferAlloc = BufferAllocs[0];

			FTempFrameAllocationBuffer::FTempAllocInfo VolatileAlloc;
			Context.GetTempFrameAllocationBuffer().Alloc(LockSize + Offset, 256, VolatileAlloc);
			check(!VolatileAlloc.Allocation.HasAllocation());

			BufferAlloc.Alloc.Reference(VolatileAlloc.Allocation);
			Data = BufferAlloc.HostPtr = VolatileAlloc.Data;
			DataOffset = Offset;

			// Patch our alloc to go directly to our offset
			BufferAlloc.Alloc.Offset += VolatileAlloc.CurrentOffset;
			BufferAlloc.Alloc.Size = VolatileAlloc.Size;

			BufferAlloc.DeviceAddress = GetBufferDeviceAddress(Device, BufferAlloc.Alloc.GetBufferHandle()) + BufferAlloc.Alloc.Offset;
		}
	}
	else
	{
		const bool bDynamic = EnumHasAnyFlags(GetUsage(), BUF_Dynamic);
		const bool bStatic = EnumHasAnyFlags(GetUsage(), BUF_Static) || !(bVolatile || bDynamic);
		const bool bUAV = EnumHasAnyFlags(GetUsage(), BUF_UnorderedAccess);
		const bool bSR = EnumHasAnyFlags(GetUsage(), BUF_ShaderResource);
		const bool bUnifiedMem = Device->HasUnifiedMemory();

		check(bStatic || bDynamic || bUAV || bSR);

		if (LockMode == RLM_ReadOnly)
		{
			check(IsInRenderingThread() && Context.IsImmediate());

			if (bUnifiedMem)
			{
				Data = BufferAllocs[CurrentBufferIndex].HostPtr;
				DataOffset = Offset;
				LockStatus = ELockStatus::PersistentMapping;
			}
			else 
			{
				Device->PrepareForCPURead();
				FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetUploadCmdBuffer();
				
				// Make sure any previous tasks have finished on the source buffer.
				VkMemoryBarrier BarrierBefore = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT };
				VulkanRHI::vkCmdPipelineBarrier(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &BarrierBefore, 0, nullptr, 0, nullptr);

				// Create a staging buffer we can use to copy data from device to cpu.
				VulkanRHI::FStagingBuffer* StagingBuffer = Device->GetStagingManager().AcquireBuffer(LockSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

				// Fill the staging buffer with the data on the device.
				VkBufferCopy Regions;
				Regions.size = LockSize;
				Regions.srcOffset = Offset + BufferAllocs[CurrentBufferIndex].Alloc.Offset;
				Regions.dstOffset = 0;
				
				VulkanRHI::vkCmdCopyBuffer(CmdBuffer->GetHandle(), BufferAllocs[CurrentBufferIndex].Alloc.GetBufferHandle(), StagingBuffer->GetHandle(), 1, &Regions);

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

			// Always use staging buffers to update 'Static' buffers since they maybe be in use by GPU atm
			const bool bUseStagingBuffer = (bStatic || !bUnifiedMem) || GVulkanForceStagingBufferOnLock;
			if (bUseStagingBuffer)
			{
				// NOTE: No need to change the CurrentBufferIndex if we're using a staging buffer for the copy

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
				AdvanceBufferIndex();

				Data = BufferAllocs[CurrentBufferIndex].HostPtr;
				DataOffset = Offset;
				LockStatus = ELockStatus::PersistentMapping;
			}
		}
	}

	check(Data);
	return (uint8*)Data + DataOffset;
}

inline void FVulkanResourceMultiBuffer::InternalUnlock(FVulkanCommandListContext& Context, VulkanRHI::FPendingBufferLock& PendingLock, FVulkanResourceMultiBuffer* MultiBuffer, int32 InBufferIndex)
{
	const uint32 LockSize = PendingLock.Size;
	const uint32 LockOffset = PendingLock.Offset;
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
	Region.dstOffset = LockOffset + MultiBuffer->BufferAllocs[InBufferIndex].Alloc.Offset;
	VulkanRHI::vkCmdCopyBuffer(CmdBuffer, StagingBuffer->GetHandle(), MultiBuffer->BufferAllocs[InBufferIndex].Alloc.GetBufferHandle(), 1, &Region);

	// High level code expects the data in MultiBuffer to be ready to read
	VkMemoryBarrier BarrierAfter = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT };
	VulkanRHI::vkCmdPipelineBarrier(CmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &BarrierAfter, 0, nullptr, 0, nullptr);

	MultiBuffer->GetParent()->GetStagingManager().ReleaseBuffer(Cmd, StagingBuffer);

	MultiBuffer->UpdateBufferAllocStates(Context);
	MultiBuffer->UpdateLinkedViews();
}

struct FRHICommandMultiBufferUnlock final : public FRHICommand<FRHICommandMultiBufferUnlock>
{
	VulkanRHI::FPendingBufferLock PendingLock;
	FVulkanResourceMultiBuffer* MultiBuffer;
	FVulkanDevice* Device;
	int32 BufferIndex;

	FRHICommandMultiBufferUnlock(FVulkanDevice* InDevice, const VulkanRHI::FPendingBufferLock& InPendingLock, FVulkanResourceMultiBuffer* InMultiBuffer, int32 InBufferIndex)
		: PendingLock(InPendingLock)
		, MultiBuffer(InMultiBuffer)
		, Device(InDevice)
		, BufferIndex(InBufferIndex)
	{
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		FVulkanResourceMultiBuffer::InternalUnlock(FVulkanCommandListContext::GetVulkanContext(CmdList.GetContext()), PendingLock, MultiBuffer, BufferIndex);
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

	if (bVolatile)
	{
		if (RHICmdList && RHICmdList->IsTopOfPipe())
		{
			RHICmdList->EnqueueLambda([this](FRHICommandListBase&)
			{
				UpdateLinkedViews();
			});
		}
		else
		{
			UpdateLinkedViews();
		}
	}
	else if (LockStatus == ELockStatus::PersistentMapping)
	{
		if (Context)
		{
			UpdateBufferAllocStates(*Context);
			UpdateLinkedViews();
		}
		else
		{
			RHICmdList->EnqueueLambda([this](FRHICommandListBase& CmdList)
			{
				FVulkanCommandListContext& Context = FVulkanCommandListContext::GetVulkanContext(CmdList.GetContext());
				UpdateBufferAllocStates(Context);
				UpdateLinkedViews();
			});
		}
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

				FVulkanResourceMultiBuffer::InternalUnlock(*Context, PendingLock, this, CurrentBufferIndex);
			}
			else
			{
				ALLOC_COMMAND_CL(*RHICmdList, FRHICommandMultiBufferUnlock)(Device, PendingLock, this, CurrentBufferIndex);
			}
		}
	}

	LockStatus = ELockStatus::Unlocked;
}

void FVulkanResourceMultiBuffer::TakeOwnership(FVulkanResourceMultiBuffer& Other)
{
	check(Other.LockStatus == ELockStatus::Unlocked);
	check(GetParent() == Other.GetParent());

	// Clean up any resource this buffer already owns
	ReleaseOwnership();

	// Transfer ownership of Other's resources to this instance
	FRHIBuffer::TakeOwnership(Other);

	BufferUsageFlags   = Other.BufferUsageFlags;
	CurrentBufferIndex = Other.CurrentBufferIndex;

	Other.BufferUsageFlags   = {};
	Other.CurrentBufferIndex = -1;

	// Swap the empty array from the ReleaseOwnership with the Other allocations
	::Swap(BufferAllocs, Other.BufferAllocs);
}

void FVulkanResourceMultiBuffer::ReleaseOwnership()
{
	check(LockStatus == ELockStatus::Unlocked);

	FRHIBuffer::ReleaseOwnership();

	uint64 TotalSize = 0;
	for (int32 Index = 0; Index < BufferAllocs.Num(); ++Index)
	{
		if (BufferAllocs[Index].Alloc.HasAllocation())
		{
			TotalSize += BufferAllocs[Index].Alloc.Size;
			Device->GetMemoryManager().FreeVulkanAllocation(BufferAllocs[Index].Alloc);
		}
		BufferAllocs[Index].Fence = nullptr;
	}
	BufferAllocs.Empty();

	if (TotalSize > 0)
	{
		UpdateVulkanBufferStats(GetDesc(), TotalSize, false);
	}
}

FBufferRHIRef FVulkanDynamicRHI::RHICreateBuffer(FRHICommandListBase& RHICmdList, FRHIBufferDesc const& Desc, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo)
{
#if VULKAN_USE_LLM
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanBuffers);
#else
	LLM_SCOPE(EnumHasAnyFlags(Desc.Usage, EBufferUsageFlags::VertexBuffer | EBufferUsageFlags::IndexBuffer) ? ELLMTag::Meshes : ELLMTag::RHIMisc);
#endif
	return new FVulkanResourceMultiBuffer(Device, Desc, CreateInfo, &RHICmdList);
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

void FVulkanDynamicRHI::RHITransferBufferUnderlyingResource(FRHICommandListBase& RHICmdList, FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer)
{
	FVulkanResourceMultiBuffer* Dst = ResourceCast(DestBuffer);
	FVulkanResourceMultiBuffer* Src = ResourceCast(SrcBuffer);

	if (Src)
	{
		// The source buffer should not have any associated views.
		check(!Src->HasLinkedViews());

		Dst->TakeOwnership(*Src);
	}
	else
	{
		Dst->ReleaseOwnership();
	}

	Dst->UpdateLinkedViews();
}

void FVulkanDynamicRHI::RHIUnlockBuffer(FRHICommandListBase& RHICmdList, FRHIBuffer* BufferRHI)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDynamicRHI_UnlockBuffer_RenderThread);
	FDynamicRHI::RHIUnlockBuffer(RHICmdList, BufferRHI);
}
