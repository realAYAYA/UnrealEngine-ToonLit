// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/TorqueSimModule.h"

namespace Chaos
{
	struct FAllInputs;
	class FSimModuleTree;

	/// <summary>
	/// Motor settings
	/// </summary>
	struct CHAOSVEHICLESCORE_API FMotorSettings
	{
		FMotorSettings()
			: MaxRPM(1.0f)
			, MaxTorque(100.0f)
			, EngineInertia(1.0f)
		{

		}
		float MaxRPM;
		float MaxTorque;
		float EngineInertia;
	};

	/// <summary>
	/// A vehicle component that provides torque output based on a torque control input
	/// The output torque is based on a square function style curve, zero at 0.0 and MaxRPM, and 1.0 at mid RPM
	/// </summary>
	class CHAOSVEHICLESCORE_API FMotorSimModule : public FTorqueSimModule, public TSimModuleSettings<FMotorSettings>
	{
	public:

		FMotorSimModule(const FMotorSettings& Settings);

		virtual eSimType GetSimType() const { return eSimType::Motor; }

		virtual const FString GetDebugName() const { return TEXT("Motor"); }

		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

	private:

	};

} // namespace Chaos
