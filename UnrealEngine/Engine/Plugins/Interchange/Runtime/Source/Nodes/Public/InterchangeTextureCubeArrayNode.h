// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeTextureNode.h"

#include "InterchangeTextureCubeArrayNode.generated.h"


UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangeTextureCubeArrayNode : public UInterchangeTextureNode
{
	GENERATED_BODY()

public:

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("TextureCubeArrayNode");
		return TypeName;
	}
};
