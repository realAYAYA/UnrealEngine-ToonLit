// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"

enum class ESparseVolumeAttributesFormat : uint8
{
	Unorm8 = 0,
	Float16 = 1,
	Float32 = 2,
};

struct FOpenVDBSparseVolumeComponentMapping
{
	int32 SourceGridIndex = INDEX_NONE;
	int32 SourceComponentIndex = INDEX_NONE;
};

struct FOpenVDBSparseVolumeAttributesDesc
{
	TStaticArray<FOpenVDBSparseVolumeComponentMapping, 4> Mappings;
	ESparseVolumeAttributesFormat Format;
};

struct FOpenVDBImportOptions
{
	TStaticArray<FOpenVDBSparseVolumeAttributesDesc, 2> Attributes;
	bool bIsSequence;
};
