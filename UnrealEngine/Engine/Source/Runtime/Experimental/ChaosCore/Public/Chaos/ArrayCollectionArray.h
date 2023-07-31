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

public:
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

	void Resize(const int Num) override
	{
		SetNum(Num);
	}

	FORCEINLINE void RemoveAt(const int Idx, const int Count) override
	{
		TArray<T>::RemoveAt(Idx, Count);
	}

	FORCEINLINE void RemoveAtSwap(const int Idx) override
	{
		TArray<T>::RemoveAtSwap(Idx);
	}

	FORCEINLINE void MoveToOtherArray(const int Idx, TArrayCollectionArrayBase& Other)
	{
		//todo: add developer check to make sure this is ok?
		auto& OtherTArray = static_cast<TArrayCollectionArray<T>&>(Other);
		OtherTArray.Emplace(MoveTemp(TArray<T>::operator [](Idx)));
		TArray<T>::RemoveAtSwap(Idx);
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