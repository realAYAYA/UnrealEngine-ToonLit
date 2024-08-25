// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TextureLayout3d.h"
#include "RenderResource.h"
#include "MeshCardRepresentation.h"
#include "LumenSparseSpanArray.h"

class FLumenCard;
class FLumenPrimitiveGroup;
class FPrimitiveSceneInfo;
struct FLumenSceneFrameTemporaries;
class FRDGBuilder;
class FSceneViewFamily;
class FScene;
class FMeshCardsBuildData;

namespace Lumen
{
	constexpr uint32 NumAxisAlignedDirections = 6;

	void UpdateCardSceneBuffer(FRDGBuilder& GraphBuilder, FLumenSceneFrameTemporaries& FrameTemporaries, const FSceneViewFamily& ViewFamily, FScene* Scene);
};


class FLumenMeshCards
{
public:
	void Initialize(
		const FMatrix& InLocalToWorld,
		int32 InPrimitiveGroupIndex,
		uint32 InFirstCardIndex,
		uint32 InNumCards,
		const FMeshCardsBuildData& MeshCardsBuildData,
		const FLumenPrimitiveGroup& PrimitiveGroup);

	void UpdateLookup(const TSparseSpanArray<FLumenCard>& Cards);

	void SetTransform(const FMatrix& InLocalToWorld);

	FBox GetWorldSpaceBounds() const
	{
		const FBox WorldSpaceBounds = LocalBounds.TransformBy(LocalToWorld);
		return WorldSpaceBounds;
	}

	FMatrix LocalToWorld = FMatrix::Identity;
	FVector3f LocalToWorldScale = FVector3f(1.0f, 1.0f, 1.0f);
	FMatrix WorldToLocalRotation = FMatrix::Identity;
	FBox LocalBounds = FBox(FVector(-1.0f), FVector(-1.0f));

	int32 PrimitiveGroupIndex = -1;
	bool bFarField = false;
	bool bHeightfield = false;
	bool bMostlyTwoSided = false;
	bool bEmissiveLightSource = false;

	uint32 FirstCardIndex = 0;
	uint32 NumCards = 0;
	uint32 CardLookup[Lumen::NumAxisAlignedDirections];

	TArray<int32, TInlineAllocator<1>> ScenePrimitiveIndices;
};

namespace LumenMeshCards
{
	float GetCardMinSurfaceArea(bool bEmissiveLightSource);
};