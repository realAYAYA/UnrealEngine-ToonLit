// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11IndexBuffer.cpp: D3D Index buffer RHI implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"

TAutoConsoleVariable<int32> GCVarUseSharedKeyedMutex(
	TEXT("r.D3D11.UseSharedKeyMutex"),
	0,
	TEXT("If 1, BUF_Shared vertex / index buffer and TexCreate_Shared texture will be created\n")
	TEXT("with the D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX flag instead of D3D11_RESOURCE_MISC_SHARED (default).\n"),
	ECVF_Default);

FD3D11Buffer::~FD3D11Buffer()
{
	D3D11BufferStats::UpdateBufferStats(*this, false);
}

void FD3D11Buffer::TakeOwnership(FD3D11Buffer& Other)
{
	FRHIBuffer::TakeOwnership(Other);
	Resource = MoveTemp(Other.Resource);
}

void FD3D11Buffer::ReleaseOwnership()
{
	FRHIBuffer::ReleaseOwnership();

	D3D11BufferStats::UpdateBufferStats(*this, false);
	Resource = nullptr;
}

FBufferRHIRef FD3D11DynamicRHI::RHICreateBuffer(FRHICommandListBase& RHICmdList, FRHIBufferDesc const& BufferDesc, ERHIAccess ResourceState, FRHIResourceCreateInfo& CreateInfo)
{
	if (BufferDesc.IsNull())
	{
		return new FD3D11Buffer(nullptr, BufferDesc);
	}

	// Explicitly check that the size is nonzero before allowing CreateBuffer to opaquely fail.
	checkf(BufferDesc.Size > 0, TEXT("Attempt to create buffer '%s' with size 0."), CreateInfo.DebugName ? CreateInfo.DebugName : TEXT("(null)"));

	// Describe the buffer.
	D3D11_BUFFER_DESC Desc{};
	Desc.ByteWidth = BufferDesc.Size;

	if (EnumHasAnyFlags(BufferDesc.Usage, BUF_AnyDynamic))
	{
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}

	if (EnumHasAnyFlags(BufferDesc.Usage, BUF_VertexBuffer))
	{
		Desc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
	}

	if (EnumHasAnyFlags(BufferDesc.Usage, BUF_IndexBuffer))
	{
		Desc.BindFlags |= D3D11_BIND_INDEX_BUFFER;
	}

	if (EnumHasAnyFlags(BufferDesc.Usage, BUF_ByteAddressBuffer))
	{
		Desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	}
	else if (EnumHasAnyFlags(BufferDesc.Usage, BUF_StructuredBuffer))
	{
		Desc.StructureByteStride = BufferDesc.Stride;
		Desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	}

	if (EnumHasAnyFlags(BufferDesc.Usage, BUF_ShaderResource))
	{
		Desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}

	if (EnumHasAnyFlags(BufferDesc.Usage, BUF_UnorderedAccess))
	{
		Desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	if (EnumHasAnyFlags(BufferDesc.Usage, BUF_DrawIndirect))
	{
		Desc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
	}

	if (EnumHasAnyFlags(BufferDesc.Usage, BUF_Shared))
	{
		if (GCVarUseSharedKeyedMutex->GetInt() != 0)
		{
			Desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
		}
		else
		{
			Desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
		}
	}

	// If a resource array was provided for the resource, create the resource pre-populated
	D3D11_SUBRESOURCE_DATA InitData;
	D3D11_SUBRESOURCE_DATA* pInitData = NULL;
	if(CreateInfo.ResourceArray)
	{
		check(BufferDesc.Size == CreateInfo.ResourceArray->GetResourceDataSize());
		InitData.pSysMem = CreateInfo.ResourceArray->GetResourceData();
		InitData.SysMemPitch = BufferDesc.Size;
		InitData.SysMemSlicePitch = 0;
		pInitData = &InitData;
	}

	TRefCountPtr<ID3D11Buffer> BufferResource;
	{
		HRESULT hr = Direct3DDevice->CreateBuffer(&Desc, pInitData, BufferResource.GetInitReference());
		if (FAILED(hr))
		{
			UE_LOG(LogD3D11RHI, Error, TEXT("Failed to create buffer '%s' with ByteWidth=%u, Usage=%d, BindFlags=0x%x, CPUAccessFlags=0x%x, MiscFlags=0x%x, StructureByteStride=%u, InitData=0x%p"),
				CreateInfo.DebugName ? CreateInfo.DebugName : TEXT(""), Desc.ByteWidth, Desc.Usage, Desc.BindFlags, Desc.CPUAccessFlags, Desc.MiscFlags, Desc.StructureByteStride, pInitData);
			VerifyD3D11Result(hr, "CreateBuffer", __FILE__, __LINE__, Direct3DDevice);
		}
	}

	if (CreateInfo.DebugName)
	{
		BufferResource->SetPrivateData(WKPDID_D3DDebugObjectName, FCString::Strlen(CreateInfo.DebugName) + 1, TCHAR_TO_ANSI(CreateInfo.DebugName));
	}

	if (CreateInfo.DebugName)
	{
		BufferResource->SetPrivateData(WKPDID_D3DDebugObjectName, FCString::Strlen(CreateInfo.DebugName) + 1, TCHAR_TO_ANSI(CreateInfo.DebugName));
	}

	if (CreateInfo.ResourceArray)
	{
		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	FD3D11Buffer* NewBuffer = new FD3D11Buffer(BufferResource, BufferDesc);
	if (CreateInfo.DebugName)
	{
		NewBuffer->SetName(CreateInfo.DebugName);
	}

	D3D11BufferStats::UpdateBufferStats(*NewBuffer, true);

	return NewBuffer;
}

void* FD3D11DynamicRHI::LockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* BufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	FD3D11Buffer* Buffer = ResourceCast(BufferRHI);
	// If this resource is bound to the device, unbind it
	ConditionalClearShaderResource(Buffer, true);

	// Determine whether the buffer is dynamic or not.
	D3D11_BUFFER_DESC Desc;
	Buffer->Resource->GetDesc(&Desc);
	const bool bIsDynamic = (Desc.Usage == D3D11_USAGE_DYNAMIC);

	FD3D11LockedKey LockedKey(Buffer->Resource);
	FD3D11LockedData LockedData;

	if(bIsDynamic)
	{
		check(LockMode == RLM_WriteOnly || LockMode == RLM_WriteOnly_NoOverwrite);

		// If the buffer is dynamic, map its memory for writing.
		D3D11_MAPPED_SUBRESOURCE MappedSubresource;

		D3D11_MAP MapType = (LockMode == RLM_WriteOnly || !GRHISupportsMapWriteNoOverwrite) ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
		VERIFYD3D11RESULT_EX(Direct3DDeviceIMContext->Map(Buffer->Resource, 0, MapType, 0, &MappedSubresource), Direct3DDevice);

		LockedData.SetData(MappedSubresource.pData);
		LockedData.Pitch = MappedSubresource.RowPitch;
	}
	else
	{
		if(LockMode == RLM_ReadOnly)
		{
			// If the static buffer is being locked for reading, create a staging buffer.
			D3D11_BUFFER_DESC StagingBufferDesc;
			ZeroMemory( &StagingBufferDesc, sizeof( D3D11_BUFFER_DESC ) );
			StagingBufferDesc.ByteWidth = Size;
			StagingBufferDesc.Usage = D3D11_USAGE_STAGING;
			StagingBufferDesc.BindFlags = 0;
			StagingBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			StagingBufferDesc.MiscFlags = 0;
			TRefCountPtr<ID3D11Buffer> StagingBuffer;
			VERIFYD3D11RESULT_EX(Direct3DDevice->CreateBuffer(&StagingBufferDesc, NULL, StagingBuffer.GetInitReference()), Direct3DDevice);
			LockedData.StagingResource = StagingBuffer;

			// Copy the contents of the buffer to the staging buffer.
			D3D11_BOX SourceBox;
			SourceBox.left = Offset;
			SourceBox.right = Offset + Size;
			SourceBox.top = SourceBox.front = 0;
			SourceBox.bottom = SourceBox.back = 1;
			Direct3DDeviceIMContext->CopySubresourceRegion(StagingBuffer, 0, 0, 0, 0, Buffer->Resource, 0, &SourceBox);

			// Map the staging buffer's memory for reading.
			D3D11_MAPPED_SUBRESOURCE MappedSubresource;
			VERIFYD3D11RESULT_EX(Direct3DDeviceIMContext->Map(StagingBuffer, 0, D3D11_MAP_READ, 0, &MappedSubresource), Direct3DDevice);
			LockedData.SetData(MappedSubresource.pData);
			LockedData.Pitch = MappedSubresource.RowPitch;
			Offset = 0;
		}
		else
		{
			// If the static buffer is being locked for writing, allocate memory for the contents to be written to.
			LockedData.AllocData(Desc.ByteWidth);
			LockedData.Pitch = Desc.ByteWidth;
		}
	}
	
	// Add the lock to the lock map.
	AddLockedData(LockedKey, LockedData);

	// Return the offset pointer
	return (void*)((uint8*)LockedData.GetData() + Offset);
}

void FD3D11DynamicRHI::UnlockBuffer_BottomOfPipe(FRHICommandListBase& RHICmdList, FRHIBuffer* BufferRHI)
{
	FD3D11Buffer* Buffer = ResourceCast(BufferRHI);

	// Determine whether the buffer is dynamic or not.
	D3D11_BUFFER_DESC Desc;
	Buffer->Resource->GetDesc(&Desc);
	const bool bIsDynamic = (Desc.Usage == D3D11_USAGE_DYNAMIC);

	// Find the outstanding lock for this buffer and remove it from the tracker.
	FD3D11LockedData LockedData;
	verifyf(RemoveLockedData(FD3D11LockedKey(Buffer->Resource), LockedData), TEXT("Buffer is not locked"));

	if(bIsDynamic)
	{
		// If the buffer is dynamic, its memory was mapped directly; unmap it.
		Direct3DDeviceIMContext->Unmap(Buffer->Resource, 0);
	}
	else
	{
		// If the static buffer lock involved a staging resource, it was locked for reading.
		if(LockedData.StagingResource)
		{
			// Unmap the staging buffer's memory.
			ID3D11Buffer* StagingBuffer = (ID3D11Buffer*)LockedData.StagingResource.GetReference();
			Direct3DDeviceIMContext->Unmap(StagingBuffer,0);
		}
		else 
		{
			// Copy the contents of the temporary memory buffer allocated for writing into the buffer.
			Direct3DDeviceIMContext->UpdateSubresource(Buffer->Resource, 0, NULL, LockedData.GetData(), LockedData.Pitch, 0);

			// Free the temporary memory buffer.
			LockedData.FreeData();
		}
	}
}

void FD3D11DynamicRHI::RHICopyBuffer(FRHIBuffer* SourceBufferRHI, FRHIBuffer* DestBufferRHI)
{
	FD3D11Buffer* SourceBuffer = ResourceCast(SourceBufferRHI);
	FD3D11Buffer* DestBuffer = ResourceCast(DestBufferRHI);

	D3D11_BUFFER_DESC SourceBufferDesc;
	SourceBuffer->Resource->GetDesc(&SourceBufferDesc);
	
	D3D11_BUFFER_DESC DestBufferDesc;
	DestBuffer->Resource->GetDesc(&DestBufferDesc);

	check(SourceBufferDesc.ByteWidth == DestBufferDesc.ByteWidth);

	Direct3DDeviceIMContext->CopyResource(DestBuffer->Resource,SourceBuffer->Resource);

	GPUProfilingData.RegisterGPUWork(1);
}

void FD3D11DynamicRHI::RHITransferBufferUnderlyingResource(FRHICommandListBase& RHICmdList, FRHIBuffer* DestBuffer, FRHIBuffer* SrcBuffer)
{
	FD3D11Buffer* Dst = ResourceCast(DestBuffer);
	FD3D11Buffer* Src = ResourceCast(SrcBuffer);

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

void FD3D11DynamicRHI::RHIBindDebugLabelName(FRHICommandListBase& RHICmdList, FRHIBuffer* BufferRHI, const TCHAR* Name)
{
	check(BufferRHI);
	FName DebugName(Name);
	BufferRHI->SetName(DebugName);
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	FD3D11Buffer* BufferD3D = ResourceCast(BufferRHI);
	if (BufferD3D->Resource != nullptr)
	{
		BufferD3D->Resource->SetPrivateData(WKPDID_D3DDebugObjectName, FCString::Strlen(Name) + 1, TCHAR_TO_ANSI(Name));
	}
#endif
}