// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/ArrayView.h"

#include "Chaos/Array.h"
#include "Chaos/ArrayCollectionArrayBase.h"

#include <algorithm>

namespace Chaos
{
template<class T>
class TArrayCollectionArray : public TArrayCollectionArrayBase, public TArray<T>
{
	using TArray<T>::SetNum;
	using TArray<T>::RemoveAt;
	using TArray<T>::RemoveAtSwap;
	using TArray<T>::Emplace;
	using TArray<T>::Shrink;
	using TArray<T>::Max;

public:
	constexpr static EAllowShrinking AllowShrinkOnRemove = EAllowShrinking::No;

	using TArray<T>::Num;

	TArrayCollectionArray()
	    : TArray<T>() {}
	TArrayCollectionArray(const TArrayCollectionArray<T>& Other) = delete;
	explicit TArrayCollectionArray(TArrayCollectionArray<T>&& Other)
	    : TArray<T>(MoveTemp(Other)) {}
	TArrayCollectionArray& operator=(TArrayCollectionArray<T>&& Other)
	{
		TArray<T>::operator=(MoveTemp(Other));
		return *this;
	}

	TArrayCollectionArray(TArray<T>&& Other)
	: TArray<T>(MoveTemp(Other))
	{
	}

	virtual ~TArrayCollectionArray() {}

	void Fill(const T& Value)
	{
		for (int32 Idx = 0; Idx < TArray<T>::Num(); ++Idx)
		{
			TArray<T>::operator[](Idx) = Value;
		}
	}

	TArrayCollectionArray<T> Clone()
	{
		TArrayCollectionArray<T> NewArray;
		static_cast<TArray<T>>(NewArray) = static_cast<TArray<T>>(*this);
		return NewArray;
	}

	// If we have more slack space than MaxSlackFraction x Num(), run the default Shrink policy
	void ApplyShrinkPolicy(const float MaxSlackFraction, const int32 MinSlack) override
	{
		// Never shrink below this size
		const int32 Slack = Max() - Num();
		if (Slack <= MinSlack)
		{
			return;
		}

		// Shrink if we exceed the maximum allowed slack
		const int32 MaxSlack = FMath::Max(MinSlack, FMath::FloorToInt(MaxSlackFraction * float(Num())));
		if (Slack > MaxSlack)
		{
			Shrink();
		}
	}

	void Resize(const int Num) override
	{
		SetNum(Num, AllowShrinkOnRemove);
	}

	FORCEINLINE void RemoveAt(const int Idx, const int Count) override
	{
		TArray<T>::RemoveAt(Idx, Count, AllowShrinkOnRemove);
	}

	FORCEINLINE void RemoveAtSwap(const int Idx) override
	{
		TArray<T>::RemoveAtSwap(Idx, 1, AllowShrinkOnRemove);
	}

	FORCEINLINE void MoveToOtherArray(const int Idx, TArrayCollectionArrayBase& Other)
	{
		//todo: add developer check to make sure this is ok?
		auto& OtherTArray = static_cast<TArrayCollectionArray<T>&>(Other);
		OtherTArray.Emplace(MoveTemp(TArray<T>::operator [](Idx)));
		TArray<T>::RemoveAtSwap(Idx, 1, AllowShrinkOnRemove);
	}

	FORCEINLINE uint64 SizeOfElem() const override
	{
		return sizeof(T);
	}
};
}

template<class T>
struct TIsContiguousContainer<Chaos::TArrayCollectionArray<T>>
{
	static constexpr bool Value = TIsContiguousContainer<TArrayView<T>>::Value;
};