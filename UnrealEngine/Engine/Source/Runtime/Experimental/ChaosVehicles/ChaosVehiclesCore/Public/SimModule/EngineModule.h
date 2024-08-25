// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/TorqueSimModule.h"
#include "SimModule/SimulationModuleBase.h"

namespace Chaos
{
	struct FAllInputs;
	class FSimModuleTree;

	struct CHAOSVEHICLESCORE_API FEngineSimModuleDatas : public FTorqueSimModuleDatas
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		FEngineSimModuleDatas(int NodeArrayIndex, const FString& InDebugString) : FTorqueSimModuleDatas(NodeArrayIndex, InDebugString) {}
#else
		FEngineSimModuleDatas(int NodeArrayIndex) : FTorqueSimModuleDatas(NodeArrayIndex) {}
#endif

		virtual eSimType GetType() override { return eSimType::Engine; }

		virtual void FillSimState(ISimulationModuleBase* SimModule) override
		{
			check(SimModule->GetSimType() == eSimType::Engine);
			FTorqueSimModuleDatas::FillSimState(SimModule);
		}

		virtual void FillNetState(const ISimulationModuleBase* SimModule) override
		{
			check(SimModule->GetSimType() == eSimType::Engine);
			FTorqueSimModuleDatas::FillNetState(SimModule);
		}

	};

	struct CHAOSVEHICLESCORE_API FEngineOutputData : public FSimOutputData
	{
		virtual FSimOutputData* MakeNewData() override { return FEngineOutputData::MakeNew(); }
		static FSimOutputData* MakeNew() { return new FEngineOutputData(); }

		virtual eSimType GetType() override { return eSimType::Engine; }
		virtual void FillOutputState(const ISimulationModuleBase* SimModule) override;
		virtual void Lerp(const FSimOutputData& InCurrent, const FSimOutputData& InNext, float Alpha) override;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		virtual FString ToString() override;
#endif

		float RPM;
		float Torque;
	};

	struct CHAOSVEHICLESCORE_API FEngineSettings
	{
		FEngineSettings()
			: MaxTorque(300.f)
			, MaxRPM(6000)
			, IdleRPM(1200)
			, EngineBrakeEffect(50.0f)
			, EngineInertia(100.0f)
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
		uint16 IdleRPM; 			// [RPM] The RPM at which the throttle sits when the car is not moving			
		float EngineBrakeEffect;	// [N.m] How much the engine slows the vehicle when the throttle is released

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

		virtual TSharedPtr<FModuleNetData> GenerateNetData(int SimArrayIndex) const
		{
			return MakeShared<FEngineSimModuleDatas>(
				SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				, GetDebugName()
#endif			
			);
		}
		virtual FSimOutputData* GenerateOutputData() const override
		{
			return FEngineOutputData::MakeNew();
		}

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
