// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanUniformBuffer.cpp: Vulkan Constant buffer implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanContext.h"
#include "VulkanLLM.h"
#include "ShaderParameterStruct.h"
#include "RHIUniformBufferDataShared.h"

static int32 GVulkanAllowUniformUpload = 1;
static FAutoConsoleVariableRef CVarVulkanAllowUniformUpload(
	TEXT("r.Vulkan.AllowUniformUpload"),
	GVulkanAllowUniformUpload,
	TEXT("Allow Uniform Buffer uploads outside of renderpasses\n")
	TEXT(" 0: Disabled, buffers are always reallocated\n")
	TEXT(" 1: Enabled, buffers are uploaded outside renderpasses"),
	ECVF_Default
);

enum
{
	PackedUniformsRingBufferSize = 16 * 1024 * 1024
};

/*-----------------------------------------------------------------------------
	Uniform buffer RHI object
-----------------------------------------------------------------------------*/


static void UpdateBindlessHandles(const void* SourceData, const FRHIUniformBufferLayout* Layout)
{
	UE::RHICore::FUniformDataReader Reader(SourceData);

	// Update views before picking up their bindless handles
	for (const FRHIUniformBufferResource& Resource : Layout->Resources)
	{
		switch (Resource.MemberType)
		{
			case UBMT_SRV:
			{
				FVulkanShaderResourceView* ShaderResourceView = Reader.Read<FVulkanShaderResourceView*>(Resource);
				if (ShaderResourceView)
				{
					ShaderResourceView->UpdateView();
				}
			}
			break;
			case UBMT_RDG_TEXTURE_SRV:
			case UBMT_RDG_BUFFER_SRV:
			{
				FRDGShaderResourceView* RDGShaderResourceView = Reader.Read<FRDGShaderResourceView*>(Resource);
				if (RDGShaderResourceView && RDGShaderResourceView->GetRHI())
				{
					FVulkanShaderResourceView* ShaderResourceView = ResourceCast(RDGShaderResourceView->GetRHI());
					ShaderResourceView->UpdateView();
				}
			}
			break;
			case UBMT_UAV:
			{
				FVulkanUnorderedAccessView* UnorderedAccessView = Reader.Read<FVulkanUnorderedAccessView*>(Resource);
				if (UnorderedAccessView)
				{
					UnorderedAccessView->UpdateView();
				}
			}
			break;
			case UBMT_RDG_TEXTURE_UAV:
			case UBMT_RDG_BUFFER_UAV:
			{
				FRDGUnorderedAccessView* RDGUnorderedAccessView = Reader.Read<FRDGUnorderedAccessView*>(Resource);
				if (RDGUnorderedAccessView && RDGUnorderedAccessView->GetRHI())
				{
					FVulkanUnorderedAccessView* UnorderedAccessView = ResourceCast(RDGUnorderedAccessView->GetRHI());
					UnorderedAccessView->UpdateView();
				}
			}
			break;
			case UBMT_SAMPLER:
			case UBMT_TEXTURE:
			case UBMT_RDG_TEXTURE:
			case UBMT_NESTED_STRUCT:
			case UBMT_INCLUDED_STRUCT:
			case UBMT_REFERENCED_STRUCT:
			{
				// Do nothing
			}
			break;

			default:
			//checkf(false, TEXT("Unhandled resource type?"));
			break;
		}
	}
}

static void UpdateUniformBufferConstants(FVulkanDevice* Device, void* DestinationData, const void* SourceData, const FRHIUniformBufferLayout* Layout)
{
	const bool bSupportsBindless = Device->SupportsBindless();
	if (bSupportsBindless)
	{
		// Update the bindless handles in views before we use them
		UpdateBindlessHandles(SourceData, Layout);
	}

	UE::RHICore::UpdateUniformBufferConstants(DestinationData, SourceData, *Layout, bSupportsBindless);
}

static inline EBufferUsageFlags UniformBufferToBufferUsage(EUniformBufferUsage Usage)
{
	switch (Usage)
	{
	default:
		ensure(0);
		// fall through...
	case UniformBuffer_SingleDraw:
		return BUF_Volatile;
	case UniformBuffer_SingleFrame:
		return BUF_Volatile;
	case UniformBuffer_MultiFrame:
		return BUF_Static;
	}
}

static bool UseRingBuffer(EUniformBufferUsage Usage)
{
	// Add a cvar to control this behavior?
	return (Usage == UniformBuffer_SingleDraw || Usage == UniformBuffer_SingleFrame);
}

static void UpdateUniformBufferHelper(FVulkanCommandListContext& Context, FVulkanUniformBuffer* VulkanUniformBuffer, const void* Data, bool bUpdateConstants = true)
{
	FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetActiveCmdBufferDirect();

	FVulkanDevice* Device = Context.GetDevice();
	const int32 DataSize = VulkanUniformBuffer->GetLayout().ConstantBufferSize;

	auto CopyUniformBufferData = [&](void* DestinationData, const void* SourceData) {

		if (bUpdateConstants)
		{
			// Update constants as the data is copied
			UpdateUniformBufferConstants(Device, DestinationData, SourceData, VulkanUniformBuffer->GetLayoutPtr());
		}
		else
		{
			// Don't touch constant, copy the data as-is
			FMemory::Memcpy(DestinationData, SourceData, DataSize);
		}
	};

	if (UseRingBuffer(VulkanUniformBuffer->Usage))
	{
		FVulkanUniformBufferUploader* UniformBufferUploader = Context.GetUniformBufferUploader();
		const VkDeviceSize UBOffsetAlignment = Device->GetLimits().minUniformBufferOffsetAlignment;
		const FVulkanAllocation& RingBufferAllocation = UniformBufferUploader->GetCPUBufferAllocation();
		uint64 RingBufferOffset = UniformBufferUploader->AllocateMemory(DataSize, UBOffsetAlignment, CmdBuffer);

		VulkanUniformBuffer->Allocation.Init(
			EVulkanAllocationEmpty, 
			EVulkanAllocationMetaUnknown, 
			RingBufferAllocation.VulkanHandle, 
			DataSize,
			RingBufferOffset,
			RingBufferAllocation.AllocatorIndex,
			RingBufferAllocation.AllocationIndex,
			RingBufferAllocation.HandleId);
		
		uint8* UploadLocation = UniformBufferUploader->GetCPUMappedPointer() + RingBufferOffset;
		CopyUniformBufferData(UploadLocation, Data);
	}
	else
	{
		check(CmdBuffer->IsOutsideRenderPass());

		VulkanRHI::FTempFrameAllocationBuffer::FTempAllocInfo LockInfo;
		Context.GetTempFrameAllocationBuffer().Alloc(DataSize, 16, LockInfo);
		CopyUniformBufferData(LockInfo.Data, Data);

		VkBufferCopy Region;
		Region.size = DataSize;
		Region.srcOffset = LockInfo.CurrentOffset + LockInfo.Allocation.Offset;
		Region.dstOffset = VulkanUniformBuffer->GetOffset();
		VkBuffer UBBuffer = VulkanUniformBuffer->Allocation.GetBufferHandle();
		VkBuffer LockHandle = LockInfo.Allocation.GetBufferHandle();

		VulkanRHI::vkCmdCopyBuffer(CmdBuffer->GetHandle(), LockHandle, UBBuffer, 1, &Region);
	}
};

FVulkanUniformBuffer::FVulkanUniformBuffer(FVulkanDevice& InDevice, const FRHIUniformBufferLayout* InLayout, const void* Contents, EUniformBufferUsage InUsage, EUniformBufferValidation Validation)
	: FRHIUniformBuffer(InLayout)
	, Device(&InDevice)
	, Usage(InUsage)
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUniformBufferCreateTime);
#endif

	// Verify the correctness of our thought pattern how the resources are delivered
	//	- If we have at least one resource, we also expect ResourceOffset to have an offset
	//	- Meaning, there is always a uniform buffer with a size specified larged than 0 bytes
	check(InLayout->Resources.Num() > 0 || InLayout->ConstantBufferSize > 0);

	// Setup resource table
	const uint32 NumResources = InLayout->Resources.Num();
	if (NumResources > 0)
	{
		// Transfer the resource table to an internal resource-array
		ResourceTable.Empty(NumResources);
		ResourceTable.AddZeroed(NumResources);

		if (Contents)
		{
			for (uint32 Index = 0; Index < NumResources; ++Index)
			{
				ResourceTable[Index] = GetShaderParameterResourceRHI(Contents, InLayout->Resources[Index].MemberOffset, InLayout->Resources[Index].MemberType);
			}
		}
	}

	if (InLayout->ConstantBufferSize > 0)
	{
		const bool bInRenderingThread = IsInRenderingThread();
		const bool bInRHIThread = IsInRHIThread();

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (UseRingBuffer(InUsage) && (bInRenderingThread || bInRHIThread)
			// :todo-jn:  Temporary check until we have a command list arg passed in to avoid a race where the RenderThread
			// would pick up other tasks (because of task retraction) and execute them as if on the RenderThread.
			&& !UE::Tasks::Private::IsThreadRetractingTask())
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		{
			if (Contents)
			{
				FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
				FVulkanUniformBuffer* UniformBuffer = this;
				int32 DataSize = InLayout->ConstantBufferSize;

				// make sure we allocate from RingBuffer on RHIT
				const bool bCanAllocOnThisThread = RHICmdList.Bypass() || (!IsRunningRHIInSeparateThread() && bInRenderingThread) || bInRHIThread;
				if (bCanAllocOnThisThread)
				{
					FVulkanCommandListContextImmediate& Context = Device->GetImmediateContext();
					UpdateUniformBufferHelper(Context, UniformBuffer, Contents);
				}
				else
				{
					void* CmdListConstantBufferData = RHICmdList.Alloc(DataSize, 16);
					UpdateUniformBufferConstants(Device, CmdListConstantBufferData, Contents, InLayout);

					RHICmdList.EnqueueLambda([UniformBuffer, DataSize, CmdListConstantBufferData](FRHICommandList& CmdList)
					{
						FVulkanCommandListContext& Context = FVulkanCommandListContext::GetVulkanContext(CmdList.GetContext());
						UpdateUniformBufferHelper(Context, UniformBuffer, CmdListConstantBufferData, false);
					});

					RHICmdList.RHIThreadFence(true);
				}
			}
		}
		else
		{
			VulkanRHI::FMemoryManager& ResourceMgr = Device->GetMemoryManager();
			// Set it directly as there is no previous one
			ResourceMgr.AllocUniformBuffer(Allocation, InLayout->ConstantBufferSize);
			if (Contents)
			{
				UpdateUniformBufferConstants(Device, Allocation.GetMappedPointer(Device), Contents, InLayout);
				Allocation.FlushMappedMemory(Device);
			}
		}
	}
}

FVulkanUniformBuffer::~FVulkanUniformBuffer()
{
	Device->GetMemoryManager().FreeUniformBuffer(Allocation);
}

void FVulkanUniformBuffer::UpdateResourceTable(const FRHIUniformBufferLayout& InLayout, const void* Contents, int32 NumResources)
{
	check(ResourceTable.Num() == NumResources);

	for (int32 Index = 0; Index < NumResources; ++Index)
	{
		const auto Parameter = InLayout.Resources[Index];
		ResourceTable[Index] = GetShaderParameterResourceRHI(Contents, Parameter.MemberOffset, Parameter.MemberType);
	}
}

void FVulkanUniformBuffer::UpdateResourceTable(FRHIResource** Resources, int32 ResourceNum)
{
	check(ResourceTable.Num() == ResourceNum);

	for (int32 ResourceIndex = 0; ResourceIndex < ResourceNum; ++ResourceIndex)
	{
		ResourceTable[ResourceIndex] = Resources[ResourceIndex];
	}
}


FUniformBufferRHIRef FVulkanDynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout* Layout, EUniformBufferUsage Usage, EUniformBufferValidation Validation)
{
	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanUniformBuffers);

	return new FVulkanUniformBuffer(*Device, Layout, Contents, Usage, Validation);
}

inline void FVulkanDynamicRHI::UpdateUniformBuffer(FRHICommandListBase& RHICmdList, FVulkanUniformBuffer* UniformBuffer, const void* Contents)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateUniformBuffers);

	const FRHIUniformBufferLayout& Layout = UniformBuffer->GetLayout();

	const int32 ConstantBufferSize = Layout.ConstantBufferSize;
	const int32 NumResources = Layout.Resources.Num();

	FVulkanAllocation NewUBAlloc;
	bool bUseUpload = GVulkanAllowUniformUpload && !RHICmdList.IsInsideRenderPass(); //inside renderpasses, a rename is enforced.
	const bool bUseRingBuffer = UseRingBuffer(UniformBuffer->Usage);

	if (!bUseUpload && !bUseRingBuffer)
	{
		if (ConstantBufferSize > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_VulkanUpdateUniformBuffersRename);
			Device->GetMemoryManager().AllocUniformBuffer(NewUBAlloc, ConstantBufferSize);
			if (Contents)
			{
				UpdateUniformBufferConstants(Device, NewUBAlloc.GetMappedPointer(Device), Contents, &Layout);
				NewUBAlloc.FlushMappedMemory(Device);
			}
		}
	}

	bool bRHIBypass = RHICmdList.Bypass();
	if (bRHIBypass)
	{
		if (ConstantBufferSize > 0)
		{
			if (bUseUpload || bUseRingBuffer)
			{			
				FVulkanCommandListContext& Context = Device->GetImmediateContext();
				UpdateUniformBufferHelper(Context, UniformBuffer, Contents);
			}
			else
			{
				UniformBuffer->UpdateAllocation(NewUBAlloc);
				Device->GetMemoryManager().FreeUniformBuffer(NewUBAlloc);
			}
		}

		UniformBuffer->UpdateResourceTable(Layout, Contents, NumResources);
	}
	else
	{
		FRHIResource** CmdListResources = nullptr;
		if (NumResources > 0)
		{
			CmdListResources = (FRHIResource**)RHICmdList.Alloc(sizeof(FRHIResource*) * NumResources, alignof(FRHIResource*));

			for (int32 Index = 0; Index < NumResources; ++Index)
			{
				CmdListResources[Index] = GetShaderParameterResourceRHI(Contents, Layout.Resources[Index].MemberOffset, Layout.Resources[Index].MemberType);
			}
		}

		if (bUseUpload || bUseRingBuffer)
		{
			void* CmdListConstantBufferData = RHICmdList.Alloc(ConstantBufferSize, 16);
			FMemory::Memcpy(CmdListConstantBufferData, Contents, ConstantBufferSize);

			FVulkanCommandListContextImmediate* Context = &Device->GetImmediateContext();

			RHICmdList.EnqueueLambda([UniformBuffer, CmdListResources, NumResources, ConstantBufferSize, CmdListConstantBufferData](FRHICommandListBase& CmdList)
			{
				FVulkanCommandListContext& Context = (FVulkanCommandListContext&)CmdList.GetContext().GetLowestLevelContext();
				UpdateUniformBufferHelper(Context, UniformBuffer, CmdListConstantBufferData);
				UniformBuffer->UpdateResourceTable(CmdListResources, NumResources);
			});
		}
		else
		{
			NewUBAlloc.Disown(); //this releases ownership while its put into the lambda
			RHICmdList.EnqueueLambda([UniformBuffer, NewUBAlloc, CmdListResources, NumResources](FRHICommandListBase& CmdList)
			{
				FVulkanAllocation Alloc;
				Alloc.Reference(NewUBAlloc);
				Alloc.Own(); //this takes ownership of the allocation
				UniformBuffer->UpdateAllocation(Alloc);
				UniformBuffer->Device->GetMemoryManager().FreeUniformBuffer(Alloc);
				UniformBuffer->UpdateResourceTable(CmdListResources, NumResources);
			});
		}
		
		RHICmdList.RHIThreadFence(true);
	}
}


void FVulkanDynamicRHI::RHIUpdateUniformBuffer(FRHICommandListBase& RHICmdList, FRHIUniformBuffer* UniformBufferRHI, const void* Contents)
{
	FVulkanUniformBuffer* UniformBuffer = ResourceCast(UniformBufferRHI);
	UpdateUniformBuffer(RHICmdList, UniformBuffer, Contents);
}

FVulkanUniformBufferUploader::FVulkanUniformBufferUploader(FVulkanDevice* InDevice)
	: VulkanRHI::FDeviceChild(InDevice)
	, CPUBuffer(nullptr)
{
	if (Device->HasUnifiedMemory())
	{
		CPUBuffer = new FVulkanRingBuffer(InDevice, PackedUniformsRingBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}
	else
	{
		if (FVulkanPlatform::SupportsDeviceLocalHostVisibleWithNoPenalty(InDevice->GetVendorId()) &&
			InDevice->GetDeviceMemoryManager().SupportsMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
		{
			CPUBuffer = new FVulkanRingBuffer(InDevice, PackedUniformsRingBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		}
		else
		{
			CPUBuffer = new FVulkanRingBuffer(InDevice, PackedUniformsRingBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		}
	}
}

FVulkanUniformBufferUploader::~FVulkanUniformBufferUploader()
{
	delete CPUBuffer;
}


FVulkanRingBuffer::FVulkanRingBuffer(FVulkanDevice* InDevice, uint64 TotalSize, VkFlags Usage, VkMemoryPropertyFlags MemPropertyFlags)
	: VulkanRHI::FDeviceChild(InDevice)
	, BufferSize(TotalSize)
	, BufferOffset(0)
	, BufferAddress(0)
	, MinAlignment(0)
{
	check(TotalSize <= (uint64)MAX_uint32);
	InDevice->GetMemoryManager().AllocateBufferPooled(Allocation, nullptr, TotalSize, 0, Usage, MemPropertyFlags, EVulkanAllocationMetaRingBuffer, __FILE__, __LINE__);
	MinAlignment = Allocation.GetBufferAlignment(Device);
	// Start by wrapping around to set up the correct fence
	BufferOffset = TotalSize;

	if (InDevice->GetOptionalExtensions().HasBufferDeviceAddress)
	{
		VkBufferDeviceAddressInfo BufferInfo;
		ZeroVulkanStruct(BufferInfo, VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
		BufferInfo.buffer = Allocation.GetBufferHandle();
		BufferAddress = VulkanRHI::vkGetBufferDeviceAddressKHR(Device->GetInstanceHandle(), &BufferInfo);
	}
}

FVulkanRingBuffer::~FVulkanRingBuffer()
{
	Device->GetMemoryManager().FreeVulkanAllocation(Allocation);
}

uint64 FVulkanRingBuffer::WrapAroundAllocateMemory(uint64 Size, uint32 Alignment, FVulkanCmdBuffer* InCmdBuffer)
{
	CA_ASSUME(InCmdBuffer != nullptr); // Suppress static analysis warning
	uint64 AllocationOffset = Align<uint64>(BufferOffset, Alignment);
	ensure(AllocationOffset + Size > BufferSize);

	// Check to see if we can wrap around the ring buffer
	if (FenceCmdBuffer)
	{
		if (FenceCounter == FenceCmdBuffer->GetFenceSignaledCounterI())
		{
			//if (FenceCounter == FenceCmdBuffer->GetSubmittedFenceCounter())
			{
				//UE_LOG(LogVulkanRHI, Error, TEXT("Ringbuffer overflow during the same cmd buffer!"));
			}
			//else
			{
				//UE_LOG(LogVulkanRHI, Error, TEXT("Wrapped around the ring buffer! Waiting for the GPU..."));
				//Device->GetImmediateContext().GetCommandBufferManager()->WaitForCmdBuffer(FenceCmdBuffer, 0.5f);
			}
		}
	}

	BufferOffset = Size;

	FenceCmdBuffer = InCmdBuffer;
	FenceCounter = InCmdBuffer->GetSubmittedFenceCounter();

	return 0;
}
