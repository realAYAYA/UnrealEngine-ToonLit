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


	struct CHAOSVEHICLESCORE_API FClutchSimModuleDatas : public FTorqueSimModuleDatas
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		FClutchSimModuleDatas(int NodeArrayIndex, const FString& InDebugString) : FTorqueSimModuleDatas(NodeArrayIndex, InDebugString) {}
#else
		FClutchSimModuleDatas(int NodeArrayIndex) : FTorqueSimModuleDatas(NodeArrayIndex) {}
#endif

		virtual eSimType GetType() override { return eSimType::Clutch; }

		virtual void FillSimState(ISimulationModuleBase* SimModule) override
		{
			check(SimModule->GetSimType() == eSimType::Clutch);
			FTorqueSimModuleDatas::FillSimState(SimModule);
		}

		virtual void FillNetState(const ISimulationModuleBase* SimModule) override
		{
			check(SimModule->GetSimType() == eSimType::Clutch);
			FTorqueSimModuleDatas::FillNetState(SimModule);
		}

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

		virtual TSharedPtr<FModuleNetData> GenerateNetData(int SimArrayIndex) const
		{
			return MakeShared<FClutchSimModuleDatas>(
				SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				, GetDebugName()
#endif			
			);
		}

		virtual eSimType GetSimType() const { return eSimType::Clutch; }

		virtual const FString GetDebugName() const { return TEXT("Clutch"); }

		virtual bool GetDebugString(FString& StringOut) const override;

		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem);

	private:

		float ClutchValue;
	};


} // namespace Chaos
