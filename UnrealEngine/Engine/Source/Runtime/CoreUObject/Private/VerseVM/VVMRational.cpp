// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMRational.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VRational);
TGlobalTrivialEmergentTypePtr<&VRational::StaticCppClassInfo> VRational::GlobalTrivialEmergentType;

VRational& VRational::Add(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()))
	{
		return VRational::New(Context,
			VInt::Add(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
			Lhs.Denominator.Get().AsInt());
	}

	return VRational::New(
		Context,
		VInt::Add(Context,
			VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
			VInt::Mul(Context, Rhs.Numerator.Get().AsInt(), Lhs.Denominator.Get().AsInt())),
		VInt::Mul(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()));
}

VRational& VRational::Sub(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()))
	{
		return VRational::New(Context,
			VInt::Sub(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
			Lhs.Denominator.Get().AsInt());
	}

	return VRational::New(
		Context,
		VInt::Sub(Context,
			VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
			VInt::Mul(Context, Rhs.Numerator.Get().AsInt(), Lhs.Denominator.Get().AsInt())),
		VInt::Mul(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()));
}

VRational& VRational::Mul(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	return VRational::New(Context,
		VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Numerator.Get().AsInt()),
		VInt::Mul(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()));
}

VRational& VRational::Div(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	return VRational::New(Context,
		VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
		VInt::Mul(Context, Lhs.Denominator.Get().AsInt(), Rhs.Numerator.Get().AsInt()));
}

VRational& VRational::Neg(FRunningContext Context, VRational& N)
{
	return VRational::New(Context, VInt::Neg(Context, N.Numerator.Get().AsInt()), N.Denominator.Get().AsInt());
}

bool VRational::Eq(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	Lhs.Reduce(Context);
	Lhs.NormalizeSigns(Context);
	Rhs.Reduce(Context);
	Rhs.NormalizeSigns(Context);

	return VInt::Eq(Context, Lhs.Numerator.Get().AsInt(), Rhs.Numerator.Get().AsInt())
		&& VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt());
}

bool VRational::Gt(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()))
	{
		return VInt::Gt(Context, Lhs.Numerator.Get().AsInt(), Rhs.Numerator.Get().AsInt());
	}

	return VInt::Gt(Context,
		VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
		VInt::Mul(Context, Rhs.Numerator.Get().AsInt(), Lhs.Denominator.Get().AsInt()));
}

bool VRational::Lt(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()))
	{
		return VInt::Lt(Context, Lhs.Numerator.Get().AsInt(), Rhs.Numerator.Get().AsInt());
	}

	return VInt::Lt(Context,
		VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
		VInt::Mul(Context, Rhs.Numerator.Get().AsInt(), Lhs.Denominator.Get().AsInt()));
}

bool VRational::Gte(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()))
	{
		return VInt::Gte(Context, Lhs.Numerator.Get().AsInt(), Rhs.Numerator.Get().AsInt());
	}

	return VInt::Gte(Context,
		VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
		VInt::Mul(Context, Rhs.Numerator.Get().AsInt(), Lhs.Denominator.Get().AsInt()));
}

bool VRational::Lte(FRunningContext Context, VRational& Lhs, VRational& Rhs)
{
	if (VInt::Eq(Context, Lhs.Denominator.Get().AsInt(), Rhs.Denominator.Get().AsInt()))
	{
		return VInt::Lte(Context, Lhs.Numerator.Get().AsInt(), Rhs.Numerator.Get().AsInt());
	}

	return VInt::Lte(Context,
		VInt::Mul(Context, Lhs.Numerator.Get().AsInt(), Rhs.Denominator.Get().AsInt()),
		VInt::Mul(Context, Rhs.Numerator.Get().AsInt(), Lhs.Denominator.Get().AsInt()));
}

VInt VRational::Floor(FRunningContext Context) const
{
	VInt IntNumerator(Numerator.Get());
	VInt IntDenominator(Denominator.Get());
	bool bHasNonZeroRemainder = false;
	VInt IntQuotient = VInt::Div(Context, IntNumerator, IntDenominator, &bHasNonZeroRemainder);
	if (bHasNonZeroRemainder && (IntNumerator.IsNegative() != IntDenominator.IsNegative()))
	{
		IntQuotient = VInt::Sub(Context, IntQuotient, VInt(1));
	}
	return IntQuotient;
}

VInt VRational::Ceil(FRunningContext Context) const
{
	VInt IntNumerator(Numerator.Get());
	VInt IntDenominator(Denominator.Get());
	bool bHasNonZeroRemainder = false;
	VInt IntQuotient = VInt::Div(Context, IntNumerator, IntDenominator, &bHasNonZeroRemainder);
	if (bHasNonZeroRemainder && (IntNumerator.IsNegative() == IntDenominator.IsNegative()))
	{
		IntQuotient = VInt::Add(Context, IntQuotient, VInt(1));
	}
	return IntQuotient;
}

void VRational::Reduce(FRunningContext Context)
{
	if (bIsReduced)
	{
		return;
	}

	VInt A = Numerator.Get().AsInt();
	VInt B = Denominator.Get().AsInt();
	while (!VInt::Eq(Context, B, VInt(0)))
	{
		VInt Remainder = VInt::Mod(Context, A, B);
		A = B;
		B = Remainder;
	}

	Numerator.Set(Context, VInt::Div(Context, Numerator.Get().AsInt(), A));
	Denominator.Set(Context, VInt::Div(Context, Denominator.Get().AsInt(), A));
	bIsReduced = true;
}

void VRational::NormalizeSigns(FRunningContext Context)
{
	VInt Denom = Denominator.Get().AsInt();
	if (VInt::Lt(Context, Denom, VInt(0)))
	{
		// The denominator is < 0, so we need to normalize the signs
		Numerator.Set(Context, VInt::Neg(Context, Numerator.Get().AsInt()));
		Denominator.Set(Context, VInt::Neg(Context, Denom));
	}
}

template <typename TVisitor>
void VRational::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Numerator, TEXT("Numerator"));
	Visitor.Visit(Denominator, TEXT("Denominator"));
}

void VRational::SerializeImpl(VRational*& This, FAllocationContext Context, FAbstractVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		VValue ScratchNumerator;
		VValue ScratchDenominator;
		Visitor.Visit(ScratchNumerator, TEXT("Numerator"));
		Visitor.Visit(ScratchDenominator, TEXT("Denominator"));
		This = &VRational::New(Context, ScratchNumerator, ScratchDenominator);
	}
	else
	{
		This->VisitReferences(Visitor);
	}
}

bool VRational::EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (!Other->IsA<VRational>())
	{
		return false;
	}
	return Eq(Context, *this, Other->StaticCast<VRational>());
}

uint32 VRational::GetTypeHashImpl()
{
	if (!bIsReduced)
	{
		// TLS lookup to reduce rationals before hashing
		// FRunningContextPromise PromiseContext;
		FRunningContext Context((FRunningContextPromise()));
		Reduce(Context);
		NormalizeSigns(Context);
	}
	return ::HashCombineFast(GetTypeHash(Numerator.Get()), GetTypeHash(Denominator.Get()));
}

void VRational::ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter)
{
	Numerator.Get().ToString(Builder, Context, Formatter);
	Builder.Append(TEXT(" / "));
	Denominator.Get().ToString(Builder, Context, Formatter);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)