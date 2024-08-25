// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "VVMHeap.h"
#include <Containers/Array.h>

namespace UE::GC
{
extern COREUOBJECT_API bool GIsFrankenGCCollecting;
}

namespace Verse
{
struct VCell;

struct FMarkStack
{
	FMarkStack() = default;

	// It's illegal to destroy a mark stack that has anything in it.
	COREUOBJECT_API ~FMarkStack();

	bool IsEmpty() const
	{
		return Stack.IsEmpty();
	}

	size_t Num() const
	{
		return Stack.Num();
	}

	VCell* Pop()
	{
		if (IsEmpty())
		{
			return nullptr;
		}
		else
		{
			return Stack.Pop(EAllowShrinking::No);
		}
	}

	bool TryMarkNonNull(const VCell* Cell)
	{
		if (!FHeap::IsMarked(Cell))
		{
			MarkSlow(Cell);
			return true;
		}
		return false;
	}

	void MarkNonNull(const VCell* Cell)
	{
		if (!FHeap::IsMarked(Cell))
		{
			MarkSlow(Cell);
		}
	}

	void FencedMarkNonNull(const VCell* Cell)
	{
		if (!FHeap::IsMarked(Cell))
		{
			FencedMarkSlow(Cell);
		}
	}

	void Mark(const VCell* Cell)
	{
		if (Cell)
		{
			MarkNonNull(Cell);
		}
	}

	void MarkAuxNonNull(const void* Aux)
	{
		if (!FHeap::IsMarked(Aux))
		{
			MarkAuxSlow(Aux);
		}
	}

	void FencedMarkAuxNonNull(const void* Aux)
	{
		if (!FHeap::IsMarked(Aux))
		{
			FencedMarkAuxSlow(Aux);
		}
	}

	void MarkAux(const void* Aux)
	{
		if (Aux)
		{
			MarkAuxNonNull(Aux);
		}
	}

	void MarkNonNull(const UObject* Object)
	{
		if (ensure(UE::GC::GIsFrankenGCCollecting))
		{
			Object->VerseMarkAsReachable();
		}
	}

	void Mark(const UObject* Object)
	{
		if (Object)
		{
			MarkNonNull(Object);
		}
	}

	void Append(FMarkStack&& Other)
	{
		Stack.Append(MoveTemp(Other.Stack));
	}

	void ReportNativeBytes(size_t Bytes)
	{
		FHeap::ReportMarkedNativeBytes(Bytes);
	}

private:
	template <std::memory_order MemoryOrder>
	void MarkSlowImpl(const VCell* Cell);

	COREUOBJECT_API void MarkSlow(const VCell* Cell);
	COREUOBJECT_API void FencedMarkSlow(const VCell* Cell);

	template <std::memory_order MemoryOrder>
	void MarkAuxSlowImpl(const void* Aux);

	COREUOBJECT_API void MarkAuxSlow(const void* Aux);
	COREUOBJECT_API void FencedMarkAuxSlow(const void* Aux);

	TArray<VCell*> Stack;
};

} // namespace Verse
#endif // WITH_VERSE_VM
