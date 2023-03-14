// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/TorqueSimModule.h"

PRAGMA_DISABLE_OPTIMIZATION

namespace Chaos
{
	struct FAllInputs;
	class FSimModuleTree;

	struct CHAOSVEHICLESCORE_API FEngineSettings
	{
		FEngineSettings()
			: MaxTorque(300.f)
			, MaxRPM(6000)
			, IdleRPM(1200)
			, EngineBrakeEffect(0.2f)
			, EngineInertia(100000.0f)
		{
			TorqueCurve.AddNormalized(0.5f);
			TorqueCurve.AddNormalized(0.5f);
			TorqueCurve.AddNormalized(0.5f);
			TorqueCurve.AddNormalized(0.5f);
			TorqueCurve.AddNormalized(0.6f);
			TorqueCurve.AddNormalized(0.7f);
			TorqueCurve.AddNormalized(0.8f);
			TorqueCurve.AddNormalized(0.9f);
			TorqueCurve.AddNormalized(1.0f);
			TorqueCurve.AddNormalized(0.9f);
			TorqueCurve.AddNormalized(0.7f);
			TorqueCurve.AddNormalized(0.5f);
		}

		FNormalisedGraph TorqueCurve;
		float MaxTorque;			// [N.m] The peak torque Y value in the normalized torque graph
		uint16 MaxRPM;				// [RPM] The absolute maximum RPM the engine can theoretically reach (last X value in the normalized torque graph)
		uint16 IdleRPM; 		// [RPM] The RPM at which the throttle sits when the car is not moving			
		float EngineBrakeEffect;	// [0..1] How much the engine slows the vehicle when the throttle is released

		float EngineInertia;
	};

	class CHAOSVEHICLESCORE_API FEngineSimModule : public FTorqueSimModule, public TSimModuleSettings<FEngineSettings>
	{
	public:

		FEngineSimModule(const FEngineSettings& Settings) : TSimModuleSettings<FEngineSettings>(Settings)
			, EngineIdleSpeed(RPMToOmega(Setup().IdleRPM))
			, MaxEngineSpeed(RPMToOmega(Setup().MaxRPM))
			, EngineStarted(true)
		{
		}

		virtual ~FEngineSimModule() {}

		virtual eSimType GetSimType() const { return eSimType::Engine; }

		virtual const FString GetDebugName() const { return TEXT("Engine"); }

		virtual bool GetDebugString(FString& StringOut) const override;

		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

		inline float GetEngineIdleSpeed() const { return EngineIdleSpeed; }
		inline float GetEngineTorque(float ThrottlePosition, float EngineRPM);
		inline float GetTorqueFromRPM(float RPM, bool LimitToIdle = true);

	protected:

		float EngineIdleSpeed;
		float MaxEngineSpeed;
		bool EngineStarted;		// is the engine turned off or has it been started

	};

} // namespace Chaos

PRAGMA_ENABLE_OPTIMIZATION
