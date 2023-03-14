// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GraphTraversal.h"

#include "Containers/EnumAsByte.h"
#include "Containers/Set.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectPin.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEnumParameter.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExposePin.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExtendMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExternalPin.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialVariation.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshGeometryOperation.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorphStackApplication.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorphStackDefinition.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshReshape.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshSwitch.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshVariation.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/Nodes/CustomizableObjectNodeAnimationPose.h"
#include "Templates/Casts.h"
#include "Templates/Function.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectIterator.h"


/** Function to follow an External Pin node.
 *
 * @param Node Possible node to follow.
 * @param PinsToVisit Discovered pins (output parameter).
 * @return True if the node was a External Pin node (node followed).
 */
bool CastExternalPinNode(const UEdGraphNode& Node, TArray<const UEdGraphPin*>& PinsToVisit)
{
	const UCustomizableObjectNodeExternalPin* ExternalPinNode = Cast<UCustomizableObjectNodeExternalPin>(&Node);
	if (ExternalPinNode)
	{
		if (const UCustomizableObjectNodeExposePin* ExposePinNode = ExternalPinNode->GetNodeExposePin())
		{
			if (const UEdGraphPin* ExposePin = ExposePinNode->InputPin())
			{
				PinsToVisit.Add(ExposePin);
			}
		}
	}

	return ExternalPinNode != nullptr;
}


/** Function to follow an Expose Pin node.
 *
 * @param Node Possible node to follow.
 * @param PinsToVisit Discovered pins (output parameter).
 * @return True if the node was a Expose Pin node (node followed).
 */
bool CastExposePinNode(const UEdGraphNode& Node, TArray<const UEdGraphPin*>& PinsToVisit)
{
	const UCustomizableObjectNodeExposePin* ExposePinNode = Cast<UCustomizableObjectNodeExposePin>(&Node);
	if (ExposePinNode)
	{
		const UCustomizableObjectNodeExternalPin* NodeExternalPin = nullptr;
		for (TObjectIterator<UCustomizableObjectNodeExternalPin> It; It; ++It)
		{
			if ((*It)->GetNodeExposePin() == ExposePinNode)
			{
				NodeExternalPin = *It;
				break;
			}
		}
		
		if (NodeExternalPin)
		{
			if (const UEdGraphPin* ExternalPin = NodeExternalPin->GetExternalPin())
			{
				PinsToVisit.Add(ExternalPin);
			}
		}
	}

	return ExposePinNode != nullptr;
}


/** Follow the given pin returning its connected pin.
 *
 * - Skips all orphan pins.
 * - Follows External Pin nodes.
 *
 * @param Pin Pin to follow.
 * @param CycleDetected If provided, it will set to true if a cycle has been found.
 * @param CastNode Function to follow External Pin nodes (following an output pin) or Expose Pin nodes (following an input pin).
 * @param Direction Direction of the pin to explore.
 */
TArray<UEdGraphPin*> FollowPinArray(const UEdGraphPin& Pin, bool* CycleDetected, const EEdGraphPinDirection Direction, const TFunction<bool(const UEdGraphNode&, TArray<const UEdGraphPin*>&)> CastNode)
{
	check(Pin.Direction == Direction); // Can only follow input pins. To follow output pins see FollowOutputPin, but be aware of its limitations!

	if (CycleDetected)
	{
		*CycleDetected = false;
	}
	
	TArray<UEdGraphPin*> Result;
	
	TSet<const UEdGraphPin*> Visited;

	TArray<const UEdGraphPin*> PinsToVisit;
	PinsToVisit.Add(&Pin);
	while (PinsToVisit.Num())
	{
		const UEdGraphPin& CurrentPin = *PinsToVisit.Pop();

		if (IsPinOrphan(CurrentPin))
		{
			continue;
		}

		Visited.FindOrAdd(&CurrentPin, CycleDetected);
		if (CycleDetected && *CycleDetected)
		{
			continue;
		}

		for (UEdGraphPin* LinkedPin : CurrentPin.LinkedTo)
		{
			if (!IsPinOrphan(*LinkedPin))
			{
				if (!CastNode(*LinkedPin->GetOwningNode(), PinsToVisit))
				{
					Result.Add(LinkedPin);
				}
			}
		}
		
	}

	return Result;
}


TArray<UEdGraphPin*> FollowInputPinArray(const UEdGraphPin& Pin, bool* CycleDetected)
{
	return FollowPinArray(Pin, CycleDetected, EGPD_Input, &CastExternalPinNode);
}


UEdGraphPin* FollowInputPin(const UEdGraphPin& Pin, bool* CycleDetected)
{
	TArray<UEdGraphPin*> Result = FollowInputPinArray(Pin, CycleDetected);
	check(Result.Num() <= 1); // Use FollowPinImmersive if the pin can have more than one input.

	if (!Result.IsEmpty())
	{
		return Result[0];
	}
	else
	{
		return nullptr;
	}
}


TArray<UEdGraphPin*> FollowOutputPinArray(const UEdGraphPin& Pin, bool* CycleDetected)
{
	return FollowPinArray(Pin, CycleDetected, EGPD_Output, &CastExposePinNode);
}


UEdGraphPin* FollowOutputPin(const UEdGraphPin& Pin, bool* CycleDetected)
{
	TArray<UEdGraphPin*> Result = FollowOutputPinArray(Pin, CycleDetected);
	check(Result.Num() <= 1); // Use FollowPinImmersive if the pin can have more than one input.

	if (!Result.IsEmpty())
	{
		return Result[0];
	}
	else
	{
		return nullptr;
	}
}


UCustomizableObjectNodeObject* GetRootNode(UCustomizableObject* Object, bool& bOutMultipleBaseObjectsFound)
{
	// Look for the base object node
	UCustomizableObjectNodeObject* Root = nullptr;
	TArray<UCustomizableObjectNodeObject*> ObjectNodes;
	Object->Source->GetNodesOfClass<UCustomizableObjectNodeObject>(ObjectNodes);

	bOutMultipleBaseObjectsFound = false;

	for (TArray<UCustomizableObjectNodeObject*>::TIterator It(ObjectNodes); It; ++It)
	{
		if ((*It)->bIsBase)
		{
			if (Root)
			{
				bOutMultipleBaseObjectsFound = true;
				break;
			}
			else
			{
				Root = *It;
			}
		}
	}

	return Root;
}


bool GetParentsUntilRoot(UCustomizableObject* Object, TArray<UCustomizableObjectNodeObject*>& ArrayNodeObject, TArray<UCustomizableObject*>& ArrayCustomizableObject)
{
	bool MultipleBaseObjectsFound;
	UCustomizableObjectNodeObject* Root = GetRootNode(Object, MultipleBaseObjectsFound);

	bool bSuccess = true;

	if (!MultipleBaseObjectsFound && (Root != nullptr))
	{
		if (!ArrayCustomizableObject.Contains(Object))
		{
			ArrayNodeObject.Add(Root);
			ArrayCustomizableObject.Add(Object);
		}
		else
		{
			// This object has already been visted which means that there is a Cycle between Customizable Objects
			return false;
		}

		if (Root->ParentObject != nullptr)
		{
			bSuccess = GetParentsUntilRoot(Root->ParentObject, ArrayNodeObject, ArrayCustomizableObject);

			UCustomizableObjectNodeObject* ParentRoot = GetRootNode(Root->ParentObject, MultipleBaseObjectsFound);

			if (!MultipleBaseObjectsFound && (ParentRoot != nullptr))
			{
				Root->SetMeshComponentNumFromParent(ParentRoot->NumMeshComponents);
			}
		}
	}

	return bSuccess;
}


bool HasCandidateAsParent(UCustomizableObjectNodeObject* Node, UCustomizableObject* ParentCandidate)
{
	if (Node->ParentObject == ParentCandidate)
	{
		return true;
	}

	if (Node->ParentObject != nullptr)
	{
		bool MultipleBaseObjectsFound;
		UCustomizableObjectNodeObject* ParentNodeObject = GetRootNode(Node->ParentObject, MultipleBaseObjectsFound);

		if ((ParentNodeObject->ParentObject == nullptr) || MultipleBaseObjectsFound)
		{
			return false;
		}
		else
		{
			return HasCandidateAsParent(ParentNodeObject, ParentCandidate);
		}
	}

	return false;
}


UCustomizableObject* GetFullGraphRootObject(UCustomizableObjectNodeObject* Node, TArray<UCustomizableObject*>& VisitedObjects)
{
	if (Node->ParentObject != nullptr)
	{
		VisitedObjects.Add(Node->ParentObject);

		bool MultipleBaseObjectsFound;
		UCustomizableObjectNodeObject* Root = GetRootNode(Node->ParentObject, MultipleBaseObjectsFound);

		if (Root->ParentObject == nullptr)
		{
			if (MultipleBaseObjectsFound)
			{
				return nullptr;
			}
			else
			{
				return Node->ParentObject;
			}
		}
		else
		{
			if (VisitedObjects.Contains(Root->ParentObject))
			{
				//There is a cycle
				return nullptr;
			}
			else
			{
				return GetFullGraphRootObject(Root, VisitedObjects);
			}
		}
	}

	return nullptr;
}


UCustomizableObjectNodeObject* GetFullGraphRootNodeObject(UCustomizableObjectNodeObject* Node, TArray<UCustomizableObject*>& VisitedObjects)
{
	if (Node->ParentObject != nullptr)
	{
		VisitedObjects.Add(Node->ParentObject);

		bool MultipleBaseObjectsFound;
		UCustomizableObjectNodeObject* Root = GetRootNode(Node->ParentObject, MultipleBaseObjectsFound);

		if (Root->ParentObject == nullptr)
		{
			if (MultipleBaseObjectsFound)
			{
				return nullptr;
			}
			else
			{
				return Root;
			}
		}
		else
		{
			if (VisitedObjects.Contains(Root->ParentObject))
			{
				//There is a cycle
				return nullptr;
			}
			else
			{
				return GetFullGraphRootNodeObject(Root, VisitedObjects);
			}
		}
	}

	return nullptr;
}


const UEdGraphPin* FindMeshBaseSource(const UEdGraphPin& Pin, const bool bOnlyLookForStaticMesh)
{
	check(Pin.Direction == EGPD_Output);
	check(Pin.PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Mesh || Pin.PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Material);
	
	const UEdGraphNode* Node = Pin.GetOwningNode();
	check(Node);

	if (Cast<UCustomizableObjectNodeSkeletalMesh>(Node))
	{
		if (!bOnlyLookForStaticMesh)
		{
			return &Pin;
		}
	}

	else if (Cast<UCustomizableObjectNodeStaticMesh>(Node))
	{
		return &Pin;
	}

	else if (const UCustomizableObjectNodeMeshGeometryOperation* TypedNodeGeom = Cast<UCustomizableObjectNodeMeshGeometryOperation>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeGeom->MeshAPin()))
		{
			return FindMeshBaseSource(*ConnectedPin, bOnlyLookForStaticMesh);
		}
	}

	else if (const UCustomizableObjectNodeMeshReshape* TypedNodeReshape = Cast<UCustomizableObjectNodeMeshReshape>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeReshape->BaseMeshPin()))
		{
			return FindMeshBaseSource(*ConnectedPin, bOnlyLookForStaticMesh);
		}
	}

	else if (const UCustomizableObjectNodeMeshMorph* TypedNodeMorph = Cast<UCustomizableObjectNodeMeshMorph>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMorph->MeshPin()))
		{
			return FindMeshBaseSource(*ConnectedPin, bOnlyLookForStaticMesh);
		}
	}

	else if (const UCustomizableObjectNodeMeshSwitch* TypedNodeSwitch = Cast<UCustomizableObjectNodeMeshSwitch>(Node))
	{
		if (const UEdGraphPin* EnumParameterPin = FollowInputPin(*TypedNodeSwitch->SwitchParameter()))
		{
			if (const UCustomizableObjectNodeEnumParameter* EnumNode = Cast<UCustomizableObjectNodeEnumParameter>(EnumParameterPin->GetOwningNode()))
			{
				if (const UEdGraphPin* DefaultPin = TypedNodeSwitch->GetElementPin(EnumNode->DefaultIndex))
				{
					if (const UEdGraphPin* ConnectedPin = FollowInputPin(*DefaultPin))
					{
						return FindMeshBaseSource(*ConnectedPin, bOnlyLookForStaticMesh);
					}
				}
			}
		}
	}

	else if (const UCustomizableObjectNodeMeshVariation* TypedNodeMeshVar = Cast<UCustomizableObjectNodeMeshVariation>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMeshVar->DefaultPin()))
		{
			return FindMeshBaseSource(*ConnectedPin, bOnlyLookForStaticMesh);
		}

		for (int32 i = 0; i < TypedNodeMeshVar->GetNumVariations(); ++i)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMeshVar->VariationPin(i)))
			{
				return FindMeshBaseSource(*ConnectedPin, bOnlyLookForStaticMesh);
			}
		}
	}

	else if (const UCustomizableObjectNodeMaterial* TypedNodeMat = Cast<UCustomizableObjectNodeMaterial>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMat->GetMeshPin()))
		{
			return FindMeshBaseSource(*ConnectedPin, bOnlyLookForStaticMesh);
		}
	}

	else if (const UCustomizableObjectNodeMaterialVariation* TypedNodeMatVar = Cast<UCustomizableObjectNodeMaterialVariation>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMatVar->DefaultPin()))
		{
			return FindMeshBaseSource(*ConnectedPin, bOnlyLookForStaticMesh);
		}
	}

	else if (const UCustomizableObjectNodeExtendMaterial* TypedNodeExtend = Cast<UCustomizableObjectNodeExtendMaterial>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeExtend->AddMeshPin()))
		{
			return FindMeshBaseSource(*ConnectedPin, bOnlyLookForStaticMesh);
		}
	}
	
	else if (const UCustomizableObjectNodeMeshMorphStackDefinition* TypedNodeMorphStackDef = Cast<UCustomizableObjectNodeMeshMorphStackDefinition>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMorphStackDef->GetMeshPin()))
		{
			return FindMeshBaseSource(*ConnectedPin, bOnlyLookForStaticMesh);
		}
	}

	else if (const UCustomizableObjectNodeMeshMorphStackApplication* TypedNodeMorphStackApp = Cast<UCustomizableObjectNodeMeshMorphStackApplication>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMorphStackApp->GetMeshPin()))
		{
			return FindMeshBaseSource(*ConnectedPin, bOnlyLookForStaticMesh);
		}
	}

	else if (const UCustomizableObjectNodeMeshReshape* NodeMeshReshape = Cast<UCustomizableObjectNodeMeshReshape>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*NodeMeshReshape->BaseMeshPin()))
		{
			return FindMeshBaseSource(*ConnectedPin, bOnlyLookForStaticMesh);
		}
	}
	
	else if (Cast<UCustomizableObjectNodeTable>(Node))
	{
		if (!bOnlyLookForStaticMesh)
		{
			return &Pin;
		}
	}

	else if (const UCustomizableObjectNodeAnimationPose* NodeMeshPose = Cast<UCustomizableObjectNodeAnimationPose>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*NodeMeshPose->GetInputMeshPin()))
		{
			return FindMeshBaseSource(*ConnectedPin, bOnlyLookForStaticMesh);
		}
	}

	else
	{
		unimplemented(); // Case missing.
	}
	
	return nullptr;
}
