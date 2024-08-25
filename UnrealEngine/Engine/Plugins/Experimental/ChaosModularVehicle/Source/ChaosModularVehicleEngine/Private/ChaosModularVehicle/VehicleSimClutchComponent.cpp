// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/VehicleSimClutchComponent.h"
#include "SimModule/SimModulesInclude.h"
#include "VehicleUtility.h"

UVehicleSimClutchComponent::UVehicleSimClutchComponent()
{
	// set defaults
	ClutchStrength = 1.0f;
}

Chaos::ISimulationModuleBase* UVehicleSimClutchComponent::CreateNewCoreModule() const
{
	// use the UE properties to setup the physics state
	Chaos::FClutchSettings Settings;

	Settings.ClutchStrength = ClutchStrength;
	Chaos::ISimulationModuleBase* Clutch = new Chaos::FClutchSimModule(Settings);
	return Clutch;
}

