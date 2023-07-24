// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeTexture2DNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeTexture2DNode)

FString UInterchangeTexture2DNode::MakeNodeUid(const FStringView NodeName)
{
	return UInterchangeTextureNode::MakeNodeUid(NodeName);
}

 UInterchangeTexture2DNode* UInterchangeTexture2DNode::Create(UInterchangeBaseNodeContainer* NodeContainer, const FStringView TextureNodeName)
{
	UInterchangeTexture2DNode* TextureNode = NewObject< UInterchangeTexture2DNode >(NodeContainer);
	const FString TextureNodeUid = UInterchangeTextureNode::MakeNodeUid(TextureNodeName);

	TextureNode->InitializeNode(TextureNodeUid, FString(TextureNodeName), EInterchangeNodeContainerType::TranslatedAsset);
	NodeContainer->AddNode(TextureNode);

	return TextureNode;
}
