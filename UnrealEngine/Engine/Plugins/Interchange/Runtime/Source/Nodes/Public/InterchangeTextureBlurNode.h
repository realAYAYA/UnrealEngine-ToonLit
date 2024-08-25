// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangeTexture2DNode.h"

#include "InterchangeTextureBlurNode.generated.h"

class UInterchangeBaseNodeContainer;

UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangeTextureBlurNode : public UInterchangeTexture2DNode
{
	GENERATED_BODY()

public:

	/**
	 * Build and return a UID name for a texture 2D node.
	 */
	static FString MakeNodeUid(const FStringView NodeName);

	/**
	 * Creates a new UInterchangeTexture2DNode and adds it to NodeContainer as a translated node.
	 */
	static UInterchangeTextureBlurNode* Create(UInterchangeBaseNodeContainer* NodeContainer, const FStringView TextureNodeName);

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	virtual FString GetTypeName() const override
	{
		return TEXT("TextureBlurNode");
	}
};
