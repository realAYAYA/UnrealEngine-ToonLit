// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFloatOscillator_Sine.h"


float UDMXControlConsoleFloatOscillator_Sine::GetNormalizedValue_Implementation(float DeltaTime)
{
	CurrentTime += DeltaTime;

	return Amplitude / 2.f * FMath::Sin(CurrentTime * FrequencyHz * PI * 2.f) + Offset;
}
