// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "Inline/VVMAbstractVisitorInline.h"
#include "VVMContext.h"
#include "VVMGlobalHeapRoot.h"
#include "VVMLog.h"
#include "VVMMarkStackVisitor.h"
#include "VVMWriteBarrier.h"

namespace Verse
{
// Ensure all created items are unique. If an old one exist then it's reused, otherwise a new is created.
// Will use a hash table in the future, but for now a simple array and linear search.

template <typename Type>
class VUniqueCreator : public FGlobalHeapRoot
{
private:
	// This should be a hash table
	static constexpr uint32 MaxSize = 4 * 1024;
	TWriteBarrier<Type> Items[MaxSize];
	uint32 ItemsEnd = 0;

	template <typename SubType, typename... ArgumentTypes>
	Type* LookupLocked(ArgumentTypes... Arguments)
	{
		for (uint32 Ix = 0; Ix < ItemsEnd; ++Ix)
		{
			if (SubType::Equals(*Items[Ix], Arguments...))
			{
				return Items[Ix].Get();
			}
		}
		return nullptr;
	}

	Type* AddLocked(FAccessContext Context, Type* Item)
	{
		V_DIE_IF(ItemsEnd >= MaxSize);
		Items[ItemsEnd].Set(Context, Item);
		ItemsEnd++;
		return Item;
	}

public:
	VUniqueCreator() = default;

	template <typename SubType, typename... ArgumentTypes>
	Type* GetOrCreate(FAllocationContext Context, ArgumentTypes... Arguments)
	{
		static_assert(std::is_base_of_v<Type, SubType>);
		// This lock acquisition is *almost* wrong. If this UniqueCreator ever participated in census or a fixpoint,
		// then allocation slow paths could call into some GC callback on UniqueCreator, and then that would need to
		// grab this lock, and we'd die.
		UE::TUniqueLock Lock(Mutex);
		if (Type* Item = LookupLocked<SubType, ArgumentTypes...>(Arguments...))
		{
			return Item;
		}
		Type* Item = SubType::New(Context, Arguments...);
		return AddLocked(Context, Item);
	}

	// Only call if certain that Item is unique
	Type* Add(FAccessContext Context, Type* Item)
	{
		UE::TUniqueLock Lock(Mutex);
		return AddLocked(Context, Item);
	}

	void Visit(FAbstractVisitor& Visitor) override
	{
		VisitImpl(Visitor);
	}

	void Visit(FMarkStackVisitor& Visitor) override
	{
		VisitImpl(Visitor);
	}

private:
	template <typename TVisitor>
	void VisitImpl(TVisitor& Visitor)
	{
		UE::TUniqueLock Lock(Mutex);
		if constexpr (TVisitor::bIsAbstractVisitor)
		{
			uint64 ScratchNumItems = ItemsEnd;
			Visitor.BeginArray(TEXT("Items"), ScratchNumItems);
			Visitor.Visit(Items, Items + ItemsEnd);
			Visitor.EndArray();
		}
		else
		{
			Visitor.Visit(Items, Items + ItemsEnd);
		}
	}

	UE::FMutex Mutex;
};
}; // namespace Verse

#endif // WITH_VERSE_VM
