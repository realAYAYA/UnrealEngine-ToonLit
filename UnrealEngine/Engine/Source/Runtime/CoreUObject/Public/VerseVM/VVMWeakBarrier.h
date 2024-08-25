// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMAux.h"
#include "VVMContext.h"
#include "VVMContextImpl.h"
#include "VVMValue.h"

namespace Verse
{
template <typename T>
struct TWeakBarrier
{
	static constexpr bool bIsVValue = std::is_same_v<T, VValue>;
	static constexpr bool bIsAux = IsTAux<T>;
	using TValue = typename std::conditional_t<bIsAux, T, typename std::conditional_t<bIsVValue, VValue, T*>>;
	using TEncodedValue = typename std::conditional<bIsVValue, uint64, T*>::type;

	TWeakBarrier() = default;

	/// Making use of this constructor is potentially expensive because it could involve a TLS lookup.
	TWeakBarrier(const TWeakBarrier& Other)
	{
		Value = RunReadBarrier(FAccessContextPromise(), Other.Value);
	}

	TWeakBarrier& operator=(const TWeakBarrier& Other)
	{
		Value = RunReadBarrier(FAccessContextPromise(), Other.Value);
		return *this;
	}

	/// Making use of this constructor is potentially expensive because it could involve a TLS lookup.
	TWeakBarrier(TWeakBarrier&& Other)
	{
		Value = RunReadBarrier(FAccessContextPromise(), Other.Value);
		Other.Reset();
	}

	TWeakBarrier& operator=(TWeakBarrier&& Other)
	{
		Value = RunReadBarrier(FAccessContextPromise(), Other.Value);
		Other.Reset();
		return *this;
	}

	TWeakBarrier(TValue Value)
		: Value(Value)
	{
	}

	TWeakBarrier& operator=(TValue InValue)
	{
		Value = RunReadBarrier(FAccessContextPromise(), InValue);
		return *this;
	}

	template <typename U = T>
	TWeakBarrier(std::enable_if_t<!bIsVValue, U>& Value)
		: Value(&Value)
	{
	}

	bool operator==(const TWeakBarrier& Other) const
	{
		return Value == Other.Value;
	}

	template <typename TResult = void>
	std::enable_if_t<!bIsVValue, TResult> Set(T& NewValue)
	{
		Value = &NewValue;
	}

	void Set(TValue NewValue)
	{
		Value = NewValue;
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

	TValue Get(FAccessContext Context) const { return RunReadBarrier(Context, Value); }
	TValue Get() const { return RunReadBarrier(FAccessContextPromise(), Value); }

	// nb: operators "*" and "->" disabled for TWeakBarrier<VValue>;
	//     use Get() + VValue member functions to check/access boxed values

	template <typename TResult = TValue>
	std::enable_if_t<!bIsVValue && !bIsAux, TResult> operator->() const { return Get(); }

	template <typename TResult = T>
	std::enable_if_t<!bIsVValue && !bIsAux, TResult&> operator*() const { return *Get(); }

	explicit operator bool() const { return !!Get(); }

	/**
	 * Clears out weak references that are unmarked (i.e. non-live).
	 *
	 * @return 	If the value was/wasn't cleared out.
	 */
	bool ClearWeakDuringCensus()
	{
		std::atomic<TEncodedValue>* ValuePtr = reinterpret_cast<std::atomic<TEncodedValue>*>(&Value);
		TValue ValueCopy = Value;
		if constexpr (bIsVValue)
		{
			uint64 EncodedValueCopy = ValueCopy.GetEncodedBits();
			if (const VCell* Cell = ValueCopy.ExtractCell())
			{
				if (!FHeap::IsMarked(Cell))
				{
					return ValuePtr->compare_exchange_strong(EncodedValueCopy, VValue().GetEncodedBits(), std::memory_order_relaxed);
				}
			}
		}
		else
		{
			if (ValueCopy && !FHeap::IsMarked(ValueCopy))
			{
				return ValuePtr->compare_exchange_strong(ValueCopy, nullptr, std::memory_order_relaxed);
			}
		}
		return false;
	}

private:
	TValue Value{};

	template <typename ContextType>
	static TValue RunReadBarrier(ContextType Context, TValue Value)
	{
		if (FHeap::GetWeakBarrierState() == EWeakBarrierState::Inactive)
		{
			return Value;
		}
		if constexpr (bIsAux)
		{
			if (!FHeap::IsMarked(Value.GetPtr()))
			{
				FAccessContext(Context).RunAuxWeakReadBarrierUnmarkedWhenActive(Value.GetPtr());
			}
		}
		else if constexpr (bIsVValue)
		{
			if (VCell* Cell = Value.ExtractCell())
			{
				if (!FHeap::IsMarked(Cell))
				{
					// Delay construction of the context (which does the expensive TLS lookup), until we actually need the mark stack to do marking.
					Cell = FAccessContext(Context).RunWeakReadBarrierUnmarkedWhenActive(Cell);
					if (Cell)
					{
						return *Cell;
					}
					else
					{
						return VValue();
					}
				}
			}
			return Value;
		}
		else
		{
			if (Value)
			{
				if (FHeap::IsMarked(Value))
				{
					return Value;
				}
				else
				{
					return BitCast<TValue>(FAccessContext(Context).RunWeakReadBarrierUnmarkedWhenActive(Value));
				}
			}
			else
			{
				return nullptr;
			}
		}
	}
};

template <typename T>
uint32 GetTypeHash(const TWeakBarrier<T>& WeakBarrier)
{
	return GetTypeHash(WeakBarrier.Get());
}
} // namespace Verse
#endif // WITH_VERSE_VM
