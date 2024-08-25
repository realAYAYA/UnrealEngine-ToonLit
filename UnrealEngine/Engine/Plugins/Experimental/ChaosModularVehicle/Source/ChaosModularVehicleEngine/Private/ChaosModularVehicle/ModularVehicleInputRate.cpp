// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleInputRate.h"

inline float FModularVehicleInputRate::InterpInputValue(float DeltaTime, float CurrentValue, float NewValue) const
{
	const float DeltaValue = NewValue - CurrentValue;

	// We are "rising" when DeltaValue has the same sign as CurrentValue (i.e. delta causes an absolute magnitude gain)
	// OR we were at 0 before, and our delta is no longer 0.
	const bool bRising = ((DeltaValue > 0.0f) == (CurrentValue > 0.0f)) ||
		((DeltaValue != 0.f) && (CurrentValue == 0.f));

	const float MaxDeltaValue = DeltaTime * (bRising ? RiseRate : FallRate);
	const float ClampedDeltaValue = FMath::Clamp(DeltaValue, -MaxDeltaValue, MaxDeltaValue);
	return CurrentValue + ClampedDeltaValue;
}

inline float FModularVehicleInputRate::CalcControlFunction(float InputValue)
{
	// user defined curve

	// else use option from drop down list
	switch (InputCurveFunction)
	{
	case EModularVehicleInputFunctionType::CustomCurve:
	{
		if (UserCurve.GetRichCurveConst() && !UserCurve.GetRichCurveConst()->IsEmpty())
		{
			float Output = FMath::Clamp(UserCurve.GetRichCurveConst()->Eval(FMath::Abs(InputValue)), 0.0f, 1.0f);
			return (InputValue < 0.f) ? -Output : Output;
		}
		else
		{
			return InputValue;
		}
	}
	break;
	case EModularVehicleInputFunctionType::SquaredFunction:
	{
		return (InputValue < 0.f) ? -InputValue * InputValue : InputValue * InputValue;
	}
	break;

	case EModularVehicleInputFunctionType::LinearFunction:
	default:
	{
		return InputValue;
	}
	break;

	}
}
