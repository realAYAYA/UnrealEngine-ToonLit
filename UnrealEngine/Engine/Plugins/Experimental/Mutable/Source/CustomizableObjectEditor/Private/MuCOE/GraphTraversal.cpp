// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GraphTraversal.h"

#include "AssetRegistry/AssetRegistryModule.h"
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
#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"
#include "MuCOE/Nodes/CustomizableObjectNodeReroute.h"
#include "UObject/UObjectIterator.h"


/** Follow the given pin returning its connected pin.
 *
 * - Skips all orphan pins.
 * - Follows External Pin and Reroute nodes.
 *
 * @param Pin Pin to follow.
 * @param bOutCycleDetected If provided, it will set to true if a cycle has been found.
 * @param Direction Direction of the pin to explore.  */
TArray<UEdGraphPin*> FollowPinArray(const UEdGraphPin& Pin, bool* bOutCycleDetected, const EEdGraphPinDirection Direction)
{
	check(Pin.Direction == Direction); // Can only follow input pins. To follow output pins see FollowOutputPin, but be aware of its limitations!

	bool bCycleDetected = false;
		
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

		Visited.FindOrAdd(&CurrentPin, &bCycleDetected);
		if (bCycleDetected)
		{
			continue;
		}

		for (UEdGraphPin* LinkedPin : CurrentPin.LinkedTo)
		{
			if (IsPinOrphan(*LinkedPin))
			{
				continue;
			}
			
			if (const UCustomizableObjectNodeExposePin* ExposePinNode = Cast<UCustomizableObjectNodeExposePin>(LinkedPin->GetOwningNode()))
			{
				check(Direction == EGPD_Output);

				const UCustomizableObjectNodeExternalPin* LinkedNode = nullptr;
				for (TObjectIterator<UCustomizableObjectNodeExternalPin> It; It; ++It)
				{
					if (IsValid(*It) && (*It)->GetNodeExposePin() == ExposePinNode)
					{
						LinkedNode = *It;
						break;
					}
				}
	
				if (LinkedNode)
				{
					if (const UEdGraphPin* ExternalPin = LinkedNode->GetExternalPin())
					{
						PinsToVisit.Add(ExternalPin);
					}
				}
			}
			else if (const UCustomizableObjectNodeExternalPin* ExternalPinNode = Cast<UCustomizableObjectNodeExternalPin>(LinkedPin->GetOwningNode()))
			{
				check(Direction == EGPD_Input);
				
				if (const UCustomizableObjectNodeExposePin* LinkedNode = ExternalPinNode->GetNodeExposePin())
				{
					if (const UEdGraphPin* ExposePin = LinkedNode->InputPin())
					{
						PinsToVisit.Add(ExposePin);
					}
				}
			}
			else if (const UCustomizableObjectNodeReroute* NodeReroute = Cast<UCustomizableObjectNodeReroute>(LinkedPin->GetOwningNode()))
			{
				PinsToVisit.Add(Direction == EGPD_Input ? NodeReroute->GetInputPin() : NodeReroute->GetOutputPin());
			}
			else
			{
				Result.Add(LinkedPin);
			}
		}
	}

	if (bOutCycleDetected)
	{
		*bOutCycleDetected = bCycleDetected;	
	}

	return Result;
}


TArray<UEdGraphPin*> FollowInputPinArray(const UEdGraphPin& Pin, bool* bOutCycleDetected)
{
	return FollowPinArray(Pin, bOutCycleDetected, EGPD_Input);
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


TArray<UEdGraphPin*> FollowOutputPinArray(const UEdGraphPin& Pin, bool* bOutCycleDetected)
{
	return FollowPinArray(Pin, bOutCycleDetected, EGPD_Output);
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


UCustomizableObject* GetRootObject(const UCustomizableObjectNode& Node)
{
	return CastChecked<UCustomizableObject>(Node.GetGraph()->GetOuter());
}


UCustomizableObject* GetRootObject(UCustomizableObject* ChildObject)
{
	// Grab a node to start the search -> Get the root since it should be always present
	bool bMultipleBaseObjectsFound = false;
	UCustomizableObjectNodeObject* ObjectRootNode = GetRootNode(ChildObject, bMultipleBaseObjectsFound);

	if (ObjectRootNode && ObjectRootNode->ParentObject)
	{
		TArray<UCustomizableObject*> VisitedNodes;
		return GetFullGraphRootObject(ObjectRootNode, VisitedNodes);
	}

	// No parent object found, return input as the parent of the graph
	// This can also mean the ObjectRootNode does not exist because it has not been opened yet (so no nodes have been generated)
	return ChildObject;
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


void GetNodeGroupObjectNodeMappingImmersive(UCustomizableObject* Object, FAssetRegistryModule& AssetRegistryModule, TSet<UCustomizableObject*>& Visited, TMultiMap<FGuid, UCustomizableObjectNodeObject*>& Mapping)
{
	Visited.Add(Object);

	TArray<FName> ArrayReferenceNames;
	AssetRegistryModule.Get().GetReferencers(*Object->GetOuter()->GetPathName(), ArrayReferenceNames, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);

	FARFilter Filter;
	for (const FName& ReferenceName : ArrayReferenceNames)
	{
		if (!ReferenceName.ToString().StartsWith(TEXT("/TempAutosave")))
		{
			Filter.PackageNames.Add(ReferenceName);
		}
	}

	Filter.bIncludeOnlyOnDiskAssets = false;
	
	TArray<FAssetData> ArrayAssetData;
	AssetRegistryModule.Get().GetAssets(Filter, ArrayAssetData);

	for (FAssetData& AssetData : ArrayAssetData)
	{
		UCustomizableObject* ChildObject = Cast<UCustomizableObject>(AssetData.GetAsset());
		if (!ChildObject)
		{
			continue;			
		}

		if (ChildObject != Object && !ChildObject->HasAnyFlags(RF_Transient))
		{
			bool bMultipleBaseObjectsFound = false;
			UCustomizableObjectNodeObject* ChildRoot = GetRootNode(ChildObject, bMultipleBaseObjectsFound);

			if (ChildRoot && !bMultipleBaseObjectsFound)
			{
				if (ChildRoot->ParentObject == Object)
				{
					Mapping.Add(ChildRoot->ParentObjectGroupId, ChildRoot);
				}
			}
		}

		if (!Visited.Contains(ChildObject))
		{
			GetNodeGroupObjectNodeMappingImmersive(ChildObject, AssetRegistryModule, Visited, Mapping);
		}
	}
}


TMultiMap<FGuid, UCustomizableObjectNodeObject*> GetNodeGroupObjectNodeMapping(UCustomizableObject* Object)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TSet<UCustomizableObject*> Visited;
	TMultiMap<FGuid, UCustomizableObjectNodeObject*> Mapping;

	GetNodeGroupObjectNodeMappingImmersive(Object, AssetRegistryModule, Visited, Mapping);
	
	return Mapping;
}