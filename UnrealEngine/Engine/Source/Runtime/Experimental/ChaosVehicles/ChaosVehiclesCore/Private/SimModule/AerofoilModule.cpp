// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/AerofoilModule.h"
#include "SimModule/SimModuleTree.h"
#include "VehicleUtility.h"

#if VEHICLE_DEBUGGING_ENABLED
UE_DISABLE_OPTIMIZATION
#endif

namespace Chaos
{

	void FAerofoilSimModule::Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem)
	{
		float Altitude = Inputs.VehicleWorldTransform.GetLocation().Z;

		switch (Setup().Type)
		{
			case EAerofoil::Elevator:
				ControlSurfaceAngle = Inputs.ControlInputs.Pitch * Setup().MaxControlAngle;
				break;

			case EAerofoil::Rudder:
				ControlSurfaceAngle = Inputs.ControlInputs.Yaw * Setup().MaxControlAngle;
				break;

			case EAerofoil::Wing:
				ControlSurfaceAngle = Inputs.ControlInputs.Roll * Setup().MaxControlAngle;
			break;
		}

		// needs local velocity at aerofoil to calculate lift and drag
		FVector LocalForce = GetForce(LocalLinearVelocity * CmToMScaling(), CmToM(Altitude), DeltaTime);
		FVector LocalOffset = GetCenterOfLiftOffset() * MToCmScaling();

		AddLocalForceAtPosition(LocalForce * MToCmScaling(), LocalOffset, true, false, false, FColor::Yellow);
	}

	float FAerofoilSimModule::CalcAngleOfAttackDegrees(const FVector& UpAxis, const FVector& InAirflowVector)
	{
		float fMag = FVector::DotProduct(UpAxis, InAirflowVector);
		return RadToDeg(FMath::Asin(fMag));
	}

	float FAerofoilSimModule::CalcLiftCoefficient(float InAngleOfAttack, float InControlSurfaceAngle)
	{
		float PeakValue = 2.0f; // typically the Coefficient can reach this peak value
		float TotalAngle = InAngleOfAttack + InControlSurfaceAngle;

		if (FMath::Abs(TotalAngle) > (Setup().StallAngle * 2.0f))
		{
			return 0.0f;
		}

		return FMath::Sin(TotalAngle * (PI * 0.5f) / Setup().StallAngle) * PeakValue;
	}

	float FAerofoilSimModule::CalcDragCoefficient(float InAngleOfAttack, float InControlSurfaceAngle)
	{
		if (InAngleOfAttack > 90.f)
		{
			InAngleOfAttack = 180.f - InAngleOfAttack;
		}

		if (InAngleOfAttack < -90.f)
		{
			InAngleOfAttack = -180.f - InAngleOfAttack;
		}

		float Value = (InAngleOfAttack + InControlSurfaceAngle) / (Setup().StallAngle + FMath::Abs(InControlSurfaceAngle));
		return (0.05f + Value * Value);
	}

	float FAerofoilSimModule::CalcCentreOfLift()
	{
		// moves backwards past stall angle
		if (AngleOfAttack > Setup().StallAngle)
		{
			return (AngleOfAttack - Setup().StallAngle) * 10.0f + 20.0f;
		}

		// moves forwards below stall angle
		return (Setup().StallAngle - AngleOfAttack) * 20.0f / Setup().StallAngle + 20.0f;
	}

	float FAerofoilSimModule::CalcDynamicPressure(float VelocitySqr, float InAltitude)
	{
		float AltitudeMultiplierEffect = 1.0f;

		return AltitudeMultiplierEffect * 0.5f * CurrentAirDensity * VelocitySqr;
	}

	FVector FAerofoilSimModule::GetCenterOfLiftOffset()
	{
		float X = 0.0f;

		if (Setup().Type == EAerofoil::Wing)
		{
			X = (CalcCentreOfLift() - 50.0f) / 100.0f;
		}

		return Setup().Offset + FVector(X, 0.0f, 0.0f);
	}

	FVector FAerofoilSimModule::GetForce(const FVector& v, float Altitude, float DeltaTime)
	{
		FVector Force(0.0f, 0.0f, 0.0f);

		float AirflowMagnitudeSqr = v.SizeSquared();

		// can only generate lift if there is airflow over aerofoil, early out
		if (FMath::Abs(AirflowMagnitudeSqr) < SMALL_NUMBER)
		{
			return Force;
		}

		// airflow direction in opposite direction to vehicle direction of travel
		AirflowNormal = -v;
		AirflowNormal.Normalize();

		// determine angle of attack for control surface
		AngleOfAttack = CalcAngleOfAttackDegrees(Setup().ForceAxis, AirflowNormal);

		// Aerofoil Camber and Control Surface just lumped together
		float TotalControlAngle = ControlSurfaceAngle + Setup().Camber;

		// dynamic pressure dependent on speed, altitude (air pressure)
		float Common = Setup().Area * CalcDynamicPressure(AirflowMagnitudeSqr, Altitude);

		// Lift and Drag coefficients are based on the angle of attack and Control Angle
		float LiftCoef = CalcLiftCoefficient(AngleOfAttack, TotalControlAngle) * Setup().LiftMultiplier;
		float DragCoef = CalcDragCoefficient(AngleOfAttack, TotalControlAngle) * Setup().DragMultiplier;

		// Combine to create a single force vector
		Force = Setup().ForceAxis * (Common * LiftCoef) + AirflowNormal * (Common * DragCoef);

		return Force;
	}

	void FAerofoilSimModule::Animate(Chaos::FClusterUnionPhysicsProxy* Proxy)
	{
		if (FPBDRigidClusteredParticleHandle* ClusterChild = GetClusterParticle(Proxy))
		{
			FQuat ControlRotation = FQuat(Setup().ControlRotationAxis, FMath::DegreesToRadians(ControlSurfaceAngle * Setup().AnimationMagnitudeMultiplier));

			FTransform InitialTransform = GetInitialParticleTransform();
			ClusterChild->ChildToParent().SetRotation(InitialTransform.GetRotation() * ControlRotation);
		}

	}

} // namespace Chaos

#if VEHICLE_DEBUGGING_ENABLED
UE_ENABLE_OPTIMIZATION
#endif
