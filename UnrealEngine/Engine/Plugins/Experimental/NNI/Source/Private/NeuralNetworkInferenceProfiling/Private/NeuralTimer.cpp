// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralTimer.h"
#include "HAL/PlatformTime.h"

/* FNeuralNetworkInferenceQATimer public functions
 *****************************************************************************/

void FNeuralTimer::Tic()
{
	TimeStart = {FPlatformTime::Seconds()};
}

double FNeuralTimer::Toc() const
{
	return (FPlatformTime::Seconds() - TimeStart) * 1e3;
}
