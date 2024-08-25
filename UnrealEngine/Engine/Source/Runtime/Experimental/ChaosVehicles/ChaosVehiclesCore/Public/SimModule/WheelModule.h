// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/TorqueSimModule.h"

namespace Chaos
{
	struct FAllInputs;
	class FSimModuleTree;
	class FClusterUnionPhysicsProxy;

	enum CHAOSVEHICLESCORE_API EWheelAxis
	{
		X,	// X forward
		Y	// Y forward
	};

	struct CHAOSVEHICLESCORE_API FWheelSimModuleDatas : public FTorqueSimModuleDatas
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		FWheelSimModuleDatas(int NodeArrayIndex, const FString& InDebugString) : FTorqueSimModuleDatas(NodeArrayIndex, InDebugString) {}
#else
		FWheelSimModuleDatas(int NodeArrayIndex) : FTorqueSimModuleDatas(NodeArrayIndex) {}
#endif

		virtual eSimType GetType() override { return eSimType::Wheel; }

		virtual void FillSimState(ISimulationModuleBase* SimModule) override
		{
			check(SimModule->GetSimType() == eSimType::Wheel);
			FTorqueSimModuleDatas::FillSimState(SimModule);
		}

		virtual void FillNetState(const ISimulationModuleBase* SimModule) override
		{
			check(SimModule->GetSimType() == eSimType::Wheel);
			FTorqueSimModuleDatas::FillNetState(SimModule);
		}

	};

	struct CHAOSVEHICLESCORE_API FWheelOutputData : public FSimOutputData
	{
		virtual FSimOutputData* MakeNewData() override { return FWheelOutputData::MakeNew(); }
		static FSimOutputData* MakeNew() { return new FWheelOutputData(); }

		virtual eSimType GetType() override { return eSimType::Wheel; }
		virtual void FillOutputState(const ISimulationModuleBase* SimModule) override;
		virtual void Lerp(const FSimOutputData& InCurrent, const FSimOutputData& InNext, float Alpha) override;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		virtual FString ToString() override;
#endif

		bool bTouchingGround;
		float ForceIntoSurface;
		float SlipAngle;
		float RPM;

		//HitLocation
		//PhysMaterial
	};

	struct CHAOSVEHICLESCORE_API FWheelSettings
	{
		FWheelSettings()
			: Radius(30.0f)
			, Width(20.0f)
			, WheelInertia(100.0f)

			, FrictionMultiplier(3.0f)
			, LateralSlipGraphMultiplier(1.0f)
			, CorneringStiffness(1000.0f)
			, SlipAngleLimit(8.0f)
			, SlipModifier(0.9f)

			, ABSEnabled(true)
			, TractionControlEnabled(true)
			, SteeringEnabled(false)
			, HandbrakeEnabled(false)
			, AutoHandbrakeEnabled(false)
			, AutoHandbrakeVelocityThreshold(10.0f)
			, MaxSteeringAngle(45)
			, MaxBrakeTorque(4000)
			, HandbrakeTorque(3000)
			, MaxRotationVel(100.0f)
			, Axis(EWheelAxis::X)
			, ReverseDirection(false)
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
		float SlipModifier;

		bool ABSEnabled;			// Advanced braking system operational
		bool TractionControlEnabled;// Straight Line Traction Control
		bool SteeringEnabled;
		bool HandbrakeEnabled;
		bool AutoHandbrakeEnabled;
		float AutoHandbrakeVelocityThreshold; 

		float MaxSteeringAngle;
		float MaxBrakeTorque;
		float HandbrakeTorque;

		float MaxRotationVel;
		EWheelAxis Axis;
		bool ReverseDirection;
	};

	class CHAOSVEHICLESCORE_API FWheelSimModule : public FTorqueSimModule, public TSimModuleSettings<FWheelSettings>
	{
		friend FWheelOutputData;

	public:

		FWheelSimModule(const FWheelSettings& Settings);

		virtual TSharedPtr<FModuleNetData> GenerateNetData(int SimArrayIndex) const
		{
			return MakeShared<FWheelSimModuleDatas>(
				SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				, GetDebugName()
#endif			
			);
		}

		virtual FSimOutputData* GenerateOutputData() const override
		{
			return FWheelOutputData::MakeNew();
		}

		virtual eSimType GetSimType() const { return eSimType::Wheel; }

		virtual const FString GetDebugName() const { return TEXT("Wheel"); }

		virtual bool GetDebugString(FString& StringOut) const override;

		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

		virtual void Animate(Chaos::FClusterUnionPhysicsProxy* Proxy) override;

		virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const override { return (InType & TorqueBased) || (InType & Velocity); }

		void SetSuspensionSimTreeIndex(int IndexIn) { SuspensionSimTreeIndex = IndexIn; }
		int GetSuspensionSimTreeIndex() const { return SuspensionSimTreeIndex; }

		float GetSteerAngleDegrees() const { return SteerAngleDegrees; }

		void SetForceIntoSurface(float ForceIntoSurfaceIn) { ForceIntoSurface = ForceIntoSurfaceIn; }
		float GetForceIntoSurface() const { return ForceIntoSurface; }
		FVector GetForceFromFriction() const { return ForceFromFriction; }
		void SetSurfaceFriction(float FrictionIn) { SurfaceFriction = FrictionIn; }
		
		/** set wheel rotational speed to match the specified linear forwards speed */
		void SetLinearSpeed(float LinearMetersPerSecondIn)
		{
			SetAngularVelocity(LinearMetersPerSecondIn / Setup().Radius);
		}

		/** get linear forwards speed from angluar velocity and wheel radius */
		float GetLinearSpeed()
		{
			return AngularVelocity * Setup().Radius;
		}

		/** Get the radius of the wheel [cm] */
		float GetEffectiveRadius() const
		{
			return Setup().Radius;
		}

	private:

		float BrakeTorque;				// [N.m]
		float ForceIntoSurface;			// [N]
		float SurfaceFriction;
		int SuspensionSimTreeIndex;

		FVector ForceFromFriction;
		float MassPerWheel;
		float SteerAngleDegrees;

		// for output
		bool bTouchingGround;
		float SlipAngle;
	};


} // namespace Chaos
