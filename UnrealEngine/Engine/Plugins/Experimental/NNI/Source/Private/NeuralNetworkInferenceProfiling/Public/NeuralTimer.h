// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class NEURALNETWORKINFERENCEPROFILING_API FNeuralTimer
{
public:
	void Tic();

	/**
	 * Time in milliseconds, but with nanosecond accuracy (e.g., 1.234567 msec).
	 */
	double Toc() const;

private:
	double TimeStart;
};
