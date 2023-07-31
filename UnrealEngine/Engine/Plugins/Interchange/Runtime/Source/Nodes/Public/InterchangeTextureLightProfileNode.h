// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeTextureNode.h"

#include "InterchangeTextureLightProfileNode.generated.h"


UCLASS(BlueprintType, Experimental)
class INTERCHANGENODES_API UInterchangeTextureLightProfileNode : public UInterchangeTextureNode
{
	GENERATED_BODY()

public:

	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("TextureLightProfileNode");
		return TypeName;
	}
};