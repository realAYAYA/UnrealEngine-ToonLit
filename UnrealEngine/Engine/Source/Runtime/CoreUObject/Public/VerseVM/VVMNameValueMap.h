// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/StringView.h"
#include "VerseVM/Inline/VVMMutableArrayInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMUTF8String.h"

namespace Verse
{
// A helper struct that maps strings to VValues
struct VNameValueMap
{
	VNameValueMap(FAllocationContext Context, uint32 Capacity)
		: NameAndValues(Context, &VMutableArray::New(Context, Capacity))
	{
	}

	// We keep names at 2*Index and Values at 2*Index+1
	TWriteBarrier<VMutableArray> NameAndValues;

	uint32 Num() const
	{
		return NameAndValues->Num() / 2;
	}

	const VUTF8String& GetName(uint32 Index) const
	{
		checkSlow(Index < static_cast<int32>(Num()));
		VValue Value = NameAndValues->GetValue(2 * Index);
		return Value.StaticCast<VUTF8String>();
	}

	VValue GetValue(uint32 Index) const
	{
		checkSlow(Index < static_cast<int32>(Num()));
		return NameAndValues->GetValue(2 * Index + 1);
	}

	template <typename CellType>
	CellType& GetCell(uint32 Index) const
	{
		return GetValue(Index).StaticCast<CellType>();
	}

	void AddValue(FAllocationContext Context, FUtf8StringView Name, VValue Value)
	{
		NameAndValues->AddValue(Context, VUTF8String::New(Context, Name));
		NameAndValues->AddValue(Context, Value);
	}

	void AddValue(FAllocationContext Context, VUTF8String& Name, VValue Value)
	{
		NameAndValues->AddValue(Context, VValue(Name));
		NameAndValues->AddValue(Context, Value);
	}

	VValue Lookup(FUtf8StringView Name) const
	{
		for (uint32 Index = 0, End = Num(); Index < End; ++Index)
		{
			if (GetName(Index).Equals(Name))
			{
				return GetValue(Index);
			}
		}
		return VValue();
	}

	template <typename CellType>
	CellType* LookupCell(FUtf8StringView Name) const
	{
		VValue Value = Lookup(Name);
		if (Value.IsCell())
		{
			VCell& Cell = Value.AsCell();
			if (Cell.IsA<CellType>())
			{
				return &Cell.StaticCast<CellType>();
			}
		}
		return nullptr;
	}

	template <typename TVisitor>
	void Visit(TVisitor& Visitor, const TCHAR* MapName)
	{
		Visitor.Visit(NameAndValues, MapName);
	}
};
} // namespace Verse

#endif // WITH_VERSE_VM
