// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFloatOscillator_Square.h"


float UDMXControlConsoleFloatOscillator_Square::GetNormalizedValue_Implementation(float DeltaTime)
{
	CurrentTime += DeltaTime;


	const float Output = CurrentTime * FrequencyHz;

	if (Output - (int)Output < 0.5) 
	{
		return Amplitude / 2.f + Offset;
	}
	else 
	{
		return -Amplitude / 2.f + Offset;
	}
}
