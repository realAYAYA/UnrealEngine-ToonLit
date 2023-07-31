// Copyright Epic Games, Inc. All Rights Reserved.
#include "SkeletalMeshDeformerHelpers.h"

#include "GPUSkinVertexFactory.h"
#include "SkeletalRenderGPUSkin.h"

FRHIShaderResourceView* FSkeletalMeshDeformerHelpers::GetBoneBufferForReading(
	FSkeletalMeshObject const* MeshObject,
	int32 LODIndex,
	int32 SectionIndex,
	bool bPreviousFrame)
{
	if (MeshObject->IsCPUSkinned())
	{
		return nullptr;
	}

	FSkeletalMeshObjectGPUSkin const* MeshObjectGPU = static_cast<FSkeletalMeshObjectGPUSkin const*>(MeshObject);
	FGPUBaseSkinVertexFactory const* BaseVertexFactory = MeshObjectGPU->GetBaseSkinVertexFactory(LODIndex, SectionIndex);
	bool bHasBoneBuffer = BaseVertexFactory != nullptr && BaseVertexFactory->GetShaderData().HasBoneBufferForReading(bPreviousFrame);
	if (!bHasBoneBuffer)
	{
		return nullptr;
	}

	return BaseVertexFactory->GetShaderData().GetBoneBufferForReading(bPreviousFrame).VertexBufferSRV;
}

FRHIShaderResourceView* FSkeletalMeshDeformerHelpers::GetMorphTargetBufferForReading(
	FSkeletalMeshObject const* MeshObject,
	int32 LODIndex,
	int32 SectionIndex,
	uint32 FrameNumber,
	bool bPreviousFrame)
{
	if (MeshObject->IsCPUSkinned())
	{
		return nullptr;
	}

	FSkeletalMeshObjectGPUSkin const* MeshObjectGPU = static_cast<FSkeletalMeshObjectGPUSkin const*>(MeshObject);
	FGPUBaseSkinVertexFactory const* BaseVertexFactory = MeshObjectGPU->GetBaseSkinVertexFactory(LODIndex, SectionIndex);
	FMorphVertexBuffer const* MorphVertexBuffer = BaseVertexFactory != nullptr ? BaseVertexFactory->GetMorphVertexBuffer(bPreviousFrame, FrameNumber) : nullptr;

	return MorphVertexBuffer != nullptr ? MorphVertexBuffer->GetSRV() : nullptr;
}

FSkeletalMeshDeformerHelpers::FClothBuffers FSkeletalMeshDeformerHelpers::GetClothBuffersForReading(
	FSkeletalMeshObject const* MeshObject,
	int32 LODIndex,
	int32 SectionIndex,
	uint32 FrameNumber,
	bool bPreviousFrame)
{
	if (MeshObject->IsCPUSkinned())
	{
		return FClothBuffers();
	}

	FSkeletalMeshObjectGPUSkin const* MeshObjectGPU = static_cast<FSkeletalMeshObjectGPUSkin const*>(MeshObject);
	FGPUBaseSkinVertexFactory const* BaseVertexFactory = MeshObjectGPU->GetBaseSkinVertexFactory(LODIndex, SectionIndex);
	FGPUBaseSkinAPEXClothVertexFactory const* ClothVertexFactory = BaseVertexFactory != nullptr ? BaseVertexFactory->GetClothVertexFactory() : nullptr;

	if (ClothVertexFactory == nullptr || !ClothVertexFactory->GetClothShaderData().HasClothBufferForReading(bPreviousFrame, FrameNumber))
	{
		return FClothBuffers();
	}

	FSkeletalMeshRenderData const& SkeletalMeshRenderData = MeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = SkeletalMeshRenderData.GetPendingFirstLOD(LODIndex);
	FSkelMeshRenderSection const& RenderSection = LodRenderData->RenderSections[SectionIndex];

	FClothBuffers Ret;
	Ret.ClothInfluenceBuffer = ClothVertexFactory->GetClothBuffer();
	Ret.ClothInfluenceBufferOffset = ClothVertexFactory->GetClothIndexOffset(RenderSection.BaseVertexIndex);
	Ret.ClothSimulatedPositionAndNormalBuffer = ClothVertexFactory->GetClothShaderData().GetClothBufferForReading(bPreviousFrame, FrameNumber).VertexBufferSRV;
	Ret.ClothToLocal = ClothVertexFactory->GetClothShaderData().GetClothToLocalForReading(bPreviousFrame, FrameNumber);
	return Ret;
}

void FSkeletalMeshDeformerHelpers::SetVertexFactoryBufferOverrides(
	FSkeletalMeshObject* MeshObject,
	int32 LODIndex,
	EOverrideType OverrideType,
	TRefCountPtr<FRDGPooledBuffer> const& PositionBuffer,
	TRefCountPtr<FRDGPooledBuffer> const& TangentBuffer,
	TRefCountPtr<FRDGPooledBuffer> const& ColorBuffer)
{
	if (MeshObject->IsCPUSkinned())
	{
		return;
	}

	FGPUSkinPassthroughVertexFactory::EOverrideFlags OverrideFlags = FGPUSkinPassthroughVertexFactory::EOverrideFlags::All;
	if (OverrideType == EOverrideType::Partial)
	{
		OverrideFlags = FGPUSkinPassthroughVertexFactory::EOverrideFlags::None;
 		OverrideFlags |= (PositionBuffer.IsValid() ? FGPUSkinPassthroughVertexFactory::EOverrideFlags::Position : FGPUSkinPassthroughVertexFactory::EOverrideFlags::None);
 		OverrideFlags |= (TangentBuffer.IsValid() ? FGPUSkinPassthroughVertexFactory::EOverrideFlags::Tangent : FGPUSkinPassthroughVertexFactory::EOverrideFlags::None);
 		OverrideFlags |= (ColorBuffer.IsValid() ? FGPUSkinPassthroughVertexFactory::EOverrideFlags::Color : FGPUSkinPassthroughVertexFactory::EOverrideFlags::None);
	}

	FSkeletalMeshObjectGPUSkin* MeshObjectGPU = static_cast<FSkeletalMeshObjectGPUSkin*>(MeshObject);
	const FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD& LOD = MeshObjectGPU->LODs[LODIndex];
	
	const int32 NumSections = MeshObject->GetRenderSections(LODIndex).Num();
	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		FGPUBaseSkinVertexFactory const* BaseVertexFactory = MeshObjectGPU->GetBaseSkinVertexFactory(LODIndex, SectionIndex);
		FGPUSkinPassthroughVertexFactory* TargetVertexFactory = LOD.GPUSkinVertexFactories.PassthroughVertexFactories[SectionIndex].Get();
		TargetVertexFactory->InvalidateStreams();
		TargetVertexFactory->UpdateVertexDeclaration(OverrideFlags, BaseVertexFactory, PositionBuffer, TangentBuffer, ColorBuffer);
	}
}

void FSkeletalMeshDeformerHelpers::ResetVertexFactoryBufferOverrides_GameThread(FSkeletalMeshObject* MeshObject, int32 LODIndex)
{
	if (MeshObject->IsCPUSkinned())
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(ResetSkinPassthroughVertexFactory)([MeshObject, LODIndex](FRHICommandList& CmdList)
	{
		SetVertexFactoryBufferOverrides(MeshObject, LODIndex, EOverrideType::All, nullptr, nullptr, nullptr);
	});
}

#if RHI_RAYTRACING

void FSkeletalMeshDeformerHelpers::UpdateRayTracingGeometry(
	FSkeletalMeshObject* MeshObject,
	int32 LODIndex,
	TRefCountPtr<FRDGPooledBuffer> const& PositionBuffer)
{
	if (MeshObject->IsCPUSkinned())
	{
		return;
	}

	FSkeletalMeshObjectGPUSkin* MeshObjectGPU = static_cast<FSkeletalMeshObjectGPUSkin*>(MeshObject);
	FSkeletalMeshRenderData& SkelMeshRenderData = MeshObjectGPU->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData& LODModel = SkelMeshRenderData.LODRenderData[LODIndex];

	const int32 NumSections = MeshObject->GetRenderSections(LODIndex).Num();

	TArray<FBufferRHIRef> VertexBufffers;
	VertexBufffers.SetNum(NumSections);
	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		VertexBufffers[SectionIndex] = PositionBuffer->GetRHI();
	}

	MeshObjectGPU->UpdateRayTracingGeometry(LODModel, LODIndex, VertexBufffers);
}

#endif // RHI_RAYTRACING
