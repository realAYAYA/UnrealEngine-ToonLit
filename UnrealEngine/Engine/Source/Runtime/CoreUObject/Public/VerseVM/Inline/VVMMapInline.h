// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !(WITH_VERSE_VM || defined(__INTELLISENSE__))
#error In order to use VerseVM, WITH_VERSE_VM must be set
#endif

#include "Async/ExternalMutex.h"
#include "Async/UniqueLock.h"
#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/VVMMap.h"
#include "VerseVM/VVMNativeAllocationGuard.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{

inline void VMapBase::Add(FAllocationContext Context, VValue Key, VValue Value)
{
	UE::FExternalMutex ExternalMutex(Mutex);
	UE::TUniqueLock Lock(ExternalMutex);

	TWriteBarrier<VValue> NewKey(Context, Key);
	TWriteBarrier<VValue> NewValue(Context, Value);

	TNativeAllocationGuard NativeAllocationGuard(this);
	InternalMap.Add(NewKey, NewValue);
}

inline VValue VMapBase::GetKey(const int32 Index)
{
	// Only works as long as nothing is removed from map
	FSetElementId Id = FSetElementId::FromInteger(Index);
	TWriteBarrier<VValue>& Result = InternalMap.Get(Id).Get<0>();
	return Result.Follow();
}

inline VValue VMapBase::GetValue(const int32 Index)
{
	// Only works as long as nothing is removed from map
	FSetElementId Id = FSetElementId::FromInteger(Index);
	TWriteBarrier<VValue>& Result = InternalMap.Get(Id).Get<1>();
	return Result.Follow();
}

template <typename GetEntryByIndex>
inline VMapBase::VMapBase(FAllocationContext Context, uint32 MaxNumEntries, const GetEntryByIndex& GetEntry, VEmergentType* Type)
	: VHeapValue(Context, Type)
{
	SetIsDeeplyMutable();
	VMapBaseInternal Map;
	Map.Reserve(MaxNumEntries);

	bool bHasDuplicates = false;
	for (uint32 Index = 0; Index < MaxNumEntries; ++Index)
	{
		VValue Key;
		VValue Value;
		TPair<VValue, VValue> Pair = GetEntry(Index);
		Key = Pair.Get<0>();
		Value = Pair.Get<1>();

		TWriteBarrier<VValue>& ExistingEntry = Map.FindOrAdd(TWriteBarrier<VValue>{Context, Key}, TWriteBarrier<VValue>{});
		if (ExistingEntry)
		{
			bHasDuplicates = true;
			break;
		}

		ExistingEntry.Set(Context, Value);
	}

	// Constructing a map in Verse has these semantics:
	// - If the same key appears more than once, it's as if only the last key was provided.
	// - The order of the map is based on the textual order a map is written in.
	// - E.g, map{K1=>V1, K2=>V2} has the order (K1, V1) then (K2, V2).
	//   And map{K1=>V1, K2=>V2, K1=>V3} has the order (K2, V2) then (K1, V3).
	// The code below achieves these semantics. Surely it can be more optimized than it is now:
	// - We do repetitive hashing
	// - We do repetitive equality checks

	if (bHasDuplicates)
	{
		Map = VMapBaseInternal{};
		Map.Reserve(MaxNumEntries);

		struct CountsKeyFuncs : TDefaultMapKeyFuncs<VValue, unsigned, false>
		{
			static bool Matches(VValue A, VValue B)
			{
				return VValue::Equal(FRunningContextPromise(), A, B, [](VValue Left, VValue Right) {
					checkSlow(!Left.IsPlaceholder());
					checkSlow(!Right.IsPlaceholder());
				});
			}

			static uint32 GetKeyHash(VValue Key)
			{
				return GetTypeHash(Key);
			}
		};

		TMap<VValue, uint32, FDefaultSetAllocator, CountsKeyFuncs> Counts;
		Counts.Reserve(MaxNumEntries);
		for (uint32 Index = 0; Index < MaxNumEntries; ++Index)
		{
			VValue Key;
			VValue Value;
			TPair<VValue, VValue> Pair = GetEntry(Index);
			Key = Pair.Get<0>();
			Value = Pair.Get<1>();

			uint32& Count = Counts.FindOrAdd(Key, 0);
			++Count;
		}

		TMap<VValue, uint32, FDefaultSetAllocator, CountsKeyFuncs> Seen;
		Seen.Reserve(MaxNumEntries);
		for (uint32 Index = 0; Index < MaxNumEntries; ++Index)
		{
			VValue Key;
			VValue Value;
			TPair<VValue, VValue> Pair = GetEntry(Index);
			Key = Pair.Get<0>();
			Value = Pair.Get<1>();

			uint32 Hash = GetTypeHash(Key);

			uint32 TargetCount = *Counts.FindByHash(Hash, Key);
			uint32& CurrentCount = Seen.FindOrAddByHash(Hash, Key, 0);
			++CurrentCount;
			checkSlow(TargetCount > 0 && CurrentCount <= TargetCount);
			if (CurrentCount == TargetCount)
			{
				Map.AddByHash(Hash, TWriteBarrier<VValue>{Context, Key}, TWriteBarrier<VValue>{Context, Value});
			}
		}
	}

	// We don't need to grab the lock here because we can't be scanned by the GC yet.
	InternalMap = MoveTemp(Map);
	FHeap::ReportAllocatedNativeBytes(InternalMap.GetAllocatedSize());
}

template <typename MapType>
inline void VMapBase::Serialize(MapType*& This, FAllocationContext Context, FAbstractVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		uint64 ScratchNumValues = 0;
		Visitor.BeginArray(TEXT("Values"), ScratchNumValues);
		This = &VMapBase::New<MapType>(Context, (uint32)ScratchNumValues).template StaticCast<MapType>();
		for (uint32 Index = (uint32)ScratchNumValues; Index != 0; --Index)
		{
			VValue Key, Value;
			Visitor.BeginObject();
			Visitor.Visit(Key, TEXT("Key"));
			Visitor.Visit(Value, TEXT("Value"));
			Visitor.EndObject();
			This->Add(Context, Key, Value);
		}
		Visitor.EndArray();
	}
	else
	{
		uint64 ScratchNumValues = This->Num();
		Visitor.BeginMap(TEXT("Values"), ScratchNumValues);
		for (TTuple<VValue, VValue> Kvp : *This)
		{
			Visitor.BeginObject();
			Visitor.Visit(Kvp.Key, TEXT("Key"));
			Visitor.Visit(Kvp.Value, TEXT("Value"));
			Visitor.EndObject();
		}
		Visitor.EndMap();
	}
}

template <typename MapType, typename GetEntryByIndex>
inline VMapBase& VMapBase::New(FAllocationContext Context, uint32 MaxNumEntries, const GetEntryByIndex& GetEntry)
{
	return *new (Context.Allocate(Verse::FHeap::DestructorSpace, sizeof(VMapBase))) VMapBase(Context, MaxNumEntries, GetEntry, &MapType::GlobalTrivialEmergentType.Get(Context));
}

} // namespace Verse
