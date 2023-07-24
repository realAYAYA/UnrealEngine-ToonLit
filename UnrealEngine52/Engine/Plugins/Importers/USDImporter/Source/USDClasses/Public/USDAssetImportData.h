// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"

#include "USDAssetImportData.generated.h"

UCLASS(AutoExpandCategories = (Options), MinimalAPI)
class UUsdAssetImportData : public UAssetImportData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString PrimPath;

	// Likely a UUSDStageImportOptions, but we don't declare it here
	// to prevent an unnecessary module dependency on USDStageImporter
	UPROPERTY()
	TObjectPtr<class UObject> ImportOptions;
};

UCLASS(AutoExpandCategories = (Options), MinimalAPI)
class UUsdAnimSequenceAssetImportData : public UUsdAssetImportData
{
	GENERATED_BODY()

public:
	// Offset into the stage in seconds to when this AnimSequence should start playing to match the
	// skeletal animation in that stage. This is required because UAnimSequences just range from the
	// first skeletal timeSample to the last, and the first sample is not necessarily the startTimeCode
	// for the stage. Note that this is wrt. the startTimeCode of the *layer*, and not the composed stage.
	// This is only used for animating USkeletalMeshComponents via the AUsdStageActor, and it is in
	// seconds since it will need to drive the UAnimSequence with a time value in the [0, LenghtSeconds] range.
	// This should be applied *after* any offset/scale conversions on the time coordinate.
	UPROPERTY()
	float LayerStartOffsetSeconds = 0.0f;
};

/** Simple wrapper because we're not allowed to have TMap properties with TArray<FString> as values */
USTRUCT()
struct FUsdPrimPathList
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> PrimPaths;
};

/** We assign these to UStaticMeshes or USkeletalMeshes generated from USD */
UCLASS( AutoExpandCategories = ( Options ), MinimalAPI )
class UUsdMeshAssetImportData : public UUsdAssetImportData
{
	GENERATED_BODY()

public:
	/**
	 * Maps from a material slot index of an UStaticMesh or USkeletalMesh to a list of source prims that contain this
	 * assignment. It can contain multiple prims in case we combine material slots and/or collapse prims
	 * (e.g. {0: ['/Root/mesh', '/Root/othermesh/geomsubset0', '/Root/othermesh/geomsubset1'] }).
	 */
	UPROPERTY()
	TMap< int32, FUsdPrimPathList > MaterialSlotToPrimPaths;
};