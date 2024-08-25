// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreePropertyHelpers.h"
#include "StateTreeEditorNode.h"
#include "Hash/Blake3.h"
#include "Misc/StringBuilder.h"
#include "UObject/Field.h"

namespace UE::StateTree::PropertyHelpers
{

void DispatchPostEditToNodes(UObject& Owner, FPropertyChangedChainEvent& InPropertyChangedEvent)
{
	// Walk back from the changed property and look for first FStateTreeEditorNode, and call the node specific post edit methods.
	
	const TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* CurrentPropNode = InPropertyChangedEvent.PropertyChain.GetHead();
	const FProperty* HeadProperty = CurrentPropNode->GetValue();
	check(HeadProperty);
	if (HeadProperty->GetOwnerClass() != Owner.GetClass())
	{
		return;
	}
	
	uint8* CurrentAddress = reinterpret_cast<uint8*>(&Owner);
	while (CurrentPropNode)
	{
		const FProperty* CurrentProperty = CurrentPropNode->GetValue();
		check(CurrentProperty);
		CurrentAddress = CurrentAddress + CurrentProperty->GetOffset_ForInternal();

		while (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(CurrentProperty))
		{
			FScriptArrayHelper Helper(ArrayProperty, CurrentAddress);
			const int32 Index = InPropertyChangedEvent.GetArrayIndex(ArrayProperty->GetName());
			if (!Helper.IsValidIndex(Index))
			{
				return;
			}

			CurrentAddress = Helper.GetRawPtr(Index);
			CurrentProperty = ArrayProperty->Inner;
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(CurrentProperty))
		{
			if (StructProperty->Struct == FInstancedStruct::StaticStruct())
			{
				FInstancedStruct& InstancedStruct = *reinterpret_cast<FInstancedStruct*>(CurrentAddress);
				CurrentAddress = InstancedStruct.GetMutableMemory();
			}
			else if (StructProperty->Struct == FStateTreeEditorNode::StaticStruct())
			{
				FStateTreeEditorNode& EditorNode = *reinterpret_cast<FStateTreeEditorNode*>(CurrentAddress);
				if (FStateTreeNodeBase* StateTreeNode = EditorNode.Node.GetMutablePtr<FStateTreeNodeBase>())
				{
					// Check that the path contains EditorNode's: Node, Instance or Instance Object
					if (const TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* EditorNodeMemberPropNode = CurrentPropNode->GetNextNode())
					{
						// Check that we have a changed property on one of the above properties.
						if (const TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* ActiveMemberPropNode = EditorNodeMemberPropNode->GetNextNode()) 
						{
							// Update the event
							const FProperty* EditorNodeChildMember = EditorNodeMemberPropNode->GetValue();
							check(EditorNodeChildMember);

							// Take copy of the event, we'll modify it.
							FEditPropertyChain PropertyChainCopy;
							for (const TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* Node = InPropertyChangedEvent.PropertyChain.GetHead(); Node->GetNextNode(); Node = Node->GetNextNode())
							{
								PropertyChainCopy.AddTail(Node->GetValue());
							}
							FPropertyChangedChainEvent PropertyChangedEvent(PropertyChainCopy, InPropertyChangedEvent);

							PropertyChangedEvent.SetActiveMemberProperty(ActiveMemberPropNode->GetValue());
							PropertyChangedEvent.PropertyChain.SetActiveMemberPropertyNode(PropertyChangedEvent.MemberProperty);

							// To be consistent with the other property chain callbacks, do not cross object boundary.
							const TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* ActivePropNode = ActiveMemberPropNode;
							while (ActivePropNode->GetNextNode())
							{
								if (CastField<FObjectProperty>(ActivePropNode->GetValue()))
								{
									break;
								}
								ActivePropNode = ActivePropNode->GetNextNode();
							}
							
							PropertyChangedEvent.Property = ActivePropNode->GetValue();
							PropertyChangedEvent.PropertyChain.SetActivePropertyNode(PropertyChangedEvent.Property);

							if (EditorNodeChildMember->GetFName() == GET_MEMBER_NAME_CHECKED(FStateTreeEditorNode, Node))
							{
								StateTreeNode->PostEditNodeChangeChainProperty(PropertyChangedEvent, EditorNode.GetInstance());
							}
							else if (EditorNodeChildMember->GetFName() == GET_MEMBER_NAME_CHECKED(FStateTreeEditorNode, Instance))
							{
								if (EditorNode.Instance.IsValid())
								{
									StateTreeNode->PostEditInstanceDataChangeChainProperty(PropertyChangedEvent, FStateTreeDataView(EditorNode.Instance));
								}
							}
							else if (EditorNodeChildMember->GetFName() == GET_MEMBER_NAME_CHECKED(FStateTreeEditorNode, InstanceObject))
							{
								if (EditorNode.InstanceObject)
								{
									StateTreeNode->PostEditInstanceDataChangeChainProperty(PropertyChangedEvent, FStateTreeDataView(EditorNode.InstanceObject));
								}
							}
						}
					}
				}

				break;
			}

			CurrentPropNode = CurrentPropNode->GetNextNode();
		}
		else
		{
			break;
		}
	}
}


FGuid MakeDeterministicID(const UObject& Owner, const FString& PropertyPath, const uint64 Seed)
{
	// From FGuid::NewDeterministicGuid(FStringView ObjectPath, uint64 Seed)
	
	// Convert the objectpath to utf8 so that whether TCHAR is UTF8 or UTF16 does not alter the hash.
	TUtf8StringBuilder<1024> Utf8ObjectPath(InPlace, Owner.GetPathName());
	TUtf8StringBuilder<1024> Utf8PropertyPath(InPlace, PropertyPath);

	FBlake3 Builder;

	// Hash this as the namespace of the Version 3 UUID, to avoid collisions with any other guids created using Blake3.
	static FGuid BaseVersion(TEXT("bf324a38-a445-45a4-8921-249554b58189"));
	Builder.Update(&BaseVersion, sizeof(FGuid));
	Builder.Update(Utf8ObjectPath.GetData(), Utf8ObjectPath.Len() * sizeof(UTF8CHAR));
	Builder.Update(Utf8PropertyPath.GetData(), Utf8PropertyPath.Len() * sizeof(UTF8CHAR));
	Builder.Update(&Seed, sizeof(Seed));

	const FBlake3Hash Hash = Builder.Finalize();

	return FGuid::NewGuidFromHash(Hash);
}

bool HasOptionalMetadata(const FProperty& Property)
{
	return Property.HasMetaData(TEXT("Optional"));
}

}; // UE::StateTree::PropertyHelpers

// ------------------------------------------------------------------------------
// FStateTreeEditPropertyPath
// ------------------------------------------------------------------------------
FStateTreeEditPropertyPath::FStateTreeEditPropertyPath(const UStruct* BaseStruct, const FString& InPath)
{
	TArray<FString> PathSegments;
	InPath.ParseIntoArray(PathSegments, TEXT("."));

	const UStruct* CurrBase = BaseStruct;
	for (const FString& Segment : PathSegments)
	{
		const FName PropertyName(Segment);
		if (const FProperty* Property = CurrBase->FindPropertyByName(PropertyName))
		{
			Path.Emplace(Property, PropertyName);

			if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				Property = ArrayProperty->Inner;
			}

			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				CurrBase = StructProperty->Struct;
			}
			else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
			{
				CurrBase = ObjectProperty->PropertyClass;
			}
		}
		else
		{
			checkf(false, TEXT("Path %s id not part of type %s."), *InPath, *GetNameSafe(BaseStruct));
			Path.Reset();
			break;
		}
	}
}

FStateTreeEditPropertyPath::FStateTreeEditPropertyPath(const FPropertyChangedChainEvent& PropertyChangedEvent)
{
	FEditPropertyChain::TDoubleLinkedListNode* PropertyNode = PropertyChangedEvent.PropertyChain.GetActiveMemberNode();
	while (PropertyNode != nullptr)
	{
		if (FProperty* Property = PropertyNode->GetValue())
		{
			const FName PropertyName = Property->GetFName(); 
			const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(PropertyName.ToString());
			Path.Emplace(Property, PropertyName, ArrayIndex);
		}
		PropertyNode = PropertyNode->GetNextNode();
	}
}

FStateTreeEditPropertyPath::FStateTreeEditPropertyPath(const FEditPropertyChain& PropertyChain)
{
	FEditPropertyChain::TDoubleLinkedListNode* PropertyNode = PropertyChain.GetActiveMemberNode();
	while (PropertyNode != nullptr)
	{
		if (FProperty* Property = PropertyNode->GetValue())
		{
			const FName PropertyName = Property->GetFName(); 
			Path.Emplace(Property, PropertyName, INDEX_NONE);
		}
		PropertyNode = PropertyNode->GetNextNode();
	}
}

bool FStateTreeEditPropertyPath::ContainsPath(const FStateTreeEditPropertyPath& InPath) const
{
	if (InPath.Path.Num() > Path.Num())
    {
    	return false;
    }

    for (TConstEnumerateRef<FStateTreeEditPropertySegment> Segment : EnumerateRange(InPath.Path))
    {
    	if (Segment->PropertyName != Path[Segment.GetIndex()].PropertyName)
    	{
    		return false;
    	}
    }
    return true;
}

/** @return true if the property path is exactly the specified path. */
bool FStateTreeEditPropertyPath::IsPathExact(const FStateTreeEditPropertyPath& InPath) const
{
	if (InPath.Path.Num() != Path.Num())
	{
		return false;
	}

	for (TConstEnumerateRef<FStateTreeEditPropertySegment> Segment : EnumerateRange(InPath.Path))
	{
		if (Segment->PropertyName != Path[Segment.GetIndex()].PropertyName)
		{
			return false;
		}
	}
	return true;
}
