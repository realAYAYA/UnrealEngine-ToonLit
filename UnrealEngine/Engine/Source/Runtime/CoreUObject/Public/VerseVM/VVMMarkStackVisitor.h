// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "VVMMarkStack.h"
#include "VVMRestValue.h"

namespace Verse
{

struct FMarkStackVisitor
{
	UE_NONCOPYABLE(FMarkStackVisitor);

	static constexpr bool bIsAbstractVisitor = false;

	// No need to save the string when using the mark stack visitor
	struct ConsumeElementName
	{
		ConsumeElementName(const TCHAR*)
		{
		}
	};

	FMarkStackVisitor(FMarkStack& InMarkStack)
		: MarkStack(InMarkStack)
	{
	}

	FORCEINLINE bool IsMarked(const VCell* InCell, ConsumeElementName ElementName)
	{
		return FHeap::IsMarked(InCell);
	}

	void VisitNonNull(const VCell* InCell, ConsumeElementName ElementName)
	{
		MarkStack.MarkNonNull(InCell);
	}

	void VisitNonNull(const UObject* InObject, ConsumeElementName ElementName)
	{
		MarkStack.MarkNonNull(InObject);
	}

	void VisitAuxNonNull(const void* InAux, ConsumeElementName ElementName)
	{
		MarkStack.MarkAuxNonNull(InAux);
	}

	FORCEINLINE void VisitEmergentType(const VCell* InEmergentType)
	{
		VisitNonNull(InEmergentType, TEXT("EmergentType"));
	}

	FORCEINLINE void Visit(const VCell* InCell, ConsumeElementName ElementName)
	{
		if (InCell != nullptr)
		{
			VisitNonNull(InCell, ElementName);
		}
	}

	FORCEINLINE void Visit(const UObject* InObject, ConsumeElementName ElementName)
	{
		if (InObject != nullptr)
		{
			VisitNonNull(InObject, ElementName);
		}
	}

	FORCEINLINE void VisitAux(const void* Aux, ConsumeElementName ElementName)
	{
		if (Aux != nullptr)
		{
			VisitAuxNonNull(Aux, ElementName);
		}
	}

	FORCEINLINE void Visit(VValue Value, ConsumeElementName ElementName)
	{
		if (VCell* Cell = Value.ExtractCell())
		{
			Visit(Cell, ElementName);
		}
		else if (Value.IsUObject())
		{
			Visit(Value.AsUObject(), ElementName);
		}
	}

	FORCEINLINE void Visit(const VRestValue& Value, ConsumeElementName ElementName)
	{
		Value.Visit(*this, TEXT(""));
	}

	// Null visitors that are only used by the abstract visitor
	FORCEINLINE void Visit(bool bValue, ConsumeElementName ElementName)
	{
	}

	FORCEINLINE void Visit(const FAnsiStringView Value, ConsumeElementName ElementName)
	{
	}

	FORCEINLINE void Visit(const FWideStringView Value, ConsumeElementName ElementName)
	{
	}

	FORCEINLINE void Visit(const FUtf8StringView Value, ConsumeElementName ElementName)
	{
	}

	// NOTE: The Value parameter can not be passed by value.
	template <typename T>
	FORCEINLINE void Visit(const TWriteBarrier<T>& Value, ConsumeElementName ElementName)
	{
		Visit(Value.Get(), ElementName);
	}

	template <typename T>
	FORCEINLINE void Visit(T Begin, T End)
	{
		for (; Begin != End; ++Begin)
		{
			Visit(*Begin, TEXT(""));
		}
	}

	// Arrays
	template <typename ElementType, typename AllocatorType>
	FORCEINLINE void Visit(const TArray<ElementType, AllocatorType>& Values, ConsumeElementName ElementName)
	{
		Visit(Values.begin(), Values.end(), ElementName);
	}

	// Sets
	template <typename ElementType, typename KeyFuncs, typename Allocator>
	FORCEINLINE void Visit(const TSet<ElementType, KeyFuncs, Allocator>& Values, ConsumeElementName ElementName)
	{
		for (const auto& Value : Values)
		{
			Visit(Value, ElementName);
		}
	}

	// Maps
	template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
	FORCEINLINE void Visit(const TMap<KeyType, ValueType, SetAllocator, KeyFuncs>& Values, ConsumeElementName ElementName)
	{
		for (const auto& Kvp : Values)
		{
			Visit(Kvp.Key, TEXT("Key"));
			Visit(Kvp.Value, TEXT("Value"));
		}
	}

	void ReportNativeBytes(size_t Bytes)
	{
		MarkStack.ReportNativeBytes(Bytes);
	}

private:
	FMarkStack& MarkStack;
};

} // namespace Verse
#endif // WITH_VERSE_VM
