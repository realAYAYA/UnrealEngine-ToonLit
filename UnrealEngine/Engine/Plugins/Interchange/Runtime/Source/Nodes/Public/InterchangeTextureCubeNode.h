// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeTextureNode.h"

#include "InterchangeTextureCubeNode.generated.h"


UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangeTextureCubeNode : public UInterchangeTextureNode
{
	GENERATED_BODY()

public:

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("TextureCubeNode");
		return TypeName;
	}
};
