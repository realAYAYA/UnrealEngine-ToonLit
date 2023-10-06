// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeActorFactoryNode.h"

#include "InterchangeSceneImportAssetFactoryNode.generated.h"

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeSceneImportAssetFactoryNode : public UInterchangeFactoryBaseNode
{
	GENERATED_BODY()

public:
	// UInterchangeFactoryBaseNode Begin
	virtual class UClass* GetObjectClass() const override;
	// UInterchangeFactoryBaseNode End

private:
};

