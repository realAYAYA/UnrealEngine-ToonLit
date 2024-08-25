// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VehicleUtility.h"
#include "SimModule/SimulationModuleBase.h"

namespace Chaos
{
	struct FAllInputs;
	class FSimModuleTree;

	enum class EAerofoil : uint8
	{
		Fixed = 0,
		Wing,
		Rudder,
		Elevator
	};

	struct CHAOSVEHICLESCORE_API FAerofoilSettings
	{
		FAerofoilSettings()
			: Offset(FVector::ZeroVector)
			, ForceAxis(FVector(0.f, 0.f, 1.f))
			, ControlRotationAxis(FVector(0.f, 1.f, 0.f))
			, Area(5.0f)
			, Camber(3.0f)
			, MaxControlAngle(1.f)
			, StallAngle(16.0f)
			, Type(EAerofoil::Fixed)
			, LiftMultiplier(1.0f)
			, DragMultiplier(1.0f)
			, AnimationMagnitudeMultiplier(1.0f)
		{
		}
		
		FVector Offset;
		FVector ForceAxis;
		FVector ControlRotationAxis;
		float Area;
		float Camber;
		float MaxControlAngle;
		float StallAngle;

		EAerofoil Type;
		float LiftMultiplier;
		float DragMultiplier;
		float AnimationMagnitudeMultiplier;

	};

	class CHAOSVEHICLESCORE_API FAerofoilSimModule : public ISimulationModuleBase, public TSimModuleSettings<FAerofoilSettings>
	{
	public:

		FAerofoilSimModule(const FAerofoilSettings& Settings) : TSimModuleSettings<FAerofoilSettings>(Settings)
			, CurrentAirDensity(RealWorldConsts::AirDensity())
			, AngleOfAttack(0.f)
			, ControlSurfaceAngle(0.f)
			, AirflowNormal(FVector::ZeroVector)
			, AerofoilId(0)
		{

		}

		virtual ~FAerofoilSimModule() {}

		virtual TSharedPtr<FModuleNetData> GenerateNetData(int NodeArrayIndex) const { return nullptr; }

		virtual eSimType GetSimType() const { return eSimType::Aerofoil; }

		virtual const FString GetDebugName() const { return TEXT("Aerofoil"); }

		virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const override { return (InType & Velocity); }

		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

		virtual void Animate(Chaos::FClusterUnionPhysicsProxy* Proxy) override;

		void SetDensityOfMedium(float InDensity)
		{
			CurrentAirDensity = InDensity;
		}

		void SetControlSurface(float CtrlSurfaceInput)
		{
			ControlSurfaceAngle = CtrlSurfaceInput * Setup().MaxControlAngle;
		}

		FVector GetCenterOfLiftOffset();

		// returns the combined force of lift and drag at an aerofoil in local coordinates
		// for direct application to the aircrafts rigid body.
		FVector GetForce(const FVector& v, float Altitude, float DeltaTime);

		/**
		 * Dynamic air pressure = 0.5 * AirDensity * Vsqr
		 */
		float CalcDynamicPressure(float VelocitySqr, float InAltitude);

		/**  Center of lift moves fore/aft based on current AngleOfAttack */
		float CalcCentreOfLift();

		/** Returns drag coefficient for the current angle of attack of the aerofoil surface */
		float CalcDragCoefficient(float InAngleOfAttack, float InControlSurfaceAngle);

		/**
		 * Returns lift coefficient for the current angle of attack of the aerofoil surface
		 * Cheating by making control surface part of entire aerofoil movement
		 */
		float CalcLiftCoefficient(float InAngleOfAttack, float InControlSurfaceAngle);

		/** Angle of attack is the angle between the aerofoil and the airflow vector */
		float CalcAngleOfAttackDegrees(const FVector& UpAxis, const FVector& InAirflowVector);

		float CurrentAirDensity;
		float AngleOfAttack;
		float ControlSurfaceAngle;
		FVector AirflowNormal;
		int AerofoilId;

	};

} // namespace Chaos