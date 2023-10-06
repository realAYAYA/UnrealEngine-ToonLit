// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/AssetUserData.h"

#include "USDAssetUserData.generated.h"

UCLASS()
class USDCLASSES_API UUsdAssetUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FString> PrimPaths;
};

UCLASS()
class USDCLASSES_API UUsdAnimSequenceAssetUserData : public UUsdAssetUserData
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

/** AssetImportData assigned to Unreal materials that are generated when parsing USD Material prims */
UCLASS()
class USDCLASSES_API UUsdMaterialAssetUserData : public UUsdAssetUserData
{
	GENERATED_BODY()

public:
	/**
	 * In the context of our reference materials that just read a single texture for each material parameter, this
	 * describes the primvar that the USD material is sampling for each texture.
	 * e.g. {'BaseColor': 'st0', 'Metallic': 'st1'}
	 */
	UPROPERTY()
	TMap<FString, FString> ParameterToPrimvar;

	/**
	 * In the context of our reference materials that just read a single texture for each material parameter, this
	 * describes the Unreal UV index that will be used for sampling by each USD primvar. The idea is to use this in
	 * combination with ParameterToPrimvar, in order to describe which UV index the material has currently assigned to
	 * each parameter. When assigning the material to meshes later, we'll compare this member with the
	 * UUsdMeshAssetImportData's own PrimvarToUVIndex to see if they are compatible or not, and if not spawn a new
	 * material instance that is.
	 * e.g. {'firstPrimvar': 0, 'st': 1, 'st1': 2}
	 */
	UPROPERTY()
	TMap<FString, int32> PrimvarToUVIndex;
};

/** We assign these to UStaticMeshes or USkeletalMeshes generated from USD */
UCLASS()
class USDCLASSES_API UUsdMeshAssetUserData : public UUsdAssetUserData
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

	/** Describes which primvars should be assigned to each UV index. */
	UPROPERTY()
	TMap<FString, int32> PrimvarToUVIndex;
};

/**
 * We assign these to persistent LevelSequences that bind to one of the actors/components that the stage actor spawns.
 * We need this as part of a mechanism to automatically repair those bindings when they break if we close/reload the stage.
 */
UCLASS()
class USDCLASSES_API UUsdLevelSequenceAssetUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	/**
	 * The LevelSequence has a Guid that is changed every time its state is modified.
	 * We pay attention to that so that we can avoid reprocessing a LevelSequence that hasn't changed
	 */
	UPROPERTY()
	FGuid LastCheckedSignature;

	/**
	 * Set of binding GUIDs that we already handled in the past.
	 * We use this so that we won't try and overwrite the changes in case the user manually clears/modifies a binding we previously setup.
	 */
	UPROPERTY()
	TSet<FGuid> HandledBindingGuids;
};