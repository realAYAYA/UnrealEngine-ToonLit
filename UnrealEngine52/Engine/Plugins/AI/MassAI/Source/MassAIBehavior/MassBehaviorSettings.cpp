// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassBehaviorSettings.h"

//----------------------------------------------------------------------//
// UMassBehaviorSettings
//----------------------------------------------------------------------//

UMassBehaviorSettings::UMassBehaviorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default values.
	MaxActivationsPerLOD[EMassLOD::High] = 100;
	MaxActivationsPerLOD[EMassLOD::Medium] = 100;
	MaxActivationsPerLOD[EMassLOD::Low] = 100;
	MaxActivationsPerLOD[EMassLOD::Off] = 100;
}
