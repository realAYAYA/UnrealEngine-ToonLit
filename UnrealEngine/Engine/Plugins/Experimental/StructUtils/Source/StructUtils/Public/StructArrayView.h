// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils.h"
#include "InstancedStruct.h"
#include "SharedStruct.h"

///////////////////////////////////////////////////////////////// FStructArrayView /////////////////////////////////////////////////////////////////

/** 
 *	A generic, transient view of a homogeneously-typed array of instances of a specific UScriptStruct
 */
struct FStructArrayView
{
	FStructArrayView(FStructArrayView&& Src)
		: ElementSize(Src.ElementSize)
		, DataPtr(Src.DataPtr)
		, NumElements(Src.NumElements)
		, FragmentType(Src.FragmentType)
	{}

	FStructArrayView(const FStructArrayView& Src)
		: ElementSize(Src.ElementSize)
		, DataPtr(Src.DataPtr)
		, NumElements(Src.NumElements)
		, FragmentType(Src.FragmentType)
	{}
	
	template<typename T>
	explicit FStructArrayView(TArray<T>& InArray)
		: ElementSize(sizeof(T))
		, DataPtr(InArray.GetData())
		, NumElements(InArray.Num())
		, FragmentType(*StaticStruct<typename TRemoveReference<T>::Type>())

	{}

	template<typename T>
	explicit FStructArrayView(TArrayView<T> InArrayView)
		: ElementSize(sizeof(T))
		, DataPtr(InArrayView.GetData())
		, NumElements(InArrayView.Num())
		, FragmentType(*StaticStruct<typename TRemoveReference<T>::Type>())
	{}

	/**
	 * Creates a sub view of the given view
	 */
	FStructArrayView(const FStructArrayView& Src, const int32 OffsetElements, const int32 InNumElements)
		: ElementSize(Src.ElementSize)
		, DataPtr((uint8*)Src.DataPtr + Src.ElementSize * OffsetElements)
		, NumElements(InNumElements)
		, FragmentType(Src.FragmentType)
	{
		checkf(InNumElements + OffsetElements <= Src.NumElements, TEXT("Requested range passes over the end of the view, %d + %d> %d")
			, InNumElements, OffsetElements, Src.NumElements);
	}

	SIZE_T GetElementSize() const { return ElementSize; }
	const void* GetData() const { return DataPtr; }
	const void* GetDataAt(const int32 Index) const { return (uint8*)DataPtr + Index * ElementSize; }
	void* GetMutableDataAt(const int32 Index) const { return (uint8*)DataPtr + Index * ElementSize; }
	int32 Num() const { return NumElements; }
	const UScriptStruct& GetFragmentType() const { return FragmentType; }

	template<typename T>
	const T& GetElementAt(const int32 Index) const
	{
		return *((T*)GetDataAt(Index));
	}

	template<typename T>
	const T& GetElementAtChecked(const int32 Index) const
	{
		check(TBaseStructure<T>::Get() == &FragmentType);
		return *((T*)GetDataAt(Index));
	}

	template<typename T>
	T& GetMutableElementAt(const int32 Index)
	{
		return *((T*)GetDataAt(Index));
	}

	template<typename T>
	T& GetMutableElementAtChecked(const int32 Index)
	{
		check(TBaseStructure<T>::Get() == &FragmentType);
		return *((T*)GetDataAt(Index));
	}

	FORCEINLINE void Swap(const int32 A, const int32 B)
	{
		checkSlow(A >= 0 && A < NumElements);
		checkSlow(B >= 0 && B < NumElements);
		FMemory::Memswap(GetMutableDataAt(A), GetMutableDataAt(B), ElementSize);
	}

private:
	SIZE_T ElementSize = 0;
	void* DataPtr = nullptr;
	int32 NumElements = 0;
	UScriptStruct& FragmentType;
};
