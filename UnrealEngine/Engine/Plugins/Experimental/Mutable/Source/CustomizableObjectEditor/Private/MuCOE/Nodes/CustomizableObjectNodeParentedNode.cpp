// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeParentedNode.h"

#include "Containers/Array.h"
#include "HAL/PlatformCrt.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"

class UCustomizableObject;


void ICustomizableObjectNodeParentedNode::SetParentNode(UCustomizableObject* Object, FGuid NodeId)
{
	SaveParentNode(Object, NodeId);
}


UCustomizableObjectNode* ICustomizableObjectNodeParentedNode::GetParentNode() const
{
	return GetCustomizableObjectExternalNode<UCustomizableObjectNode>(GetParentObject(), GetParentNodeId());
}

