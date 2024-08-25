// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "Templates/UnrealTemplate.h"
#include "Chaos/ChaosArchive.h"
#include "UObject/UE5MainStreamObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ManagedArrayCollection)

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

int32 FManagedArrayCollection::NumAttributes(FName Group) const
{
	int32 Num=0;
	for (const TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			Num++;
		}
	}
	return Num;
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

FManagedArrayCollection::EArrayType FManagedArrayCollection::GetAttributeType(FName Name, FName Group) const
{
	const FKeyType Key = FManagedArrayCollection::MakeMapKey(Name, Group);
	if (const FValueType* Attribute = Map.Find(Key))
	{
		return Attribute->ArrayType;
	}
	return EArrayType::FNoneType;
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

int32 FManagedArrayCollection::NumElements(FName GroupName) const
{
	int32 Num = 0;
	if (const FGroupInfo* Group = GroupInfo.Find(GroupName))
	{
		Num = Group->Size;
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

void FManagedArrayCollection::Append(const FManagedArrayCollection& InCollection)
{
	bool bMatchingAttributes = true;
	for (const TTuple<FKeyType, FValueType>& Entry : InCollection.Map)
	{
		if (HasAttribute(Entry.Key.Get<0>(), Entry.Key.Get<1>()))
		{
			const FValueType& OriginalValue = InCollection.Map[Entry.Key];
			const FValueType& DestValue = Map[Entry.Key];

			// If we don't have a type match don't attempt the copy.
			if (OriginalValue.ArrayType != DestValue.ArrayType)
			{
				bMatchingAttributes = false;
				ensureMsgf(false, TEXT("Failed : Type error in FManagedArrayCollection::AppendCollection (%s:%s)"), 
					*Entry.Key.Get<0>().ToString(), *Entry.Key.Get<1>().ToString());
			}
		}
	}
	if (bMatchingAttributes)
	{
		// make space first. 
		for (const FName& Group : InCollection.GroupNames())
		{
			if (HasGroup(Group) && NumElements(Group))
			{
				InsertElements(InCollection.NumElements(Group), 0, Group);
			}
			else if (!HasGroup(Group))
			{
				AddGroup(Group);
				AddElements(InCollection.NumElements(Group), Group);
			}
		}

		// copy values
		for (const TTuple<FKeyType, FValueType>& Entry : InCollection.Map)
		{
			FName AttributeName = Entry.Key.Get<0>();
			FName GroupName = Entry.Key.Get<1>();

			if (HasAttribute(AttributeName, GroupName))
			{
				Map[Entry.Key].Value->CopyRange(*Entry.Value.Value, 0, Entry.Value.Value->Num());
			}
			else
			{
				// Copied from FManagedArrayCollection::CopyAttribute, but this 
				// does not manage destination size, as that is pre allocated above. 
				FKeyType Key = FManagedArrayCollection::MakeMapKey(AttributeName, GroupName);

				if (!HasAttribute(AttributeName, GroupName))
				{
					const FValueType& V = InCollection.Map[Key];
					EArrayType Type = V.ArrayType;
					FValueType Value(Type, *NewManagedTypedArray(Type));
					Value.Value->Resize(NumElements(GroupName));
					Value.GroupIndexDependency = V.GroupIndexDependency;
					Value.Saved = V.Saved;
					Value.bExternalValue = V.bExternalValue;
					Map.Add(Key, MoveTemp(Value));
				}

				const FValueType& OriginalValue = InCollection.Map[Key];
				const FValueType& DestValue = Map[Key];
				check(OriginalValue.ArrayType == DestValue.ArrayType);
				DestValue.Value->Init(*OriginalValue.Value);
			}
		}
	}
}


void FManagedArrayCollection::RemoveAttribute(FName Name, FName Group)
{
	FKeyType Key = FManagedArrayCollection::MakeMapKey(Name, Group);
	FValueType* FoundValue = Map.Find(Key);
	if (FoundValue != nullptr)
	{
		FoundValue->Value->Empty();
	}
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

void FManagedArrayCollection::SetDependency(FName Name, FName Group, FName DependencyGroup, bool bAllowCircularDependency)
{
	ensure(HasAttribute(Name, Group));
	FKeyType Key = FManagedArrayCollection::MakeMapKey(Name, Group);
	if (ensure(bAllowCircularDependency || !IsConnected(DependencyGroup, Group)))
	{
		Map[Key].GroupIndexDependency = DependencyGroup;
	}
}

FName FManagedArrayCollection::GetDependency(FName Name, FName Group) const
{
	check(HasAttribute(Name, Group));
	const FKeyType Key = FManagedArrayCollection::MakeMapKey(Name, Group);
	return Map[Key].GroupIndexDependency;
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

void FManagedArrayCollection::SetDefaults(FName Group, uint32 StartSize, uint32 NumElements)
{
	for (TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Key.Get<1>() == Group)
		{
			Entry.Value.Value->SetDefaults(StartSize, NumElements, Entry.Value.GroupIndexDependency != "");
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

void FManagedArrayCollection::CopyMatchingAttributesFrom(const FManagedArrayCollection& FromCollection, const TArrayView<const FAttributeAndGroupId> SkipList)
{
	MatchOptionalDefaultAttributes(FromCollection);

	// we only want to resize the groups that are in common 
	for (const TPair<FName, FGroupInfo>& Pair: FromCollection.GroupInfo)
	{
		const FName& GroupName = Pair.Key;
		if (HasGroup(GroupName))
		{
			Resize(Pair.Value.Size, GroupName);
		}
	}

	for (TTuple<FKeyType, FValueType>& Entry : Map)
	{
		const FName& AttributeName = Entry.Key.Get<0>();
		const FName& GroupName = Entry.Key.Get<1>();

		if (SkipList.Contains(FAttributeAndGroupId{ AttributeName, GroupName }))
		{
			continue;
		}
		if (const FValueType* FromAttribute = FromCollection.Map.Find(MakeMapKey(AttributeName, GroupName)))
		{
			FValueType& ToAttribute = Entry.Value;
			if (ToAttribute.ArrayType == FromAttribute->ArrayType)
			{
				ToAttribute.Value->Init(*FromAttribute->Value);
			}
		}
	}
}

void FManagedArrayCollection::CopyMatchingAttributesFrom(
	const FManagedArrayCollection& InCollection,
	const TMap<FName, TSet<FName>>* SkipList)
{
	MatchOptionalDefaultAttributes(InCollection);

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
	if (OriginalValue.ArrayType == DestValue.ArrayType)
	{
		DestValue.Value->Init(*OriginalValue.Value);
	}
	else
	{
		DestValue.Value->Convert(*OriginalValue.Value);
	}
}

bool FManagedArrayCollection::IsConnected(FName StartingNode, FName TargetNode)
{
	if (!StartingNode.IsNone())
	{
		TMap<FName, TArray<FName> > DMap;
		for (const TTuple<FKeyType, FValueType>& Entry : Map)
		{
			if (!DMap.Contains(Entry.Key.Get<1>()))
				DMap.Add(Entry.Key.Get<1>(), TArray<FName>());
			if (!Entry.Value.GroupIndexDependency.IsNone())
				DMap[Entry.Key.Get<1>()].AddUnique(Entry.Value.GroupIndexDependency);
		}

		if (DMap.Contains(StartingNode))
		{
			TSet<FName> Visited;
			TArray<FName> SearchSet = DMap[StartingNode];
			while (SearchSet.Num())
			{
				FName Curr = SearchSet.Pop();
				if (Curr.IsEqual(TargetNode))
				{
					return true;
				}

				if (!Visited.Contains(Curr))
				{
					Visited.Add(Curr);
					if (DMap.Contains(Curr))
					{
						if (!DMap[Curr].IsEmpty())
							SearchSet.Append(DMap[Curr]);
					}
				}
			}
		}
	}
	return false;
}

FString FManagedArrayCollection::ToString() const
{
	FString Buffer;

	const TArray<FStringFormatArg> CollectionInfos = { FString::FormatAsNumber((int32)GetAllocatedSize()) };
	Buffer += FString::Format(TEXT("All attributes [{0} bytes]\n"), CollectionInfos);

	for (FName GroupName : GroupNames())
	{
		const TArray<FStringFormatArg> GroupNameInfos = { GroupName.ToString(), FString::FormatAsNumber((int32)NumElements(GroupName)) };
		Buffer += FString::Format(TEXT("{0} - [{1} elements]\n"), GroupNameInfos);
		for (FName AttributeName : AttributeNames(GroupName))
		{
			FKeyType Key = FManagedArrayCollection::MakeMapKey(AttributeName, GroupName);
			const FValueType& Value = Map[Key];

			const SIZE_T AttributeAllocatedSize = Value.Value? Value.Value->GetAllocatedSize() : 0;
			const FString AttributeAllocatedSizeStr = FString::FormatAsNumber((int32)AttributeAllocatedSize);

			const TArray<FStringFormatArg> AttributeInfos = { AttributeName.ToString(), AttributeAllocatedSizeStr };
			Buffer += FString::Format(TEXT(" |-- {0} [{1} bytes]\n"), AttributeInfos);
		}
	}
	return Buffer;
}

SIZE_T FManagedArrayCollection::GetAllocatedSize() const
{
	SIZE_T AllocatedSize = Map.GetAllocatedSize();
	for (const TTuple<FKeyType, FValueType>& Entry : Map)
	{
		if (Entry.Value.Value)
		{
			AllocatedSize += Entry.Value.Value->GetAllocatedSize();
		}
	}
	return AllocatedSize;
}

void FManagedArrayCollection::GetElementSizeInfoForGroups(TArray<TPair<FName, SIZE_T>>& OutSizeInfo) const
{
	// Group name to total element size map
	TMap<FName, SIZE_T> GroupToElementSizeMap;

	for (const TPair<FKeyType, FValueType>& Attribute : Map)
	{
		SIZE_T& GroupSize = GroupToElementSizeMap.FindOrAdd(Attribute.Key.Get<1>());
		if (FManagedArrayBase* Array = Attribute.Value.Value)
		{
			GroupSize += Array->GetTypeSize();
		}
	}

	for (const TPair<FName, SIZE_T>& GroupAndElementSize : GroupToElementSizeMap)
	{
		OutSizeInfo.Add(GroupAndElementSize);
	}
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
				if (Existing->ArrayType == Pair.Value.ArrayType)
				{
					Existing->Value->ExchangeArrays(*Pair.Value.Value);	//if there is already an entry do an exchange. This way external arrays get correct serialization
					//question: should we validate if group dependency has changed in some invalid way?
				}
				else
				{
					Existing->Value->Convert(*Pair.Value.Value);
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
