// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMSuspension.h"

namespace Verse
{
struct VPlaceholder : public VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	TWriteBarrier<VValue> Value;

	static VPlaceholder& New(FAllocationContext Context, uint16 SplitDepth)
	{
		return *new (Context.AllocateFastCell(sizeof(VPlaceholder))) VPlaceholder(Context, SplitDepth);
	}

	uint16 SplitDepth() const { return Misc2And3; }
	bool HasSuspension() const { return Mode() == EMode::HasSuspension; }
	bool HasValue() const { return Mode() == EMode::HasValue; }
	bool HasParent() const { return Mode() == EMode::HasParent; }

	VSuspension* GetSuspension() const
	{
		checkSlow(HasSuspension());
		if (Value.Get().IsUninitialized())
		{
			return nullptr;
		}
		else
		{
			return &Value.Get().AsCell().StaticCast<VSuspension>();
		}
	}

	VValue GetValue() const
	{
		checkSlow(HasValue());
		return Value.Get();
	}

	VPlaceholder* GetParent() const
	{
		checkSlow(HasParent());
		return &Value.Get().AsPlaceholder();
	}

	void Unify(FAccessContext Context, VPlaceholder& Other)
	{
		checkSlow(HasSuspension() && Other.HasSuspension());
		if (!GetSuspension())
		{
			SetParent(Context, Other);
			return;
		}
		if (!Other.GetSuspension())
		{
			Other.SetParent(Context, *this);
			return;
		}

		GetSuspension()->Tail().Next.Set(Context, Other.GetSuspension());
		Other.SetParent(Context, *this);
	}

	VSuspension* SetValue(FAccessContext Context, VValue InValue)
	{
		checkSlow(!InValue.IsPlaceholder());
		VSuspension* Result = GetSuspension();
		Mode() = EMode::HasValue;
		Value.Set(Context, InValue);
		return Result;
	}

	VValue Follow()
	{
		const VPlaceholder* Current = this;
		while (true)
		{
			// TODO: Should we path compress? We should figure out if that makes sense
			// once we're fully transactional.
			if (Current->HasValue())
			{
				return Current->GetValue();
			}
			if (Current->HasSuspension())
			{
				return VValue::Placeholder(*Current);
			}
			Current = Current->GetParent();
		}
	}

	// TODO: For perf, think through if FIFO or LIFO will be faster.
	void EnqueueSuspension(FRunningContext Context, VSuspension& Suspension)
	{
		VPlaceholder& Root = Follow().GetRootPlaceholder();
		if (VSuspension* PreviousSuspension = Root.GetSuspension())
		{
			Suspension.Next.Set(Context, PreviousSuspension);
		}
		Root.Value.Set(Context, Suspension);
	}

private:
	void SetParent(FAccessContext Context, VPlaceholder& Other)
	{
		Mode() = EMode::HasParent;
		Value.Set(Context, VValue::Placeholder(Other));
	}

	enum class EMode : uint8
	{
		HasSuspension,
		HasValue,
		HasParent,
	};

	EMode& Mode() const { return *BitCast<EMode*>(&Misc3); }

	VPlaceholder(FAllocationContext Context, uint16 InputSplitDepth)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
	{
		checkSlow(SplitDepth() == InputSplitDepth);
		Misc2And3 = InputSplitDepth;
		Mode() = EMode::HasSuspension;
		checkSlow(!GetSuspension());
	}
};
} // namespace Verse
#endif // WITH_VERSE_VM
