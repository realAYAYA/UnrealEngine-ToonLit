// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/VehicleSimTransmissionComponent.h"
#include "SimModule/SimModulesInclude.h"
#include "VehicleUtility.h"


UVehicleSimTransmissionComponent::UVehicleSimTransmissionComponent()
{
	// set defaults
	ForwardRatios.Add(2.85f);
	ForwardRatios.Add(2.02f);
	ForwardRatios.Add(1.35f);
	ForwardRatios.Add(1.0f);

	ReverseRatios.Add(2.86f);

	FinalDriveRatio = 3.08f;

	ChangeUpRPM = 4500;
	ChangeDownRPM = 1600;
	GearChangeTime = 0.5f;
	TransmissionEfficiency = 0.9f;
	TransmissionType = EModuleTransType::Automatic;
	AutoReverse = true;
}

Chaos::ISimulationModuleBase* UVehicleSimTransmissionComponent::CreateNewCoreModule() const
{
	// use the UE properties to setup the physics state
	Chaos::FTransmissionSettings Settings;

	Settings.ForwardRatios = ForwardRatios;
	Settings.ReverseRatios = ReverseRatios;
	Settings.FinalDriveRatio = FinalDriveRatio;
	Settings.ChangeUpRPM = ChangeUpRPM;
	Settings.ChangeDownRPM = ChangeDownRPM;
	Settings.GearChangeTime = GearChangeTime;
	Settings.TransmissionEfficiency = TransmissionEfficiency;
	Settings.TransmissionType = static_cast<Chaos::FTransmissionSettings::ETransType>(TransmissionType);

	Settings.AutoReverse = AutoReverse;

	Chaos::ISimulationModuleBase* Transmission = new Chaos::FTransmissionSimModule(Settings);
	return Transmission;
}
