// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/WheelModule.h"
#include "SimModule/SimModuleTree.h"
#include "VehicleUtility.h"

#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_DISABLE_OPTIMIZATION
#endif

namespace Chaos
{

	FWheelSimModule::FWheelSimModule(const FWheelSettings& Settings)
		: TSimModuleSettings<FWheelSettings>(Settings)
		, BrakeTorque(0.0f)
		, ForceIntoSurface(0.0f)
		, SurfaceFriction(1.0f)
		, SuspensionSimTreeIndex(INVALID_INDEX)
		, ForceFromFriction(FVector::ZeroVector)
		, MassPerWheel(500.0f*0.25f)
		, SteerAngleDegrees(0.0f)
	{

	}

	void FWheelSimModule::Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem)
	{	
		float Re = Setup().Radius;
		float K = 0.4f;
		float TorqueScaling = 0.00005f;
		float TractionControlAndABSScaling = 0.98f;	// how close to perfection is the system working

		float HandbrakeTorque = Setup().HandbrakeEnabled ? Inputs.Handbrake * Setup().HandbrakeTorque : 0.0f;
		SteerAngleDegrees = Setup().SteeringEnabled ? Inputs.Steering * Setup().MaxSteeringAngle : 0.0f;
		BrakeTorque = Inputs.Brake * Setup().MaxBrakeTorque + HandbrakeTorque;
		LoadTorque = 0.0f;

		// TODO: think about doing this properly, stops vehicles rolling around on their own too much
		// kindof an auto handbrake
		if (Inputs.Brake < SMALL_NUMBER && Inputs.Throttle < SMALL_NUMBER && ModuleLocalVelocity.X < 10.0f)
		{
			BrakeTorque = Setup().HandbrakeTorque;
		}

		bool bTouchingGround = ForceIntoSurface > SMALL_NUMBER;

		if (bTouchingGround)
		{
			FRotator SteeringRotator(0.f, SteerAngleDegrees, 0.f);
			FVector LocalWheelVelocity = SteeringRotator.UnrotateVector(ModuleLocalVelocity);

			float GroundAngularVelocity = LocalWheelVelocity.X / Re;
			float Delta = GroundAngularVelocity - AngularVelocity;
			float TorqueFromGroundInteraction = Delta * Setup().WheelInertia / DeltaTime; // torque from wheels moving over terrain

			// X is longitudinal direction, Y is lateral
			float SlipAngle = FVehicleUtility::CalculateSlipAngle(LocalWheelVelocity.Y, LocalWheelVelocity.X);

			float AppliedLinearDriveForce = DriveTorque / Re;
			float AppliedLinearBrakeForce = BrakeTorque / Re;

			// Longitudinal multiplier now affecting both brake and steering equally
			float AvailableGrip = ForceIntoSurface * SurfaceFriction * Setup().FrictionMultiplier;

			float FinalLongitudinalForce = 0.f;
			float FinalLateralForce = 0.f;

			// currently just letting the brake override the throttle
			bool Braking = (BrakeTorque > FMath::Abs(DriveTorque));
			bool WheelLocked = false;

			// are we actually touching the ground
			if (ForceIntoSurface > SMALL_NUMBER)
			{
				// ABS limiting brake force to match force from the grip available
				if (Setup().ABSEnabled && Braking && FMath::Abs(AppliedLinearBrakeForce) > AvailableGrip)
				{
					if ((Braking && Setup().ABSEnabled) || (!Braking && Setup().TractionControlEnabled))
					{
						float Sign = (AppliedLinearBrakeForce > 0.0f) ? 1.0f : -1.0f;
						AppliedLinearBrakeForce = AvailableGrip * TractionControlAndABSScaling * Sign;
					}
				}

				// Traction control limiting drive force to match force from grip available
				if (Setup().TractionControlEnabled && !Braking && FMath::Abs(AppliedLinearDriveForce) > AvailableGrip)
				{
					float Sign = (AppliedLinearDriveForce > 0.0f) ? 1.0f : -1.0f;
					AppliedLinearDriveForce = AvailableGrip * TractionControlAndABSScaling * Sign;
				}

				if (Braking)
				{
					// whether the velocity is +ve or -ve when we brake we are slowing the vehicle down
					// so force is opposing current direction of travel.
					float ForceRequiredToBringToStop = MassPerWheel * K * (LocalWheelVelocity.X) / DeltaTime;
					FinalLongitudinalForce = AppliedLinearBrakeForce;

					// check we are not applying more force than required so we end up overshooting 
					// and accelerating in the opposite direction
					FinalLongitudinalForce = FMath::Clamp(FinalLongitudinalForce, -FMath::Abs(ForceRequiredToBringToStop), FMath::Abs(ForceRequiredToBringToStop));

					// ensure the brake opposes current direction of travel
					if (LocalWheelVelocity.X > 0.0f)
					{
						FinalLongitudinalForce = -FinalLongitudinalForce;
					}

				}
				else
				{
					FinalLongitudinalForce = AppliedLinearDriveForce;
				}

				float ForceRequiredToBringToStop = -(MassPerWheel * K * LocalWheelVelocity.Y) / DeltaTime;

				// use slip angle to generate a sideways force
				if (Setup().LateralSlipGraph.IsEmpty())
				{
					float AngleLimit = FMath::DegreesToRadians(Setup().SlipAngleLimit);
					float ClippedAngle = FMath::Clamp(SlipAngle, -AngleLimit, AngleLimit);
					FinalLateralForce = FMath::Abs(SlipAngle) * Setup().CorneringStiffness;
				}
				else
				{
					FinalLateralForce = Setup().LateralSlipGraph.EvaluateY(FMath::RadiansToDegrees(SlipAngle)) * Setup().LateralSlipGraphMultiplier;
				}

				if (FinalLateralForce > FMath::Abs(ForceRequiredToBringToStop))
				{
					FinalLateralForce = FMath::Abs(ForceRequiredToBringToStop);
				}
				if (LocalWheelVelocity.Y > 0.0f)
				{
					FinalLateralForce = -FinalLateralForce;
				}

			}

			ForceFromFriction = FVector::ZeroVector;
			ForceFromFriction.X = FinalLongitudinalForce;
			ForceFromFriction.Y = FinalLateralForce;

			AddLocalForce(SteeringRotator.RotateVector(ForceFromFriction));
			TransmitTorque(VehicleModuleSystem, DriveTorque, BrakeTorque);

			DriveTorque -= AvailableGrip;
			if (DriveTorque < 0.0f)
			{
				DriveTorque = 0.0f;
			}
			else
			{ 
				DriveTorque *= TorqueScaling;
			}
			BrakingTorque -= AvailableGrip;
			if (BrakingTorque < 0.0f)
			{
				BrakingTorque = 0.0f;
			}

			LoadTorque = TorqueFromGroundInteraction;
		}

		IntegrateAngularVelocity(DeltaTime, Setup().WheelInertia, Setup().MaxRotationVel);
	}


	bool FWheelSimModule::GetDebugString(FString& StringOut) const
	{
		FTorqueSimModule::GetDebugString(StringOut);
		StringOut += FString::Format(TEXT("Drive {0}, Brake {1}, Load {2} RPM {3}  AngVel {4} LongitudinalForce {5}")
			, { DriveTorque, BrakingTorque, LoadTorque, GetRPM(), AngularVelocity, ForceFromFriction.X });
		return true;
	}


} // namespace Chaos


#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_ENABLE_OPTIMIZATION
#endif
