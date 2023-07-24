// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/ChassisModule.h"
#include "SimModule/SimModuleTree.h"


namespace Chaos
{

	FChassisSimModule::FChassisSimModule(const FChassisSettings& Settings)
		: TSimModuleSettings<FChassisSettings>(Settings)
	{

	}

	void FChassisSimModule::Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem)
	{
		AddLocalForce(FVector(20000000,0,0));

	}


} // namespace Chaos
