// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMArray.h"
#include "VerseVM/VVMArrayBase.h"
#include "VerseVM/VVMInt.h"

namespace Verse
{

inline bool VArrayBase::IsInBounds(uint32 Index) const
{
	return Index < Num();
}

inline bool VArrayBase::IsInBounds(const VInt& Index, const uint32 Bounds) const
{
	if (Index.IsInt64())
	{
		const int64 IndexInt64 = Index.AsInt64();
		return (IndexInt64 >= 0) && (IndexInt64 < Bounds);
	}
	else
	{
		// Array maximum size is limited to the maximum size of a unsigned 32-bit integer.
		// So even if it's a `VHeapInt`, if it fails the `IsInt64` check, it is definitely out-of-bounds.
		return false;
	}
}

inline VValue VArrayBase::GetValue(uint32 Index)
{
	checkSlow(IsInBounds(Index));
	return Values.Get()[Index].Follow();
}

inline void VArrayBase::SetValue(FAccessContext Context, uint32 Index, VValue Value)
{
	checkSlow(IsInBounds(Index));
	Values.Get()[Index].Set(Context, Value);
}

template <typename T>
inline T& VArrayBase::Concat(FAllocationContext Context, VArrayBase& Lhs, VArrayBase& Rhs)
{
	T& NewArray = T::New(Context, Lhs.Num() + Rhs.Num());
	uint32 Index = 0;
	for (uint32 I = 0; I < Lhs.Num(); ++I)
	{
		NewArray.SetValue(Context, Index++, Lhs.GetValue(I));
	}
	for (uint32 J = 0; J < Rhs.Num(); ++J)
	{
		NewArray.SetValue(Context, Index++, Rhs.GetValue(J));
	}
	return NewArray;
}

template <typename TVisitor>
inline void VArrayBase::VisitReferencesImpl(TVisitor& Visitor)
{
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		Visitor.VisitAux(GetData(), TEXT("ValuesBuffer")); // Visit the buffer we allocated for the array as Aux memory
		uint64 ScratchNumValues = Num();
		Visitor.BeginArray(TEXT("Values"), ScratchNumValues);
		Visitor.Visit(GetData(), GetData() + Num()); // Visit allocated elements in the buffer
		Visitor.EndArray();
	}
	else
	{
		Visitor.VisitAux(GetData(), TEXT("ValuesBuffer")); // Visit the buffer we allocated for the array as Aux memory
		Visitor.Visit(GetData(), GetData() + Num());       // Visit allocated elements in the buffer
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM
