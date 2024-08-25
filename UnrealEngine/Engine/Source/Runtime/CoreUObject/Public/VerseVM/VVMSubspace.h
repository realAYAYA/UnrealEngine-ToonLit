// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "verse_heap_ue.h"
#include <cstddef>

namespace Verse
{
struct FAllocationContext;

// This is really a pas_heap. Hence, we don't let you create it directly. We create a pas_heap
// and cast to this.
class FSubspace final
{
public:
	static FSubspace* Create(size_t MinAlign = 1, size_t ReservationSize = 0, size_t ReservationAlignment = 1)
	{
		return reinterpret_cast<FSubspace*>(verse_heap_create(MinAlign, ReservationSize, ReservationAlignment));
	}

	std::byte* GetBase() const
	{
		return static_cast<std::byte*>(verse_heap_get_base(const_cast<pas_heap*>(reinterpret_cast<const pas_heap*>(this))));
	}

private:
	friend struct FAllocationContext;
	friend struct FContextImpl;

	std::byte* TryAllocate(size_t Size)
	{
		return static_cast<std::byte*>(verse_heap_try_allocate(reinterpret_cast<pas_heap*>(this), Size));
	}
	std::byte* TryAllocate(size_t Size, size_t Alignment)
	{
		return static_cast<std::byte*>(verse_heap_try_allocate_with_alignment(reinterpret_cast<pas_heap*>(this), Size, Alignment));
	}
	std::byte* Allocate(size_t Size)
	{
		return static_cast<std::byte*>(verse_heap_allocate(reinterpret_cast<pas_heap*>(this), Size));
	}
	std::byte* Allocate(size_t Size, size_t Alignment)
	{
		return static_cast<std::byte*>(verse_heap_allocate_with_alignment(reinterpret_cast<pas_heap*>(this), Size, Alignment));
	}

	FSubspace() = delete;
	FSubspace(const FSubspace&) = delete;
};

} // namespace Verse
#endif // WITH_VERSE_VM
