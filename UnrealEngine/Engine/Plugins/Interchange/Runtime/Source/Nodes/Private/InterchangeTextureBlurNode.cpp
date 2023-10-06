// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeTextureBlurNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeTextureBlurNode)

FString UInterchangeTextureBlurNode::MakeNodeUid(const FStringView NodeName)
{
	return UInterchangeTextureNode::MakeNodeUid(NodeName);
}

UInterchangeTextureBlurNode* UInterchangeTextureBlurNode::Create(UInterchangeBaseNodeContainer* NodeContainer, const FStringView TextureNodeName)
{
	UInterchangeTextureBlurNode* TextureNode = NewObject<UInterchangeTextureBlurNode>(NodeContainer);
	const FString TextureNodeUid = UInterchangeTextureNode::MakeNodeUid(TextureNodeName);

	TextureNode->InitializeNode(TextureNodeUid, FString(TextureNodeName), EInterchangeNodeContainerType::TranslatedAsset);
	NodeContainer->AddNode(TextureNode);

	return TextureNode;
}
