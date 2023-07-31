// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterialBase.h"

#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"


void UCustomizableObjectNodeEditMaterialBase::BeginPostDuplicate(bool bDuplicateForPIE)
{
	Super::BeginPostDuplicate(bDuplicateForPIE);

	if (ParentMaterialObject)
	{
		if (UCustomizableObjectGraph* CEdGraph = Cast<UCustomizableObjectGraph>(GetGraph()))
		{
			ParentMaterialNodeId = CEdGraph->RequestNotificationForNodeIdChange(ParentMaterialNodeId, NodeGuid);
		}
	}
}


void UCustomizableObjectNodeEditMaterialBase::UpdateReferencedNodeId(const FGuid& NewGuid)
{
	if (ParentMaterialObject)
	{
		ParentMaterialNodeId = NewGuid;
	}
}


UCustomizableObjectNode& UCustomizableObjectNodeEditMaterialBase::GetNode()
{
	return *this;
}


void UCustomizableObjectNodeEditMaterialBase::SetParentNode(UCustomizableObject* Object, FGuid NodeId)
{
	FCustomizableObjectNodeParentedMaterial::SetParentNode(Object, NodeId);
	
	// Update layout grid widget.
	TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();
	if (Editor.IsValid())
	{
		Editor->UpdateGraphNodeProperties();
	}
}

void UCustomizableObjectNodeEditMaterialBase::SaveParentNode(UCustomizableObject* Object, FGuid NodeId)
{
	ParentMaterialObject = Object;
	ParentMaterialNodeId = NodeId;
}


FGuid UCustomizableObjectNodeEditMaterialBase::GetParentNodeId() const
{
	return ParentMaterialNodeId;
}


UCustomizableObject* UCustomizableObjectNodeEditMaterialBase::GetParentObject() const
{
	return ParentMaterialObject;
}

