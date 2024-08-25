// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/VehicleSimChassisComponent.h"
#include "SimModule/SimModulesInclude.h"
#include "VehicleUtility.h"

UVehicleSimChassisComponent::UVehicleSimChassisComponent()
{
	AreaMetresSquared = 0.0f;
	DragCoefficient = 0.5f;
	DensityOfMedium = Chaos::RealWorldConsts::AirDensity();
	XAxisMultiplier = 1.0f;
	YAxisMultiplier = 1.0f;
	AngularDamping = 0.0f;
}

Chaos::ISimulationModuleBase* UVehicleSimChassisComponent::CreateNewCoreModule() const
{
	Chaos::FChassisSettings Settings;
	Settings.AreaMetresSquared = AreaMetresSquared;
	Settings.DragCoefficient = DragCoefficient;
	Settings.DensityOfMedium = DensityOfMedium;
	Settings.XAxisMultiplier = XAxisMultiplier;
	Settings.YAxisMultiplier = YAxisMultiplier;
	Settings.AngularDamping = AngularDamping;

	Chaos::ISimulationModuleBase* Chassis = new Chaos::FChassisSimModule(Settings);

	return Chassis;
}
