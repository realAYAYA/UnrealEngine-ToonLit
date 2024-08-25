// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"
#include "VerseVM/VVMUTF8String.h"

namespace Verse
{
template <typename T>
inline FUtf8StringView FUniqueStringSetKeyFuncsBase<T>::GetSetKey(VUniqueString& Element)
{
	return Element.AsStringView();
}

template <typename T>
inline FUtf8StringView FUniqueStringSetKeyFuncsBase<T>::GetSetKey(const T& Element)
{
	if (const VUniqueString* String = Element.Get())
	{
		return String->AsStringView();
	}
	else
	{
		return FUtf8StringView();
	}
}

template <typename T>
inline bool FUniqueStringSetKeyFuncsBase<T>::Matches(FUtf8StringView A, FUtf8StringView B)
{
	return A.Equals(B, ESearchCase::CaseSensitive);
}

template <typename T>
inline uint32 FUniqueStringSetKeyFuncsBase<T>::GetKeyHash(FUtf8StringView Key)
{
	return GetTypeHash(Key);
}

template struct FUniqueStringSetKeyFuncsBase<TWeakBarrier<VUniqueString>>;
template struct FUniqueStringSetKeyFuncsBase<TWriteBarrier<VUniqueString>>;

inline VUniqueStringSet::FConstIterator::FConstIterator(SetType::TRangedForConstIterator InCurrentIteration)
	: CurrentIteration(InCurrentIteration) {}

inline FSetElementId VUniqueStringSet::FConstIterator::GetId() const
{
	return CurrentIteration.GetId();
}

inline const TWriteBarrier<VUniqueString>* VUniqueStringSet::FConstIterator::operator->() const
{
	return &*CurrentIteration;
}

inline const TWriteBarrier<VUniqueString>& VUniqueStringSet::FConstIterator::operator*() const
{
	return *CurrentIteration;
}

inline bool VUniqueStringSet::FConstIterator::operator==(const FConstIterator& Rhs) const
{
	return CurrentIteration == Rhs.CurrentIteration;
}

inline bool VUniqueStringSet::FConstIterator::operator!=(const FConstIterator& Rhs) const
{
	return CurrentIteration != Rhs.CurrentIteration;
}

inline VUniqueStringSet::FConstIterator& VUniqueStringSet::FConstIterator::operator++()
{
	++CurrentIteration;
	return *this;
}

inline VUniqueStringSet::FConstIterator VUniqueStringSet::begin() const
{
	return Strings.begin();
}

inline VUniqueStringSet::FConstIterator VUniqueStringSet::end() const
{
	return Strings.end();
}

inline VUniqueStringSet& VUniqueStringSet::New(FAllocationContext Context, TSet<VUniqueString*> InStrings)
{
	return Pool->Intern(Context, InStrings);
}

inline VUniqueStringSet& VUniqueStringSet::New(FAllocationContext Context, const std::initializer_list<FUtf8StringView>& InStrings)
{
	TSet<VUniqueString*> StringSet;
	StringSet.Reserve(InStrings.size());
	for (const FUtf8StringView& String : InStrings)
	{
		VUniqueString& UniqueString = VUniqueString::New(Context, String);
		StringSet.Add(&UniqueString);
	}
	return Pool->Intern(Context, StringSet);
}

inline VUniqueStringSet& VUniqueStringSet::Make(FAllocationContext Context, const TSet<VUniqueString*>& InSet)
{
	// All of the string sets' memory is being managed externally via `TSet` so this is OK.
	const size_t NumBytes = sizeof(VUniqueStringSet);
	return *new (Context.Allocate(FHeap::DestructorSpace, NumBytes)) VUniqueStringSet(Context, InSet);
}

inline bool VUniqueStringSet::operator==(const VUniqueStringSet& Other) const
{
	// We can compare by pointer address, because two unique string sets that are the same should have been vended the same way.
	return this == &Other;
}

inline uint32 VUniqueStringSet::Num() const
{
	return Strings.Num();
}

inline FSetElementId VUniqueStringSet::FindId(const FUtf8StringView& String) const
{
	return Strings.FindId(String);
}

inline bool VUniqueStringSet::IsValidId(const FSetElementId& Id) const
{
	return Strings.IsValidId(Id);
}

inline VUniqueStringSet::SetType VUniqueStringSet::FormSet(FAllocationContext Context, const TSet<VUniqueString*>& InSet)
{
	VUniqueStringSet::SetType Strings;
	Strings.Reserve(InSet.Num());
	for (VUniqueString* InString : InSet)
	{
		TWriteBarrier<VUniqueString> Test{Context, InString};
		Strings.Add({Context, *InString});
	}
	return Strings;
}

inline VUniqueStringSet::VUniqueStringSet(FAllocationContext Context, const TSet<VUniqueString*>& InSet)
	: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
	, Strings(FormSet(Context, InSet))
{
}

inline bool VUniqueStringSet::Equals(const TSet<VUniqueString*>& A, const TSet<VUniqueString*>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}
	return GetTypeHash(A) == GetTypeHash(B);
}

inline uint32 GetTypeHash(const TSet<VUniqueString*>& Set)
{
	// Each of the strings in the set should already be pointing at a globally-unique string; thus we should be
	// able to reliably hash just the pointers of each string, rather than having to hash the contents of each
	// specific string.
	// TODO: (yiliang.siew) Is there potentially a better commutative hash function than just `XOR`?
	uint32 Result = 0;
	for (const VUniqueString* const Element : Set)
	{
		Result ^= PointerHash(Element);
	}
	return Result;
}

inline uint32 GetTypeHash(const VUniqueStringSet& Set)
{
	// Each of the strings in the set should already be pointing at a globally-unique string; thus we should be
	// able to reliably hash just the pointers of each string, rather than having to hash the contents of each
	// specific string.
	uint32 Result = 0;
	for (const TWriteBarrier<VUniqueString>& Element : Set)
	{
		Result ^= PointerHash(Element.Get());
	}
	return Result;
}

inline FHashableUniqueStringSetKey::FHashableUniqueStringSetKey()
	: Type(EType::Invalid) {}

inline FHashableUniqueStringSetKey::FHashableUniqueStringSetKey(const TSet<VUniqueString*>& InSet)
	: Set(&InSet)
	, Type(EType::Set) {}

inline FHashableUniqueStringSetKey::FHashableUniqueStringSetKey(const VUniqueStringSet& InCell)
	: Cell(&InCell)
	, Type(EType::Cell) {}

inline bool FHashableUniqueStringSetKey::operator==(const FHashableUniqueStringSetKey& Other) const
{
	auto CompareCellAndSet = [](const VUniqueStringSet& InCell, const TSet<VUniqueString*>& InSet) {
		if (InCell.Num() != static_cast<uint32>(InSet.Num()))
		{
			return false;
		}
		for (const TWriteBarrier<VUniqueString>& UniqueString : InCell)
		{
			if (!InSet.Contains(UniqueString.Get()))
			{
				return false;
			}
		}
		return true;
	};

	auto CompareCells = [](const VUniqueStringSet& InCell, const VUniqueStringSet& InOther) {
		// We are just doing a basic pointer comparison here because this path should only ever get hit
		// when both cells are not the same. If you hit this, check how you're adding to the unique string set pool.
		V_DIE_IF(InCell == InOther);
		checkSlow(!InCell.Equals(InOther));
		return false;
	};

	if (Type == EType::Invalid || Other.Type == EType::Invalid)
	{
		V_DIE_IF(Type == Other.Type); // We shouldn't have a case of a null-to-null lookup.
		return false;
	}

	switch (Type)
	{
		case EType::Cell:
			switch (Other.Type)
			{
				case EType::Cell:
					return CompareCells(*Cell, *Other.Cell);
				case EType::Set:
					return CompareCellAndSet(*Cell, *Other.Set);
				case EType::Invalid:
					VERSE_UNREACHABLE(); // Should have been hit above.
				default:
					return false;
			}
			break;
		case EType::Set:
			switch (Other.Type)
			{
				case EType::Cell:
					return CompareCellAndSet(*Other.Cell, *Set);
				case EType::Invalid: // Should have been hit above.
				case EType::Set:
					// There shouldn't be a case where a key lookup causes a set-to-set comparison,
					// because the map entries should only be cells.
					VERSE_UNREACHABLE();
					break;
				default:
					return false;
			}
			break;

		case EType::Invalid: // Should have been hit above.
		default:
			VERSE_UNREACHABLE();
			break;
	}

	return true;
};

inline FHashableUniqueStringSetKeyFuncs::KeyInitType FHashableUniqueStringSetKeyFuncs::GetSetKey(FHashableUniqueStringSetKeyFuncs::ElementInitType& Element)
{
	return {Element};
}

inline FHashableUniqueStringSetKeyFuncs::KeyInitType FHashableUniqueStringSetKeyFuncs::GetSetKey(const TWeakBarrier<VUniqueStringSet>& Element)
{
	const VUniqueStringSet* ElementSet = Element.Get();
	if (!ElementSet)
	{
		// We can hit this after `FHeap::Terminate`, but before `ConductCensus` is called, so
		// the memory still "lives", but the cell is not marked and is impending being swept.
		// In such a case, we treat it as if it doesn't exist in the set at all.
		return {};
	}
	return {*ElementSet};
}

inline bool FHashableUniqueStringSetKeyFuncs::Matches(FHashableUniqueStringSetKeyFuncs::KeyInitType A, FHashableUniqueStringSetKeyFuncs::KeyInitType B)
{
	return A == B;
}

inline uint32 FHashableUniqueStringSetKeyFuncs::GetKeyHash(KeyInitType Key)
{
	switch (Key.Type)
	{
		case FHashableUniqueStringSetKey::EType::Cell:
			V_DIE_UNLESS(Key.Cell);
			return GetTypeHash(*Key.Cell);
		case FHashableUniqueStringSetKey::EType::Set:
			V_DIE_UNLESS(Key.Set);
			return GetTypeHash(*Key.Set);
		case FHashableUniqueStringSetKey::EType::Invalid:
		default:
			break;
	}
	VERSE_UNREACHABLE();
}

} // namespace Verse
#endif // WITH_VERSE_VM
