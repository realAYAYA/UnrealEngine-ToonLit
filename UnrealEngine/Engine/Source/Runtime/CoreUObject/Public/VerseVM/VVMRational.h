// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "VVMContext.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMInt.h"
#include "VVMValue.h"

namespace Verse
{

struct VRational : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	TWriteBarrier<VValue> Numerator;
	TWriteBarrier<VValue> Denominator;

	static VRational& Add(FRunningContext Context, VRational& Lhs, VRational& Rhs);
	static VRational& Sub(FRunningContext Context, VRational& Lhs, VRational& Rhs);
	static VRational& Mul(FRunningContext Context, VRational& Lhs, VRational& Rhs);
	static VRational& Div(FRunningContext Context, VRational& Lhs, VRational& Rhs);
	static VRational& Neg(FRunningContext Context, VRational& N);
	static bool Eq(FRunningContext Context, VRational& Lhs, VRational& Rhs);
	static bool Gt(FRunningContext Context, VRational& Lhs, VRational& Rhs);
	static bool Lt(FRunningContext Context, VRational& Lhs, VRational& Rhs);
	static bool Gte(FRunningContext Context, VRational& Lhs, VRational& Rhs);
	static bool Lte(FRunningContext Context, VRational& Lhs, VRational& Rhs);

	VInt Floor(FRunningContext Context) const;
	VInt Ceil(FRunningContext Context) const;

	void Reduce(FRunningContext Context);
	void NormalizeSigns(FRunningContext Context);
	bool IsZero() const { return Numerator.Get().AsInt().IsZero(); }
	bool IsReduced() const { return bIsReduced; }

	static VRational& New(FAllocationContext Context, VInt InNumerator, VInt InDenominator)
	{
		return *new (Context.AllocateFastCell(sizeof(VRational))) VRational(Context, InNumerator, InDenominator);
	}

	static VRational& New(FAllocationContext Context, VValue InNumerator, VValue InDenominator)
	{
		return *new (Context.AllocateFastCell(sizeof(VRational))) VRational(Context, InNumerator, InDenominator);
	}

	COREUOBJECT_API bool EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder);

	COREUOBJECT_API uint32 GetTypeHashImpl();

	COREUOBJECT_API void ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter);

	static void SerializeImpl(VRational*& This, FAllocationContext Context, FAbstractVisitor& Visitor);

private:
	VRational(FAllocationContext Context, VValue InNumerator, VValue InDenominator)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, bIsReduced(false)
	{
		checkSlow(InDenominator.IsInt() && InNumerator.IsInt());
		checkSlow(!InDenominator.AsInt().IsZero());
		Numerator.Set(Context, InNumerator);
		Denominator.Set(Context, InDenominator);
	}

	VRational(FAllocationContext Context, VInt InNumerator, VInt InDenominator)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, bIsReduced(false)
	{
		checkSlow(!InDenominator.IsZero());
		Numerator.Set(Context, InNumerator);
		Denominator.Set(Context, InDenominator);
	}

	bool bIsReduced;
};

} // namespace Verse
#endif // WITH_VERSE_VM
