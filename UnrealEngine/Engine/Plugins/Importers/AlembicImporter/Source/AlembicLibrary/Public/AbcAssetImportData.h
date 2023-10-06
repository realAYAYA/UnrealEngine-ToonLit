// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "EditorFramework/AssetImportData.h"
#include "AbcImportSettings.h"
#include "AbcAssetImportData.generated.h"

/**
* Base class for import data and options used when importing any asset from Alembic
*/
UCLASS()
class ALEMBICLIBRARY_API UAbcAssetImportData : public UAssetImportData
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY()
	TArray<FString> TrackNames;

	UPROPERTY()
	FAbcSamplingSettings SamplingSettings;

	UPROPERTY()
	FAbcNormalGenerationSettings NormalGenerationSettings;

	UPROPERTY()
	FAbcMaterialSettings MaterialSettings;

	UPROPERTY()
	FAbcCompressionSettings CompressionSettings;

	UPROPERTY()
	FAbcStaticMeshSettings StaticMeshSettings;

	UPROPERTY()
	FAbcGeometryCacheSettings GeometryCacheSettings;

	UPROPERTY()
	FAbcConversionSettings ConversionSettings;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
