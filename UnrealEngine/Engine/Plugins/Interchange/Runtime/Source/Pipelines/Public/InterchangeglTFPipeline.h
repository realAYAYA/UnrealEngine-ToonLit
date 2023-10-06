// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "InterchangePipelineBase.h"
#include "InterchangeSourceData.h"
#include "Nodes/InterchangeBaseNodeContainer.h"

#include "Engine/DeveloperSettings.h"

#include "InterchangeglTFPipeline.generated.h"

class UInterchangeBaseNodeContainer;
class UInterchangeMaterialInstanceFactoryNode;
class UInterchangeShaderGraphNode;

UCLASS(config = Interchange, meta = (DisplayName = "Interchange glTF"))
class INTERCHANGEPIPELINES_API UGLTFPipelineSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category = "PredefinedglTFMaterialLibrary", meta = (DisplayName = "Predefined glTF Material Library"))
	TMap<FString, FSoftObjectPath> MaterialParents;

	TArray<FString> ValidateMaterialInstancesAndParameters() const;

	void BuildMaterialInstance(const UInterchangeShaderGraphNode* ShaderGraphNode, UInterchangeMaterialInstanceFactoryNode* materialInstanceNode);

	bool IsMaterialParentsEditible() { return bMaterialParentsEditible; }
	void SetMaterialParentsEditible(bool bEditible) { bMaterialParentsEditible = bEditible; }
private:
	TSet<FString> GenerateExpectedParametersList(const FString& Identifier) const;

	static const TArray<FString> ExpectedMaterialInstanceIdentifiers; //Default MaterialInstance' identifiers

	//Helper for the Settings Customizer to decide if the ParentMaterials should be editible or not
	//Should be editible from Project Settings, and should NOT be editible from the Import
	//It is set by the Pipeline's Customizer.
	//Equals to Pipeline->CanEditPropertiesStates()
	bool bMaterialParentsEditible = true;
};

UCLASS(BlueprintType)
class INTERCHANGEPIPELINES_API UInterchangeGLTFPipeline : public UInterchangePipelineBase
{
	GENERATED_BODY()

	UInterchangeGLTFPipeline();

public:
	TObjectPtr<UGLTFPipelineSettings> GLTFPipelineSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "glTF", meta = (DisplayName = "Use glTF Material Instance Library"))
	bool bUseGLTFMaterialInstanceLibrary = false;

protected:
	virtual void AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset) override;
	virtual void ExecutePipeline(UInterchangeBaseNodeContainer* BaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas) override;

	virtual bool CanExecuteOnAnyThread(EInterchangePipelineTask PipelineTask) override
	{
		// This pipeline creates UObjects and assets. Not safe to execute outside of main thread.
		return true;
	}
};