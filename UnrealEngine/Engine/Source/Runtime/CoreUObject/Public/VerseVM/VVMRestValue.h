// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMCell.h"
#include "VVMValue.h"
#include "VVMWriteBarrier.h"

namespace Verse
{
struct VFrame;

struct VRestValue
{
	VRestValue(const VRestValue&) = default;
	VRestValue& operator=(const VRestValue&) = default;

	VRestValue(uint16 SplitDepth)
	{
		Reset(SplitDepth);
	}

	void Reset(uint16 SplitDepth)
	{
		SetNonCellNorPlaceholder(VValue::Root(SplitDepth));
	}

	void Set(FAccessContext Context, VValue NewValue)
	{
		checkSlow(!NewValue.IsRoot());
		Value.Set(Context, NewValue);
	}

	void SetTransactionally(FAccessContext Context, VCell& Owner, VValue NewValue);

	void SetNonCellNorPlaceholder(VValue NewValue)
	{
		Value.SetNonCellNorPlaceholder(NewValue);
	}

	bool CanDefQuickly() const
	{
		return Value.Get().IsRoot();
	}

	VValue Get(FAllocationContext Context);

	bool operator==(const VRestValue& Other) const;

	FString ToString(FAllocationContext Context, const FCellFormatter& Formatter) const;
	void ToString(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter) const;

	template <typename TVisitor>
	FORCEINLINE void Visit(TVisitor& Visitor, const TCHAR* ElementName)
	{
		Visitor.Visit(Value, ElementName);
	}

	template <typename TVisitor>
	FORCEINLINE void Visit(TVisitor& Visitor, const TCHAR* ElementName) const
	{
		Visitor.Visit(Value, ElementName);
	}

	friend uint32 GetTypeHash(VRestValue RestValue);

private:
	TWriteBarrier<VValue> Value;

	// TODO: This default constructor is here just to appease VFrame and VArray.
	// It would be nice to find a way to omit it entirely.
	VRestValue() = default;

	friend struct VValue;
	friend struct VFrame;
	friend struct VArray;
	friend struct VObject;
};
} // namespace Verse
#endif // WITH_VERSE_VM
