// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/ThrusterModule.h"
#include "SimModule/SimModuleTree.h"
#include "VehicleUtility.h"

#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_DISABLE_OPTIMIZATION
#endif

namespace Chaos
{

	FThrusterSimModule::FThrusterSimModule(const FThrusterSettings& Settings)
		: TSimModuleSettings<FThrusterSettings>(Settings)
	{

	}

	void FThrusterSimModule::Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem)
	{
		// applies continuous force
		FVector Force = Setup().ForceAxis * Setup().MaxThrustForce * Inputs.Throttle;
		AddLocalForceAtPosition(Force, Setup().ForceOffset, true, false, false, FColor::Magenta);

	}

} // namespace Chaos

#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_ENABLE_OPTIMIZATION
#endif
