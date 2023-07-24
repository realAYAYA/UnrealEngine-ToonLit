// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class CONTROLRIG_API FCRSimUtils
{
public:

	static void ComputeWeightsFromMass(float MassA, float MassB, float& OutWeightA, float& OutWeightB);
};