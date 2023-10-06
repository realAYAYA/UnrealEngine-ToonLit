// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LumenMeshCards.h"

class FLumenHeightfield
{
public:
	int32 MeshCardsIndex = -1;

	void Initialize(int32 InMeshCardsIndex)
	{
		MeshCardsIndex = InMeshCardsIndex;
	}
};

struct FLumenHeightfieldGPUData
{
	// Must match LUMEN_HEIGHTFIELD_DATA_STRIDE in LumenCardCommon.ush
	enum { DataStrideInFloat4s = 3 };
	enum { DataStrideInBytes = DataStrideInFloat4s * 16 };

	static void FillData(const class FLumenHeightfield& RESTRICT Heightfield, const TSparseSpanArray<FLumenMeshCards>& MeshCards, FVector4f* RESTRICT OutData);
};