// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/VehicleSimSuspensionComponent.h"
#include "SimModule/SimModulesInclude.h"
#include "VehicleUtility.h"


UVehicleSimSuspensionComponent::UVehicleSimSuspensionComponent()
{
	// set defaults
	SuspensionAxis = FVector(0, 0, -1);
	SuspensionMaxRaise = 5.0f;
	SuspensionMaxDrop = 5.0f;
	SpringRate = 100.0f;
	SpringPreload = 50.0f;
	SpringDamping = 0.9f;
	SuspensionForceEffect = 100.0f;
	bAnimationEnabled = true;
}

Chaos::ISimulationModuleBase* UVehicleSimSuspensionComponent::CreateNewCoreModule() const
{
	// use the UE properties to setup the physics state
	Chaos::FSuspensionSettings Settings;

	Settings.SuspensionAxis = SuspensionAxis;
	Settings.MaxRaise = SuspensionMaxRaise;
	Settings.MaxDrop = SuspensionMaxDrop;
	Settings.SpringRate = Chaos::MToCm(SpringRate);
	Settings.SpringPreload = Chaos::MToCm(SpringPreload);
	Settings.SpringDamping = SpringDamping;
	Settings.SuspensionForceEffect = SuspensionForceEffect;

	//Settings.SwaybarEffect = SwaybarEffect;

	Chaos::ISimulationModuleBase* Suspension = new Chaos::FSuspensionSimModule(Settings);
	Suspension->SetAnimationEnabled(bAnimationEnabled);
	return Suspension;

};