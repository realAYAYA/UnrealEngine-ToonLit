// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"

struct FText3DDynamicData
{
	FText3DDynamicData()
	{
	}

	FText3DDynamicData(const TArray<int32>& InIndices, const TArray<FDynamicMeshVertex>& InVertices) :
		Indices(InIndices),
		Vertices(InVertices)
	{
	}

	TArray<int32> Indices;
	TArray<FDynamicMeshVertex> Vertices;
	TArray<int32> GlyphStartVertices;
};

struct FText3DMesh : public FText3DDynamicData
{
	bool IsEmpty() const;
};

enum class EText3DGroupType : uint8
{
	Front = 0,
	Bevel = 1,
	Extrude = 2,
	Back = 3,

	TypeCount = 4
};

using TText3DMeshList = TArray<FText3DMesh, TFixedAllocator<static_cast<int32>(EText3DGroupType::TypeCount)>>;
