// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFBufferUtility.h"
#include "Rendering/SkeletalMeshRenderData.h"

bool FGLTFBufferUtility::HasCPUAccess(const FRawStaticIndexBuffer* IndexBuffer)
{
#if WITH_EDITOR
	return true;
#else
	struct FRawStaticIndexBufferHack : FIndexBuffer
	{
		TResourceArray<uint8, INDEXBUFFER_ALIGNMENT> IndexStorage;
		int32 CachedNumIndices;
		bool b32Bit;
		bool bShouldExpandTo32Bit;
	};

	static_assert(sizeof(FRawStaticIndexBufferHack) == sizeof(FRawStaticIndexBuffer), "FRawStaticIndexBufferHack memory layout doesn't match FRawStaticIndexBuffer");
	return reinterpret_cast<const FRawStaticIndexBufferHack*>(IndexBuffer)->IndexStorage.GetAllowCPUAccess();
#endif
}

bool FGLTFBufferUtility::HasCPUAccess(const FRawStaticIndexBuffer16or32Interface* IndexBuffer)
{
#if WITH_EDITOR
	return true;
#else
	return IndexBuffer->GetNeedsCPUAccess();
#endif
}

bool FGLTFBufferUtility::HasCPUAccess(const FPositionVertexBuffer* VertexBuffer)
{
#if WITH_EDITOR
	return true;
#else
	struct FPositionVertexBufferHack : FVertexBuffer
	{
		FShaderResourceViewRHIRef PositionComponentSRV;
		TMemoryImagePtr<FStaticMeshVertexDataInterface> VertexData;
		uint8* Data;
		uint32 Stride;
		uint32 NumVertices;
		bool bNeedsCPUAccess;
	};

	static_assert(sizeof(FPositionVertexBufferHack) == sizeof(FPositionVertexBuffer), "FPositionVertexBufferHack memory layout doesn't match FPositionVertexBuffer");
	const FStaticMeshVertexDataInterface* VertexData = reinterpret_cast<const FPositionVertexBufferHack*>(VertexBuffer)->VertexData;
	return VertexData != nullptr && VertexData->GetAllowCPUAccess();
#endif
}

bool FGLTFBufferUtility::HasCPUAccess(const FColorVertexBuffer* VertexBuffer)
{
#if WITH_EDITOR
	return true;
#else
	struct FColorVertexBufferHack : FVertexBuffer
	{
		FStaticMeshVertexDataInterface* VertexData;
		FShaderResourceViewRHIRef ColorComponentsSRV;
		uint8* Data;
		uint32 Stride;
		uint32 NumVertices;
		bool NeedsCPUAccess;
	};

	static_assert(sizeof(FColorVertexBufferHack) == sizeof(FColorVertexBuffer), "FColorVertexBufferHack memory layout doesn't match FColorVertexBuffer");
	const FStaticMeshVertexDataInterface* VertexData = reinterpret_cast<const FColorVertexBufferHack*>(VertexBuffer)->VertexData;
	return VertexData != nullptr && VertexData->GetAllowCPUAccess();
#endif
}

bool FGLTFBufferUtility::HasCPUAccess(const FStaticMeshVertexBuffer* VertexBuffer)
{
#if WITH_EDITOR
	return true;
#else
	return const_cast<FStaticMeshVertexBuffer*>(VertexBuffer)->GetAllowCPUAccess();
#endif
}

bool FGLTFBufferUtility::HasCPUAccess(const FSkinWeightVertexBuffer* VertexBuffer)
{
#if WITH_EDITOR
	return true;
#else
	return VertexBuffer->GetNeedsCPUAccess();
#endif
}

const void* FGLTFBufferUtility::GetCPUBuffer(const FRawStaticIndexBuffer* IndexBuffer)
{
	return IndexBuffer->Is32Bit() ? static_cast<const void*>(IndexBuffer->AccessStream32()) : static_cast<const void*>(IndexBuffer->AccessStream16());
}

const void* FGLTFBufferUtility::GetCPUBuffer(const FRawStaticIndexBuffer16or32Interface* IndexBuffer)
{
	return IndexBuffer->GetResourceDataSize() > 0 ? const_cast<FRawStaticIndexBuffer16or32Interface*>(IndexBuffer)->GetPointerTo(0) : nullptr;
}

const void* FGLTFBufferUtility::GetCPUBuffer(const FSkinWeightDataVertexBuffer* VertexBuffer)
{
	return VertexBuffer->GetWeightData();
}

const void* FGLTFBufferUtility::GetCPUBuffer(const FSkinWeightLookupVertexBuffer* VertexBuffer)
{
	struct FSkinWeightLookupVertexBufferHack : FVertexBuffer
	{
		FShaderResourceViewRHIRef SRVValue;
		bool bNeedsCPUAccess;
		FStaticMeshVertexDataInterface* LookupData;
		uint8* Data;
		uint32 NumVertices;
	};

	static_assert(sizeof(FSkinWeightLookupVertexBufferHack) == sizeof(FSkinWeightLookupVertexBuffer), "FSkinWeightLookupVertexBufferHack memory layout doesn't match FSkinWeightLookupVertexBuffer");
	return reinterpret_cast<const FSkinWeightLookupVertexBufferHack*>(VertexBuffer)->Data;
}

void FGLTFBufferUtility::ReadRHIBuffer(FRHIBuffer* SourceBuffer, TArray<uint8>& OutData)
{
	OutData.Empty();

	if (SourceBuffer == nullptr)
	{
		return;
	}

	const uint32 NumBytes = SourceBuffer->GetSize();
	if (NumBytes == 0)
	{
		return;
	}

	const EBufferUsageFlags Usage = SourceBuffer->GetUsage();
	if ((Usage & BUF_Static) != BUF_Static)
	{
		return; // Some RHI implementations only support reading static buffers
	}

	OutData.AddUninitialized(NumBytes);
	void *DstData = OutData.GetData();

	ENQUEUE_RENDER_COMMAND(ReadRHIBuffer)(
		[SourceBuffer, NumBytes, DstData](FRHICommandListImmediate& RHICmdList)
		{
			const void* SrcData = RHICmdList.LockBuffer(SourceBuffer, 0, NumBytes, RLM_ReadOnly);
			FMemory::Memcpy(DstData, SrcData, NumBytes);
			RHICmdList.UnlockBuffer(SourceBuffer);
		}
	);

	FlushRenderingCommands();
}
