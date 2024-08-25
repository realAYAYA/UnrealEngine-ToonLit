// Copyright Epic Games, Inc. All Rights Reserved.
#include "SkeletalMeshDeformerHelpers.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "SkeletalRenderGPUSkin.h"
#include "RenderingThread.h"

FRHIShaderResourceView* FSkeletalMeshDeformerHelpers::GetBoneBufferForReading(
	FSkeletalMeshObject const* InMeshObject,
	int32 InLodIndex,
	int32 InSectionIndex,
	bool bInPreviousFrame)
{
	if (InMeshObject->IsCPUSkinned())
	{
		return nullptr;
	}

	FSkeletalMeshObjectGPUSkin const* MeshObjectGPU = static_cast<FSkeletalMeshObjectGPUSkin const*>(InMeshObject);
	FGPUBaseSkinVertexFactory const* BaseVertexFactory = MeshObjectGPU->GetBaseSkinVertexFactory(InLodIndex, InSectionIndex);
	bool bHasBoneBuffer = BaseVertexFactory != nullptr && BaseVertexFactory->GetShaderData().HasBoneBufferForReading(bInPreviousFrame);
	if (!bHasBoneBuffer)
	{
		return nullptr;
	}

	return BaseVertexFactory->GetShaderData().GetBoneBufferForReading(bInPreviousFrame).VertexBufferSRV;
}

FRHIShaderResourceView* FSkeletalMeshDeformerHelpers::GetMorphTargetBufferForReading(
	FSkeletalMeshObject const* InMeshObject,
	int32 InLodIndex,
	int32 InSectionIndex,
	uint32 InFrameNumber,
	bool bInPreviousFrame)
{
	if (InMeshObject->IsCPUSkinned())
	{
		return nullptr;
	}

	FSkeletalMeshObjectGPUSkin const* MeshObjectGPU = static_cast<FSkeletalMeshObjectGPUSkin const*>(InMeshObject);
	FGPUBaseSkinVertexFactory const* BaseVertexFactory = MeshObjectGPU->GetBaseSkinVertexFactory(InLodIndex, InSectionIndex);
	FMorphVertexBuffer const* MorphVertexBuffer = BaseVertexFactory != nullptr ? BaseVertexFactory->GetMorphVertexBuffer(bInPreviousFrame) : nullptr;

	return MorphVertexBuffer != nullptr ? MorphVertexBuffer->GetSRV() : nullptr;
}

FSkeletalMeshDeformerHelpers::FClothBuffers FSkeletalMeshDeformerHelpers::GetClothBuffersForReading(
	FSkeletalMeshObject const* InMeshObject,
	int32 InLodIndex,
	int32 InSectionIndex,
	uint32 InFrameNumber,
	bool bInPreviousFrame)
{
	if (InMeshObject->IsCPUSkinned())
	{
		return FClothBuffers();
	}

	FSkeletalMeshObjectGPUSkin const* MeshObjectGPU = static_cast<FSkeletalMeshObjectGPUSkin const*>(InMeshObject);
	FGPUBaseSkinVertexFactory const* BaseVertexFactory = MeshObjectGPU->GetBaseSkinVertexFactory(InLodIndex, InSectionIndex);
	FGPUBaseSkinAPEXClothVertexFactory const* ClothVertexFactory = BaseVertexFactory != nullptr ? BaseVertexFactory->GetClothVertexFactory() : nullptr;

	if (ClothVertexFactory == nullptr || !ClothVertexFactory->GetClothShaderData().HasClothBufferForReading(bInPreviousFrame))
	{
		return FClothBuffers();
	}

	FSkeletalMeshRenderData const& SkeletalMeshRenderData = InMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = SkeletalMeshRenderData.GetPendingFirstLOD(InLodIndex);
	FSkelMeshRenderSection const& RenderSection = LodRenderData->RenderSections[InSectionIndex];

	FClothBuffers Ret;
	Ret.ClothInfluenceBuffer = ClothVertexFactory->GetClothBuffer();
	Ret.ClothInfluenceBufferOffset = ClothVertexFactory->GetClothIndexOffset(RenderSection.BaseVertexIndex);
	Ret.ClothSimulatedPositionAndNormalBuffer = ClothVertexFactory->GetClothShaderData().GetClothBufferForReading(bInPreviousFrame).VertexBufferSRV;
	Ret.ClothToLocal = ClothVertexFactory->GetClothShaderData().GetClothToLocalForReading(bInPreviousFrame);
	return Ret;
}

FRDGBuffer* FSkeletalMeshDeformerHelpers::AllocateVertexFactoryPositionBuffer(FRDGBuilder& GraphBuilder, FSkeletalMeshObject* InMeshObject, int32 InLodIndex, TCHAR const* InBufferName)
{
	if (InMeshObject->IsCPUSkinned())
	{
		return nullptr;
	}

	FSkeletalMeshRenderData const& SkeletalMeshRenderData = InMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = SkeletalMeshRenderData.GetPendingFirstLOD(InLodIndex);
	const int32 NumVertices = LodRenderData->GetNumVertices();

	FSkeletalMeshObjectGPUSkin* MeshObjectGPU = static_cast<FSkeletalMeshObjectGPUSkin*>(InMeshObject);
	FGPUBaseSkinVertexFactory const* BaseVertexFactory = MeshObjectGPU->GetBaseSkinVertexFactory(InLodIndex, 0);
	uint32 Frame = BaseVertexFactory->GetShaderData().UpdatedFrameNumber;

	FMeshDeformerGeometry& DeformerGeometry = MeshObjectGPU->GetDeformerGeometry(InLodIndex);

	FRDGBuffer* PositionBuffer = nullptr;
	if (DeformerGeometry.Position && Frame == DeformerGeometry.PositionUpdatedFrame)
	{
		PositionBuffer = GraphBuilder.RegisterExternalBuffer(DeformerGeometry.Position);
	}
	else
	{
		DeformerGeometry.PrevPosition = DeformerGeometry.Position;
		DeformerGeometry.PrevPositionSRV = DeformerGeometry.PositionSRV;

		const uint32 PosBufferBytesPerElement = 4;
		PositionBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(PosBufferBytesPerElement, NumVertices * 3), InBufferName, ERDGBufferFlags::None);
		DeformerGeometry.Position = GraphBuilder.ConvertToExternalBuffer(PositionBuffer);
		DeformerGeometry.PositionSRV = DeformerGeometry.Position->GetOrCreateSRV(GraphBuilder.RHICmdList, FRHIBufferSRVCreateInfo(PF_R32_FLOAT));
		DeformerGeometry.PositionUpdatedFrame = Frame;
		GraphBuilder.SetBufferAccessFinal(PositionBuffer, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask);

#if RHI_RAYTRACING
		// Update ray tracing geometry whenever we recreate the position buffer.
		FSkeletalMeshRenderData& SkelMeshRenderData = MeshObjectGPU->GetSkeletalMeshRenderData();
		FSkeletalMeshLODRenderData& LODModel = SkelMeshRenderData.LODRenderData[InLodIndex];

		const int32 NumSections = InMeshObject->GetRenderSections(InLodIndex).Num();

		TArray<FBufferRHIRef> VertexBuffers;
		VertexBuffers.SetNum(NumSections);
		for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			VertexBuffers[SectionIndex] = DeformerGeometry.Position->GetRHI();
		}

		MeshObjectGPU->UpdateRayTracingGeometry(GraphBuilder.RHICmdList, LODModel, InLodIndex, VertexBuffers);
#endif // RHI_RAYTRACING
	}

	return PositionBuffer;
}

FRDGBuffer* FSkeletalMeshDeformerHelpers::AllocateVertexFactoryTangentBuffer(FRDGBuilder& GraphBuilder, FSkeletalMeshObject* InMeshObject, int32 InLodIndex, TCHAR const* InBufferName)
{
	if (InMeshObject->IsCPUSkinned())
	{
		return nullptr;
	}

	FSkeletalMeshRenderData const& SkeletalMeshRenderData = InMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = SkeletalMeshRenderData.GetPendingFirstLOD(InLodIndex);
	const int32 NumVertices = LodRenderData->GetNumVertices();

	FSkeletalMeshObjectGPUSkin* MeshObjectGPU = static_cast<FSkeletalMeshObjectGPUSkin*>(InMeshObject);
	FGPUBaseSkinVertexFactory const* BaseVertexFactory = MeshObjectGPU->GetBaseSkinVertexFactory(InLodIndex, 0);
	uint32 Frame = BaseVertexFactory->GetShaderData().UpdatedFrameNumber;

	FMeshDeformerGeometry& DeformerGeometry = MeshObjectGPU->GetDeformerGeometry(InLodIndex);

	FRDGBuffer* TangentBuffer = nullptr;
	if (DeformerGeometry.Tangent && Frame == DeformerGeometry.TangentUpdatedFrame)
	{
		TangentBuffer = GraphBuilder.RegisterExternalBuffer(DeformerGeometry.Tangent);
	}
	else
	{
		const uint32 TangentBufferBytesPerElement = 8;
		TangentBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(TangentBufferBytesPerElement, NumVertices * 2), InBufferName, ERDGBufferFlags::None);
		DeformerGeometry.Tangent = GraphBuilder.ConvertToExternalBuffer(TangentBuffer);
		const EPixelFormat TangentsFormat = IsOpenGLPlatform(GMaxRHIShaderPlatform) ? PF_R16G16B16A16_SINT : PF_R16G16B16A16_SNORM;
		DeformerGeometry.TangentSRV = DeformerGeometry.Tangent->GetOrCreateSRV(GraphBuilder.RHICmdList, FRHIBufferSRVCreateInfo(TangentsFormat));
		DeformerGeometry.TangentUpdatedFrame = Frame;
		GraphBuilder.SetBufferAccessFinal(TangentBuffer, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask);
	}

	return TangentBuffer;
}

FRDGBuffer* FSkeletalMeshDeformerHelpers::AllocateVertexFactoryColorBuffer(FRDGBuilder& GraphBuilder, FSkeletalMeshObject* InMeshObject, int32 InLodIndex, TCHAR const* InBufferName)
{
	if (InMeshObject->IsCPUSkinned())
	{
		return nullptr;
	}

	FSkeletalMeshRenderData const& SkeletalMeshRenderData = InMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = SkeletalMeshRenderData.GetPendingFirstLOD(InLodIndex);
	const int32 NumVertices = LodRenderData->GetNumVertices();

	FSkeletalMeshObjectGPUSkin* MeshObjectGPU = static_cast<FSkeletalMeshObjectGPUSkin*>(InMeshObject);
	FGPUBaseSkinVertexFactory const* BaseVertexFactory = MeshObjectGPU->GetBaseSkinVertexFactory(InLodIndex, 0);
	uint32 Frame = BaseVertexFactory->GetShaderData().UpdatedFrameNumber;

	FMeshDeformerGeometry& DeformerGeometry = MeshObjectGPU->GetDeformerGeometry(InLodIndex);

	FRDGBuffer* ColorBuffer = nullptr;
	if (DeformerGeometry.Color && Frame == DeformerGeometry.ColorUpdatedFrame)
	{
		ColorBuffer = GraphBuilder.RegisterExternalBuffer(DeformerGeometry.Color);
	}
	else
	{
		const uint32 ColorBufferBytesPerElement = 4;
		ColorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(ColorBufferBytesPerElement, NumVertices), InBufferName, ERDGBufferFlags::None);
		DeformerGeometry.Color = GraphBuilder.ConvertToExternalBuffer(ColorBuffer);
		DeformerGeometry.ColorSRV = DeformerGeometry.Color->GetOrCreateSRV(GraphBuilder.RHICmdList, FRHIBufferSRVCreateInfo(PF_R8G8B8A8));
		DeformerGeometry.ColorUpdatedFrame = Frame;
		GraphBuilder.SetBufferAccessFinal(ColorBuffer, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask);
	}

	return ColorBuffer;
}

void FSkeletalMeshDeformerHelpers::UpdateVertexFactoryBufferOverrides(FSkeletalMeshObject* InMeshObject, int32 InLodIndex)
{
	UpdateVertexFactoryBufferOverrides(FRHICommandListImmediate::Get(), InMeshObject, InLodIndex);
}

void FSkeletalMeshDeformerHelpers::UpdateVertexFactoryBufferOverrides(FRHICommandListBase& RHICmdList, FSkeletalMeshObject* InMeshObject, int32 InLodIndex)
{
	if (InMeshObject->IsCPUSkinned())
	{
		return;
	}

	FSkeletalMeshObjectGPUSkin* MeshObjectGPU = static_cast<FSkeletalMeshObjectGPUSkin*>(InMeshObject);
	FMeshDeformerGeometry& DeformerGeometry = MeshObjectGPU->GetDeformerGeometry(InLodIndex);
	
	FGPUSkinPassthroughVertexFactory::FAddVertexAttributeDesc Desc;
	Desc.FrameNumber = DeformerGeometry.PositionUpdatedFrame;

	if (DeformerGeometry.PositionSRV)
	{
		Desc.VertexAttributes.Add(FGPUSkinPassthroughVertexFactory::VertexPosition);
		Desc.SRVs[FGPUSkinPassthroughVertexFactory::Position] = DeformerGeometry.PositionSRV;
		Desc.SRVs[FGPUSkinPassthroughVertexFactory::PreviousPosition] = DeformerGeometry.PrevPositionSRV;
	}
	if (DeformerGeometry.TangentSRV)
	{
		Desc.VertexAttributes.Add(FGPUSkinPassthroughVertexFactory::VertexTangent);
		Desc.SRVs[FGPUSkinPassthroughVertexFactory::Tangent] = DeformerGeometry.TangentSRV;
	}
	if (DeformerGeometry.ColorSRV)
	{
		Desc.VertexAttributes.Add(FGPUSkinPassthroughVertexFactory::VertexColor);
		Desc.SRVs[FGPUSkinPassthroughVertexFactory::Color] = DeformerGeometry.ColorSRV;
	}

	if (Desc.VertexAttributes.Num() == 0)
	{
		return;
	}

	const FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD& LOD = MeshObjectGPU->LODs[InLodIndex];
	const int32 NumSections = InMeshObject->GetRenderSections(InLodIndex).Num();
	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		FGPUBaseSkinVertexFactory const* BaseVertexFactory = MeshObjectGPU->GetBaseSkinVertexFactory(InLodIndex, SectionIndex);
		FGPUSkinPassthroughVertexFactory* TargetVertexFactory = LOD.GPUSkinVertexFactories.PassthroughVertexFactories[SectionIndex].Get();
		TargetVertexFactory->SetVertexAttributes(RHICmdList, BaseVertexFactory, Desc);
	}
}

void FSkeletalMeshDeformerHelpers::ResetVertexFactoryBufferOverrides(FSkeletalMeshObject* InMeshObject, int32 LODIndex)
{
	if (InMeshObject->IsCPUSkinned())
	{
		return;
	}

	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

	FSkeletalMeshObjectGPUSkin* MeshObjectGPU = static_cast<FSkeletalMeshObjectGPUSkin*>(InMeshObject);
	FMeshDeformerGeometry& DeformerGeometry = MeshObjectGPU->GetDeformerGeometry(LODIndex);

	// This can be called per frame even when already reset. So early out if we don't need to do anything.
	const bool bIsReset = DeformerGeometry.PositionUpdatedFrame == 0 && DeformerGeometry.TangentUpdatedFrame == 0 && DeformerGeometry.ColorUpdatedFrame == 0;
	if (bIsReset)
	{
		return;
	}

	// Reset stored buffers.
	DeformerGeometry.Reset();

	// Reset vertex factories.
	const FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD& LOD = MeshObjectGPU->LODs[LODIndex];
	const int32 NumSections = InMeshObject->GetRenderSections(LODIndex).Num();
	for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		FGPUBaseSkinVertexFactory const* BaseVertexFactory = MeshObjectGPU->GetBaseSkinVertexFactory(LODIndex, SectionIndex);
		FGPUSkinPassthroughVertexFactory* TargetVertexFactory = LOD.GPUSkinVertexFactories.PassthroughVertexFactories[SectionIndex].Get();
		TargetVertexFactory->ResetVertexAttributes();
		FGPUSkinPassthroughVertexFactory::FDataType Data;
		BaseVertexFactory->CopyDataTypeForLocalVertexFactory(Data);
		TargetVertexFactory->SetData(RHICmdList, Data);
	}

#if RHI_RAYTRACING
	// Reset ray tracing geometry.
	FSkeletalMeshRenderData& SkelMeshRenderData = MeshObjectGPU->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData& LODModel = SkelMeshRenderData.LODRenderData[LODIndex];
	FBufferRHIRef VertexBuffer = LODModel.StaticVertexBuffers.PositionVertexBuffer.VertexBufferRHI;

	TArray<FBufferRHIRef> VertexBuffers;
	VertexBuffers.Init(VertexBuffer, NumSections);
	MeshObjectGPU->UpdateRayTracingGeometry(RHICmdList, LODModel, LODIndex, VertexBuffers);
#endif // RHI_RAYTRACING
}
