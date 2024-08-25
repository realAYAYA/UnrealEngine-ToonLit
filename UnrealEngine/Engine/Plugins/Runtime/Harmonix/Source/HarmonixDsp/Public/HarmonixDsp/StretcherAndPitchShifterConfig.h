// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StretcherAndPitchShifterConfig.generated.h"

// base class for all Stretcher and Pitch Shifter config settings
// allows configuration to appear in the "Pitch Shifter Settings"
// along with any other pitch shifter settings
UCLASS(Abstract, EditInlineNew, CollapseCategories, Config = Engine, defaultconfig)
class HARMONIXDSP_API UStretcherAndPitchShifterConfig : public UObject
{
	GENERATED_BODY()
};
