// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "verse_heap_object_set_ue.h"
#include "verse_heap_ue.h"

namespace Verse
{
class FSubspace;

class FHeapIterationSet final
{
public:
	static FHeapIterationSet* Create()
	{
		return reinterpret_cast<FHeapIterationSet*>(verse_heap_object_set_create());
	}

	static FHeapIterationSet* AllObjects()
	{
		return reinterpret_cast<FHeapIterationSet*>(&verse_heap_all_objects);
	}

	void Add(FSubspace* Subspace)
	{
		verse_heap_add_to_set(reinterpret_cast<pas_heap*>(Subspace), reinterpret_cast<verse_heap_object_set*>(this));
	}

	void StartIterateBeforeHandshake()
	{
		verse_heap_object_set_start_iterate_before_handshake(reinterpret_cast<verse_heap_object_set*>(this));
	}

	size_t StartIterateAfterHandshake()
	{
		return verse_heap_object_set_start_iterate_after_handshake(
			reinterpret_cast<verse_heap_object_set*>(this));
	}

	void IterateRange(size_t Begin,
		size_t End,
		verse_heap_iterate_filter Filter,
		void (*Callback)(void* Object, void* Arg),
		void* Arg)
	{
		// FIXME: It would be super awesome if this could call verse_heap_object_set_iterate_range_inline.
		verse_heap_object_set_iterate_range(
			reinterpret_cast<verse_heap_object_set*>(this), Begin, End, Filter, Callback, Arg);
	}

	void EndIterate()
	{
		verse_heap_object_set_end_iterate(reinterpret_cast<verse_heap_object_set*>(this));
	}

private:
	FHeapIterationSet() = delete;
	FHeapIterationSet(const FHeapIterationSet&) = delete;
};

} // namespace Verse
#endif // WITH_VERSE_VM
