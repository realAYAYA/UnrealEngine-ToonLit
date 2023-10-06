// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/SuspensionModule.h"
#include "SimModule/SimModuleTree.h"
#include "SimModule/WheelModule.h"
#include "VehicleUtility.h"

#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_DISABLE_OPTIMIZATION
#endif

namespace Chaos
{

	FSuspensionSimModule::FSuspensionSimModule(const FSuspensionSettings& Settings)
		: TSimModuleSettings<FSuspensionSettings>(Settings)
		, SpringDisplacement(0.f)
		, LastDisplacement(0.f)
		, WheelSimTreeIndex(INVALID_INDEX)
	{
		AccessSetup().MaxLength = FMath::Abs(Settings.MaxRaise + Settings.MaxDrop);
	}

	float FSuspensionSimModule::GetSpringLength() const
	{
		return  -(Setup().MaxLength - SpringDisplacement);
	}

	void FSuspensionSimModule::SetSpringLength(float InLength, float WheelRadius)
	{
		float DisplacementInput = InLength;
		DisplacementInput = FMath::Max(0.f, DisplacementInput);
		SpringDisplacement = Setup().MaxLength - DisplacementInput;
	}

	void FSuspensionSimModule::GetWorldRaycastLocation(const FTransform& BodyTransform, float WheelRadius, FSpringTrace& OutTrace)
	{
		FVector LocalDirection = Setup().SuspensionAxis;
		FVector WorldLocation = BodyTransform.TransformPosition(GetParentRelativeTransform().GetLocation()/*Setup().LocalOffset*/);
		FVector WorldDirection = BodyTransform.TransformVector(LocalDirection);

		OutTrace.Start = WorldLocation - WorldDirection * (Setup().MaxRaise);
		OutTrace.End = WorldLocation + WorldDirection * (Setup().MaxDrop + WheelRadius);
		float TraceLength = OutTrace.Start.Z - OutTrace.End.Z;
	}

	void FSuspensionSimModule::Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem)
	{
		float ForceIntoSurface = 0.0f;
		if (SpringDisplacement > 0)
		{
			float Damping = (SpringDisplacement < LastDisplacement) ? Setup().CompressionDamping : Setup().ReboundDamping;
			float SpringSpeed = (LastDisplacement - SpringDisplacement) / DeltaTime;

			float StiffnessForce = SpringDisplacement * Setup().SpringRate;
			float DampingForce = SpringSpeed * Damping;
			float SuspensionForce = StiffnessForce - DampingForce;
			LastDisplacement = SpringDisplacement;

			if (SuspensionForce > 0)
			{
				ForceIntoSurface = SuspensionForce;
				AddLocalForce(Setup().SuspensionAxis * -SuspensionForce, true, false, true, FColor::Green);
			}
		}

		// tell wheels how much they are being pressed into the ground
		if (SimModuleTree)
		{
			if (Chaos::ISimulationModuleBase* Module = SimModuleTree->AccessSimModule(WheelSimTreeIndex))
			{
				check(Module->GetSimType() == eSimType::Wheel);
				Chaos::FWheelSimModule* Wheel = static_cast<Chaos::FWheelSimModule*>(Module);

				Wheel->SetForceIntoSurface(ForceIntoSurface);
			}
			

		}
	}



} // namespace Chaos

#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_ENABLE_OPTIMIZATION
#endif