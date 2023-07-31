// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangePipelineBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeDatasmithPipeline.generated.h"

class UDatasmithScene;
class UMaterialInterface;
class UInterchangeGenericAnimationPipeline;
class UInterchangeGenericCommonMeshesProperties;
class UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties;
class UInterchangeDatasmithLevelPipeline;
class UInterchangeDatasmithMaterialPipeline;
class UInterchangeDatasmithStaticMeshPipeline;
class UInterchangeDatasmithSceneNode;
class UInterchangeDatasmithTexturePipeline;
class UTexture;
struct FDatasmithImportContext;

UCLASS(BlueprintType, Experimental)
class DATASMITHINTERCHANGE_API UInterchangeDatasmithPipeline : public UInterchangePipelineBase
{
	GENERATED_BODY()

	UInterchangeDatasmithPipeline();

	UPROPERTY(VisibleAnywhere, Instanced, Category = "Textures")
	TObjectPtr<UInterchangeDatasmithTexturePipeline> TexturePipeline;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "Materials")
	TObjectPtr<UInterchangeDatasmithMaterialPipeline> MaterialPipeline;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "Mesh")
	TObjectPtr<UInterchangeDatasmithStaticMeshPipeline> MeshPipeline;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "Level")
	TObjectPtr<UInterchangeDatasmithLevelPipeline> LevelPipeline;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "Animation")
	TObjectPtr<UInterchangeGenericAnimationPipeline> AnimationPipeline;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "Common Meshes")
	TObjectPtr<UInterchangeGenericCommonMeshesProperties> CommonMeshesProperties;

	UPROPERTY(VisibleAnywhere, Instanced, Category = "Common Skeletal Meshes and Animations")
	TObjectPtr<UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties> CommonSkeletalMeshesAndAnimationsProperties;

protected:
	virtual void ExecutePreImportPipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas) override;
	virtual void ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* InBaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport) override;

	virtual bool CanExecuteOnAnyThread(EInterchangePipelineTask PipelineTask) override
	{
		// This pipeline creates UObjects and assets. Not safe to execute outside of main thread.
		return false;
	}

private:
	UInterchangeBaseNodeContainer* BaseNodeContainer = nullptr;

	// Fill up the UDatasmithScene with all the data its needs for DnD
	void PostImportDatasmithSceneAsset(UDatasmithScene& DatasmithSceneAsset);
};
