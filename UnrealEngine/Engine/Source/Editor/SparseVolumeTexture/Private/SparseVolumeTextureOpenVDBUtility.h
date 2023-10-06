// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"

namespace UE
{
	namespace SVT
	{
		struct FTextureData;
	}
}

enum class EOpenVDBGridType : uint8
{
	Unknown = 0,
	Half,
	Half2,
	Half3,
	Half4,
	Float,
	Float2,
	Float3,
	Float4,
	Double,
	Double2,
	Double3,
	Double4,
};

struct FOpenVDBGridInfo
{
	FMatrix44f Transform;
	FIntVector3 VolumeActiveAABBMin;
	FIntVector3 VolumeActiveAABBMax;
	FIntVector3 VolumeActiveDim;
	FVector VolumeVoxelSize;
	FString Name;
	FString DisplayString; // Contains Index (into source file grids), Type and Name
	uint32 Index;
	uint32 NumComponents;
	EOpenVDBGridType Type;
	bool bIsInWorldSpace;
	bool bHasUniformVoxels;
};

struct FOpenVDBToSVTConversionResult
{
	struct FSparseVolumeAssetHeader* Header;
	TArray<uint32>* PageTable;
	TArray<uint8>* PhysicalTileDataA;
	TArray<uint8>* PhysicalTileDataB;
};

bool IsOpenVDBGridValid(const FOpenVDBGridInfo& GridInfo, const FString& Filename);

bool GetOpenVDBGridInfo(TArray64<uint8>& SourceFile, bool bCreateStrings, TArray<FOpenVDBGridInfo>* OutGridInfo);

bool ConvertOpenVDBToSparseVolumeTexture(TArray64<uint8>& SourceFile, const struct FOpenVDBImportOptions& ImportOptions, const FIntVector3& VolumeBoundsMin, bool bBakeTranslation, UE::SVT::FTextureData& OutResult);

const TCHAR* OpenVDBGridTypeToString(EOpenVDBGridType Type);

#endif // WITH_EDITOR