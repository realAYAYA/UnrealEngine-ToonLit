// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "Templates/UnrealTemplate.h"
#include "Chaos/ChaosArchive.h"
#include "UObject/UE5MainStreamObjectVersion.h"

DEFINE_LOG_CATEGORY_STATIC(FManagedArrayCollectionLogging, NoLogging, All);

int8 FManagedArrayCollection::Invalid = INDEX_NONE;


FManagedArrayCollection::FManagedArrayCollection()
{
	Version = 9;
}


void FManagedArrayCollection::AddGroup(FName Group)
{
	ensure(!GroupInfo.Contains(Group));
	FGroupInfo info{
		0
	};
	GroupInfo.Add(Group, info);
}

void FManagedArrayCollection::RemoveElements(const FName& Group, const TArray<int32>& SortedDeletionList, FProcessingParameters Params)
{
	if (SortedDeletionList.Num())
	{
		int32 GroupSize = NumElements(Group);
		int32 DelListNum = SortedDeletionList.Num();
		GeometryCollectionAlgo::ValidateSortedList(SortedDeletionList, GroupSize);
		ensure(GroupSize >= DelListNum);

		TArray<int32> Offsets;
		GeometryCollectionAlgo::BuildIncrementMask(SortedDeletionList, GroupSize, Offsets);

		TSet<int32> DeletionSet(SortedDeletionList);
		for (const TTuple<FKeyType, FValueType>& Entry : Map)
		{
			//
			// Reindex attributes dependent on the group being resized
			//
			if (Entry.Value.GroupIndexDependency == Group && Params.bReindexDependentAttibutes)
			{
				Entry.Value.Value->Reindex(Offsets, GroupSize - DelListNum, SortedDeletionList, DeletionSet);
			}

			//
			//  Resize the array and clobber deletion indices
			//
			if (Entry.Key.Get<1>() == Group)
			{
				Entry.Value.Value->RemoveElements(SortedDeletionList);
			}

		}
		GroupInfo[Group].Size -= DelListNum;
	}
}

void FManagedArrayCollection::RemoveElements(const FName& Group, int32 NumberElements, int32 Position)
{
	TArray<int32> SortedDeletionList;
	SortedDeletionList.SetNumUninitialized(NumberElements);
	for (int32 Idx = 0; Idx < NumberElements; ++Idx)
	{
		SortedDeletionList[Idx] = Position + Idx;
	}
	RemoveElements(Group, SortedDeletionList);
}

TArray<FName> FManagedArrayCollection::GroupNames() const
{
	TArray<FName> keys;
	if (GroupInfo.Num())
	{
		GroupInfo.GetKeys(keys);
	}
	return keys;
}

bool FManagedArrayCollection::HasAttribute(FName Name, FName Group) const
{
	FKeyType Key = FManagedArrayCollection::MakeMapKey(Name, Group);
	return Map.Contains(Key);
}

bool FManagedArrayCollection::HasAttributes(const TArray<FManagedArrayCollection::FManagedType>& Types) const
{
	for (const FManagedType& ManagedType : Types)
	{
		FKeyType Key = FManagedArrayCollection::MakeMapKey(ManagedType.Name, ManagedType.Group);
		if(Map.Contains(Key))
		{
			const FValueType& FoundValue = Map[Key];

			if (FoundValue.ArrayType != ManagedType.Type)
			{
				return false;
			}
		}
	}
	return true;
}


bool FManagedArrayCollection::IsAttributeDirty(FName Name, FName Group) const
{
	const FKeyType Key = FManagedArrayCollection::MakeMapKey(Name, Group);
	if (const FValueType* Attribute = Map.Find(Key))
	{
		return (Attribute->Value)? Attribute->Value->IsDirty(): false;
	}
	return false;
}

bool FManagedArrayCollection::IsAttributePersistent(FName Name, FName Group) const
{
	const FKeyType Key = FManagedArrayCollection::MakeMapKey(Name, Group);
	if (const FValueType* Attribute = Map.Find(Key))
	{
		return Attribute->Saved;
	}
	return false;
}

TArray<FName> FManagedArrayCollection::AttributeNames(FName Group) const
{
	TArray<FName> AttributeNames;
	for (const TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			AttributeNames.Add(Entry.Key.Get<0>());
		}
	}
	return AttributeNames;
}

int32 FManagedArrayCollection::NumElements(FName Group) const
{
	int32 Num = 0;
	if (GroupInfo.Contains(Group))
	{
		Num = GroupInfo[Group].Size;
	}
	return Num;
}

int32 FManagedArrayCollection::AddElements(int32 NumberElements, FName Group)
{
	int32 StartSize = 0;
	if (!GroupInfo.Contains(Group))
	{
		AddGroup(Group);
	}

	StartSize = GroupInfo[Group].Size;
	for (TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			Entry.Value.Value->Resize(StartSize + NumberElements);
		}
	}
	GroupInfo[Group].Size += NumberElements;

	SetDefaults(Group, StartSize, NumberElements);

	return StartSize;
}

int32 FManagedArrayCollection::InsertElements(int32 NumberElements, int32 Position, FName Group)
{
	const int32 OldGroupSize = AddElements(NumberElements, Group);
	const int32 NewGroupSize = OldGroupSize + NumberElements;
	check(Position <= OldGroupSize);
	const int32 NumberElementsToMove = OldGroupSize - Position;
	const int32 MoveToPosition = Position + NumberElements;

	TArray<int32> NewOrder;
	NewOrder.SetNumUninitialized(NewGroupSize);

	for (int32 Idx = 0; Idx < Position; ++Idx)
	{
		NewOrder[Idx] = Idx;
	}
	for (int32 Idx = Position; Idx < MoveToPosition; ++Idx)
	{
		NewOrder[Idx] = Idx + NumberElementsToMove;
	}
	for (int32 Idx = MoveToPosition; Idx < NewGroupSize; ++Idx)
	{
		NewOrder[Idx] = Idx - NumberElements;
	}

	ReorderElements(Group, NewOrder);

	return Position;
}

void FManagedArrayCollection::RemoveAttribute(FName Name, FName Group)
{
	FKeyType Key = FManagedArrayCollection::MakeMapKey(Name, Group);
	Map.Remove(Key);
}

void FManagedArrayCollection::RemoveGroup(FName Group)
{
	TArray<FName> DelList;
	for (TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			DelList.Add(Entry.Key.Get<0>());
		}
		if (Entry.Value.GroupIndexDependency == Group)
		{
			Entry.Value.GroupIndexDependency = "";
		}
	}
	for (const FName& AttrName : DelList)
	{
		Map.Remove(FManagedArrayCollection::MakeMapKey(AttrName, Group));
	}

	GroupInfo.Remove(Group);
}

void FManagedArrayCollection::Resize(int32 Size, FName Group)
{
	ensure(HasGroup(Group));
	const int32 CurSize = NumElements(Group);
	if (CurSize == Size)
	{
		return;
	}

	ensureMsgf(Size > NumElements(Group), TEXT("Use RemoveElements to shrink a group."));
	const int32 StartSize = GroupInfo[Group].Size;
	for (TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			Entry.Value.Value->Resize(Size);
		}
	}
	GroupInfo[Group].Size = Size;
}

void FManagedArrayCollection::Reserve(int32 Size, FName Group)
{
	ensure(HasGroup(Group));
	const int32 CurSize = NumElements(Group);
	if (CurSize >= Size)
	{
		return;
	}

	const int32 StartSize = GroupInfo[Group].Size;
	for (TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			Entry.Value.Value->Reserve(Size);
		}
	}
}

void FManagedArrayCollection::EmptyGroup(FName Group)
{
	ensure(HasGroup(Group));

	for (TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			Entry.Value.Value->Empty();
		}
	}

	GroupInfo[Group].Size = 0;
}

void FManagedArrayCollection::ReorderElements(FName Group, const TArray<int32>& NewOrder)
{
	const int32 GroupSize = GroupInfo[Group].Size;
	check(GroupSize == NewOrder.Num());

	TArray<int32> InverseNewOrder;
	InverseNewOrder.Init(-1, GroupSize);
	for (int32 Idx = 0; Idx < GroupSize; ++Idx)
	{
		InverseNewOrder[NewOrder[Idx]] = Idx;
	}

	for (TTuple<FKeyType, FValueType>& Entry : Map)
	{
		// Reindex attributes dependent on the group being reordered
		if (Entry.Value.GroupIndexDependency == Group)
		{
			Entry.Value.Value->ReindexFromLookup(InverseNewOrder);
		}

		if (Entry.Key.Get<1>() == Group)
		{
			Entry.Value.Value->Reorder(NewOrder);
		}
	}
}

void FManagedArrayCollection::SetDependency(FName Name, FName Group, FName DependencyGroup)
{
	ensure(HasAttribute(Name, Group));
	if (ensure(!HasCycle(Group, DependencyGroup)))
	{
		FKeyType Key = FManagedArrayCollection::MakeMapKey(Name, Group);
		Map[Key].GroupIndexDependency = DependencyGroup;
	}
}

void FManagedArrayCollection::RemoveDependencyFor(FName Group)
{
	ensure(HasGroup(Group));
	for (TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Value.GroupIndexDependency == Group)
		{
			Entry.Value.GroupIndexDependency = "";
		}
	}
}

void FManagedArrayCollection::SyncGroupSizeFrom(const FManagedArrayCollection& InCollection, FName Group)
{
	if (!HasGroup(Group))
	{
		AddGroup(Group);
	}

	Resize(InCollection.GroupInfo[Group].Size, Group);
}

void FManagedArrayCollection::CopyMatchingAttributesFrom(
	const FManagedArrayCollection& InCollection,
	const TMap<FName, TSet<FName>>* SkipList)
{
	for (const auto& Pair : InCollection.GroupInfo)
	{
		SyncGroupSizeFrom(InCollection, Pair.Key);
	}
	for (TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (SkipList)
		{
			if (const TSet<FName>* Attrs = SkipList->Find(Entry.Key.Get<1>()))
			{
				if (Attrs->Contains(Entry.Key.Get<0>()))
				{
					continue;
				}
			}
		}
		if (InCollection.HasAttribute(Entry.Key.Get<0>(), Entry.Key.Get<1>()))
		{
			const FValueType& OriginalValue = InCollection.Map[Entry.Key];
			const FValueType& DestValue = Map[Entry.Key];

			// If we don't have a type match don't attempt the copy.
			if(OriginalValue.ArrayType == DestValue.ArrayType)
			{
				CopyAttribute(InCollection, Entry.Key.Get<0>(), Entry.Key.Get<1>());
			}
		}
	}

}

void FManagedArrayCollection::CopyAttribute(const FManagedArrayCollection& InCollection, FName Name, FName Group)
{
	CopyAttribute(InCollection, /*SrcName=*/Name, /*DestName=*/Name, Group);
}

void FManagedArrayCollection::CopyAttribute(const FManagedArrayCollection& InCollection, FName SrcName, FName DestName, FName Group)
{
	SyncGroupSizeFrom(InCollection, Group);
	FKeyType SrcKey = FManagedArrayCollection::MakeMapKey(SrcName, Group);
	FKeyType DestKey = FManagedArrayCollection::MakeMapKey(DestName, Group);

	if (!HasAttribute(DestName, Group))
	{
		const FValueType& V = InCollection.Map[SrcKey];
		EArrayType Type = V.ArrayType;
		FValueType Value(Type, *NewManagedTypedArray(Type));
		Value.Value->Resize(NumElements(Group));
		Value.GroupIndexDependency = V.GroupIndexDependency;
		Value.Saved = V.Saved;
		Value.bExternalValue = V.bExternalValue;
		Map.Add(DestKey, MoveTemp(Value));
	}

	const FValueType& OriginalValue = InCollection.Map[SrcKey];
	const FValueType& DestValue = Map[DestKey];
	check(OriginalValue.ArrayType == DestValue.ArrayType);
	DestValue.Value->Init(*OriginalValue.Value);
}

FName FManagedArrayCollection::GetDependency(FName SearchGroup)
{
	FName GroupIndexDependency = "";

	for (const TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<1>() == SearchGroup)
		{
			GroupIndexDependency = Entry.Value.GroupIndexDependency;
		}
	}

	return GroupIndexDependency;
}

bool FManagedArrayCollection::HasCycle(FName NewGroup, FName DependencyGroup)
{
	if (!DependencyGroup.IsNone())
	{
		// The system relies adding a dependency on it own group in order to run the reinding methods
		// this is why we don't include the case if (NewGroup == DependencyGroup) return true;

		while (!(DependencyGroup = GetDependency(DependencyGroup)).IsNone())
		{
			// check if we are looping back to the group we are testing against
			if (DependencyGroup == NewGroup)
			{
				return true;
			}
		}

	}

	return false;
}


#include <sstream> 
#include <string>
FString FManagedArrayCollection::ToString() const
{
	FString Buffer("");
	for (FName GroupName : GroupNames())
	{
		Buffer += GroupName.ToString() + "\n";
		for (FName AttributeName : AttributeNames(GroupName))
		{
			FKeyType Key = FManagedArrayCollection::MakeMapKey(AttributeName, GroupName);
			const FValueType& Value = Map[Key];

			const void* PointerAddress = static_cast<const void*>(Value.Value);
			std::stringstream AddressStream;
			AddressStream << PointerAddress;

			Buffer += GroupName.ToString() + ":" + AttributeName.ToString() + " [" + FString(AddressStream.str().c_str()) + "]\n";
		}
	}
	return Buffer;
}

static const FName GuidName("GUID");

// this is a reference wrapper to avoid copying attributes as some may not support copy (unique ptr ones) 
// this is used during serialization to build a transient filtered map of attributes that can be saved
struct FManagedArrayCollectionValueTypeWrapper
{
	FManagedArrayCollectionValueTypeWrapper()
		: ValueRef(nullptr)
	{}

	FManagedArrayCollectionValueTypeWrapper(FManagedArrayCollection::FValueType* ValueRefIn)
		: ValueRef(ValueRefIn)
	{}

	FManagedArrayCollection::FValueType* ValueRef;
};
FArchive& operator<<(FArchive& Ar, FManagedArrayCollectionValueTypeWrapper& ValueIn)
{
	// simple forwarding to the original object 
	Ar << (*ValueIn.ValueRef);
	return Ar;
}

void FManagedArrayCollection::Serialize(Chaos::FChaosArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	if (Ar.IsSaving()) Version = 9;
	Ar << Version;

	if (Ar.IsLoading())
	{
		//We can't serialize entire tmap in place because we may have new groups. todo(ocohen): baked data should be simpler since all entries exist
		TMap< FName, FGroupInfo> TmpGroupInfo;
		Ar << TmpGroupInfo;

		for (TTuple<FName, FGroupInfo>& Group : TmpGroupInfo)
		{
			GroupInfo.Add(Group);
		}

		//We can't serialize entire tmap in place because some entries may have changed types or memory ownership (internal vs external).
		//todo(ocohen): baked data should be simpler since all entries are guaranteed to exist
		TMap< FKeyType, FValueType> TmpMap;
		Ar << TmpMap;

		for (TTuple<FKeyType, FValueType>& Pair : TmpMap)
		{
			if (FValueType* Existing = Map.Find(Pair.Key))
			{
				if (ensureMsgf(Existing->ArrayType == Pair.Value.ArrayType, TEXT("Type change not supported. Ignoring serialized data")))
				{
					Existing->Value->ExchangeArrays(*Pair.Value.Value);	//if there is already an entry do an exchange. This way external arrays get correct serialization
					//question: should we validate if group dependency has changed in some invalid way?
				}
			}
			else
			{
				//todo(ocohen): how do we remove old values? Maybe have an unused attribute concept
				//no existing entry so it is owned by the map
				Map.Add(Pair.Key, MoveTemp(Pair.Value));
			}
		}

		TArray<FKeyType> ToRemoveKeys;
		for (const TTuple<FKeyType, FValueType>& Pair : Map)
		{
			if (!Pair.Value.bExternalValue && !TmpMap.Find(Pair.Key))
			{
				ToRemoveKeys.Add(Pair.Key);
			}
		}
		for (const FKeyType& Key : ToRemoveKeys)
		{
			Map.Remove(Key);
		}

		//it's possible new entries have been added but are not in old content. Resize these.
		for (TTuple<FKeyType, FValueType>& Pair : Map)
		{
			const int32 GroupSize = GroupInfo[Pair.Key.Get<1>()].Size;
			if (GroupSize != Pair.Value.Value->Num())
			{
				Pair.Value.Value->Resize(GroupSize);
			}
		}

		// strip out GUID Attributes
		for (auto& GroupName : GroupNames())
		{
			if (HasAttribute(GuidName, GroupName))
			{
				RemoveAttribute(GuidName, GroupName);
			}
		}

	}
	else // Ar.IsSaving()
	{
		Ar << GroupInfo;
		// Unless it's an undo/redo transaction, strip out the keys that we don't want to save
		if (!Ar.IsTransacting())
		{
			// we do create a wrapper around ValueType, to avoid copies and save memory 
			TMap<FKeyType, FManagedArrayCollectionValueTypeWrapper> ToSaveMap;
			for (TTuple<FKeyType, FValueType>& Pair : Map)
			{
				if (Pair.Value.Saved)
				{
					ToSaveMap.Emplace(Pair.Key, FManagedArrayCollectionValueTypeWrapper(&Pair.Value));
				}
			}
			Ar << ToSaveMap;
		}
		else
		{
			Ar << Map;
		}
	}
}

FArchive& operator<<(FArchive& Ar, FManagedArrayCollection::FGroupInfo& GroupInfo)
{
	int Version = 4;
	Ar << Version;
	Ar << GroupInfo.Size;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FManagedArrayCollection::FValueType& ValueIn)
{
	int Version = 4;	//todo: version per entry is really bloated
	Ar << Version;

	int ArrayTypeAsInt = static_cast<int>(ValueIn.ArrayType);
	Ar << ArrayTypeAsInt;
	ValueIn.ArrayType = static_cast<FManagedArrayCollection::EArrayType>(ArrayTypeAsInt);

	if (Version < 4)
	{
		int ArrayScopeAsInt;
		Ar << ArrayScopeAsInt;	//assume all serialized old content was for rest collection
	}

	if (Version >= 2)
	{
		Ar << ValueIn.GroupIndexDependency;
		Ar << ValueIn.Saved;
	}

	if (ValueIn.Value == nullptr)
	{
		ValueIn.Value = NewManagedTypedArray(ValueIn.ArrayType);
	}
	
	// Note: We switched to always saving the value here, and use the Saved flag
	// to remove the property from the overall Map (see FManagedArrayCollection::Serialize above)
	bool bNewSavedBehavior = Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::ManagedArrayCollectionAlwaysSerializeValue;
	if (bNewSavedBehavior || ValueIn.Saved)
	{
		ValueIn.Value->Serialize(static_cast<Chaos::FChaosArchive&>(Ar));
	}

	return Ar;
}