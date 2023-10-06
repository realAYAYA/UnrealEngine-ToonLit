// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeMaterialInstanceNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "InterchangeShaderGraphNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeMaterialInstanceNode)

FString UInterchangeMaterialInstanceNode::GetTypeName() const
{
    return TEXT("MaterialInstanceNode");
}

FString UInterchangeMaterialInstanceNode::MakeNodeUid(const FStringView NodeName, const FStringView ParentNodeUid)
{
	FString MaterialInstanceNodeUid = FString(UInterchangeBaseNode::HierarchySeparator) + TEXT("MaterialInstances") + FString(UInterchangeBaseNode::HierarchySeparator);

	if(ParentNodeUid.IsEmpty())
	{
		MaterialInstanceNodeUid += NodeName;
	}
	else
	{
		MaterialInstanceNodeUid += ParentNodeUid + FString(UInterchangeBaseNode::HierarchySeparator) + NodeName;
	}

	return MaterialInstanceNodeUid;
}

UInterchangeMaterialInstanceNode* UInterchangeMaterialInstanceNode::Create(UInterchangeBaseNodeContainer* NodeContainer, const FStringView NodeName, const FStringView ParentNodeUid)
{
	check(NodeContainer);

	const FString MaterialInstanceNodeUid = MakeNodeUid(NodeName, ParentNodeUid);

	UInterchangeMaterialInstanceNode* MaterialInstanceNode = NewObject<UInterchangeMaterialInstanceNode>(NodeContainer);
	MaterialInstanceNode->InitializeNode(MaterialInstanceNodeUid, FString(NodeName), EInterchangeNodeContainerType::TranslatedAsset);

	NodeContainer->AddNode(MaterialInstanceNode);
	NodeContainer->SetNodeParentUid(MaterialInstanceNodeUid, FString(ParentNodeUid));

	return MaterialInstanceNode;
}

bool UInterchangeMaterialInstanceNode::SetCustomParent(const FString& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(Parent, FString);
}

bool UInterchangeMaterialInstanceNode::GetCustomParent(FString& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(Parent, FString);
}

bool UInterchangeMaterialInstanceNode::AddScalarParameterValue(const FString& ParameterName, float AttributeValue)
{
	return AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(ParameterName), AttributeValue);
}

bool UInterchangeMaterialInstanceNode::GetScalarParameterValue(const FString& ParameterName, float& AttributeValue) const
{
	return GetFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(ParameterName), AttributeValue);
}

bool UInterchangeMaterialInstanceNode::AddVectorParameterValue(const FString& ParameterName, const FLinearColor& AttributeValue)
{
	return AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(ParameterName), AttributeValue);
}

bool UInterchangeMaterialInstanceNode::GetVectorParameterValue(const FString& ParameterName, FLinearColor& AttributeValue) const
{
	return GetLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(ParameterName), AttributeValue);
}

bool UInterchangeMaterialInstanceNode::AddTextureParameterValue(const FString& ParameterName, const FString& AttributeValue)
{
	return AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(ParameterName), AttributeValue);
}

bool UInterchangeMaterialInstanceNode::GetTextureParameterValue(const FString& ParameterName, FString& AttributeValue) const
{
	return GetStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(ParameterName), AttributeValue);
}

bool UInterchangeMaterialInstanceNode::AddStaticSwitchParameterValue(const FString& ParameterName, bool AttributeValue)
{
	return AddBooleanAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(ParameterName), AttributeValue);
}

bool UInterchangeMaterialInstanceNode::GetStaticSwitchParameterValue(const FString& ParameterName, bool& AttributeValue) const
{
	return GetBooleanAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(ParameterName), AttributeValue);
}
