// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

class HARMONIX_API FVariableSpeedTimer
{
public:
	FVariableSpeedTimer();

	void Start();
	void Stop();
	bool IsRunning() const { return Running; }

	void   SetSpeed(double NewSpeed);
	double GetSpeed() const { return Speed; }

	double Ms(); 

	// reset time (does not stop timer or change speed)
	void Reset(double InitialMs = 0.0);

private:
	double GetSeconds();

	// NOTE: Internally we track time in seconds because that is what the 
	// Unreal "platform time" interface works in. BUT, this means that we 
	// need to convert seconds <-> milliseconds for the user of this class.
	double CurrentStartTime;
	bool   Running = false;
	double AccumulatedSeconds;
	double Speed;
};

