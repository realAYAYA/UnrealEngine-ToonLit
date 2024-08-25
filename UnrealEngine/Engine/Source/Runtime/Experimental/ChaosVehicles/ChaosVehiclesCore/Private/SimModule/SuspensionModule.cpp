// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/SuspensionModule.h"
#include "SimModule/SimModuleTree.h"
#include "SimModule/WheelModule.h"
#include "Chaos/PBDSuspensionConstraints.h"
#include "PhysicsProxy/SuspensionConstraintProxy.h"
#include "PBDRigidsSolver.h"
#include "VehicleUtility.h"

#if VEHICLE_DEBUGGING_ENABLED
UE_DISABLE_OPTIMIZATION
#endif

namespace Chaos
{

	FSuspensionSimModule::FSuspensionSimModule(const FSuspensionSettings& Settings)
		: TSimModuleSettings<FSuspensionSettings>(Settings)
		, SpringDisplacement(0.f)
		, LastDisplacement(0.f)
		, SpringSpeed(0.f)
		, WheelSimTreeIndex(INVALID_IDX)
		, Constraint(nullptr)
		, ConstraintIndex(INVALID_IDX)
		, TargetPos(FVector::ZeroVector)
		, ImpactNormal(FVector::ZeroVector)
		, WheelInContact(false)
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
		FVector Local = GetParentRelativeTransform().GetLocation(); // change to just a vector and GetLocalLocation
		FVector WorldLocation = BodyTransform.TransformPosition(Local);
		FVector WorldDirection = BodyTransform.TransformVector(LocalDirection);

		OutTrace.Start = WorldLocation - WorldDirection * (Setup().MaxRaise);
		OutTrace.End = WorldLocation + WorldDirection * (Setup().MaxDrop + WheelRadius);
		float TraceLength = OutTrace.Start.Z - OutTrace.End.Z;
	}

	void FSuspensionSimModule::Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem)
	{
		{
			float ForceIntoSurface = 0.0f;
			if (SpringDisplacement > 0)
			{
				float Damping = Setup().SpringDamping;
				SpringSpeed = (LastDisplacement - SpringDisplacement) / DeltaTime;

				float StiffnessForce = SpringDisplacement * Setup().SpringRate;
				float DampingForce = SpringSpeed * Damping;
				float SuspensionForce = StiffnessForce - DampingForce;
				LastDisplacement = SpringDisplacement;

				if (SuspensionForce > 0)
				{
					ForceIntoSurface = SuspensionForce * Setup().SuspensionForceEffect;

					if (Constraint == nullptr)
					{
						AddLocalForce(Setup().SuspensionAxis * -SuspensionForce, true, false, true, FColor::Green);
					}
				}
			}

			// tell wheels how much they are being pressed into the ground
			if (SimModuleTree && WheelSimTreeIndex != INVALID_IDX)
			{
				if (Chaos::ISimulationModuleBase* Module = SimModuleTree->AccessSimModule(WheelSimTreeIndex))
				{
					check(Module->GetSimType() == eSimType::Wheel);
					Chaos::FWheelSimModule* Wheel = static_cast<Chaos::FWheelSimModule*>(Module);

					Wheel->SetForceIntoSurface(ForceIntoSurface);
				}
		
			}
		}

		if (Constraint)
		{
			UpdateConstraint();
		}

	}

	void FSuspensionSimModule::Animate(Chaos::FClusterUnionPhysicsProxy* Proxy)
	{
		if (FPBDRigidClusteredParticleHandle* ClusterChild = GetClusterParticle(Proxy))
		{
			float CurrentSpringLength = GetSpringLength();

			FVector RestPos = GetInitialParticleTransform().GetTranslation();

			FVector Movement = Setup().SuspensionAxis * (Setup().MaxRaise + CurrentSpringLength);
			Movement = GetComponentTransform().TransformVector(Movement);
			FVector NewPos = RestPos - Movement;
			ClusterChild->ChildToParent().SetTranslation(NewPos); // local frame for module
		}
	}

	void FSuspensionSimModule::SetSuspensionConstraint(FSuspensionConstraint* InConstraint)
	{
		Constraint = InConstraint;
	}

	void FSuspensionSimModule::UpdateConstraint()
	{
		if (Constraint && Constraint->IsValid())
		{
			if (FSuspensionConstraintPhysicsProxy* Proxy = Constraint->GetProxy<FSuspensionConstraintPhysicsProxy>())
			{
				Chaos::FPhysicsSolver* Solver = Proxy->GetSolver<Chaos::FPhysicsSolver>();
				Solver->SetSuspensionTarget(Constraint, TargetPos, ImpactNormal, WheelInContact);
			}
		}
	}

	void FSuspensionSimModuleDatas::FillSimState(ISimulationModuleBase* SimModule)
	{
		check(SimModule->GetSimType() == eSimType::Suspension);
		if (FSuspensionSimModule* Sim = static_cast<FSuspensionSimModule*>(SimModule))
		{
			Sim->SpringDisplacement = SpringDisplacement;
			Sim->LastDisplacement = LastDisplacement;
		}
	}

	void FSuspensionSimModuleDatas::FillNetState(const ISimulationModuleBase* SimModule)
	{
		check(SimModule->GetSimType() == eSimType::Suspension);
		if (const FSuspensionSimModule* Sim = static_cast<const FSuspensionSimModule*>(SimModule))
		{
			SpringDisplacement = Sim->SpringDisplacement;
			LastDisplacement = Sim->LastDisplacement;
		}
	}

	void FSuspensionSimModuleDatas::Lerp(const float LerpFactor, const FModuleNetData& Min, const FModuleNetData& Max)
	{
		const FSuspensionSimModuleDatas& MinData = static_cast<const FSuspensionSimModuleDatas&>(Min);
		const FSuspensionSimModuleDatas& MaxData = static_cast<const FSuspensionSimModuleDatas&>(Max);

		SpringDisplacement = FMath::Lerp(MinData.SpringDisplacement, MaxData.SpringDisplacement, LerpFactor);
		LastDisplacement = FMath::Lerp(MinData.LastDisplacement, MaxData.LastDisplacement, LerpFactor);
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FString FSuspensionSimModuleDatas::ToString() const
	{
		return FString::Printf(TEXT("Module:%s SpringDisplacement:%f LastDisplacement:%f"),
			*DebugString, SpringDisplacement, LastDisplacement);
	}
#endif

	void FSuspensionOutputData::FillOutputState(const ISimulationModuleBase* SimModule)
	{
		check(SimModule->GetSimType() == eSimType::Suspension);

		FSimOutputData::FillOutputState(SimModule);

		if (const FSuspensionSimModule* Sim = static_cast<const FSuspensionSimModule*>(SimModule))
		{
			SpringDisplacement = Sim->SpringDisplacement;
			SpringSpeed = Sim->SpringSpeed;
		}
	}

	void FSuspensionOutputData::Lerp(const FSimOutputData& InCurrent, const FSimOutputData& InNext, float Alpha)
	{
		const FSuspensionOutputData& Current = static_cast<const FSuspensionOutputData&>(InCurrent);
		const FSuspensionOutputData& Next = static_cast<const FSuspensionOutputData&>(InNext);

		SpringDisplacement = FMath::Lerp(Current.SpringDisplacement, Next.SpringDisplacement, Alpha);
		SpringSpeed = FMath::Lerp(Current.SpringSpeed, Next.SpringSpeed, Alpha);
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FString FSuspensionOutputData::ToString()
	{
		return FString::Printf(TEXT("%s, SpringDisplacement=%3.3f, SpringSpeed=%3.3f"), *DebugString, SpringDisplacement, SpringSpeed);
	}
#endif

} // namespace Chaos

#if VEHICLE_DEBUGGING_ENABLED
UE_ENABLE_OPTIMIZATION
#endif
