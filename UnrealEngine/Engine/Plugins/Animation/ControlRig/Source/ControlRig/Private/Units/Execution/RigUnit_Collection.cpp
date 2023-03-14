// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_Collection.h"
#include "Units/Execution/RigUnit_Item.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_Collection)

#if WITH_EDITOR
#include "Units/RigUnitTest.h"
#endif

#define FRigUnit_CollectionChain_Hash 1
#define FRigUnit_CollectionNameSearch_Hash 2
#define FRigUnit_CollectionChildren_Hash 3
#define FRigUnit_CollectionReplaceItems_Hash 4
#define FRigUnit_CollectionItems_Hash 5
#define FRigUnit_CollectionUnion_Hash 6
#define FRigUnit_CollectionIntersection_Hash 7
#define FRigUnit_CollectionDifference_Hash 8
#define FRigUnit_CollectionReverse_Hash 9
#define FRigUnit_CollectionCount_Hash 10
#define FRigUnit_CollectionItemAtIndex_Hash 11
#define FRigUnit_CollectionGetAll_Hash 12

FRigUnit_CollectionChain_Execute()
{
	FRigUnit_CollectionChainArray::StaticExecute(RigVMExecuteContext, FirstItem, LastItem, Reverse, Collection.Keys, Context);
}

FRigVMStructUpgradeInfo FRigUnit_CollectionChain::GetUpgradeInfo() const
{
	FRigUnit_CollectionChainArray NewNode;
	NewNode.FirstItem = FirstItem;
	NewNode.LastItem = LastItem;
	NewNode.Reverse = Reverse;

	return FRigVMStructUpgradeInfo(*this, NewNode);
}

FRigUnit_CollectionChainArray_Execute()
{
	uint32 Hash = FRigUnit_CollectionChain_Hash + Context.Hierarchy->GetTopologyVersion() * 17;
	Hash = HashCombine(Hash, GetTypeHash(FirstItem));
	Hash = HashCombine(Hash, GetTypeHash(LastItem));
	Hash = HashCombine(Hash, Reverse ? 1 : 0);

	FRigElementKeyCollection Collection;
	if(const FRigElementKeyCollection* Cache = Context.Hierarchy->FindCachedCollection(Hash))
	{
		Collection = *Cache;
	}
	else
	{
		Collection = FRigElementKeyCollection::MakeFromChain(Context.Hierarchy, FirstItem, LastItem, Reverse);
		if (Collection.IsEmpty())
		{
			if (Context.Hierarchy->GetIndex(FirstItem) == INDEX_NONE)
			{
				if(Context.State != EControlRigState::Init)
				{
					UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("First Item '%s' is not valid."), *FirstItem.ToString());
				}
			}
			if (Context.Hierarchy->GetIndex(LastItem) == INDEX_NONE)
			{
				if(Context.State != EControlRigState::Init)
				{
					UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Last Item '%s' is not valid."), *LastItem.ToString());
				}
			}
		}
		Context.Hierarchy->AddCachedCollection(Hash, Collection);
	}

	Items = Collection.Keys;
}

FRigUnit_CollectionNameSearch_Execute()
{
	FRigUnit_CollectionNameSearchArray::StaticExecute(RigVMExecuteContext, PartialName, TypeToSearch, Collection.Keys, Context);
}

FRigVMStructUpgradeInfo FRigUnit_CollectionNameSearch::GetUpgradeInfo() const
{
	FRigUnit_CollectionNameSearchArray NewNode;
	NewNode.PartialName = PartialName;
	NewNode.TypeToSearch = TypeToSearch;

	return FRigVMStructUpgradeInfo(*this, NewNode);
}

FRigUnit_CollectionNameSearchArray_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	uint32 Hash = FRigUnit_CollectionNameSearch_Hash + Context.Hierarchy->GetTopologyVersion() * 17;
	Hash = HashCombine(Hash, GetTypeHash(PartialName));
	Hash = HashCombine(Hash, (int32)TypeToSearch * 8);

	FRigElementKeyCollection Collection;
	if(const FRigElementKeyCollection* Cache = Context.Hierarchy->FindCachedCollection(Hash))
	{
		Collection = *Cache;
	}
	else
	{
		Collection = FRigElementKeyCollection::MakeFromName(Context.Hierarchy, PartialName, (uint8)TypeToSearch);
		Context.Hierarchy->AddCachedCollection(Hash, Collection);
	}

	Items = Collection.Keys;
}

FRigUnit_CollectionChildren_Execute()
{
	FRigUnit_CollectionChildrenArray::StaticExecute(RigVMExecuteContext, Parent, bIncludeParent, bRecursive, TypeToSearch, Collection.Keys, Context);
}

FRigVMStructUpgradeInfo FRigUnit_CollectionChildren::GetUpgradeInfo() const
{
	FRigUnit_CollectionChildrenArray NewNode;
	NewNode.Parent = Parent;
	NewNode.bIncludeParent = bIncludeParent;
	NewNode.bRecursive = bRecursive;
	NewNode.TypeToSearch = TypeToSearch;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("Collection.Keys"), TEXT("Items"));
	return Info;
}

FRigUnit_CollectionChildrenArray_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	uint32 Hash = FRigUnit_CollectionChildren_Hash + Context.Hierarchy->GetTopologyVersion() * 17;
	Hash = HashCombine(Hash, GetTypeHash(Parent));
	Hash = HashCombine(Hash, bRecursive ? 2 : 0);
	Hash = HashCombine(Hash, bIncludeParent ? 1 : 0);
	Hash = HashCombine(Hash, (int32)TypeToSearch * 8);

	FRigElementKeyCollection Collection;
	if(const FRigElementKeyCollection* Cache = Context.Hierarchy->FindCachedCollection(Hash))
	{
		Collection = *Cache;
	}
	else
	{
		Collection = FRigElementKeyCollection::MakeFromChildren(Context.Hierarchy, Parent, bRecursive, bIncludeParent, (uint8)TypeToSearch);
		if (Collection.IsEmpty())
		{
			if (Context.Hierarchy->GetIndex(Parent) == INDEX_NONE)
			{
				UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Parent '%s' is not valid."), *Parent.ToString());
			}
		}
		Context.Hierarchy->AddCachedCollection(Hash, Collection);
	}

	Items = Collection.Keys;
}

FRigUnit_CollectionGetAll_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	uint32 Hash = FRigUnit_CollectionGetAll_Hash + Context.Hierarchy->GetTopologyVersion() * 17;
	Hash = HashCombine(Hash, (int32)TypeToSearch * 8);

	FRigElementKeyCollection Collection;
	if(const FRigElementKeyCollection* Cache = Context.Hierarchy->FindCachedCollection(Hash))
	{
		Collection = *Cache;
	}
	else
	{
		Context.Hierarchy->Traverse([&Collection, TypeToSearch](FRigBaseElement* InElement, bool &bContinue)
		{
			bContinue = true;
			
			const FRigElementKey Key = InElement->GetKey();
			if(((uint8)TypeToSearch & (uint8)Key.Type) == (uint8)Key.Type)
			{
				Collection.AddUnique(Key);
			}
		});
		
		if (Collection.IsEmpty())
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("%s"), TEXT("No elements found for given filter."));
		}
		Context.Hierarchy->AddCachedCollection(Hash, Collection);
	}

	Items = Collection.Keys;
}

#if WITH_EDITOR

IMPLEMENT_RIGUNIT_AUTOMATION_TEST(FRigUnit_CollectionChildren)
{
	const FRigElementKey Root = Controller->AddBone(TEXT("Root"), FRigElementKey(), FTransform(FVector(0.f, 0.f, 0.f)), true, ERigBoneType::User);
	const FRigElementKey BoneA = Controller->AddBone(TEXT("BoneA"), Root, FTransform(FVector(0.f, 0.f, 0.f)), true, ERigBoneType::User);
	const FRigElementKey BoneB = Controller->AddBone(TEXT("BoneB"), BoneA, FTransform(FVector(0.f, 0.f, 0.f)), true, ERigBoneType::User);
	const FRigElementKey BoneC = Controller->AddBone(TEXT("BoneC"), Root, FTransform(FVector(0.f, 0.f, 0.f)), true, ERigBoneType::User);

	Unit.Parent = Root;
	Unit.bIncludeParent = false;
	Unit.bRecursive = false;
	Execute();
	AddErrorIfFalse(Unit.Collection.Num() == 2, TEXT("unexpected result"));
	AddErrorIfFalse(Unit.Collection[0] == BoneA, TEXT("unexpected result"));
	AddErrorIfFalse(Unit.Collection[1] == BoneC, TEXT("unexpected result"));

	Unit.bIncludeParent = true;
	Unit.bRecursive = false;
	Execute();
	AddErrorIfFalse(Unit.Collection.Num() == 3, TEXT("unexpected result"));
	AddErrorIfFalse(Unit.Collection[0] == Root, TEXT("unexpected result"));
	AddErrorIfFalse(Unit.Collection[1] == BoneA, TEXT("unexpected result"));
	AddErrorIfFalse(Unit.Collection[2] == BoneC, TEXT("unexpected result"));

	Unit.bIncludeParent = true;
	Unit.bRecursive = true;
	Execute();
	AddErrorIfFalse(Unit.Collection.Num() == 4, TEXT("unexpected result"));
	AddErrorIfFalse(Unit.Collection[0] == Root, TEXT("unexpected result"));
	AddErrorIfFalse(Unit.Collection[1] == BoneA, TEXT("unexpected result"));
	AddErrorIfFalse(Unit.Collection[2] == BoneC, TEXT("unexpected result"));
	AddErrorIfFalse(Unit.Collection[3] == BoneB, TEXT("unexpected result"));

	return true;
}

#endif

FRigUnit_CollectionReplaceItems_Execute()
{
	FRigUnit_CollectionReplaceItemsArray::StaticExecute(RigVMExecuteContext, Items.Keys, Old, New, RemoveInvalidItems, bAllowDuplicates, Collection.Keys, Context);
}

FRigVMStructUpgradeInfo FRigUnit_CollectionReplaceItems::GetUpgradeInfo() const
{
	FRigUnit_CollectionReplaceItemsArray NewNode;
	NewNode.Items = Items.Keys;
	NewNode.Old = Old;
	NewNode.New = New;
	NewNode.RemoveInvalidItems = RemoveInvalidItems;
	NewNode.bAllowDuplicates = bAllowDuplicates;

	return FRigVMStructUpgradeInfo(*this, NewNode);
}

FRigUnit_CollectionReplaceItemsArray_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	uint32 Hash = FRigUnit_CollectionReplaceItems_Hash + Context.Hierarchy->GetTopologyVersion() * 17;
	Hash = HashCombine(Hash, GetTypeHash(Items));
	Hash = HashCombine(Hash, 12 * GetTypeHash(Old));
	Hash = HashCombine(Hash, 13 * GetTypeHash(New));
	Hash = HashCombine(Hash, RemoveInvalidItems ? 14 : 0);

	FRigElementKeyCollection Collection;
	if(const FRigElementKeyCollection* Cache = Context.Hierarchy->FindCachedCollection(Hash))
	{
		Collection = *Cache;
	}
	else
	{
		Collection.Reset();
		
		for (int32 Index = 0; Index < Items.Num(); Index++)
		{
			FRigElementKey Key = Items[Index];
			FRigUnit_ItemReplace::StaticExecute(RigVMExecuteContext, Key, Old, New, Key, Context);

			if (Context.Hierarchy->GetIndex(Key) != INDEX_NONE)
			{
				if(bAllowDuplicates)
				{
					Collection.Add(Key);
				}
				else
				{
					Collection.AddUnique(Key);
				}
			}
			else if(!RemoveInvalidItems)
			{
				Collection.Add(FRigElementKey());
			}
		}

		Context.Hierarchy->AddCachedCollection(Hash, Collection);
	}
	
	Result = Collection.Keys;
}

FRigUnit_CollectionItems_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	Collection.Reset();
	for (const FRigElementKey& Key : Items)
	{
		if(bAllowDuplicates)
		{
			Collection.Add(Key);
		}
		else
		{
			Collection.AddUnique(Key);
		}
	}
}

FRigUnit_CollectionGetItems_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	Items = Collection.GetKeys();
}

FRigUnit_CollectionGetParentIndices_Execute()
{
	FRigUnit_CollectionGetParentIndicesItemArray::StaticExecute(RigVMExecuteContext, Collection.Keys, ParentIndices, Context);
}

FRigVMStructUpgradeInfo FRigUnit_CollectionGetParentIndices::GetUpgradeInfo() const
{
	FRigUnit_CollectionGetParentIndicesItemArray NewNode;
	NewNode.Items = Collection.Keys;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("Collection"), TEXT("Items"));
	return Info;
}

FRigUnit_CollectionGetParentIndicesItemArray_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	if(Context.Hierarchy == nullptr)
	{
		ParentIndices.Reset();
		return;
	}
	
	ParentIndices.SetNumUninitialized(Items.Num());

	for(int32 Index=0;Index<Items.Num();Index++)
	{
		ParentIndices[Index] = INDEX_NONE;

		const int32 ItemIndex = Context.Hierarchy->GetIndex(Items[Index]);
		if(ItemIndex == INDEX_NONE)
		{
			continue;;
		}

		switch(Items[Index].Type)
		{
			case ERigElementType::Curve:
			{
				continue;
			}
			case ERigElementType::Bone:
			{
				ParentIndices[Index] = Context.Hierarchy->GetFirstParent(ItemIndex);
				break;
			}
			default:
			{
				if(const FRigBaseElement* ChildElement = Context.Hierarchy->Get(ItemIndex))
				{
					TArray<int32> ItemParents = Context.Hierarchy->GetParents(ItemIndex);
					for(int32 ParentIndex = 0; ParentIndex < ItemParents.Num(); ParentIndex++)
					{
						const FRigElementWeight Weight = Context.Hierarchy->GetParentWeight(ChildElement, ParentIndex, false);
						if(!Weight.IsAlmostZero())
						{
							ParentIndices[Index] = ItemParents[ParentIndex];
						}
					}
				}
				break;
			}
		}

		if(ParentIndices[Index] != INDEX_NONE)
		{
			int32 ParentIndex = ParentIndices[Index];
			ParentIndices[Index] = INDEX_NONE;

			while(ParentIndices[Index] == INDEX_NONE && ParentIndex != INDEX_NONE)
			{
				ParentIndices[Index] = Items.Find(Context.Hierarchy->GetKey(ParentIndex));
				ParentIndex = Context.Hierarchy->GetFirstParent(ParentIndex);
			}
		}
	}
}

FRigUnit_CollectionUnion_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	Collection = FRigElementKeyCollection::MakeUnion(A, B, bAllowDuplicates);
}

FRigVMStructUpgradeInfo FRigUnit_CollectionUnion::GetUpgradeInfo() const
{
	// this node is no longer supported. you can rely on generic array nodes for this now
	return FRigVMStructUpgradeInfo();
}

FRigUnit_CollectionIntersection_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	Collection = FRigElementKeyCollection::MakeIntersection(A, B);
}

FRigVMStructUpgradeInfo FRigUnit_CollectionIntersection::GetUpgradeInfo() const
{
	// this node is no longer supported. you can rely on generic array nodes for this now
	return FRigVMStructUpgradeInfo();
}

FRigUnit_CollectionDifference_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	Collection = FRigElementKeyCollection::MakeDifference(A, B);
}

FRigVMStructUpgradeInfo FRigUnit_CollectionDifference::GetUpgradeInfo() const
{
	// this node is no longer supported. you can rely on generic array nodes for this now
	return FRigVMStructUpgradeInfo();
}

FRigUnit_CollectionReverse_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	Reversed = FRigElementKeyCollection::MakeReversed(Collection);
}

FRigVMStructUpgradeInfo FRigUnit_CollectionReverse::GetUpgradeInfo() const
{
	// this node is no longer supported. you can rely on generic array nodes for this now
	return FRigVMStructUpgradeInfo();
}

FRigUnit_CollectionCount_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	Count = Collection.Num();
}

FRigVMStructUpgradeInfo FRigUnit_CollectionCount::GetUpgradeInfo() const
{
	// this node is no longer supported. you can rely on generic array nodes for this now
	return FRigVMStructUpgradeInfo();
}

FRigUnit_CollectionItemAtIndex_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	if (Collection.IsValidIndex(Index))
	{
		Item = Collection[Index];
	}
	else
	{
		Item = FRigElementKey();
	}
}

FRigVMStructUpgradeInfo FRigUnit_CollectionItemAtIndex::GetUpgradeInfo() const
{
	// this node is no longer supported. you can rely on generic array nodes for this now
	return FRigVMStructUpgradeInfo();
}

FRigUnit_CollectionLoop_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
    Count = Collection.Num();
   	Continue = Collection.IsValidIndex(Index);
	Ratio = GetRatioFromIndex(Index, Count);

	if(Continue)
	{
		Item = Collection[Index];
	}
	else
	{
		Item = FRigElementKey();
	}
}

FRigVMStructUpgradeInfo FRigUnit_CollectionLoop::GetUpgradeInfo() const
{
	// this node is no longer supported. you can rely on generic array nodes for this now
	return FRigVMStructUpgradeInfo();
}

FRigUnit_CollectionAddItem_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT();

	Result = Collection;
	Result.AddUnique(Item);
}

FRigVMStructUpgradeInfo FRigUnit_CollectionAddItem::GetUpgradeInfo() const
{
	// this node is no longer supported. you can rely on generic array nodes for this now
	return FRigVMStructUpgradeInfo();
}
