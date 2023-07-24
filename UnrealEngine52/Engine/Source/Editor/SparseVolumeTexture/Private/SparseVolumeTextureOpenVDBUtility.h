// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"

enum class EOpenVDBGridType : uint8
{
	Unknown = 0,
	Float = 1,
	Float2 = 2,
	Float3 = 3,
	Float4 = 4,
	Double = 5,
	Double2 = 6,
	Double3 = 7,
	Double4 = 8,
};

struct FOpenVDBGridInfo
{
	FMatrix44f Transform;
	FVector VolumeActiveAABBMin;
	FVector VolumeActiveAABBMax;
	FVector VolumeActiveDim;
	FVector VolumeVoxelSize;
	FString Name;
	FString DisplayString; // Contains Index (into source file grids), Type and Name
	uint32 Index;
	uint32 NumComponents;
	EOpenVDBGridType Type;
	bool bIsInWorldSpace;
	bool bHasUniformVoxels;
};

bool IsOpenVDBGridValid(const FOpenVDBGridInfo& GridInfo, const FString& Filename);

bool GetOpenVDBGridInfo(TArray<uint8>& SourceFile, bool bCreateStrings, TArray<FOpenVDBGridInfo>* OutGridInfo);

bool ConvertOpenVDBToSparseVolumeTexture(
	TArray<uint8>& SourceFile,
	struct FSparseVolumeRawSourcePackedData& PackedDataA,
	struct FSparseVolumeRawSourcePackedData& PackedDataB,
	struct FOpenVDBToSVTConversionResult* OutResult,
	bool bOverrideActiveMinMax,
	FVector ActiveMin,
	FVector ActiveMax);

const TCHAR* OpenVDBGridTypeToString(EOpenVDBGridType Type);

#endif // WITH_EDITOR