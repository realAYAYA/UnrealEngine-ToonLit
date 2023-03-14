// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeCommonPipelineDataFactoryNode.h"

#include "InterchangeSceneNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeCommonPipelineDataFactoryNode)

namespace UE::Interchange::CommonPipelineData
{
	FString GetCommonPipelineDataUniqueID()
	{
		static FString StaticUid = TEXT("CommonPipelineDataFactoryNode");
		return StaticUid;
	}
}

UInterchangeCommonPipelineDataFactoryNode* UInterchangeCommonPipelineDataFactoryNode::FindOrCreateUniqueInstance(UInterchangeBaseNodeContainer* NodeContainer)
{
	const FString StaticUid = UE::Interchange::CommonPipelineData::GetCommonPipelineDataUniqueID();
	UInterchangeCommonPipelineDataFactoryNode* CommonPipelineDataFactoryNode = Cast<UInterchangeCommonPipelineDataFactoryNode>(NodeContainer->GetFactoryNode(StaticUid));
	if (!CommonPipelineDataFactoryNode)
	{
		CommonPipelineDataFactoryNode = NewObject<UInterchangeCommonPipelineDataFactoryNode>(NodeContainer, NAME_None);
		CommonPipelineDataFactoryNode->InitializeNode(StaticUid, StaticUid, EInterchangeNodeContainerType::FactoryData);
		NodeContainer->AddNode(CommonPipelineDataFactoryNode);
	}
	return CommonPipelineDataFactoryNode;
}
UInterchangeCommonPipelineDataFactoryNode* UInterchangeCommonPipelineDataFactoryNode::GetUniqueInstance(const UInterchangeBaseNodeContainer* NodeContainer)
{
	static FString StaticUid = UE::Interchange::CommonPipelineData::GetCommonPipelineDataUniqueID();
	return Cast<UInterchangeCommonPipelineDataFactoryNode>(NodeContainer->GetFactoryNode(StaticUid));
}

bool UInterchangeCommonPipelineDataFactoryNode::GetCustomGlobalOffsetTransform(FTransform& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(GlobalOffsetTransform, FTransform);
}

UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Common Pipeline Data")
bool UInterchangeCommonPipelineDataFactoryNode::SetCustomGlobalOffsetTransform(const UInterchangeBaseNodeContainer* NodeContainer, const FTransform& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(GlobalOffsetTransform, FTransform);
	//Reset all scene node container cache
	UInterchangeSceneNode::ResetAllGlobalTransformCaches(NodeContainer);
}

bool UInterchangeCommonPipelineDataFactoryNode::GetBakeMeshes(bool& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(BakeMeshes, bool);
}

bool UInterchangeCommonPipelineDataFactoryNode::SetBakeMeshes(const UInterchangeBaseNodeContainer* NodeContainer, const bool& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(BakeMeshes, bool);
}