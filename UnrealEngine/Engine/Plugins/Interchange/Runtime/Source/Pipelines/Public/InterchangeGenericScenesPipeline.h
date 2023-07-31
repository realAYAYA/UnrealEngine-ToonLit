// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangePipelineBase.h"
#include "InterchangeSourceData.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeGenericScenesPipeline.generated.h"

class UInterchangeActorFactoryNode;
class UInterchangeSceneNode;
class UInterchangeSceneVariantSetsNode;



UCLASS(BlueprintType, editinlinenew)
class INTERCHANGEPIPELINES_API UInterchangeGenericLevelPipeline : public UInterchangePipelineBase
{
	GENERATED_BODY()

protected:

	virtual void ExecutePreImportPipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas) override;

	virtual bool CanExecuteOnAnyThread(EInterchangePipelineTask PipelineTask) override
	{
		return true;
	}

	/**
	 * PreImport step called for each translated SceneNode.
	 */
	virtual void ExecuteSceneNodePreImport(const FTransform& GlobalOffsetTransform, const UInterchangeSceneNode* SceneNode);

	/**
	 * PreImport step called for each translated SceneVariantSetNode.
	 */
	virtual void ExecuteSceneVariantSetNodePreImport(const UInterchangeSceneVariantSetsNode& SceneVariantSetNode);

	/**
	 * Return a new Actor Factory Node to be used for the given SceneNode.
	 */
	virtual UInterchangeActorFactoryNode* CreateActorFactoryNode(const UInterchangeSceneNode* SceneNode, const UInterchangeBaseNode* TranslatedAssetNode) const;

	/**
	 * Use to set up the given factory node's attributes after its initialization.
	 */
	virtual void SetUpFactoryNode(UInterchangeActorFactoryNode* ActorFactoryNode, const UInterchangeSceneNode* SceneNode, const UInterchangeBaseNode* TranslatedAssetNode) const;
	
protected:
	UInterchangeBaseNodeContainer* BaseNodeContainer = nullptr;
};


