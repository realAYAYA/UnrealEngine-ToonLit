// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "InterchangeGenericMaterialPipeline.h"

#include "InterchangeDatasmithMaterialPipeline.generated.h"

class UMaterialInterface;
class UInterchangeDatasmithMaterialNode;
class UInterchangeDatasmithPbrMaterialNode;
class UInterchangeFactoryBaseNode;
class UInterchangeMaterialFactoryNode;
class UInterchangeMaterialInstanceFactoryNode;
class UInterchangeShaderGraphNode;

UCLASS(BlueprintType, Experimental)
class DATASMITHINTERCHANGE_API UInterchangeDatasmithMaterialPipeline : public UInterchangeGenericMaterialPipeline
{
	GENERATED_BODY()

	UInterchangeDatasmithMaterialPipeline();

protected:
	virtual void ExecutePreImportPipeline(UInterchangeBaseNodeContainer* BaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas) override;
	virtual void ExecutePostImportPipeline(const UInterchangeBaseNodeContainer* BaseNodeContainer, const FString& NodeKey, UObject* CreatedAsset, bool bIsAReimport) override;

	virtual bool CanExecuteOnAnyThread(EInterchangePipelineTask PipelineTask) override
	{
		// This pipeline creates UObjects and assets. Not safe to execute outside of main thread.
		return false;
	}

private:
	void PreImportMaterialNode(UInterchangeBaseNodeContainer* NodeContainer, UInterchangeDatasmithMaterialNode* MaterialNode);
	void PostImportMaterialInstanceFactoryNode(const UInterchangeBaseNodeContainer* NodeContainer, UInterchangeMaterialInstanceFactoryNode* FactoryNode, UMaterialInterface* CreatedMaterial);
	void UpdateMaterialFactoryNodes(const TArray<UInterchangeShaderNode*>& ShaderNodes);
};