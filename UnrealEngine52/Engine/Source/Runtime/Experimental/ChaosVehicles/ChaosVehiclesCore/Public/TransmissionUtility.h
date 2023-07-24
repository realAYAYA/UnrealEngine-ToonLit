// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "SimpleVehicle.h"
#include "TransmissionSystem.h"
#include "WheelSystem.h"

namespace Chaos
{

	class CHAOSVEHICLESCORE_API FTransmissionUtility
	{
	public:

		static bool IsWheelPowered(const EDifferentialType DifferentialType, const FSimpleWheelSim& PWheel)
		{
			return IsWheelPowered(DifferentialType, PWheel.Setup().AxleType, PWheel.EngineEnabled);
		}

		static bool IsWheelPowered(const EDifferentialType DifferentialType, const FSimpleWheelConfig::EAxleType AxleType, const bool EngineEnabled = false);

		static int GetNumWheelsOnAxle(FSimpleWheelConfig::EAxleType AxleType, const TArray<FSimpleWheelSim>& Wheels);

		static int GetNumDrivenWheels(const TArray<FSimpleWheelSim>& Wheels);

		static float GetTorqueRatioForWheel(const FSimpleDifferentialSim& PDifferential, const int WheelIndex, const TArray<FSimpleWheelSim>& Wheels);
	};

}


