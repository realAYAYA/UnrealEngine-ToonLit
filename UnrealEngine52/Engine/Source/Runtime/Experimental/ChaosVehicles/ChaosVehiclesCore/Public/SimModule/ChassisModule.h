// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/SimulationModuleBase.h"


namespace Chaos
{
	struct FAllInputs;
	class FSimModuleTree;

	/// <summary>
	/// Chassis settings
	/// </summary>
	struct CHAOSVEHICLESCORE_API FChassisSettings
	{
		FChassisSettings()
		{

		}

	};

	/// <summary>
	/// A vehicle component that transmits torque from one source to another, i.e. from an engine or differential to wheels
	///
	/// </summary>
	class CHAOSVEHICLESCORE_API FChassisSimModule : public ISimulationModuleBase, public TSimModuleSettings<FChassisSettings>
	{
	public:
		FChassisSimModule(const FChassisSettings& Settings);

		virtual eSimType GetSimType() const { return eSimType::Chassis; }

		virtual const FString GetDebugName() const { return TEXT("Chassis"); }

		virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const override { return (InType & NonFunctional); }

		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

	private:
	};


} // namespace Chaos
