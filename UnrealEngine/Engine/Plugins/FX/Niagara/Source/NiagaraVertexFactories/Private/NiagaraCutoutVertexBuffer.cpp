// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NiagaraCutoutVertexBuffer.h: Niagara cutout uv buffer implementation.
=============================================================================*/

#include "NiagaraCutoutVertexBuffer.h"

FNiagaraCutoutVertexBuffer::FNiagaraCutoutVertexBuffer(int32 ZeroInitCount)
{
	if (ZeroInitCount > 0)
	{
		Data.AddZeroed(ZeroInitCount);
	}
}

void FNiagaraCutoutVertexBuffer::InitRHI()
{
	if (Data.Num())
	{
		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo(TEXT("FNiagaraCutoutVertexBuffer"));

		const int32 DataSize = Data.Num() * sizeof(FVector2f);
		VertexBufferRHI = RHICreateBuffer(DataSize, BUF_Static | BUF_VertexBuffer | BUF_ShaderResource, 0, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
		void* BufferData = RHILockBuffer(VertexBufferRHI, 0, DataSize, RLM_WriteOnly);
		FMemory::Memcpy(BufferData, Data.GetData(), DataSize);
		RHIUnlockBuffer(VertexBufferRHI);
		VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, sizeof(FVector2f), PF_G32R32F);

		Data.Empty();
	}
}

void FNiagaraCutoutVertexBuffer::ReleaseRHI()
{
	VertexBufferSRV.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

TGlobalResource<FNiagaraCutoutVertexBuffer> GFNiagaraNullCutoutVertexBuffer(4);

