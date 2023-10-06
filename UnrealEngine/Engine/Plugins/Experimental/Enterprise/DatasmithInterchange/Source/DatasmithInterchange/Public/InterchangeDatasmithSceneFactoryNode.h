// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeFactoryBaseNode.h"

#include "InterchangeDatasmithSceneFactoryNode.generated.h"

UCLASS(BlueprintType, Experimental)
class DATASMITHINTERCHANGE_API UInterchangeDatasmithSceneFactoryNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	void InitializeDatasmithFactorySceneNode(const FString& UniqueID, const FString& DisplayLabel, const FString& InAssetClass)
	{
		InitializeNode(UniqueID, DisplayLabel, EInterchangeNodeContainerType::FactoryData);

		FString OperationName = GetTypeName() + TEXT(".SetAssetClassName");
		InterchangePrivateNodeBase::SetCustomAttribute<FString>(*Attributes, UE::Interchange::FBaseNodeStaticData::ClassTypeAttributeKey(), OperationName, InAssetClass);
	}

	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const override;

	virtual UClass* GetObjectClass() const override;
};