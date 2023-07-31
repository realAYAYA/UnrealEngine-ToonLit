// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ClothingAsset.h"
#include "ClothingAssetFactoryInterface.h"
#include "Containers/UnrealString.h"
#include "GPUSkinPublicDefs.h"
#include "HAL/Platform.h"
#include "Logging/LogMacros.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"

#include "ClothingAssetFactory.generated.h"

class UClothingAssetBase;
class UObject;
class USkeletalMesh;
struct FClothLODDataCommon;
struct FSkeletalMeshClothBuildParams;


DECLARE_LOG_CATEGORY_EXTERN(LogClothingAssetFactory, Log, All);

class FSkeletalMeshLODModel;
class UClothingAssetCommon;

namespace nvidia
{
	namespace apex
	{
		class ClothingAsset;
	}
}

namespace NvParameterized
{
	class Interface;
}

UCLASS(hidecategories=Object)
class CLOTHINGSYSTEMEDITOR_API UClothingAssetFactory : public UClothingAssetFactoryBase
{
	GENERATED_BODY()

public:

	UClothingAssetFactory(const FObjectInitializer& ObjectInitializer);

	// Import the given file, treating it as an APEX asset file and return the resulting asset
	virtual UClothingAssetBase* Import(const FString& Filename, USkeletalMesh* TargetMesh, FName InName = NAME_None) override;
	virtual UClothingAssetBase* Reimport(const FString& Filename, USkeletalMesh* TargetMesh, UClothingAssetBase* OriginalAsset) override;
	virtual UClothingAssetBase* CreateFromSkeletalMesh(USkeletalMesh* TargetMesh, FSkeletalMeshClothBuildParams& Params) override;
	virtual UClothingAssetBase* CreateFromExistingCloth(USkeletalMesh* TargetMesh, USkeletalMesh* SourceMesh, UClothingAssetBase* SourceAsset) override;
	virtual UClothingAssetBase* ImportLodToClothing(USkeletalMesh* TargetMesh, FSkeletalMeshClothBuildParams& Params) override;
	// Tests whether the given filename should be able to be imported
	virtual bool CanImport(const FString& Filename) override;

	// Given an APEX asset, build a UClothingAssetCommon containing the required data
	virtual UClothingAssetBase* CreateFromApexAsset(nvidia::apex::ClothingAsset* InApexAsset, USkeletalMesh* TargetMesh, FName InName = NAME_None) override;

private:

	// Utility methods for skeletal mesh extraction //////////////////////////

	/** Handles internal import of LODs */
	bool ImportToLodInternal(USkeletalMesh* SourceMesh, int32 SourceLodIndex, int32 SourceSectionIndex, UClothingAssetCommon* DestAsset, FClothLODDataCommon& DestLod, int32 DestLodIndex, const FClothLODDataCommon* InParameterRemapSource = nullptr);

	//////////////////////////////////////////////////////////////////////////

};
