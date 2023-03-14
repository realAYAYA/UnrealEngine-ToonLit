// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeUseMaterial.h"

#include "Delegates/Delegate.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Misc/Guid.h"
#include "MuCOE/CustomizableObjectPin.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeParentedMaterial.h"
#include "Templates/ChooseClass.h"
#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"

class UCustomizableObject;


bool FCustomizableObjectNodeUseMaterial::IsNodeOutDatedAndNeedsRefreshWork()
{
	UCustomizableObjectNode& Node = GetNode();
	const FCustomizableObjectNodeParentedMaterial& NodeParentedMaterial = GetNodeParentedMaterial();

	const TMap<FGuid, FEdGraphPinReference>& PinsParameter = GetPinsParameter();
	
	const bool bOutdated = [&]()
	{
		if (const UCustomizableObjectNodeMaterial* ParentMaterialNode = NodeParentedMaterial.GetParentMaterialNode())
		{
			if (ParentMaterialNode->RealMaterialDataHasChanged())
			{
				return true;
			}

			for (const TTuple<FGuid, FEdGraphPinReference> Pair : PinsParameter)
			{
				const UEdGraphPin& Pin = *Pair.Value.Get();
				if (!IsPinOrphan(Pin))
				{
					if (UsesImage(Pair.Key) && !ParentMaterialNode->HasParameter(Pair.Key))
					{
						return true;
					}
				}
			}
		}

		return false;
	}();
	
	// Remove previous compilation warnings
	if (!bOutdated && Node.bHasCompilerMessage)
	{
		Node.RemoveWarnings();
		Node.GetGraph()->NotifyGraphChanged();
	}

	return bOutdated;
}


void FCustomizableObjectNodeUseMaterial::PreSetParentNodeWork(UCustomizableObject* Object, const FGuid NodeId)
{
	const FCustomizableObjectNodeParentedMaterial& NodeParentedMaterial = GetNodeParentedMaterial();

	if (UCustomizableObjectNodeMaterial* ParentMaterialNode = NodeParentedMaterial.GetParentMaterialNode())
	{
		ParentMaterialNode->PostReconstructNodeDelegate.Remove(PostReconstructNodeDelegateHandler);
		ParentMaterialNode->PostImagePinModeChangedDelegate.Remove(PostTextureParameterModeChangedDelegateHandle);
	}
}


void FCustomizableObjectNodeUseMaterial::PostSetParentNodeWork(UCustomizableObject* Object, const FGuid NodeId)
{
	UCustomizableObjectNode& Node = GetNode();
	const FCustomizableObjectNodeParentedMaterial& NodeParentedMaterial = GetNodeParentedMaterial();

	if (UCustomizableObjectNodeMaterial* ParentMaterialNode = NodeParentedMaterial.GetParentMaterialNode())
	{
		PostReconstructNodeDelegateHandler = ParentMaterialNode->PostReconstructNodeDelegate.AddUObject(&Node, &UCustomizableObjectNode::ReconstructNode);
		PostTextureParameterModeChangedDelegateHandle = ParentMaterialNode->PostImagePinModeChangedDelegate.AddUObject(&Node, &UCustomizableObjectNode::ReconstructNode);
	}
	
	GetNode().ReconstructNode();
}


void FCustomizableObjectNodeUseMaterial::PinConnectionListChangedWork(UEdGraphPin* Pin)
{
	if (Pin == OutputPin())
	{
		if (const TSharedPtr<ICustomizableObjectEditor> Editor = GetNode().GetGraphEditor())
		{
			Editor->UpdateGraphNodeProperties();
		}

		GetNode().ReconstructNode();
	}
}


void FCustomizableObjectNodeUseMaterial::CustomRemovePinWork(UEdGraphPin& Pin)
{
	TMap<FGuid, FEdGraphPinReference>& PinsParameter = GetPinsParameter();
	
	for(TMap<FGuid, FEdGraphPinReference>::TIterator It = PinsParameter.CreateIterator(); It; ++It)
	{
		if (It.Value().Get() == &Pin) // We could improve performance if FEdGraphPinReference exposed the pin id.
		{
			It.RemoveCurrent();
			break;
		}
	}
}


const UCustomizableObjectNode& FCustomizableObjectNodeUseMaterial::GetNode() const
{
	return const_cast<FCustomizableObjectNodeUseMaterial*>(this)->GetNode();
}


const TMap<FGuid, FEdGraphPinReference>& FCustomizableObjectNodeUseMaterial::GetPinsParameter() const
{
	return const_cast<FCustomizableObjectNodeUseMaterial*>(this)->GetPinsParameter();
}


bool FCustomizableObjectNodeUseMaterial::UsesImage(const FGuid& ImageId) const
{
	if (const UEdGraphPin* Pin = GetUsedImagePin(ImageId))
	{
		return FollowInputPin(*Pin) != nullptr;
	}
	else
	{
		return false;
	}
}


const UEdGraphPin* FCustomizableObjectNodeUseMaterial::GetUsedImagePin(const FGuid& ImageId) const
{
	const TMap<FGuid, FEdGraphPinReference>& PinsParameter = GetPinsParameter();

	if (const FEdGraphPinReference* PinReference = PinsParameter.Find(ImageId))
	{
		if (const UEdGraphPin& Pin = *PinReference->Get(); !IsPinOrphan(Pin)) // We always have a valid pin reference. If it is nullptr, it means that something went wrong.
		{
			return &Pin;
		}
	}
	
	return nullptr;
}


void FCustomizableObjectNodeUseMaterial::PostBackwardsCompatibleFixupWork()
{
	UCustomizableObjectNode& Node = GetNode();
	const FCustomizableObjectNodeParentedMaterial& NodeParentedMaterial = GetNodeParentedMaterial();

	if (UCustomizableObjectNodeMaterial* ParentMaterialNode = NodeParentedMaterial.GetParentMaterialNode())
	{
		PostReconstructNodeDelegateHandler = ParentMaterialNode->PostReconstructNodeDelegate.AddUObject(&Node, &UCustomizableObjectNode::ReconstructNode);
		PostTextureParameterModeChangedDelegateHandle = ParentMaterialNode->PostImagePinModeChangedDelegate.AddUObject(&Node, &UCustomizableObjectNode::ReconstructNode);
	}

	Node.ReconstructNode(); // Reconstruct the node since the parent could have changed while not loaded. 
}
