// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EditorFramework/AssetImportData.h"
#include "FbxAssetImportData.generated.h"

class UFbxSceneImportData;

/**
 * Base class for import data and options used when importing any asset from FBX
 */
UCLASS(BlueprintType, config=EditorPerProjectUserSettings, HideCategories=Object, abstract, MinimalAPI)
class UFbxAssetImportData : public UAssetImportData
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category=Transform, meta=(ImportType="StaticMesh|SkeletalMesh|Animation", ImportCategory="Transform"))
	FVector ImportTranslation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category=Transform, meta=(ImportType="StaticMesh|SkeletalMesh|Animation", ImportCategory="Transform"))
	FRotator ImportRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category=Transform, meta=(ImportType="StaticMesh|SkeletalMesh|Animation", ImportCategory="Transform"))
	float ImportUniformScale;

	/** Whether to convert scene from FBX scene. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = Miscellaneous, meta = (ImportType = "StaticMesh|SkeletalMesh|Animation", ImportCategory = "Miscellaneous", ToolTip = "Convert the scene from FBX coordinate system to UE coordinate system"))
	bool bConvertScene;

	/** Whether to force the front axis to be align with X instead of -Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = Miscellaneous, meta = (editcondition = "bConvertScene", ImportType = "StaticMesh|SkeletalMesh|Animation", ImportCategory = "Miscellaneous", ToolTip = "Convert the scene from FBX coordinate system to UE coordinate system with front X axis instead of -Y"))
	bool bForceFrontXAxis;

	/** Whether to convert the scene from FBX unit to UE unit (centimeter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = Miscellaneous, meta = (ImportType = "StaticMesh|SkeletalMesh|Animation", ImportCategory = "Miscellaneous", ToolTip = "Convert the scene from FBX unit to UE unit (centimeter)."))
	bool bConvertSceneUnit;

	/* Use by the reimport factory to answer CanReimport, if true only factory for scene reimport will return true */
	UPROPERTY()
	bool bImportAsScene;

	/* Use by the reimport factory to answer CanReimport, if true only factory for scene reimport will return true */
	UPROPERTY()
	TObjectPtr<UFbxSceneImportData> FbxSceneImportDataReference;
};
