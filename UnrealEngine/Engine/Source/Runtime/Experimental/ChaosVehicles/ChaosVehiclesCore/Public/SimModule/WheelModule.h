// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/TorqueSimModule.h"

namespace Chaos
{
	struct FAllInputs;
	class FSimModuleTree;

	struct CHAOSVEHICLESCORE_API FWheelSettings
	{
		FWheelSettings()
			: Radius(30.0f)
			, Width(20.0f)
			, WheelInertia(100000.0f) // TODO: ?? defaults and proper value ??

			, FrictionMultiplier(3.0f)
			, LateralSlipGraphMultiplier(1.0f)
			, CorneringStiffness(1000.0f)
			, SlipAngleLimit(8.0f)

			, ABSEnabled(true)
			, TractionControlEnabled(true)
			, SteeringEnabled(false)
			, HandbrakeEnabled(false)
			, MaxSteeringAngle(45)
			, MaxBrakeTorque(4000)
			, HandbrakeTorque(3000)
			, MaxRotationVel(100.0f)

		{

		}

		float Radius;
		float Width;
		float WheelInertia;

		float FrictionMultiplier;
		float LateralSlipGraphMultiplier;
		float CorneringStiffness;
		FGraph LateralSlipGraph;
		float SlipAngleLimit;

		bool ABSEnabled;			// Advanced braking system operational
		bool TractionControlEnabled;// Straight Line Traction Control
		bool SteeringEnabled;
		bool HandbrakeEnabled;

		float MaxSteeringAngle;
		float MaxBrakeTorque;
		float HandbrakeTorque;

		float MaxRotationVel;


	};

	class CHAOSVEHICLESCORE_API FWheelSimModule : public FTorqueSimModule, public TSimModuleSettings<FWheelSettings>
	{
	public:

		FWheelSimModule(const FWheelSettings& Settings);

		virtual eSimType GetSimType() const { return eSimType::Wheel; }

		virtual const FString GetDebugName() const { return TEXT("Wheel"); }

		virtual bool GetDebugString(FString& StringOut) const override;

		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

		virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const override { return (InType & TorqueBased)||(InType & Velocity); }

		void SetSuspensionSimTreeIndex(int IndexIn) { SuspensionSimTreeIndex = IndexIn; }
		int GetSuspensionSimTreeIndex() const { return SuspensionSimTreeIndex; }

		float GetSteerAngleDegrees() const { return SteerAngleDegrees; }

		void SetForceIntoSurface(float ForceIntoSurfaceIn) { ForceIntoSurface = ForceIntoSurfaceIn; }

		void SetSurfaceFriction(float FrictionIn) { SurfaceFriction = FrictionIn; }

	private:

		float BrakeTorque;				// [N.m]
		float ForceIntoSurface;			// [N]
		float SurfaceFriction;
		int SuspensionSimTreeIndex;

		FVector ForceFromFriction;
		float MassPerWheel;
		float SteerAngleDegrees;

	};


} // namespace Chaos
