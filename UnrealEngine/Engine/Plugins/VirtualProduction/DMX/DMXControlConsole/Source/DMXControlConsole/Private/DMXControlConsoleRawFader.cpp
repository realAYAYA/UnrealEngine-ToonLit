// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleRawFader.h"


UDMXControlConsoleRawFader::UDMXControlConsoleRawFader()
{
	FaderName = TEXT("Fader");

#if WITH_EDITOR
	bCanEditDMXAssignment = true;
#endif // WITH_EDITOR
}
