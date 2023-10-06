// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeGenericAssetsPipelineSharedSettings.h"
#include "InterchangePipelineBase.h"
#include "InterchangeSourceData.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeGenericAssetsPipeline.generated.h"

class UInterchangeGenericAnimationPipeline;
class UInterchangeGenericMaterialPipeline;
class UInterchangeGenericMeshPipeline;
class UInterchangeGenericTexturePipeline;

/**
 * This pipeline is the generic pipeline option for all meshes type and should be call before specialized Mesh pipeline (like generic static mesh or skeletal mesh pipelines)
 * All shared import options between mesh type should be added here.
 *
 */
UCLASS(BlueprintType, editinlinenew)
class INTERCHANGEPIPELINES_API UInterchangeGenericAssetsPipeline : public UInterchangePipelineBase
{
	GENERATED_BODY()

public:
	UInterchangeGenericAssetsPipeline();
	
	//////	COMMON_CATEGORY Properties //////
	/* Allow user to choose the re-import strategy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common", meta = (AdjustPipelineAndRefreshDetailOnChange = "True"))
	EReimportStrategyFlags ReimportStrategy = EReimportStrategyFlags::ApplyNoProperties;

	/** If enable and AssetName is empty and there is only one asset and one source data, we will name the asset like the source data name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common")
	bool bUseSourceNameForAsset = true;

	/** If not empty, and there is only one asset and one source data, we will name the asset with this string. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common", meta = (StandAlonePipelineProperty = "True", AlwaysResetToDefault = "True"))
	FString AssetName;

	/** Translation offset applied to meshes and animations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common", meta = (DisplayName = "Offset Translation"))
	FVector ImportOffsetTranslation;

	/** Rotation offset applied to meshes and animations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common", meta = (DisplayName = "Offset Rotation"))
	FRotator ImportOffsetRotation;

	/** Uniform scale offset applied to meshes and animations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Common", meta = (DisplayName = "Offset Uniform Scale"))
	float ImportOffsetUniformScale = 1.0f;

	//////	COMMON_MESHES_CATEGORY Properties //////
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Instanced, Category = "Common Meshes")
	TObjectPtr<UInterchangeGenericCommonMeshesProperties> CommonMeshesProperties;
		
	//////  COMMON_SKELETAL_ANIMATIONS_CATEGORY //////
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Instanced, Category = "Common Skeletal Meshes and Animations")
	TObjectPtr<UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties> CommonSkeletalMeshesAndAnimationsProperties;

	//////	MESHES_CATEGORY Properties //////
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Instanced, Category = "Meshes")
	TObjectPtr<UInterchangeGenericMeshPipeline> MeshPipeline;

	//////	ANIMATIONS_CATEGORY Properties //////
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Instanced, Category = "Animation")
	TObjectPtr<UInterchangeGenericAnimationPipeline> AnimationPipeline;

	//////	MATERIALS_CATEGORY Properties //////
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Instanced, Category = "Materials")
	TObjectPtr<UInterchangeGenericMaterialPipeline> MaterialPipeline;

	virtual void PreDialogCleanup(const FName PipelineStackName) override;

	virtual bool IsSettingsAreValid(TOptional<FText>& OutInvalidReason) const override;


	virtual void AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset) override;

	virtual bool IsScripted() override
	{
		return false;
	}

#if WITH_EDITOR
	virtual bool GetPropertyPossibleValues(const FName PropertyPath, TArray<FString>& PossibleValues) override;
#endif

protected:

	virtual void ExecutePipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas) override;
	virtual void ExecutePostFactoryPipeline(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport) override;
	virtual void ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport) override;

	virtual bool CanExecuteOnAnyThread(EInterchangePipelineTask PipelineTask) override
	{
		//We cannot run asynchronously because of the two following issues
		// Post Translator Task: material pipeline is loading assets (Parent Material)
		// Post Import Task: physics asset need to create a scene preview to be created
		return false;
	}

	virtual void SetReimportSourceIndex(UClass* ReimportObjectClass, const int32 SourceFileIndex) override;

	//virtual bool ExecuteExportPipeline(UInterchangeBaseNodeContainer* BaseNodeContainer) override;
private:
	
	/**
	 * Implement pipeline option bUseSourceNameForAsset
	 */
	void ImplementUseSourceNameForAssetOption();

#if WITH_EDITORONLY_DATA
	/**
	 * Adds the user defined attributes (UInterchangeUserDefinedAttributesAPI) to the package meta data (UMetaData)
	 */
	void AddPackageMetaData(UObject* CreatedAsset, const UInterchangeBaseNode* Node);
#endif // WITH_EDITORONLY_DATA

	UInterchangeBaseNodeContainer* BaseNodeContainer;
	TArray<const UInterchangeSourceData*> SourceDatas;
	
};
