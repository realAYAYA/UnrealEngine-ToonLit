// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/TorqueSimModule.h"


namespace Chaos
{
	struct FAllInputs;
	class FSimModuleTree;

	/// <summary>
	/// Axle settings
	/// </summary>
	struct CHAOSVEHICLESCORE_API FAxleSettings
	{
		FAxleSettings()
			: AxleInertia(1.f) // TODO: defaults
		{

		}

		float AxleInertia;
	};

	/// <summary>
	/// A vehicle component that transmits torque from one source to another, i.e. from an engine or differential to wheels
	///
	/// </summary>
	class CHAOSVEHICLESCORE_API FAxleSimModule : public FTorqueSimModule, public TSimModuleSettings<FAxleSettings>
	{
	public:
		FAxleSimModule(const FAxleSettings& Settings);

		virtual eSimType GetSimType() const { return eSimType::Axle; }

		virtual const FString GetDebugName() const { return TEXT("Axle"); }

		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

	private:
	};


} // namespace Chaos
