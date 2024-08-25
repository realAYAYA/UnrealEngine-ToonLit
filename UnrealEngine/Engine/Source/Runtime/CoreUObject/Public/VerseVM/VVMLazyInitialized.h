// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "VVMLog.h"
#include <atomic>

namespace Verse
{

COREUOBJECT_API void AddLeakedObjectForLazyInitialization(void* Object);

// Acts like a smart pointer to T, meant to be used in global context, that:
//
// - Lazily initializes itself. It's only when you first touch it that anything happens.
// - Never destroys itself.
// - Is thread-safe. It's fine if you race on initialization.
// - Doesn't do any fences or atomic ops in steady state.
// - Has a leak "by design" - if there is a race on initialization, then there will be a leak.
//
// Basically, if N threads race on initialization, then you might allocate N versions of T but only one will win,
// and the others will be allowed to leak. Since this will Never Happen (TM), it's fine.
template <typename T>
struct TLazyInitialized
{
	TLazyInitialized() = default;

	T& Get()
	{
		T* Result = Data.load(std::memory_order_relaxed);
		std::atomic_signal_fence(std::memory_order_seq_cst);
		if (Result)
		{
			return *Result;
		}
		else
		{
			return GetSlow();
		}
	}

	T& operator*()
	{
		return Get();
	}

	T* operator->()
	{
		return &Get();
	}

	const T& Get() const
	{
		return const_cast<TLazyInitialized*>(this)->Get();
	}

	const T& operator*() const
	{
		return Get();
	}

	const T* operator->() const
	{
		return &Get();
	}

private:
	FORCENOINLINE T& GetSlow()
	{
		T* Object = new T();
		T* Expected = nullptr;
		Data.compare_exchange_strong(Expected, Object);
		T* Result;
		if (Expected)
		{
			AddLeakedObjectForLazyInitialization(Object);
			Result = Expected;
		}
		else
		{
			Result = Object;
		}
		V_DIE_UNLESS(Data.load() == Result);
		return *Result;
	}

	std::atomic<T*> Data = nullptr;
};

} // namespace Verse
#endif // WITH_VERSE_VM
