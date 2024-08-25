// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenMeshCards.h"
#include "RendererPrivate.h"
#include "MeshCardRepresentation.h"
#include "ComponentRecreateRenderStateContext.h"
#include "LumenHeightfields.h"
#include "MeshCardBuild.h"
#include "InstanceDataSceneProxy.h"

TAutoConsoleVariable<float> CVarLumenMeshCardsMinSize(
	TEXT("r.LumenScene.SurfaceCache.MeshCardsMinSize"),
	10.0f,
	TEXT("Minimum mesh cards world space size to be included in Lumen Scene."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenMeshCardsMergeComponents = 1;
FAutoConsoleVariableRef CVarLumenMeshCardsMergeComponents(
	TEXT("r.LumenScene.SurfaceCache.MeshCardsMergeComponents"),
	GLumenMeshCardsMergeComponents,
	TEXT("Whether to merge all components with the same RayTracingGroupId into a single MeshCards."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenMeshCardsMergeInstances = 0;
FAutoConsoleVariableRef CVarLumenMeshCardsMergeInstances(
	TEXT("r.LumenScene.SurfaceCache.MeshCardsMergeInstances"),
	GLumenMeshCardsMergeInstances,
	TEXT("Whether to merge all instances of a Instanced Static Mesh Component into a single MeshCards."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenMeshCardsMergedCardMinSurfaceArea = 0.05f;
FAutoConsoleVariableRef CVarLumenMeshCardsMergedCardMinSurfaceArea(
	TEXT("r.LumenScene.SurfaceCache.MeshCardsMergedCardMinSurfaceArea"),
	GLumenMeshCardsMergedCardMinSurfaceArea,
	TEXT("Minimum area to spawn a merged card."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenMeshCardsMergeInstancesMaxSurfaceAreaRatio = 1.7f;
FAutoConsoleVariableRef CVarLumenMeshCardsMergeInstancesMaxSurfaceAreaRatio(
	TEXT("r.LumenScene.SurfaceCache.MeshCardsMergeInstancesMaxSurfaceAreaRatio"),
	GLumenMeshCardsMergeInstancesMaxSurfaceAreaRatio,
	TEXT("Only merge if the (combined box surface area) / (summed instance box surface area) < MaxSurfaceAreaRatio"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenMeshCardsMergedResolutionScale = .3f;
FAutoConsoleVariableRef CVarLumenMeshCardsMergedResolutionScale(
	TEXT("r.LumenScene.SurfaceCache.MeshCardsMergedResolutionScale"),
	GLumenMeshCardsMergedResolutionScale,
	TEXT("Scale on the resolution calculation for a merged MeshCards.  This compensates for the merged box getting a higher resolution assigned due to being closer to the viewer."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenMeshCardsMergedMaxWorldSize = 10000.0f;
FAutoConsoleVariableRef CVarLumenMeshCardsMergedMaxWorldSize(
	TEXT("r.LumenScene.SurfaceCache.MeshCardsMergedMaxWorldSize"),
	GLumenMeshCardsMergedMaxWorldSize,
	TEXT("Only merged bounds less than this size on any axis are considered, since Lumen Scene streaming relies on object granularity."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenMeshCardsCullFaces = 1;
FAutoConsoleVariableRef CVarLumenMeshCardsCullFaces(
	TEXT("r.LumenScene.SurfaceCache.MeshCardsCullFaces"),
	GLumenMeshCardsCullFaces,
	TEXT(""),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenMeshCardsDebugSingleCard = -1;
FAutoConsoleVariableRef CVarLumenMeshCardsDebugSingleCard(
	TEXT("r.LumenScene.SurfaceCache.MeshCardsDebugSingleCard"),
	GLumenMeshCardsDebugSingleCard,
	TEXT("Spawn only a specified card on mesh. Useful for debugging."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenSurfaceCacheHeightfieldCaptureMargin(
	TEXT("r.Lumen.SurfaceCache.HeightfieldCaptureMargin"),
	100.0f,
	TEXT("Amount to expand heightfield component bbox for card capture purposes."),
	ECVF_RenderThreadSafe
);

extern int32 GLumenSceneUploadEveryFrame;

namespace LumenMeshCards
{
	FVector3f GetAxisAlignedDirection(uint32 AxisAlignedDirectionIndex);
};

FVector3f LumenMeshCards::GetAxisAlignedDirection(uint32 AxisAlignedDirectionIndex)
{
	const uint32 AxisIndex = AxisAlignedDirectionIndex / 2;

	FVector3f Direction(0.0f, 0.0f, 0.0f);
	Direction[AxisIndex] = AxisAlignedDirectionIndex & 1 ? 1.0f : -1.0f;
	return Direction;
}

float LumenMeshCards::GetCardMinSurfaceArea(bool bEmissiveLightSource)
{
	const float MeshCardsMinSize = CVarLumenMeshCardsMinSize.GetValueOnRenderThread();
	return MeshCardsMinSize * MeshCardsMinSize * (bEmissiveLightSource ? 0.2f : 1.0f);
}

class FLumenCardGPUData
{
public:
	// Must match usf
	enum { DataStrideInFloat4s = 10 };
	enum { DataStrideInBytes = DataStrideInFloat4s * sizeof(FVector4f) };

	static void PackSurfaceMipMap(const FLumenCard& Card, int32 ResLevel, uint32& PackedSizeInPages, uint32& PackedPageTableOffset)
	{
		PackedSizeInPages = 0;
		PackedPageTableOffset = 0;

		if (Card.IsAllocated())
		{
			const FLumenSurfaceMipMap& MipMap = Card.GetMipMap(ResLevel);

			if (MipMap.IsAllocated())
			{
				PackedSizeInPages = MipMap.SizeInPagesX | (MipMap.SizeInPagesY << 16);
				PackedPageTableOffset = MipMap.PageTableSpanOffset;
			}
		}
	}

	static void FillData(const FLumenCard& RESTRICT Card, const FLumenPrimitiveGroup* InPrimitiveGroup, FVector4f* RESTRICT OutData)
	{
		// Note: layout must match GetLumenCardData in usf

		const FDFVector3 WorldPosition(Card.WorldOBB.Origin);

		OutData[0] = WorldPosition.High;
		OutData[1] = FVector4f(Card.WorldOBB.AxisX[0], Card.WorldOBB.AxisY[0], Card.WorldOBB.AxisZ[0], WorldPosition.Low.X);
		OutData[2] = FVector4f(Card.WorldOBB.AxisX[1], Card.WorldOBB.AxisY[1], Card.WorldOBB.AxisZ[1], WorldPosition.Low.Y);
		OutData[3] = FVector4f(Card.WorldOBB.AxisX[2], Card.WorldOBB.AxisY[2], Card.WorldOBB.AxisZ[2], WorldPosition.Low.Z);

		const FIntPoint ResLevelBias = Card.ResLevelToResLevelXYBias();
		const uint32 LightingChannelMask = InPrimitiveGroup ? InPrimitiveGroup->LightingChannelMask : UINT32_MAX;

		uint32 Packed3W = 0;
		Packed3W = uint8(ResLevelBias.X) & 0xFF;
		Packed3W |= (uint8(ResLevelBias.Y) & 0xFF) << 8;
		Packed3W |= (uint8(Card.AxisAlignedDirectionIndex) & 0xF) << 16;
		Packed3W |= (LightingChannelMask & 0xF) << 20;
		Packed3W |= Card.bVisible && Card.IsAllocated() ? (1 << 24) : 0;
		Packed3W |= Card.bHeightfield && Card.IsAllocated() ? (1 << 25) : 0;

		OutData[4] = FVector4f(Card.WorldOBB.Extent.X, Card.WorldOBB.Extent.Y, Card.WorldOBB.Extent.Z, 0.0f);
		OutData[4].W = *((float*)&Packed3W);

		// Map low-res level for diffuse
		uint32 PackedSizeInPages = 0;
		uint32 PackedPageTableOffset = 0;
		PackSurfaceMipMap(Card, Card.MinAllocatedResLevel, PackedSizeInPages, PackedPageTableOffset);

		// Map hi-res for specular
		uint32 PackedHiResSizeInPages = 0;
		uint32 PackedHiResPageTableOffset = 0;
		PackSurfaceMipMap(Card, Card.MaxAllocatedResLevel, PackedHiResSizeInPages, PackedHiResPageTableOffset);

		OutData[5].X = *((float*)&PackedSizeInPages);
		OutData[5].Y = *((float*)&PackedPageTableOffset);
		OutData[5].Z = *((float*)&PackedHiResSizeInPages);
		OutData[5].W = *((float*)&PackedHiResPageTableOffset);

		float AverageTexelSize = 100.0f;
		if (Card.IsAllocated())
		{
			FLumenMipMapDesc MipMapDesc;
			Card.GetMipMapDesc(Card.MinAllocatedResLevel, MipMapDesc);
			AverageTexelSize = 0.5f * (Card.MeshCardsOBB.Extent.X / MipMapDesc.Resolution.X + Card.MeshCardsOBB.Extent.Y / MipMapDesc.Resolution.Y);
		}

		OutData[6] = FVector4f(Card.MeshCardsOBB.AxisX[0], Card.MeshCardsOBB.AxisY[0], Card.MeshCardsOBB.AxisZ[0], Card.MeshCardsOBB.Origin.X);
		OutData[7] = FVector4f(Card.MeshCardsOBB.AxisX[1], Card.MeshCardsOBB.AxisY[1], Card.MeshCardsOBB.AxisZ[1], Card.MeshCardsOBB.Origin.Y);
		OutData[8] = FVector4f(Card.MeshCardsOBB.AxisX[2], Card.MeshCardsOBB.AxisY[2], Card.MeshCardsOBB.AxisZ[2], Card.MeshCardsOBB.Origin.Z);
		OutData[9] = FVector4f(Card.MeshCardsOBB.Extent, AverageTexelSize);

		static_assert(DataStrideInFloat4s == 10, "Data stride doesn't match");
	}
};

struct FLumenMeshCardsGPUData
{
	// Must match LUMEN_MESH_CARDS_DATA_STRIDE in LumenCardCommon.ush
	enum { DataStrideInFloat4s = 6 };
	enum { DataStrideInBytes = DataStrideInFloat4s * 16 };

	static void FillData(const class FLumenMeshCards& RESTRICT MeshCards, FVector4f* RESTRICT OutData);
};

void FLumenMeshCardsGPUData::FillData(const FLumenMeshCards& RESTRICT MeshCards, FVector4f* RESTRICT OutData)
{
	// Note: layout must match GetLumenMeshCardsData in usf

	const FDFVector3 WorldOrigin(MeshCards.LocalToWorld.GetOrigin());

	OutData[0] = WorldOrigin.High;
	OutData[1] = FVector4f(FVector4(MeshCards.WorldToLocalRotation.GetScaledAxis(EAxis::X), WorldOrigin.Low.X));
	OutData[2] = FVector4f(FVector4(MeshCards.WorldToLocalRotation.GetScaledAxis(EAxis::Y), WorldOrigin.Low.Y));
	OutData[3] = FVector4f(FVector4(MeshCards.WorldToLocalRotation.GetScaledAxis(EAxis::Z), WorldOrigin.Low.Z));

	uint32 PackedData[4];
	PackedData[0] = MeshCards.FirstCardIndex;
	PackedData[1] = MeshCards.NumCards & 0xFFFF;
	PackedData[1] |= MeshCards.bHeightfield ? 0x10000 : 0;
	PackedData[1] |= MeshCards.bMostlyTwoSided ? 0x20000 : 0;
	PackedData[2] = MeshCards.CardLookup[0];
	PackedData[3] = MeshCards.CardLookup[1];
	OutData[4] = *(FVector4f*)&PackedData;

	PackedData[0] = MeshCards.CardLookup[2];
	PackedData[1] = MeshCards.CardLookup[3];
	PackedData[2] = MeshCards.CardLookup[4];
	PackedData[3] = MeshCards.CardLookup[5];
	OutData[5] = *(FVector4f*)&PackedData;

	static_assert(DataStrideInFloat4s == 6, "Data stride doesn't match");
}

struct FLumenPrimitiveGroupGPUData
{
	// Must match LUMEN_PRIMITIVE_GROUP_DATA_STRIDE in LumenScene.usf
	enum { DataStrideInFloat4s = 2 };
	enum { DataStrideInBytes = DataStrideInFloat4s * 16 };

	static void FillData(const class FLumenPrimitiveGroup& RESTRICT PrimitiveGroup, FVector4f* RESTRICT OutData);
};

void FLumenPrimitiveGroupGPUData::FillData(const FLumenPrimitiveGroup& RESTRICT PrimitiveGroup, FVector4f* RESTRICT OutData)
{
	// Note: layout must match GetLumenPrimitiveGroupData in usf

	OutData[0] = PrimitiveGroup.WorldSpaceBoundingBox.GetCenter();
	OutData[1] = PrimitiveGroup.WorldSpaceBoundingBox.GetExtent();

	uint32 MeshCardsIndex = PrimitiveGroup.MeshCardsIndex >= 0 ? PrimitiveGroup.MeshCardsIndex : UINT32_MAX;
	OutData[0].W = *((float*) &MeshCardsIndex);

	uint32 PackedFlags = 0;
	PackedFlags |= PrimitiveGroup.bValidMeshCards		? 0x01 : 0;
	PackedFlags |= PrimitiveGroup.bFarField				? 0x02 : 0;
	PackedFlags |= PrimitiveGroup.bHeightfield			? 0x04 : 0;
	PackedFlags |= PrimitiveGroup.bEmissiveLightSource	? 0x08 : 0;
	OutData[1].W = *((float*) &PackedFlags);

	static_assert(DataStrideInFloat4s == 2, "Data stride doesn't match");
}

void UpdateLumenMeshCards(FRDGBuilder& GraphBuilder, const FScene& Scene, const FDistanceFieldSceneData& DistanceFieldSceneData, FLumenSceneFrameTemporaries& FrameTemporaries, FLumenSceneData& LumenSceneData)
{
	LLM_SCOPE_BYTAG(Lumen);
	QUICK_SCOPE_CYCLE_COUNTER(UpdateLumenMeshCards);

	extern int32 GLumenSceneUploadEveryFrame;
	if (GLumenSceneUploadEveryFrame)
	{
		LumenSceneData.HeightfieldIndicesToUpdateInBuffer.Reset();
		for (int32 i = 0; i < LumenSceneData.Heightfields.Num(); ++i)
		{
			LumenSceneData.HeightfieldIndicesToUpdateInBuffer.Add(i);
		}

		LumenSceneData.MeshCardsIndicesToUpdateInBuffer.Reset();
		for (int32 i = 0; i < LumenSceneData.MeshCards.Num(); ++i)
		{
			LumenSceneData.MeshCardsIndicesToUpdateInBuffer.Add(i);
		}

		LumenSceneData.PrimitiveGroupIndicesToUpdateInBuffer.Reset();
		for (int32 i = 0; i < LumenSceneData.PrimitiveGroups.Num(); ++i)
		{
			LumenSceneData.PrimitiveGroupIndicesToUpdateInBuffer.Add(i);
		}
	}

	// Upload primitive groups
	{
		QUICK_SCOPE_CYCLE_COUNTER(UpdatePrimitiveGroups);

		const uint32 NumPrimitiveGroups = LumenSceneData.PrimitiveGroups.Num();
		const uint32 PrimitiveGroupNumFloat4s = FMath::RoundUpToPowerOfTwo(NumPrimitiveGroups * FLumenPrimitiveGroupGPUData::DataStrideInFloat4s);
		const uint32 PrimitiveGroupNumBytes = PrimitiveGroupNumFloat4s * sizeof(FVector4f);
		FRDGBuffer* PrimitiveGroupBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, LumenSceneData.PrimitiveGroupBuffer, PrimitiveGroupNumBytes, TEXT("Lumen.PrimitiveGroup"));
		FrameTemporaries.PrimitiveGroupBufferSRV = GraphBuilder.CreateSRV(PrimitiveGroupBuffer);

		const int32 NumPrimitiveGroupUploads = LumenSceneData.PrimitiveGroupIndicesToUpdateInBuffer.Num();

		if (NumPrimitiveGroupUploads > 0)
		{
			FLumenPrimitiveGroup NullPrimitiveGroup;
			NullPrimitiveGroup.WorldSpaceBoundingBox.Min = FVector3f(0.0f, 0.0f, 0.0f);
			NullPrimitiveGroup.WorldSpaceBoundingBox.Max = FVector3f(0.0f, 0.0f, 0.0f);

			LumenSceneData.PrimitiveGroupUploadBuffer.Init(GraphBuilder, NumPrimitiveGroupUploads, FLumenPrimitiveGroupGPUData::DataStrideInBytes, true, TEXT("Lumen.PrimitiveGroupUpload"));

			for (int32 Index : LumenSceneData.PrimitiveGroupIndicesToUpdateInBuffer)
			{
				if (Index < LumenSceneData.PrimitiveGroups.Num())
				{
					const FLumenPrimitiveGroup& PrimitiveGroup = LumenSceneData.PrimitiveGroups.IsAllocated(Index) ? LumenSceneData.PrimitiveGroups[Index] : NullPrimitiveGroup;

					FVector4f* Data = (FVector4f*)LumenSceneData.PrimitiveGroupUploadBuffer.Add_GetRef(Index);
					FLumenPrimitiveGroupGPUData::FillData(PrimitiveGroup, Data);
				}
			}

			LumenSceneData.PrimitiveGroupUploadBuffer.ResourceUploadTo(GraphBuilder, PrimitiveGroupBuffer);
		}
	}

	// Upload MeshCards
	{
		QUICK_SCOPE_CYCLE_COUNTER(UpdateMeshCards);

		const uint32 NumMeshCards = LumenSceneData.MeshCards.Num();
		const uint32 MeshCardsNumFloat4s = FMath::RoundUpToPowerOfTwo(NumMeshCards * FLumenMeshCardsGPUData::DataStrideInFloat4s);
		const uint32 MeshCardsNumBytes = MeshCardsNumFloat4s * sizeof(FVector4f);
		FRDGBuffer* MeshCardsBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, LumenSceneData.MeshCardsBuffer, MeshCardsNumBytes, TEXT("Lumen.MeshCards"));
		FrameTemporaries.MeshCardsBufferSRV = GraphBuilder.CreateSRV(MeshCardsBuffer);

		const int32 NumMeshCardsUploads = LumenSceneData.MeshCardsIndicesToUpdateInBuffer.Num();

		if (NumMeshCardsUploads > 0)
		{
			FLumenMeshCards NullMeshCards;
			LumenSceneData.MeshCardsUploadBuffer.Init(GraphBuilder, NumMeshCardsUploads, FLumenMeshCardsGPUData::DataStrideInBytes, true, TEXT("Lumen.MeshCardsUpload"));

			for (int32 Index : LumenSceneData.MeshCardsIndicesToUpdateInBuffer)
			{
				if (Index < LumenSceneData.MeshCards.Num())
				{
					const FLumenMeshCards& MeshCards = LumenSceneData.MeshCards.IsAllocated(Index) ? LumenSceneData.MeshCards[Index] : NullMeshCards;

					FVector4f* Data = (FVector4f*)LumenSceneData.MeshCardsUploadBuffer.Add_GetRef(Index);
					FLumenMeshCardsGPUData::FillData(MeshCards, Data);
				}
			}

			LumenSceneData.MeshCardsUploadBuffer.ResourceUploadTo(GraphBuilder, MeshCardsBuffer);
		}
	}

	// Upload Heightfields
	{
		QUICK_SCOPE_CYCLE_COUNTER(UpdateHeightfields);

		const uint32 NumHeightfields = LumenSceneData.Heightfields.Num();
		const uint32 HeightfieldsNumFloat4s = FMath::RoundUpToPowerOfTwo(NumHeightfields * FLumenHeightfieldGPUData::DataStrideInFloat4s);
		const uint32 HeightfieldsNumBytes = HeightfieldsNumFloat4s * sizeof(FVector4f);
		FRDGBuffer* HeightfieldBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, LumenSceneData.HeightfieldBuffer, HeightfieldsNumBytes, TEXT("Lumen.Heightfield"));
		FrameTemporaries.HeightfieldBufferSRV = GraphBuilder.CreateSRV(HeightfieldBuffer);

		const int32 NumHeightfieldsUploads = LumenSceneData.HeightfieldIndicesToUpdateInBuffer.Num();

		if (NumHeightfieldsUploads > 0)
		{
			FLumenHeightfield NullHeightfield;

			LumenSceneData.HeightfieldUploadBuffer.Init(GraphBuilder, NumHeightfieldsUploads, FLumenHeightfieldGPUData::DataStrideInBytes, true, TEXT("Lumen.HeightfieldUpload"));

			for (int32 Index : LumenSceneData.HeightfieldIndicesToUpdateInBuffer)
			{
				if (Index < LumenSceneData.Heightfields.Num())
				{
					const FLumenHeightfield& Heightfield = LumenSceneData.Heightfields.IsAllocated(Index) ? LumenSceneData.Heightfields[Index] : NullHeightfield;

					FVector4f* Data = (FVector4f*)LumenSceneData.HeightfieldUploadBuffer.Add_GetRef(Index);
					FLumenHeightfieldGPUData::FillData(Heightfield, LumenSceneData.MeshCards, Data);
				}
			}

			LumenSceneData.HeightfieldUploadBuffer.ResourceUploadTo(GraphBuilder, HeightfieldBuffer);
		}
	}

	// Upload SceneInstanceIndexToMeshCardsIndexBuffer
	{
		QUICK_SCOPE_CYCLE_COUNTER(UpdateSceneInstanceIndexToMeshCardsIndexBuffer);

		if (GLumenSceneUploadEveryFrame)
		{
			LumenSceneData.PrimitivesToUpdateMeshCards.Reset();

			for (int32 PrimitiveIndex = 0; PrimitiveIndex < Scene.Primitives.Num(); ++PrimitiveIndex)
			{
				LumenSceneData.PrimitivesToUpdateMeshCards.Add(PrimitiveIndex);
			}
		}

		const int32 NumIndices = FMath::Max(FMath::RoundUpToPowerOfTwo(Scene.GPUScene.GetInstanceIdUpperBoundGPU()), 1024u);
		const uint32 IndexSizeInBytes = GPixelFormats[PF_R32_UINT].BlockBytes;
		const uint32 IndicesSizeInBytes = NumIndices * IndexSizeInBytes;
		FRDGBuffer* SceneInstanceIndexToMeshCardsIndexBuffer = ResizeByteAddressBufferIfNeeded(GraphBuilder, LumenSceneData.SceneInstanceIndexToMeshCardsIndexBuffer, IndicesSizeInBytes, TEXT("Lumen.SceneInstanceIndexToMeshCardsIndexBuffer"));
		FrameTemporaries.SceneInstanceIndexToMeshCardsIndexBufferSRV = GraphBuilder.CreateSRV(SceneInstanceIndexToMeshCardsIndexBuffer);

		uint32 NumIndexUploads = 0;

		for (int32 PrimitiveIndex : LumenSceneData.PrimitivesToUpdateMeshCards)
		{
			if (PrimitiveIndex < Scene.Primitives.Num())
			{
				const FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene.Primitives[PrimitiveIndex];
				NumIndexUploads += PrimitiveSceneInfo->GetNumInstanceSceneDataEntries();
			}
		}

		if (NumIndexUploads > 0)
		{
			LumenSceneData.SceneInstanceIndexToMeshCardsIndexUploadBuffer.Init(GraphBuilder, NumIndexUploads, IndexSizeInBytes, false, TEXT("Lumen.SceneInstanceIndexToMeshCardsIndexUploadBuffer"));

			for (int32 PrimitiveIndex : LumenSceneData.PrimitivesToUpdateMeshCards)
			{
				if (PrimitiveIndex < Scene.Primitives.Num())
				{
					const FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene.Primitives[PrimitiveIndex];
					const int32 NumInstances = PrimitiveSceneInfo->GetNumInstanceSceneDataEntries();
					const int32 InstanceDataOffset = PrimitiveSceneInfo->GetInstanceSceneDataOffset();

					for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
					{
						const int32 MeshCardsIndex = LumenSceneData.GetMeshCardsIndex(PrimitiveSceneInfo, InstanceIndex);

						int32 DestIndex = InstanceDataOffset + InstanceIndex;
						if (DestIndex < NumIndices)
						{
							LumenSceneData.SceneInstanceIndexToMeshCardsIndexUploadBuffer.Add(DestIndex, &MeshCardsIndex);
						}
					}
				}
			}

			LumenSceneData.SceneInstanceIndexToMeshCardsIndexUploadBuffer.ResourceUploadTo(GraphBuilder, SceneInstanceIndexToMeshCardsIndexBuffer);
		}
	}
}

void Lumen::UpdateCardSceneBuffer(FRDGBuilder& GraphBuilder, FLumenSceneFrameTemporaries& FrameTemporaries, const FSceneViewFamily& ViewFamily, FScene* Scene)
{
	LLM_SCOPE_BYTAG(Lumen);

	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateCardSceneBuffer);
	QUICK_SCOPE_CYCLE_COUNTER(UpdateCardSceneBuffer);
	RDG_EVENT_SCOPE(GraphBuilder, "UpdateCardSceneBuffer");
	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(*ViewFamily.Views[0]);

	// CardBuffer
	{
		FRDGBuffer* CardBuffer = nullptr;

		{
			const int32 NumCardEntries = LumenSceneData.Cards.Num();
			const uint32 CardSceneNumFloat4s = NumCardEntries * FLumenCardGPUData::DataStrideInFloat4s;
			const uint32 CardSceneNumBytes = FMath::DivideAndRoundUp(CardSceneNumFloat4s, 16384u) * 16384 * sizeof(FVector4f);
			CardBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, LumenSceneData.CardBuffer, FMath::RoundUpToPowerOfTwo(CardSceneNumFloat4s) * sizeof(FVector4f), TEXT("Lumen.Cards"));
			FrameTemporaries.CardBufferSRV = GraphBuilder.CreateSRV(CardBuffer);
		}

		if (GLumenSceneUploadEveryFrame)
		{
			LumenSceneData.CardIndicesToUpdateInBuffer.Reset();

			for (int32 i = 0; i < LumenSceneData.Cards.Num(); i++)
			{
				LumenSceneData.CardIndicesToUpdateInBuffer.Add(i);
			}
		}

		const int32 NumCardDataUploads = LumenSceneData.CardIndicesToUpdateInBuffer.Num();

		if (NumCardDataUploads > 0)
		{
			FLumenCard NullCard;

			LumenSceneData.CardUploadBuffer.Init(GraphBuilder, NumCardDataUploads, FLumenCardGPUData::DataStrideInBytes, true, TEXT("Lumen.CardUploadBuffer"));

			for (int32 Index : LumenSceneData.CardIndicesToUpdateInBuffer)
			{
				if (Index < LumenSceneData.Cards.Num())
				{
					const FLumenCard& Card = LumenSceneData.Cards.IsAllocated(Index) ? LumenSceneData.Cards[Index] : NullCard;

					FLumenPrimitiveGroup* PrimitiveGroup = nullptr;
					if (Card.MeshCardsIndex >= 0)
					{
						const FLumenMeshCards& MeshCardsInstance = LumenSceneData.MeshCards[Card.MeshCardsIndex];
						if (MeshCardsInstance.PrimitiveGroupIndex >= 0)
						{
							PrimitiveGroup = &LumenSceneData.PrimitiveGroups[MeshCardsInstance.PrimitiveGroupIndex];
						}
					}

					FVector4f* Data = (FVector4f*)LumenSceneData.CardUploadBuffer.Add_GetRef(Index);
					FLumenCardGPUData::FillData(Card, PrimitiveGroup, Data);
				}
			}

			LumenSceneData.CardUploadBuffer.ResourceUploadTo(GraphBuilder, CardBuffer);
		}
	}

	UpdateLumenMeshCards(GraphBuilder, *Scene, Scene->DistanceFieldSceneData, FrameTemporaries, LumenSceneData);
}

int32 FLumenSceneData::GetMeshCardsIndex(const FPrimitiveSceneInfo* PrimitiveSceneInfo, int32 InstanceIndex) const
{
	if (PrimitiveSceneInfo->LumenPrimitiveGroupIndices.Num() > 0)
	{
		const int32 IndexInArray = FMath::Min(InstanceIndex, PrimitiveSceneInfo->LumenPrimitiveGroupIndices.Num() - 1);
		const int32 PrimitiveGroupIndex = PrimitiveSceneInfo->LumenPrimitiveGroupIndices[IndexInArray];
		const FLumenPrimitiveGroup& PrimitiveGroup = PrimitiveGroups[PrimitiveGroupIndex];

		return PrimitiveGroup.MeshCardsIndex;
	}

	return -1;
}

class FLumenMergedMeshCards
{
public:
	FLumenMergedMeshCards()
	{
		MergedBounds.Init();

		for (int32 AxisAlignedDirectionIndex = 0; AxisAlignedDirectionIndex < Lumen::NumAxisAlignedDirections; ++AxisAlignedDirectionIndex)
		{
			InstanceCardAreaPerDirection[AxisAlignedDirectionIndex] = 0;
		}
	}

	void AddInstance(FBox InstanceBox, FMatrix InstanceToMerged, const FMeshCardsBuildData& MeshCardsBuildData)
	{
		MergedBounds += InstanceBox.TransformBy(InstanceToMerged);

		for (const FLumenCardBuildData& CardBuildData : MeshCardsBuildData.CardBuildData)
		{
			const FVector3f AxisX = FVector4f(InstanceToMerged.TransformVector((FVector)CardBuildData.OBB.AxisX));
			const FVector3f AxisY = FVector4f(InstanceToMerged.TransformVector((FVector)CardBuildData.OBB.AxisY));
			const FVector3f AxisZ = FVector4f(InstanceToMerged.TransformVector((FVector)CardBuildData.OBB.AxisZ));
			const FVector3f Extent = CardBuildData.OBB.Extent * FVector3f(AxisX.Length(), AxisY.Length(), AxisZ.Length());

			const float InstanceCardArea = Extent.X * Extent.Y;
			const FVector3f CardDirection = AxisZ.GetUnsafeNormal();

			for (int32 AxisAlignedDirectionIndex = 0; AxisAlignedDirectionIndex < Lumen::NumAxisAlignedDirections; ++AxisAlignedDirectionIndex)
			{
				const FVector3f AxisDirection = LumenMeshCards::GetAxisAlignedDirection(AxisAlignedDirectionIndex);
				const float AxisProjection = CardDirection.Dot(AxisDirection);

				if (AxisProjection > 0.0f)
				{
					InstanceCardAreaPerDirection[AxisAlignedDirectionIndex] += AxisProjection * InstanceCardArea;
				}
			}
		}
	}

	FBox MergedBounds;
	float InstanceCardAreaPerDirection[Lumen::NumAxisAlignedDirections];
};

void BuildMeshCardsDataForHeightfield(const FLumenPrimitiveGroup& PrimitiveGroup, FMeshCardsBuildData& MeshCardsBuildData, FMatrix& MeshCardsLocalToWorld)
{
	const FPrimitiveSceneProxy* Proxy = PrimitiveGroup.Primitives[0]->Proxy;

	MeshCardsLocalToWorld = Proxy->GetLocalToWorld();

	// Make sure that the card isn't placed directly on the geometry
	const FVector BoundsMargin = FVector(CVarLumenSurfaceCacheHeightfieldCaptureMargin.GetValueOnRenderThread()) / MeshCardsLocalToWorld.GetScaleVector();

	MeshCardsBuildData.Bounds = Proxy->GetLocalBounds().GetBox().ExpandBy(BoundsMargin);

	// Add a single top down card
	MeshCardsBuildData.CardBuildData.SetNum(1);
	{
		FLumenCardBuildData& CardBuildData = MeshCardsBuildData.CardBuildData[0];

		// Set rotation
		uint32 AxisAlignedDirectionIndex = 5;
		CardBuildData.OBB.AxisZ = LumenMeshCards::GetAxisAlignedDirection(AxisAlignedDirectionIndex);
		CardBuildData.OBB.AxisZ.FindBestAxisVectors(CardBuildData.OBB.AxisX, CardBuildData.OBB.AxisY);
		CardBuildData.OBB.AxisX = FVector3f::CrossProduct(CardBuildData.OBB.AxisZ, CardBuildData.OBB.AxisY);
		CardBuildData.OBB.AxisX.Normalize();

		CardBuildData.OBB.Origin = (FVector3f)MeshCardsBuildData.Bounds.GetCenter();
		CardBuildData.OBB.Extent = CardBuildData.OBB.RotateLocalToCard((FVector3f)MeshCardsBuildData.Bounds.GetExtent()).GetAbs();

		CardBuildData.AxisAlignedDirectionIndex = AxisAlignedDirectionIndex;
	}
}

void BuildMeshCardsDataForMergedInstances(const FLumenPrimitiveGroup& PrimitiveGroup, FMeshCardsBuildData& MeshCardsBuildData, FMatrix& MeshCardsLocalToWorld)
{
	MeshCardsLocalToWorld.SetIdentity();

	// Pick first largest bbox as a reference frame
	float LargestInstanceArea = -1.0f;
	for (const FPrimitiveSceneInfo* PrimitiveSceneInfo : PrimitiveGroup.Primitives)
	{
		const FMatrix& PrimitiveToWorld = PrimitiveSceneInfo->Proxy->GetLocalToWorld();
		const FBoxSphereBounds& PrimitiveBounds = PrimitiveSceneInfo->Proxy->GetBounds();
		float InstanceArea = BoxSurfaceArea(PrimitiveBounds.BoxExtent);
		FMatrix InstanceMeshCardsLocalToWorld = PrimitiveToWorld;

		if (const FInstanceSceneDataBuffers *InstanceSceneData = PrimitiveSceneInfo->GetInstanceSceneDataBuffers())
		{
			for (int32 InstanceIndex = 0; InstanceIndex < InstanceSceneData->GetNumInstances(); ++InstanceIndex)
			{
				InstanceArea = BoxSurfaceArea((FVector)InstanceSceneData->GetInstanceLocalBounds(InstanceIndex).GetExtent());
				InstanceMeshCardsLocalToWorld = InstanceSceneData->GetInstanceToWorld(InstanceIndex);
			}
		}
		if (InstanceArea > LargestInstanceArea)
		{
			MeshCardsLocalToWorld = InstanceMeshCardsLocalToWorld;
			LargestInstanceArea = InstanceArea;
		}
	}

	const FMatrix WorldToMeshCardsLocal = MeshCardsLocalToWorld.Inverse();

	MeshCardsBuildData.Bounds.Init();

	FLumenMergedMeshCards MergedMeshCards;

	for (const FPrimitiveSceneInfo* PrimitiveSceneInfo : PrimitiveGroup.Primitives)
	{
		const FCardRepresentationData* CardRepresentationData = PrimitiveSceneInfo->Proxy->GetMeshCardRepresentation();

		if (CardRepresentationData)
		{
			const FMatrix& PrimitiveToWorld = PrimitiveSceneInfo->Proxy->GetLocalToWorld();
			const FMeshCardsBuildData& PrimitiveMeshCardsBuildData = CardRepresentationData->MeshCardsBuildData;
			const FMatrix PrimitiveLocalToMeshCardsLocal = PrimitiveToWorld * WorldToMeshCardsLocal;

			if (const FInstanceSceneDataBuffers *InstanceSceneData = PrimitiveSceneInfo->GetInstanceSceneDataBuffers())
			{
				for (int32 InstanceIndex = 0; InstanceIndex < InstanceSceneData->GetNumInstances(); ++InstanceIndex)
				{
					FMatrix InstanceToWorld = InstanceSceneData->GetInstanceToWorld(InstanceIndex);
					MergedMeshCards.AddInstance(
						InstanceSceneData->GetInstanceLocalBounds(InstanceIndex).ToBox(),
						InstanceToWorld * WorldToMeshCardsLocal,
						PrimitiveMeshCardsBuildData);
				}
			}
			else
			{
				MergedMeshCards.AddInstance(
					PrimitiveSceneInfo->Proxy->GetLocalBounds().GetBox(),
					PrimitiveLocalToMeshCardsLocal,
					PrimitiveMeshCardsBuildData);
			}
		}
	}

	// Spawn cards only on faces passing min area threshold
	TArray<int32, TInlineAllocator<Lumen::NumAxisAlignedDirections>> AxisAlignedDirectionsToSpawnCards;
	for (int32 AxisAlignedDirectionIndex = 0; AxisAlignedDirectionIndex < Lumen::NumAxisAlignedDirections; ++AxisAlignedDirectionIndex)
	{
		FVector3f MergedExtent = (FVector3f)MergedMeshCards.MergedBounds.GetExtent();
		MergedExtent[AxisAlignedDirectionIndex / 2] = 1.0f;
		const float MergedFaceArea = MergedExtent.X * MergedExtent.Y * MergedExtent.Z;

		if (MergedMeshCards.InstanceCardAreaPerDirection[AxisAlignedDirectionIndex] > GLumenMeshCardsMergedCardMinSurfaceArea * MergedFaceArea)
		{
			AxisAlignedDirectionsToSpawnCards.Add(AxisAlignedDirectionIndex);
		}
	}

	if (MergedMeshCards.MergedBounds.IsValid && AxisAlignedDirectionsToSpawnCards.Num() > 0)
	{
		// Make sure BBox isn't empty and we can generate card representation for it. This handles e.g. infinitely thin planes.
		const FVector SafeCenter = MergedMeshCards.MergedBounds.GetCenter();
		const FVector SafeExtent = FVector::Max(MergedMeshCards.MergedBounds.GetExtent() + 1.0f, FVector(5.0f));
		const FBox SafeMergedBounds = FBox(SafeCenter - SafeExtent, SafeCenter + SafeExtent);

		MeshCardsBuildData.Bounds = SafeMergedBounds;

		MeshCardsBuildData.CardBuildData.SetNum(AxisAlignedDirectionsToSpawnCards.Num());
		uint32 CardBuildDataIndex = 0;

		for (int32 AxisAlignedDirectionIndex : AxisAlignedDirectionsToSpawnCards)
		{
			FLumenCardBuildData& CardBuildData = MeshCardsBuildData.CardBuildData[CardBuildDataIndex];
			++CardBuildDataIndex;

			// Set rotation
			CardBuildData.OBB.AxisZ = LumenMeshCards::GetAxisAlignedDirection(AxisAlignedDirectionIndex);
			CardBuildData.OBB.AxisZ.FindBestAxisVectors(CardBuildData.OBB.AxisX, CardBuildData.OBB.AxisY);
			CardBuildData.OBB.AxisX = FVector3f::CrossProduct(CardBuildData.OBB.AxisZ, CardBuildData.OBB.AxisY);
			CardBuildData.OBB.AxisX.Normalize();

			CardBuildData.OBB.Origin = (FVector3f)SafeMergedBounds.GetCenter();	// LWC_TODO: Precision Loss
			CardBuildData.OBB.Extent = CardBuildData.OBB.RotateLocalToCard((FVector3f)SafeMergedBounds.GetExtent() + FVector3f(1.0f)).GetAbs();

			CardBuildData.AxisAlignedDirectionIndex = AxisAlignedDirectionIndex;
		}
	}
}

void FLumenSceneData::AddMeshCards(int32 PrimitiveGroupIndex)
{
	FLumenPrimitiveGroup& PrimitiveGroup = PrimitiveGroups[PrimitiveGroupIndex];

	if (PrimitiveGroup.MeshCardsIndex < 0)
	{
		if (PrimitiveGroup.bHeightfield)
		{
			// Landscape component handling
			FMatrix LocalToWorld;
			FMeshCardsBuildData MeshCardsBuildData;
			BuildMeshCardsDataForHeightfield(PrimitiveGroup, MeshCardsBuildData, LocalToWorld);

			AddMeshCardsFromBuildData(PrimitiveGroupIndex, LocalToWorld, MeshCardsBuildData, PrimitiveGroup);
		}
		else if (PrimitiveGroup.HasMergedInstances())
		{
			// Multiple meshes merged together
			FMatrix LocalToWorld;
			FMeshCardsBuildData MeshCardsBuildData;
			BuildMeshCardsDataForMergedInstances(PrimitiveGroup, MeshCardsBuildData, LocalToWorld);

			AddMeshCardsFromBuildData(PrimitiveGroupIndex, LocalToWorld, MeshCardsBuildData, PrimitiveGroup);
		}
		else
		{
			// Single mesh
			ensure(PrimitiveGroup.Primitives.Num() == 1);
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveGroup.Primitives[0];

			FMatrix LocalToWorld = PrimitiveSceneInfo->Proxy->GetLocalToWorld();
			if (const FInstanceSceneDataBuffers *InstanceData = PrimitiveSceneInfo->GetInstanceSceneDataBuffers())
			{
				const int32 PrimitiveInstanceIndex = FMath::Clamp(PrimitiveGroup.PrimitiveInstanceIndex, 0, InstanceData->GetNumInstances() - 1);
				LocalToWorld = InstanceData->GetInstanceToWorld(PrimitiveInstanceIndex);
			}

			const FCardRepresentationData* CardRepresentationData = PrimitiveSceneInfo->Proxy->GetMeshCardRepresentation();
			if (CardRepresentationData)
			{
				const FMeshCardsBuildData& MeshCardsBuildData = CardRepresentationData->MeshCardsBuildData;
				AddMeshCardsFromBuildData(PrimitiveGroupIndex, LocalToWorld, MeshCardsBuildData, PrimitiveGroup);
			}
		}

		if (PrimitiveGroup.MeshCardsIndex >= 0)
		{
			// Copy ScenePrimitive->GetIndex() in order to prevent from deferencing possibly deleted ScenePrimitive*
			FLumenMeshCards& MeshCardsInstance = MeshCards[PrimitiveGroup.MeshCardsIndex];

			MeshCardsInstance.ScenePrimitiveIndices.Reset();
			MeshCardsInstance.ScenePrimitiveIndices.Reserve(PrimitiveGroup.Primitives.Num());

			for (const FPrimitiveSceneInfo* ScenePrimitive : PrimitiveGroup.Primitives)
			{
				if (ScenePrimitive->IsIndexValid())
				{
					MeshCardsInstance.ScenePrimitiveIndices.Add(ScenePrimitive->GetIndex());
					PrimitivesToUpdateMeshCards.Add(ScenePrimitive->GetIndex());
				}
			}
		}
		else
		{
			// Can't spawn mesh cards, mark this primitive as invalid
			PrimitiveGroup.bValidMeshCards = false;
			PrimitiveGroupIndicesToUpdateInBuffer.Add(PrimitiveGroupIndex);
		}
	}
}

bool IsMatrixOrthogonal(const FMatrix& Matrix)
{
	const FVector MatrixScale = Matrix.GetScaleVector();

	if (MatrixScale.GetAbsMin() >= KINDA_SMALL_NUMBER)
	{
		FVector AxisX;
		FVector AxisY;
		FVector AxisZ;
		Matrix.GetUnitAxes(AxisX, AxisY, AxisZ);

		return FMath::Abs(AxisX | AxisY) < KINDA_SMALL_NUMBER
			&& FMath::Abs(AxisX | AxisZ) < KINDA_SMALL_NUMBER
			&& FMath::Abs(AxisY | AxisZ) < KINDA_SMALL_NUMBER;
	}

	return false;
}

bool MeshCardCullTest(const FLumenCardBuildData& CardBuildData, const FVector3f LocalToWorldScale, float MinFaceSurfaceArea, int32 CardIndex)
{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if (GLumenMeshCardsDebugSingleCard >= 0)
	{
		return GLumenMeshCardsDebugSingleCard == CardIndex;
	}
#endif

	const FVector3f ScaledBoundsSize = 2.0f * CardBuildData.OBB.Extent * LocalToWorldScale;
	const float SurfaceArea = ScaledBoundsSize.X * ScaledBoundsSize.Y;
	const bool bCardPassedCulling = (!GLumenMeshCardsCullFaces || SurfaceArea > MinFaceSurfaceArea);

	return bCardPassedCulling;
}

void FLumenSceneData::AddMeshCardsFromBuildData(int32 PrimitiveGroupIndex, const FMatrix& LocalToWorld, const FMeshCardsBuildData& MeshCardsBuildData, FLumenPrimitiveGroup& PrimitiveGroup)
{
	PrimitiveGroup.MeshCardsIndex = -1;
	PrimitiveGroup.HeightfieldIndex = -1;

	const FVector3f LocalToWorldScale = (FVector3f)LocalToWorld.GetScaleVector();
	const FVector3f ScaledBoundSize = (FVector3f)MeshCardsBuildData.Bounds.GetSize() * LocalToWorldScale;
	const FVector3f FaceSurfaceArea(ScaledBoundSize.Y * ScaledBoundSize.Z, ScaledBoundSize.X * ScaledBoundSize.Z, ScaledBoundSize.Y * ScaledBoundSize.X);
	const float LargestFaceArea = FaceSurfaceArea.GetMax();
	const float MinFaceSurfaceArea = LumenMeshCards::GetCardMinSurfaceArea(PrimitiveGroup.bEmissiveLightSource);

	if (LargestFaceArea > MinFaceSurfaceArea
		&& IsMatrixOrthogonal(LocalToWorld)) // #lumen_todo: implement card capture for non orthogonal local to world transforms
	{
		const int32 NumBuildDataCards = MeshCardsBuildData.CardBuildData.Num();

		uint32 NumCards = 0;

		for (int32 CardIndexInBuildData = 0; CardIndexInBuildData < NumBuildDataCards; ++CardIndexInBuildData)
		{
			const FLumenCardBuildData& CardBuildData = MeshCardsBuildData.CardBuildData[CardIndexInBuildData];

			if (MeshCardCullTest(CardBuildData, LocalToWorldScale, MinFaceSurfaceArea, CardIndexInBuildData))
			{
				++NumCards;
			}
		}

		if (NumCards > 0)
		{
			const int32 FirstCardIndex = Cards.AddSpan(NumCards);

			const int32 MeshCardsIndex = MeshCards.AddSpan(1);
			PrimitiveGroup.MeshCardsIndex = MeshCardsIndex;
			FLumenMeshCards& MeshCardsInstance = MeshCards[MeshCardsIndex];
			MeshCardsInstance.Initialize(
				LocalToWorld,
				PrimitiveGroupIndex,
				FirstCardIndex,
				NumCards,
				MeshCardsBuildData,
				PrimitiveGroup);

			MeshCardsIndicesToUpdateInBuffer.Add(MeshCardsIndex);

			if (PrimitiveGroup.bHeightfield)
			{
				const int32 HeightfieldIndex = Heightfields.AddSpan(1);
				PrimitiveGroup.HeightfieldIndex = HeightfieldIndex;
				Heightfields[HeightfieldIndex].Initialize(MeshCardsIndex);

				HeightfieldIndicesToUpdateInBuffer.Add(HeightfieldIndex);
			}

			// Add cards
			int32 LocalCardIndex = 0;
			for (int32 CardIndexInBuildData = 0; CardIndexInBuildData < NumBuildDataCards; ++CardIndexInBuildData)
			{	
				const FLumenCardBuildData& CardBuildData = MeshCardsBuildData.CardBuildData[CardIndexInBuildData];

				if (MeshCardCullTest(CardBuildData, LocalToWorldScale, MinFaceSurfaceArea, CardIndexInBuildData))
				{
					const int32 CardInsertIndex = FirstCardIndex + LocalCardIndex;

					Cards[CardInsertIndex].Initialize(
						PrimitiveGroup.CardResolutionScale,
						LocalToWorld,
						MeshCardsInstance,
						CardBuildData,
						LocalCardIndex,
						MeshCardsIndex,
						CardIndexInBuildData);

					CardIndicesToUpdateInBuffer.Add(CardInsertIndex);

					++LocalCardIndex;
				}
			}

			MeshCardsInstance.UpdateLookup(Cards);

			PrimitiveGroupIndicesToUpdateInBuffer.Add(PrimitiveGroupIndex);
		}
	}
}

void FLumenSceneData::RemoveMeshCards(int32 PrimitiveGroupIndex)
{
	FLumenPrimitiveGroup& PrimitiveGroup = PrimitiveGroups[PrimitiveGroupIndex];

	if (PrimitiveGroup.MeshCardsIndex >= 0)
	{
		FLumenMeshCards& MeshCardsInstance = MeshCards[PrimitiveGroup.MeshCardsIndex];

		for (uint32 CardIndex = MeshCardsInstance.FirstCardIndex; CardIndex < MeshCardsInstance.FirstCardIndex + MeshCardsInstance.NumCards; ++CardIndex)
		{
			RemoveCardFromAtlas(CardIndex);
		}

		if (PrimitiveGroup.HeightfieldIndex >= 0)
		{
			Heightfields.RemoveSpan(PrimitiveGroup.HeightfieldIndex, 1);
			HeightfieldIndicesToUpdateInBuffer.Add(PrimitiveGroup.HeightfieldIndex);
		}

		// Update surface cache mapping
		for (int32 ScenePrimitiveIndex : MeshCardsInstance.ScenePrimitiveIndices)
		{
			PrimitivesToUpdateMeshCards.Add(ScenePrimitiveIndex);
		}
		MeshCardsInstance.ScenePrimitiveIndices.Reset();

		Cards.RemoveSpan(MeshCardsInstance.FirstCardIndex, MeshCardsInstance.NumCards);
		MeshCards.RemoveSpan(PrimitiveGroup.MeshCardsIndex, 1);

		MeshCardsIndicesToUpdateInBuffer.Add(PrimitiveGroup.MeshCardsIndex);

		PrimitiveGroup.MeshCardsIndex = -1;
		PrimitiveGroup.HeightfieldIndex = -1;

		PrimitiveGroupIndicesToUpdateInBuffer.Add(PrimitiveGroupIndex);
	}
}

void FLumenSceneData::UpdateMeshCards(const FMatrix& LocalToWorld, int32 MeshCardsIndex, const FMeshCardsBuildData& MeshCardsBuildData)
{
	if (MeshCardsIndex >= 0 && IsMatrixOrthogonal(LocalToWorld))
	{
		FLumenMeshCards& MeshCardsInstance = MeshCards[MeshCardsIndex];
		MeshCardsInstance.SetTransform(LocalToWorld);
		MeshCardsIndicesToUpdateInBuffer.Add(MeshCardsIndex);

		for (uint32 LocalCardIndex = 0; LocalCardIndex < MeshCardsInstance.NumCards; ++LocalCardIndex)
		{
			const uint32 CardIndex = MeshCardsInstance.FirstCardIndex + LocalCardIndex;
			FLumenCard& Card = Cards[CardIndex];

			Card.SetTransform(LocalToWorld, MeshCardsInstance);

			CardIndicesToUpdateInBuffer.Add(CardIndex);
		}
	}
}

void FLumenSceneData::InvalidateSurfaceCache(FRHIGPUMask GPUMask, int32 MeshCardsIndex)
{
	if (MeshCardsIndex >= 0)
	{
		FLumenMeshCards& MeshCardsInstance = MeshCards[MeshCardsIndex];
		for (uint32 CardIndex = MeshCardsInstance.FirstCardIndex; CardIndex < MeshCardsInstance.FirstCardIndex + MeshCardsInstance.NumCards; ++CardIndex)
		{
			const FLumenCard& LumenCard = Cards[CardIndex];
			for (int32 ResLevel = LumenCard.MinAllocatedResLevel; ResLevel <= LumenCard.MaxAllocatedResLevel; ++ResLevel)
			{
				const FLumenSurfaceMipMap& MipMap = LumenCard.GetMipMap(ResLevel);
				if (MipMap.IsAllocated())
				{
					for (int32 LocalPageIndex = 0; LocalPageIndex < MipMap.SizeInPagesX * MipMap.SizeInPagesY; ++LocalPageIndex)
					{
						const int32 PageIndex = MipMap.GetPageTableIndex(LocalPageIndex);
						if (GetPageTableEntry(PageIndex).IsMapped())
						{
							for (uint32 GPUIndex : GPUMask)
							{
								if (PagesToRecaptureHeap[GPUIndex].IsPresent(PageIndex))
								{
									PagesToRecaptureHeap[GPUIndex].Update(GetSurfaceCacheUpdateFrameIndex(), PageIndex);
								}
								else
								{
									PagesToRecaptureHeap[GPUIndex].Add(GetSurfaceCacheUpdateFrameIndex(), PageIndex);
								}								
							}
						}
					}
				}
			}
		}
	}
}

void FLumenSceneData::RemoveCardFromAtlas(int32 CardIndex)
{
	FLumenCard& Card = Cards[CardIndex];
	Card.DesiredLockedResLevel = 0;
	FreeVirtualSurface(Card, Card.MinAllocatedResLevel, Card.MaxAllocatedResLevel);
	CardIndicesToUpdateInBuffer.Add(CardIndex);
}

FLumenCard::FLumenCard()
{
	bVisible = false;
	LocalOBB.Reset();
	WorldOBB.Reset();
	MeshCardsOBB.Reset();
	IndexInMeshCards = -1;
}

FLumenCard::~FLumenCard()
{
	for (int32 MipIndex = 0; MipIndex < UE_ARRAY_COUNT(SurfaceMipMaps); ++MipIndex)
	{
		ensure(SurfaceMipMaps[MipIndex].PageTableSpanSize == 0);
	}
}

void FLumenCard::Initialize(
	float InResolutionScale,
	const FMatrix& LocalToWorld,
	const FLumenMeshCards& InMeshCardsInstance,
	const FLumenCardBuildData& CardBuildData,
	int32 InIndexInMeshCards,
	int32 InMeshCardsIndex,
	uint8 InIndexInBuildData)
{
	check(CardBuildData.AxisAlignedDirectionIndex < Lumen::NumAxisAlignedDirections);

	LocalOBB = CardBuildData.OBB;
	IndexInMeshCards = InIndexInMeshCards;
	MeshCardsIndex = InMeshCardsIndex;
	IndexInBuildData = InIndexInBuildData;
	ResolutionScale = InResolutionScale;
	AxisAlignedDirectionIndex = CardBuildData.AxisAlignedDirectionIndex;
	bHeightfield = InMeshCardsInstance.bHeightfield;

	SetTransform(LocalToWorld, InMeshCardsInstance);

	CardAspect = WorldOBB.Extent.X / WorldOBB.Extent.Y;
}

void FLumenCard::SetTransform(const FMatrix& LocalToWorld, const FLumenMeshCards& MeshCards)
{
	WorldOBB = FLumenCardOBBd(LocalOBB).Transform(LocalToWorld);

	MeshCardsOBB.AxisX = FVector4f(MeshCards.WorldToLocalRotation.TransformVector(FVector(WorldOBB.AxisX)));
	MeshCardsOBB.AxisY = FVector4f(MeshCards.WorldToLocalRotation.TransformVector(FVector(WorldOBB.AxisY)));
	MeshCardsOBB.AxisZ = FVector4f(MeshCards.WorldToLocalRotation.TransformVector(FVector(WorldOBB.AxisZ)));
	MeshCardsOBB.Origin = LocalOBB.Origin * MeshCards.LocalToWorldScale;
	MeshCardsOBB.Extent = LocalOBB.RotateCardToLocal(LocalOBB.Extent).GetAbs() * MeshCards.LocalToWorldScale;
}

void FLumenCard::UpdateMinMaxAllocatedLevel()
{
	MinAllocatedResLevel = UINT8_MAX;
	MaxAllocatedResLevel = 0;

	for (int32 ResLevelIndex = Lumen::MinResLevel; ResLevelIndex <= Lumen::MaxResLevel; ++ResLevelIndex)
	{
		if (GetMipMap(ResLevelIndex).IsAllocated())
		{
			MinAllocatedResLevel = FMath::Min<int32>(MinAllocatedResLevel, ResLevelIndex);
			MaxAllocatedResLevel = FMath::Max<int32>(MaxAllocatedResLevel, ResLevelIndex);
		}
	}
}

FIntPoint FLumenCard::ResLevelToResLevelXYBias() const
{
	FIntPoint Bias(0, 0);

	// ResLevel bias to account for card's aspect
	if (CardAspect >= 1.0f)
	{
		Bias.Y = FMath::FloorLog2(FMath::RoundToInt(CardAspect));
	}
	else
	{
		Bias.X = FMath::FloorLog2(FMath::RoundToInt(1.0f / CardAspect));
	}

	Bias.X = FMath::Clamp<int32>(Bias.X, 0, Lumen::MaxResLevel - Lumen::MinResLevel);
	Bias.Y = FMath::Clamp<int32>(Bias.Y, 0, Lumen::MaxResLevel - Lumen::MinResLevel);
	return Bias;
}

void FLumenCard::GetMipMapDesc(int32 ResLevel, FLumenMipMapDesc& Desc) const
{
	check(ResLevel >= Lumen::MinResLevel && ResLevel <= Lumen::MaxResLevel);

	const FIntPoint ResLevelBias = ResLevelToResLevelXYBias();
	Desc.ResLevelX = FMath::Clamp<int32>(ResLevel - ResLevelBias.X, (int32)Lumen::MinResLevel, (int32)Lumen::MaxResLevel);
	Desc.ResLevelY = FMath::Clamp<int32>(ResLevel - ResLevelBias.Y, (int32)Lumen::MinResLevel, (int32)Lumen::MaxResLevel);

	// Allocations which exceed a physical page are aligned to multiples of a virtual page to maximize atlas usage
	if (Desc.ResLevelX > Lumen::SubAllocationResLevel || Desc.ResLevelY > Lumen::SubAllocationResLevel)
	{
		// Clamp res level to page size
		Desc.ResLevelX = FMath::Max<int32>(Desc.ResLevelX, Lumen::SubAllocationResLevel);
		Desc.ResLevelY = FMath::Max<int32>(Desc.ResLevelY, Lumen::SubAllocationResLevel);

		Desc.bSubAllocation = false;
		Desc.SizeInPages.X = 1u << (Desc.ResLevelX - Lumen::SubAllocationResLevel);
		Desc.SizeInPages.Y = 1u << (Desc.ResLevelY - Lumen::SubAllocationResLevel);
		Desc.Resolution.X = Desc.SizeInPages.X * Lumen::VirtualPageSize;
		Desc.Resolution.Y = Desc.SizeInPages.Y * Lumen::VirtualPageSize;
		Desc.PageResolution.X = Lumen::PhysicalPageSize;
		Desc.PageResolution.Y = Lumen::PhysicalPageSize;
	}
	else
	{
		Desc.bSubAllocation = true;
		Desc.SizeInPages.X = 1;
		Desc.SizeInPages.Y = 1;
		Desc.Resolution.X = 1 << Desc.ResLevelX;
		Desc.Resolution.Y = 1 << Desc.ResLevelY;
		Desc.PageResolution.X = Desc.Resolution.X;
		Desc.PageResolution.Y = Desc.Resolution.Y;
	}
}

void FLumenCard::GetSurfaceStats(const TSparseSpanArray<FLumenPageTableEntry>& PageTable, FSurfaceStats& Stats) const
{
	if (IsAllocated())
	{
		for (int32 ResLevelIndex = MinAllocatedResLevel; ResLevelIndex <= MaxAllocatedResLevel; ++ResLevelIndex)
		{
			const FLumenSurfaceMipMap& MipMap = GetMipMap(ResLevelIndex);

			if (MipMap.IsAllocated())
			{
				uint32 NumVirtualTexels = 0;
				uint32 NumPhysicalTexels = 0;

				for (int32 LocalPageIndex = 0; LocalPageIndex < MipMap.SizeInPagesX * MipMap.SizeInPagesY; ++LocalPageIndex)
				{
					const int32 PageTableIndex = MipMap.GetPageTableIndex(LocalPageIndex);
					const FLumenPageTableEntry& PageTableEntry = PageTable[PageTableIndex];

					NumVirtualTexels += PageTableEntry.GetNumVirtualTexels();
					NumPhysicalTexels += PageTableEntry.GetNumPhysicalTexels();
				}

				Stats.NumVirtualTexels += NumVirtualTexels;
				Stats.NumPhysicalTexels += NumPhysicalTexels;

				if (MipMap.bLocked)
				{
					Stats.NumLockedVirtualTexels += NumVirtualTexels;
					Stats.NumLockedPhysicalTexels += NumPhysicalTexels;
				}
			}
		}

		if (DesiredLockedResLevel > MinAllocatedResLevel)
		{
			Stats.DroppedResLevels += DesiredLockedResLevel - MinAllocatedResLevel;
		}
	}
}

void FLumenMeshCards::Initialize(
	const FMatrix& InLocalToWorld,
	int32 InPrimitiveGroupIndex,
	uint32 InFirstCardIndex,
	uint32 InNumCards,
	const FMeshCardsBuildData& MeshCardsBuildData,
	const FLumenPrimitiveGroup& PrimitiveGroup)
{
	PrimitiveGroupIndex = InPrimitiveGroupIndex;

	LocalBounds = MeshCardsBuildData.Bounds;
	bMostlyTwoSided = MeshCardsBuildData.bMostlyTwoSided;

	FirstCardIndex = InFirstCardIndex;
	NumCards = InNumCards;

	bFarField = PrimitiveGroup.bFarField;
	bHeightfield = PrimitiveGroup.bHeightfield;
	bEmissiveLightSource = PrimitiveGroup.bEmissiveLightSource;

	SetTransform(InLocalToWorld);
}

void FLumenMeshCards::UpdateLookup(const TSparseSpanArray<FLumenCard>& Cards)
{
	for (int32 AxisAlignedDirectionIndex = 0; AxisAlignedDirectionIndex < Lumen::NumAxisAlignedDirections; ++AxisAlignedDirectionIndex)
	{
		CardLookup[AxisAlignedDirectionIndex] = 0;
	}

	for (uint32 LocalCardIndex = 0; LocalCardIndex < NumCards; ++LocalCardIndex)
	{
		const uint32 CardIndex = FirstCardIndex + LocalCardIndex;
		const FLumenCard& Card = Cards[CardIndex];
		
		const uint32 BitMask = (1 << LocalCardIndex);
		CardLookup[Card.AxisAlignedDirectionIndex] |= BitMask;
	}
}

void FLumenMeshCards::SetTransform(const FMatrix& InLocalToWorld)
{
	LocalToWorld = InLocalToWorld;
	LocalToWorldScale = FVector3f(LocalToWorld.GetScaleVector());

	WorldToLocalRotation = LocalToWorld;
	WorldToLocalRotation.RemoveScaling();
	WorldToLocalRotation.SetOrigin(FVector::ZeroVector);
	WorldToLocalRotation = WorldToLocalRotation.GetTransposed();
}
