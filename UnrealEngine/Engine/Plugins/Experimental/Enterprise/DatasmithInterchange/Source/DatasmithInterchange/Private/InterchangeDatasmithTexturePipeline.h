// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "InterchangeGenericTexturePipeline.h"

#include "InterchangeDatasmithTexturePipeline.generated.h"

class IDatasmithScene;
class UInterchangeBaseNodeContainer;
class UInterchangeSourceData;
class UInterchangeTextureFactoryNode;
class UObject;

UCLASS(BlueprintType, Experimental)
class DATASMITHINTERCHANGE_API UInterchangeDatasmithTexturePipeline : public UInterchangeGenericTexturePipeline
{
	GENERATED_BODY()

public:
	virtual void ExecutePreImportPipeline(UInterchangeBaseNodeContainer* BaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas) override;

private:
	void PreImportTextureFactoryNode(UInterchangeBaseNodeContainer* BaseNodeContainer, UInterchangeTextureFactoryNode* TextureFactoryNode) const;
};