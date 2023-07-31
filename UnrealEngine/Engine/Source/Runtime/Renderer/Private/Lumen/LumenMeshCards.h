// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenMeshCards.h
=============================================================================*/

#pragma once

#include "TextureLayout3d.h"
#include "RenderResource.h"
#include "MeshCardRepresentation.h"
#include "LumenSparseSpanArray.h"

class FLumenCard;
class FPrimitiveSceneInfo;
struct FLumenSceneFrameTemporaries;

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
		const FBox& InLocalBounds,
		int32 InPrimitiveGroupIndex,
		uint32 InFirstCardIndex,
		uint32 InNumCards,
		bool InFarField,
		bool InHeightfield,
		bool InEmissiveLightSource)
	{
		PrimitiveGroupIndex = InPrimitiveGroupIndex;

		LocalBounds = InLocalBounds;
		SetTransform(InLocalToWorld);
		FirstCardIndex = InFirstCardIndex;
		NumCards = InNumCards;
		bFarField = InFarField;
		bHeightfield = InHeightfield;
		bEmissiveLightSource = InEmissiveLightSource;
	}

	void UpdateLookup(const TSparseSpanArray<FLumenCard>& Cards);

	void SetTransform(const FMatrix& InLocalToWorld);

	FBox GetWorldSpaceBounds() const
	{
		const FBox WorldSpaceBounds = LocalBounds.TransformBy(LocalToWorld);
		return WorldSpaceBounds;
	}

	FMatrix LocalToWorld;
	FVector3f LocalToWorldScale;
	FMatrix WorldToLocalRotation;
	FBox LocalBounds;

	int32 PrimitiveGroupIndex = -1;
	bool bFarField = false;
	bool bHeightfield = false;
	bool bEmissiveLightSource = false;

	uint32 FirstCardIndex = 0;
	uint32 NumCards = 0;
	uint32 CardLookup[Lumen::NumAxisAlignedDirections];
};

namespace LumenMeshCards
{
	float GetCardMinSurfaceArea(bool bEmissiveLightSource);
};