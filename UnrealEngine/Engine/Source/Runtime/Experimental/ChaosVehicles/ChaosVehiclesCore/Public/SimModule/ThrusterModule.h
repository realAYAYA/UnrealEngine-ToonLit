// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/SimulationModuleBase.h"


namespace Chaos
{
	struct FAllInputs;
	class FSimModuleTree;

	/// <summary>
	/// Thruster settings
	/// </summary>
	struct CHAOSVEHICLESCORE_API FThrusterSettings
	{
		FThrusterSettings()
			: MaxThrustForce(0)
			, ForceAxis(FVector(1.0f, 0.0f, 0.0f))
			, ForceOffset(FVector::ZeroVector)
		{

		}

		float MaxThrustForce;
		FVector ForceAxis;
		FVector ForceOffset;
	};

	/// <summary>
	/// A vehicle component that transmits torque from one source to another, i.e. from an engine or differential to wheels
	///
	/// </summary>
	class CHAOSVEHICLESCORE_API FThrusterSimModule : public ISimulationModuleBase, public TSimModuleSettings<FThrusterSettings>
	{
	public:
		FThrusterSimModule(const FThrusterSettings& Settings);

		virtual eSimType GetSimType() const { return eSimType::Thruster; }

		virtual const FString GetDebugName() const { return TEXT("Thruster"); }

		virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const override { return (InType & NonFunctional); }

		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

	private:
	};


} // namespace Chaos
