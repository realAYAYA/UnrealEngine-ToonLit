// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFBufferAdapter.h"
#include "Converters/GLTFBufferUtility.h"

#include "Rendering/ColorVertexBuffer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "RawIndexBuffer.h"
#include "RHI.h"
#include "RHIResources.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"

class FGLTFBufferAdapterCPU final : public IGLTFBufferAdapter
{
public:

	FGLTFBufferAdapterCPU(const void* Data)
		: Data(Data)
	{
	}

	virtual const uint8* GetData() override
	{
		return static_cast<const uint8*>(Data);
	}

	const void* Data;
};

class FGLTFBufferAdapterGPU final : public IGLTFBufferAdapter
{
public:

	FGLTFBufferAdapterGPU(FRHIBuffer* RHIBuffer)
	{
		FGLTFBufferUtility::ReadRHIBuffer(RHIBuffer, DataBuffer);
	}

	virtual const uint8* GetData() override
	{
		return DataBuffer.Num() > 0 ? DataBuffer.GetData() : nullptr;
	}

	TArray<uint8> DataBuffer;
};

TUniquePtr<IGLTFBufferAdapter> IGLTFBufferAdapter::GetIndices(const FRawStaticIndexBuffer* IndexBuffer)
{
	const void* IndexData = FGLTFBufferUtility::GetCPUBuffer(IndexBuffer);
	if (IndexData != nullptr && FGLTFBufferUtility::HasCPUAccess(IndexBuffer)) return MakeUnique<FGLTFBufferAdapterCPU>(IndexData);
	return MakeUnique<FGLTFBufferAdapterGPU>(IndexBuffer->IndexBufferRHI);
}

TUniquePtr<IGLTFBufferAdapter> IGLTFBufferAdapter::GetIndices(const FRawStaticIndexBuffer16or32Interface* IndexBuffer)
{
	const void* IndexData =FGLTFBufferUtility::GetCPUBuffer(IndexBuffer);
	if (IndexData != nullptr && FGLTFBufferUtility::HasCPUAccess(IndexBuffer)) return MakeUnique<FGLTFBufferAdapterCPU>(IndexData);
	return MakeUnique<FGLTFBufferAdapterGPU>(IndexBuffer->IndexBufferRHI);
}

TUniquePtr<IGLTFBufferAdapter> IGLTFBufferAdapter::GetPositions(const FPositionVertexBuffer* VertexBuffer)
{
	const void* PositionData = const_cast<FPositionVertexBuffer*>(VertexBuffer)->GetVertexData();
	if (PositionData != nullptr && FGLTFBufferUtility::HasCPUAccess(VertexBuffer)) return MakeUnique<FGLTFBufferAdapterCPU>(PositionData);
	return MakeUnique<FGLTFBufferAdapterGPU>(VertexBuffer->VertexBufferRHI);
}

TUniquePtr<IGLTFBufferAdapter> IGLTFBufferAdapter::GetColors(const FColorVertexBuffer* VertexBuffer)
{
	const void* ColorData = const_cast<FColorVertexBuffer*>(VertexBuffer)->GetVertexData();
	if (ColorData != nullptr && FGLTFBufferUtility::HasCPUAccess(VertexBuffer)) return MakeUnique<FGLTFBufferAdapterCPU>(ColorData);
	return MakeUnique<FGLTFBufferAdapterGPU>(VertexBuffer->VertexBufferRHI);
}

TUniquePtr<IGLTFBufferAdapter> IGLTFBufferAdapter::GetTangents(const FStaticMeshVertexBuffer* VertexBuffer)
{
	const void* TangentData = const_cast<FStaticMeshVertexBuffer*>(VertexBuffer)->GetTangentData();
	if (TangentData != nullptr && FGLTFBufferUtility::HasCPUAccess(VertexBuffer)) return MakeUnique<FGLTFBufferAdapterCPU>(TangentData);
	return MakeUnique<FGLTFBufferAdapterGPU>(VertexBuffer->TangentsVertexBuffer.VertexBufferRHI);
}

TUniquePtr<IGLTFBufferAdapter> IGLTFBufferAdapter::GetUVs(const FStaticMeshVertexBuffer* VertexBuffer)
{
	const void* UVData = const_cast<FStaticMeshVertexBuffer*>(VertexBuffer)->GetTexCoordData();
	if (UVData != nullptr && FGLTFBufferUtility::HasCPUAccess(VertexBuffer)) return MakeUnique<FGLTFBufferAdapterCPU>(UVData);
	return MakeUnique<FGLTFBufferAdapterGPU>(VertexBuffer->TexCoordVertexBuffer.VertexBufferRHI);
}

TUniquePtr<IGLTFBufferAdapter> IGLTFBufferAdapter::GetInfluences(const FSkinWeightVertexBuffer* VertexBuffer)
{
	const FSkinWeightDataVertexBuffer* InfluenceBuffer = VertexBuffer->GetDataVertexBuffer();
	const void* InfluenceData = FGLTFBufferUtility::GetCPUBuffer(InfluenceBuffer);
	if (InfluenceData != nullptr && FGLTFBufferUtility::HasCPUAccess(VertexBuffer)) return MakeUnique<FGLTFBufferAdapterCPU>(InfluenceData);
	return MakeUnique<FGLTFBufferAdapterGPU>(InfluenceBuffer->VertexBufferRHI);
}

TUniquePtr<IGLTFBufferAdapter> IGLTFBufferAdapter::GetLookups(const FSkinWeightVertexBuffer* VertexBuffer)
{
	const FSkinWeightLookupVertexBuffer* LookupBuffer = VertexBuffer->GetLookupVertexBuffer();
	const void* LookupData = FGLTFBufferUtility::GetCPUBuffer(LookupBuffer);
	if (LookupData != nullptr && FGLTFBufferUtility::HasCPUAccess(VertexBuffer)) return MakeUnique<FGLTFBufferAdapterCPU>(LookupData);
	return MakeUnique<FGLTFBufferAdapterGPU>(LookupBuffer->VertexBufferRHI);
}
