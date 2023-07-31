// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Nodes/InterchangeFactoryBaseNode.h"

#include "InterchangeCommonPipelineDataFactoryNode.generated.h"

class UInterchangeBaseNodeContainer;


/* This factory node is the place where pipeline can set global data that can be use by factories. */
UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeCommonPipelineDataFactoryNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	/* The pipelines that want to modify the common data shoiuld ensure they create the unique common pipeline node. */
	static UInterchangeCommonPipelineDataFactoryNode* FindOrCreateUniqueInstance(UInterchangeBaseNodeContainer* NodeContainer);
	
	/* If the unique instance doesn't exist it will return nullptr. This function should be use by the factories, to avoid creating a node. */
	static UInterchangeCommonPipelineDataFactoryNode* GetUniqueInstance(const UInterchangeBaseNodeContainer* NodeContainer);

	/** Return the global offset transform set by the pipelines. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Common Pipeline Data")
	bool GetCustomGlobalOffsetTransform(FTransform& AttributeValue) const;

	/** Pipeline can set a global transform, factories will use this global offset when importing asset. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Common Pipeline Data")
	bool SetCustomGlobalOffsetTransform(const UInterchangeBaseNodeContainer* NodeContainer, const FTransform& AttributeValue);

	/** Return the Bake Meshes set by the pipelines. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Common Pipeline Data")
	bool GetBakeMeshes(bool& AttributeValue) const;

	/** Pipeline can set Bake Meshes, factories will use this to identify if Global transforms should be applied to Meshes/Skeletals. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Common Pipeline Data")
	bool SetBakeMeshes(const UInterchangeBaseNodeContainer* NodeContainer, const bool& AttributeValue);

private:
	UInterchangeCommonPipelineDataFactoryNode() {};

	const UE::Interchange::FAttributeKey Macro_CustomGlobalOffsetTransformKey = UE::Interchange::FAttributeKey(TEXT("GlobalOffsetTransform"));
	const UE::Interchange::FAttributeKey Macro_CustomBakeMeshesKey = UE::Interchange::FAttributeKey(TEXT("BakeMeshes"));
};