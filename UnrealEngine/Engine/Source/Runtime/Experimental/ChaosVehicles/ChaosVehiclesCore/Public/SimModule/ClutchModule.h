// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/TorqueSimModule.h"


namespace Chaos
{
	struct FAllInputs;
	class FSimModuleTree;

	struct CHAOSVEHICLESCORE_API FClutchSettings
	{
		FClutchSettings()
			: ClutchStrength(1.f)
		{

		}

		float ClutchStrength;
	};

	/// <summary>
	/// 
	/// a vehicle component that transmits torque from one source to another through a clutch system, i.e. connect an engine to a transmission
	///
	/// Input Controls - Clutch pedal, normalized value 0 to 1 expected
	/// Other Inputs - 
	/// Outputs - 
	/// 
	/// </summary>
	class CHAOSVEHICLESCORE_API FClutchSimModule : public FTorqueSimModule, public TSimModuleSettings<FClutchSettings>
	{
	public:

		FClutchSimModule(const FClutchSettings& Settings);

		virtual eSimType GetSimType() const { return eSimType::Clutch; }

		virtual const FString GetDebugName() const { return TEXT("Clutch"); }

		virtual bool GetDebugString(FString& StringOut) const override;

		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem);

	private:

		float ClutchValue;
	};


} // namespace Chaos
