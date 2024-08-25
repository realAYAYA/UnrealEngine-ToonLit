// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CastedTo.h"

enum class ESmartPointer
{
	Strong,
	Shared,
	Weak,
	// Unique(?) - not sure if needed
};

/* A container to hold one of the FOUR Smart Pointer types: TStrongObjectPtr, TWeakObjectPtr, TSharedPtr, or TWeakPtr. 
* During construction, there will be a compile time attempt to static_cast a raw pointer 'From' to 'T'
* 'From' must derive from 'UObject' xOr 'TSharedFromThis' for this class to be usable
*/
template<typename T>
class TSmartPtr
{
public:
	TSmartPtr() : Container(MakeShared<TPointerContainer<T>>()) {}

	template<typename From>
	TSmartPtr(From* InObject)
	{
		Set(InObject, ESmartPointer::Weak);
	}

	template<typename From>
	TSmartPtr(From* InObject, const ESmartPointer InType)
	{
		Set(InObject, InType);
	}

	template<typename From>
	TSmartPtr<T>& Set(From* InObject, const ESmartPointer InType)
	{
		if (InType == ESmartPointer::Strong || InType == ESmartPointer::Shared)
		{
			ensureAlwaysMsgf(
				(TIsDerivedFrom<From, UObject>::IsDerived && InType == ESmartPointer::Strong) ||
				(IsDerivedFromSharedFromThis<From>() && InType == ESmartPointer::Shared),
				TEXT("Mismatch of Strong and Shared. We can continue on holding a 'strong' reference, but you shouldn't mix UObjects and Shared Pointers"));

			Container = MakeShared<TStrongCastable<From, T>>(InObject);
		}
		else if (InType == ESmartPointer::Weak)
		{
			Container = MakeShared<TWeakCastable<From, T>>(InObject);
		}

		return *this;
	}

	T* Get() const { return Container->Get(); }
	bool IsValid() const { return Container->IsValid(); }
	void Reset() const { Container->Reset(); }

private:
	TSharedRef<TPointerContainer<T>> Container = MakeShared<TPointerContainer<T>>();
};