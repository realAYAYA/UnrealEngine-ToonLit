// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeTextureNode.h"

#include "InterchangeTexture2DArrayNode.generated.h"




UCLASS(BlueprintType, Experimental)
class INTERCHANGENODES_API UInterchangeTexture2DArrayNode : public UInterchangeTextureNode
{
	GENERATED_BODY()

public:

	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("Texture2DArrayNode");
		return TypeName;
	}
};