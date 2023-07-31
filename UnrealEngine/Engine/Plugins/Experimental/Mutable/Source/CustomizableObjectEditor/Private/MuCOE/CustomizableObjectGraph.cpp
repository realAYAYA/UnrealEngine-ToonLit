// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectGraph.h"

#include "Containers/Array.h"
#include "EdGraph/EdGraphNode.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "Templates/Casts.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectPtr.h"

class UObject;


UCustomizableObjectGraph::UCustomizableObjectGraph()
	: Super()
{
	Schema = UEdGraphSchema_CustomizableObject::StaticClass();
}


void UCustomizableObjectGraph::PostLoad()
{
	Super::PostLoad();

	// Make sure all nodes have finished loading.
	for (UEdGraphNode* Node : Nodes)
	{
		if (UCustomizableObjectNode* CustomizableObjectNode = Cast<UCustomizableObjectNode>(Node))
		{
			CustomizableObjectNode->ConditionalPostLoad();
		}
	}

	// Execute backwards compatible code for all nodes. It requires all nodes to be loaded.
	for (UEdGraphNode* Node : Nodes)
	{
		if (UCustomizableObjectNode* CustomizableObjectNode = Cast<UCustomizableObjectNode>(Node))
		{
			CustomizableObjectNode->BackwardsCompatibleFixup();
		}
	}

	// Do any additional work which require nodes to be valid (i.e., have executed BackwardsCompatibleFixup).
	for (UEdGraphNode* Node : Nodes)
	{
		if (UCustomizableObjectNode* CustomizableObjectNode = Cast<UCustomizableObjectNode>(Node))
		{
			CustomizableObjectNode->PostBackwardsCompatibleFixup();
		}
	}
}


void UCustomizableObjectGraph::NotifyNodeIdChanged(const FGuid& OldGuid, const FGuid& NewGuid)
{
	NotifiedNodeIdsMap.FindOrAdd(OldGuid) = NewGuid;

	if (TSet<FGuid>* NodesToNotify = NodesToNotifyMap.Find(OldGuid))
	{
		for (FGuid& NodeId : *NodesToNotify)
		{
			for (int32 i = 0; i < Nodes.Num(); ++i)
			{
				if (Nodes[i]->NodeGuid == NodeId)
				{
					if (UCustomizableObjectNode* Node = Cast<UCustomizableObjectNode>(Nodes[i]))
					{
						Node->UpdateReferencedNodeId(NewGuid);
					}
				}
			}
		}

		NodesToNotify->Empty();
	}
}

FGuid UCustomizableObjectGraph::RequestNotificationForNodeIdChange(const FGuid& OldGuid, const FGuid& NodeToNotifyGuid)
{
	if (const FGuid* Value = NotifiedNodeIdsMap.Find(OldGuid))
	{
		return *Value;
	}

	NodesToNotifyMap.FindOrAdd(OldGuid).Add(NodeToNotifyGuid);
	return OldGuid;
}


void UCustomizableObjectGraph::PostRename(UObject* OldOuter, const FName OldName)
{
	// Regenerate the Base Object Guid
	TArray<UCustomizableObjectNodeObject*> ObjectNodes;
	GetNodesOfClass<UCustomizableObjectNodeObject>(ObjectNodes);

	for (UCustomizableObjectNodeObject* ObjectNode : ObjectNodes)
	{
		if (ObjectNode->bIsBase)
		{
			ObjectNode->Identifier = FGuid::NewGuid();
			break;
		}
	}
}


void UCustomizableObjectGraph::PostDuplicate(bool bDuplicateForPIE)
{
	// In BeginPostDuplicate, nodes can call RequestNotificationForNodeIdChange
	for (UEdGraphNode* Node : Nodes)
	{
		if (UCustomizableObjectNode* CustomizableObjectNode = Cast<UCustomizableObjectNode>(Node))
		{
			CustomizableObjectNode->BeginPostDuplicate(bDuplicateForPIE);
		}
	}

	TMap<FGuid, FGuid> NewGuids;

	for (UEdGraphNode* Node : Nodes)
	{
		// Generate new Guid
		const FGuid NewGuid = FGuid::NewGuid();
		NewGuids.Add(Node->NodeGuid) = NewGuid;

		// Notify Guid is going to change. Recive the new Guid
		NotifyNodeIdChanged(Node->NodeGuid, NewGuid);
	}

	// Chenge all nodes Guids
	for (UEdGraphNode* Node : Nodes)
	{
		Node->NodeGuid = NewGuids[Node->NodeGuid];
	}

	Super::PostDuplicate(bDuplicateForPIE);
}
