// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "VVMAux.h"
#include "VVMContext.h"
#include "VVMContextImpl.h"
#include "VVMValue.h"

namespace Verse
{

// This class is a smart pointer that runs a GC write barrier whenever the stored pointer is changed
// Fundamental law of concurrent GC: when thoust mutated the heap thou shalt run the barrier template
// When a VValue of heap pointer type (VCell*) is written, a barrier is run (int32s and floats don't run a barrier)
// The barrier will inform the GC about a new edge in the heap, and GC will immediately mark the cell if a collection is ongoing
// This is necessary because GC might have already visited the previous content of Value, and might miss the updated value in that case
// No barrier is needed when _deleting_ heap references, therefore we don't care about the previous Value during mutation
// It does not matter if the barrier is run before or after mutation since we unconditionally mark only the new value
// We also run the barrier during TWriteBarrier construction since a white (= otherwise unreachable) cell might have been assigned
// New cells are allocated black (marked, reachable) so we are not worried about those here
template <typename T>
struct TWriteBarrier
{
	static constexpr bool bIsVValue = std::is_same_v<T, VValue>;
	static constexpr bool bIsAux = IsTAux<T>;
	using TValue = typename std::conditional_t<bIsAux, T, typename std::conditional_t<bIsVValue, VValue, T*>>;
	using TEncodedValue = typename std::conditional<bIsVValue, uint64, T*>::type;

	TWriteBarrier() = default;

	/// Making use of this constructor is potentially expensive because it could involve a TLS lookup.
	TWriteBarrier(const TWriteBarrier& Other)
	{
		RunBarrier(FAccessContextPromise(), Other.Value);
		Value = Other.Value;
	}

	TWriteBarrier& operator=(const TWriteBarrier& Other)
	{
		RunBarrier(FAccessContextPromise(), Other.Value);
		Value = Other.Value;
		return *this;
	}

	/// Making use of this constructor is potentially expensive because it could involve a TLS lookup.
	TWriteBarrier(TWriteBarrier&& Other)
	{
		RunBarrier(FAccessContextPromise(), Other.Value);
		Move(Value, Other.Value);
	}

	TWriteBarrier& operator=(TWriteBarrier&& Other)
	{
		RunBarrier(FAccessContextPromise(), Other.Value);
		Move(Value, Other.Value);
		return *this;
	}

	bool Equals(const TWriteBarrier& Other) const
	{
		return Value == Other.Value;
	}

	// Needed to allow for `TWriteBarrier` to be usable with `TMap`.
	bool operator==(const TWriteBarrier& Other) const
	{
		return Equals(Other);
	}

	TWriteBarrier(FAccessContext Context, TValue Value)
	{
		Set(Context, Value);
	}

	template <
		typename U = T,
		std::enable_if_t<!bIsVValue && std::is_convertible_v<U*, T*>>* = nullptr>
	TWriteBarrier(FAccessContext Context, std::enable_if_t<!bIsVValue, U>& Value)
	{
		Set(Context, &Value);
	}

	template <typename TResult = void>
	std::enable_if_t<!bIsVValue, TResult> Set(FAccessContext Context, T& NewValue)
	{
		Set(Context, &NewValue);
	}

	void Reset()
	{
		if constexpr (bIsVValue)
		{
			Value = {};
		}
		else
		{
			Value = nullptr;
		}
	}

	void Set(FAccessContext Context, TValue NewValue)
	{
		RunBarrier(Context, NewValue);
		Value = NewValue;
	}

	template <typename TResult = void>
	std::enable_if_t<bIsVValue, TResult> SetTransactionally(FAccessContext Context, VCell& Owner, TValue NewValue);

	template <typename TResult = void>
	std::enable_if_t<bIsVValue, TResult> SetNonCellNorPlaceholder(VValue NewValue)
	{
		checkSlow(!NewValue.IsCell());
		checkSlow(!NewValue.IsPlaceholder());
		Value = NewValue;
	}

	TValue Get() const { return Value; }
	template <typename TResult = TValue>
	std::enable_if_t<bIsVValue, TResult> Follow() { return Get().Follow(); }

	// nb: operators "*" and "->" disabled for TWriteBarrier<VValue>;
	//     use Get() + VValue member functions to check/access boxed values

	template <typename TResult = TValue>
	std::enable_if_t<!bIsVValue && !bIsAux, TResult> operator->() const { return Value; }

	template <typename TResult = T>
	std::enable_if_t<!bIsVValue && !bIsAux, TResult&> operator*() const { return *Value; }

	explicit operator bool() const { return !!Value; }

	friend uint32 GetTypeHash(const TWriteBarrier<T>& WriteBarrier)
	{
		if constexpr (bIsVValue)
		{
			return GetTypeHash(WriteBarrier.Get());
		}
		else
		{
			return GetTypeHash(*WriteBarrier.Get());
		}
	}

private:
	TValue Value{};

	template <typename ContextType>
	static void RunBarrier(ContextType Context, TValue Value)
	{
		if (!FHeap::IsMarking())
		{
			return;
		}
		if constexpr (bIsAux)
		{
			if (!FHeap::IsMarked(Value.GetPtr()))
			{
				FAccessContext(Context).RunAuxWriteBarrierNonNullDuringMarking(Value.GetPtr());
			}
		}
		else if constexpr (bIsVValue)
		{
			if (VCell* Cell = Value.ExtractCell())
			{
				if (!FHeap::IsMarked(Cell))
				{
					// Delay construction of the context (which does the expensive TLS lookup), until we actually need the mark stack to do marking.
					FAccessContext(Context).RunWriteBarrierNonNullDuringMarking(Cell);
				}
			}
			else if (Value.IsUObject())
			{
				if (UE::GC::GIsIncrementalReachabilityPending)
				{
					Value.AsUObject()->VerseMarkAsReachable();
				}
			}
		}
		else
		{
			VCell* Cell = reinterpret_cast<VCell*>(Value);
			if (Cell && !FHeap::IsMarked(Cell))
			{
				FAccessContext(Context).RunWriteBarrierNonNullDuringMarking(Cell);
			}
		}
	}
};
} // namespace Verse
#endif // WITH_VERSE_VM
