// Copyright Epic Games, Inc. All Rights Reserved.
#include "Widgets/Input/SSpinBox.h"

float SpinBoxComputeExponentSliderFraction(float FractionFilled, float StartFractionFilled, float SliderExponent)
{
	if (FractionFilled <= StartFractionFilled)
	{
		float DeltaFraction = (StartFractionFilled - FractionFilled)/StartFractionFilled;
		float LeftFractionFilled = 1.0f - FMath::Pow(1.0f - DeltaFraction, SliderExponent);
		FractionFilled = StartFractionFilled - (StartFractionFilled*LeftFractionFilled);
	}
	else
	{
		float DeltaFraction = (FractionFilled - StartFractionFilled)/(1.0f - StartFractionFilled);
		float RightFractionFilled = 1.0f - FMath::Pow(1.0f - DeltaFraction, SliderExponent);
		FractionFilled = StartFractionFilled + (1.0f - StartFractionFilled) * RightFractionFilled;
	}
	return FractionFilled;
}


