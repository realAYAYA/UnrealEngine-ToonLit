// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12UniformBuffer.cpp: D3D uniform buffer RHI implementation.
	=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "UniformBuffer.h"
#include "ShaderParameterStruct.h"
#include "RHIUniformBufferDataShared.h"

FUniformBufferRHIRef FD3D12DynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout* Layout, EUniformBufferUsage Usage, EUniformBufferValidation Validation)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12UpdateUniformBufferTime);

	if (Contents && Validation == EUniformBufferValidation::ValidateResources)
	{
		ValidateShaderParameterResourcesRHI(Contents, *Layout);
	}

	//Note: This is not overly efficient in the mGPU case (we create two+ upload locations) but the CPU savings of having no extra indirection to the resource are worth
	//      it in single node.
	// Create the uniform buffer
	FD3D12UniformBuffer* UniformBufferOut = GetAdapter().CreateLinkedObject<FD3D12UniformBuffer>(FRHIGPUMask::All(), [&](FD3D12Device* Device) -> FD3D12UniformBuffer*
	{
		// If NumBytesActualData == 0, this uniform buffer contains no constants, only a resource table.
		FD3D12UniformBuffer* NewUniformBuffer = new FD3D12UniformBuffer(Device, Layout, Usage);
		check(nullptr != NewUniformBuffer);

		const uint32 NumBytesActualData = Layout->ConstantBufferSize;
		if (NumBytesActualData > 0)
		{
			// Is this check really needed?
			check(Align(NumBytesActualData, 16) == NumBytesActualData);
			check(Align(Contents, 16) == Contents);
			check(NumBytesActualData <= D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16);

#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
			// Create an offline CBV descriptor
			NewUniformBuffer->View = new FD3D12ConstantBufferView(Device);
#endif

			// Uniform buffers can be created without contents and updated later.
			if (Contents)
			{
				void* MappedData = nullptr;
				if (Usage == EUniformBufferUsage::UniformBuffer_MultiFrame)
				{
					// Uniform buffers that live for multiple frames must use the more expensive and persistent allocation path
					FD3D12UploadHeapAllocator& Allocator = GetAdapter().GetUploadHeapAllocator(Device->GetGPUIndex());
					MappedData = Allocator.AllocUploadResource(NumBytesActualData, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, NewUniformBuffer->ResourceLocation);
				}
				else
				{
					// Uniform buffers which will live for 1 frame at the max can be allocated very efficiently from a ring buffer
					FD3D12FastConstantAllocator& Allocator = GetAdapter().GetTransientUniformBufferAllocator();
					MappedData = Allocator.Allocate(NumBytesActualData, NewUniformBuffer->ResourceLocation, nullptr);
				}
				check(NewUniformBuffer->ResourceLocation.GetOffsetFromBaseOfResource() % 16 == 0);

				// Copy the data to the upload heap
				check(MappedData != nullptr);

				UE::RHICore::UpdateUniformBufferConstants(MappedData, Contents, *Layout);

#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
				NewUniformBuffer->View->CreateView(&NewUniformBuffer->ResourceLocation, 0, NumBytesActualData);
#endif
			}
		}

		// The GPUVA is used to see if this uniform buffer contains constants or is just a resource table.
		check((Contents && NumBytesActualData > 0) ? (0 != NewUniformBuffer->ResourceLocation.GetGPUVirtualAddress()) : (0 == NewUniformBuffer->ResourceLocation.GetGPUVirtualAddress()));
		return NewUniformBuffer;
	});

	check(UniformBufferOut);

	if (Layout->Resources.Num())
	{
		const int32 NumResources = Layout->Resources.Num();

		for (FD3D12UniformBuffer& CurrentBuffer : *UniformBufferOut)
		{
			CurrentBuffer.GetResourceTable().SetNumZeroed(NumResources);

			if (Contents)
			{
				for (int32 Index = 0; Index < NumResources; ++Index)
				{
					CurrentBuffer.GetResourceTable()[Index] = GetShaderParameterResourceRHI(Contents, Layout->Resources[Index].MemberOffset, Layout->Resources[Index].MemberType);
				}
			}
		}
	}

	INC_MEMORY_STAT_BY(STAT_UniformBufferMemory, UniformBufferOut->ResourceLocation.GetSize());

	return UniformBufferOut;
}

FRHICOMMAND_MACRO(FRHICommandD3D12UpdateUniformBuffer)
{
	TRefCountPtr<FD3D12UniformBuffer> UniformBuffer;
	FD3D12ResourceLocation UpdatedLocation;
	TArrayView<FRHIResource*> UpdatedResources;

	FORCEINLINE_DEBUGGABLE FRHICommandD3D12UpdateUniformBuffer(FD3D12UniformBuffer* InUniformBuffer, FD3D12ResourceLocation& InUpdatedLocation, FRHIResource** InUpdatedResources, int32 InNumResources)
		: UniformBuffer(InUniformBuffer)
		, UpdatedLocation(InUpdatedLocation.GetParentDevice())
		, UpdatedResources(InUpdatedResources, InNumResources)
	{
		FD3D12ResourceLocation::TransferOwnership(UpdatedLocation, InUpdatedLocation);

		for (FRHIResource* Resource : UpdatedResources)
		{
			Resource->AddRef();
		}
	}

	~FRHICommandD3D12UpdateUniformBuffer()
	{
		for (FRHIResource* Resource : UpdatedResources)
		{
			Resource->Release();
		}
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		for (int32 i = 0; i < UpdatedResources.Num(); ++i)
		{
			//check(UniformBuffer->ResourceTable[i]);
			UniformBuffer->GetResourceTable()[i] = UpdatedResources[i];
			check(UniformBuffer->GetResourceTable()[i]);
		}
		FD3D12ResourceLocation::TransferOwnership(UniformBuffer->ResourceLocation, UpdatedLocation);
#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
		const uint32 NumBytes = Align(UniformBuffer->GetLayout().ConstantBufferSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		UniformBuffer->View->CreateView(&UniformBuffer->ResourceLocation, 0, NumBytes);
#endif
	}
};

void FD3D12DynamicRHI::RHIUpdateUniformBuffer(FRHICommandListBase& RHICmdList, FRHIUniformBuffer* UniformBufferRHI, const void* Contents)
{
	check(UniformBufferRHI);

	const FRHIUniformBufferLayout& Layout = UniformBufferRHI->GetLayout();
	ValidateShaderParameterResourcesRHI(Contents, Layout);

	const bool bBypass = RHICmdList.Bypass();

	FD3D12UniformBuffer* FirstUniformBuffer = ResourceCast(UniformBufferRHI);

	const uint32 NumBytes = Layout.ConstantBufferSize;
	const int32 NumResources = Layout.Resources.Num();

	FRHIResource** CmdListResources = nullptr;

	if (NumResources)
	{
		CmdListResources = bBypass
			? (FRHIResource**)FMemory_Alloca(sizeof(FRHIResource*) * NumResources) 
			: (FRHIResource**)RHICmdList.Alloc(sizeof(FRHIResource*) * NumResources, alignof(FRHIResource*));

		for (int32 Index = 0; Index < NumResources; ++Index)
		{
			const FRHIUniformBufferResource& Parameter = Layout.Resources[Index];
			CmdListResources[Index] = GetShaderParameterResourceRHI(Contents, Parameter.MemberOffset, Parameter.MemberType);
		}
	}

	// Update buffers on all GPUs by looping over FD3D12LinkedAdapterObject chain
	for (FD3D12UniformBuffer& UniformBuffer : *FirstUniformBuffer)
	{
		check(UniformBuffer.GetResourceTable().Num() == NumResources);

		FD3D12Device* Device = UniformBuffer.GetParentDevice();

		FD3D12ResourceLocation UpdatedResourceLocation(Device);

		if (NumBytes > 0)
		{
			void* MappedData = nullptr;

			if (UniformBuffer.UniformBufferUsage == UniformBuffer_MultiFrame)
			{
				FD3D12UploadHeapAllocator& Allocator = GetAdapter().GetUploadHeapAllocator(Device->GetGPUIndex());
				MappedData = Allocator.AllocUploadResource(NumBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, UpdatedResourceLocation);
			}
			else
			{
				FD3D12FastConstantAllocator& Allocator = GetAdapter().GetTransientUniformBufferAllocator();

				MappedData = Allocator.Allocate(NumBytes, UpdatedResourceLocation, nullptr);
			}

			check(MappedData != nullptr);
			UE::RHICore::UpdateUniformBufferConstants(MappedData, Contents, Layout);
		}

		if (bBypass)
		{
			FRHICommandD3D12UpdateUniformBuffer Cmd(&UniformBuffer, UpdatedResourceLocation, CmdListResources, NumResources);
			Cmd.Execute(RHICmdList);
		}
		else
		{
			new (RHICmdList.AllocCommand<FRHICommandD3D12UpdateUniformBuffer>()) FRHICommandD3D12UpdateUniformBuffer(&UniformBuffer, UpdatedResourceLocation, CmdListResources, NumResources);

			//fence is required to stop parallel recording threads from recording with the old bad state of the uniformbuffer resource table.  This command MUST execute before dependent recording starts.
			RHICmdList.RHIThreadFence(true);
		}
	}

}

FD3D12UniformBuffer::~FD3D12UniformBuffer()
{
	check(IsInRHIThread() || IsInRenderingThread());

	int64 BufferSize = ResourceLocation.GetSize();
	DEC_MEMORY_STAT_BY(STAT_UniformBufferMemory, BufferSize);

#if D3D12RHI_USE_CONSTANT_BUFFER_VIEWS
	delete View;
#endif
}
