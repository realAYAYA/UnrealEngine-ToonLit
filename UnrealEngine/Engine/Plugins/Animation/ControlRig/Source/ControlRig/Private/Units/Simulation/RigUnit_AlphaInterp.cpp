// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Simulation/RigUnit_AlphaInterp.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_AlphaInterp)

FRigUnit_AlphaInterp_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	ScaleBiasClamp.bMapRange = bMapRange;
	ScaleBiasClamp.bClampResult = bClampResult;
	ScaleBiasClamp.bInterpResult = bInterpResult;

	if (Context.State == EControlRigState::Init)
	{
		ScaleBiasClamp.Reinitialize();
	}
	else
	{
		ScaleBiasClamp.InRange = InRange;
		ScaleBiasClamp.OutRange = OutRange;
		ScaleBiasClamp.ClampMin = ClampMin;
		ScaleBiasClamp.ClampMax = ClampMax;
		ScaleBiasClamp.Scale = Scale;
		ScaleBiasClamp.Bias = Bias;
		ScaleBiasClamp.InterpSpeedIncreasing = InterpSpeedIncreasing;
		ScaleBiasClamp.InterpSpeedDecreasing = InterpSpeedDecreasing;

		Result = ScaleBiasClamp.ApplyTo(Value, Context.DeltaTime);
	}
}

FString FRigUnit_AlphaInterp::ProcessPinLabelForInjection(const FString& InLabel) const
{
	FString Formula;
	if (bMapRange)
	{
		Formula += FString::Printf(TEXT(" Map(%.02f, %.02f, %.02f, %.02f)"), InRange.Min, InRange.Max, OutRange.Min, OutRange.Max);
	}
	if (bInterpResult)
	{
		Formula += FString::Printf(TEXT(" Interp(%.02f, %.02f)"), InterpSpeedIncreasing, InterpSpeedDecreasing);
	}
	if (bClampResult)
	{
		Formula += FString::Printf(TEXT(" Clamp(%.02f, %.02f)"), ClampMin, ClampMax);
	}

	if (Formula.IsEmpty())
	{
		return InLabel;
	}
	return FString::Printf(TEXT("%s: %s"), *InLabel, *Formula);
}

FRigUnit_AlphaInterpVector_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	ScaleBiasClamp.bMapRange = bMapRange;
	ScaleBiasClamp.bClampResult = bClampResult;
	ScaleBiasClamp.bInterpResult = bInterpResult;

	if (Context.State == EControlRigState::Init)
	{
		ScaleBiasClamp.Reinitialize();
	}
	else
	{
		ScaleBiasClamp.InRange = InRange;
		ScaleBiasClamp.OutRange = OutRange;
		ScaleBiasClamp.ClampMin = ClampMin;
		ScaleBiasClamp.ClampMax = ClampMax;
		ScaleBiasClamp.Scale = Scale;
		ScaleBiasClamp.Bias = Bias;
		ScaleBiasClamp.InterpSpeedIncreasing = InterpSpeedIncreasing;
		ScaleBiasClamp.InterpSpeedDecreasing = InterpSpeedDecreasing;

		Result.X = ScaleBiasClamp.ApplyTo(Value.X, Context.DeltaTime);
		Result.Y = ScaleBiasClamp.ApplyTo(Value.Y, Context.DeltaTime);
		Result.Z = ScaleBiasClamp.ApplyTo(Value.Z, Context.DeltaTime);
	}
}

FString FRigUnit_AlphaInterpVector::ProcessPinLabelForInjection(const FString& InLabel) const
{
	FString Formula;
	if (bMapRange)
	{
		Formula += FString::Printf(TEXT(" Map(%.02f, %.02f, %.02f, %.02f)"), InRange.Min, InRange.Max, OutRange.Min, OutRange.Max);
	}
	if (bInterpResult)
	{
		Formula += FString::Printf(TEXT(" Interp(%.02f, %.02f)"), InterpSpeedIncreasing, InterpSpeedDecreasing);
	}
	if (bClampResult)
	{
		Formula += FString::Printf(TEXT(" Clamp(%.02f, %.02f)"), ClampMin, ClampMax);
	}

	if (Formula.IsEmpty())
	{
		return InLabel;
	}
	return FString::Printf(TEXT("%s: %s"), *InLabel, *Formula);
}

FRigUnit_AlphaInterpQuat_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	ScaleBiasClamp.bMapRange = bMapRange;
	ScaleBiasClamp.bClampResult = bClampResult;
	ScaleBiasClamp.bInterpResult = bInterpResult;

	if (Context.State == EControlRigState::Init)
	{
		ScaleBiasClamp.Reinitialize();
	}
	else
	{
		ScaleBiasClamp.InRange = InRange;
		ScaleBiasClamp.OutRange = OutRange;
		ScaleBiasClamp.ClampMin = ClampMin;
		ScaleBiasClamp.ClampMax = ClampMax;
		ScaleBiasClamp.Scale = Scale;
		ScaleBiasClamp.Bias = Bias;
		ScaleBiasClamp.InterpSpeedIncreasing = InterpSpeedIncreasing;
		ScaleBiasClamp.InterpSpeedDecreasing = InterpSpeedDecreasing;

		const float T = ScaleBiasClamp.ApplyTo(1.f, Context.DeltaTime);
		Result = FQuat::Slerp(FQuat::Identity, Value, T);
	}
}

FString FRigUnit_AlphaInterpQuat::ProcessPinLabelForInjection(const FString& InLabel) const
{
	FString Formula;
	if (bMapRange)
	{
		Formula += FString::Printf(TEXT(" Map(%.02f, %.02f, %.02f, %.02f)"), InRange.Min, InRange.Max, OutRange.Min, OutRange.Max);
	}
	if (bInterpResult)
	{
		Formula += FString::Printf(TEXT(" Interp(%.02f, %.02f)"), InterpSpeedIncreasing, InterpSpeedDecreasing);
	}
	if (bClampResult)
	{
		Formula += FString::Printf(TEXT(" Clamp(%.02f, %.02f)"), ClampMin, ClampMax);
	}

	if (Formula.IsEmpty())
	{
		return InLabel;
	}
	return FString::Printf(TEXT("%s: %s"), *InLabel, *Formula);
}
