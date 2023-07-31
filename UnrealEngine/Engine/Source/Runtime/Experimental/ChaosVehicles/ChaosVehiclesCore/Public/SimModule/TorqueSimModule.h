// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/SimulationModuleBase.h"
#include "VehicleUtility.h"

namespace Chaos
{


class CHAOSVEHICLESCORE_API FTorqueSimModule : public ISimulationModuleBase
{
public:
	FTorqueSimModule()
		: DriveTorque(0.f)
		, LoadTorque(0.f)
		, BrakingTorque(0.f)
		, AngularVelocity(0.f)
		, AngularPosition(0.0f)
		, CombinedInertia(0.0f)
	{
	}

	/**
	 * Is Module of a specific type - used for casting
	 */
	virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const { return (InType & TorqueBased); }

	void SetDriveTorque(float TorqueIn) { DriveTorque = TorqueIn; }
	float GetDriveTorque() const { return DriveTorque; }

	void SetLoadTorque(float TorqueIn) { LoadTorque = TorqueIn; }
	float GetLoadTorque() const { return LoadTorque; }

	void SetBrakingTorque(float TorqueIn) { BrakingTorque = TorqueIn; }
	float GetBrakingTorque() const { return BrakingTorque; }

	void SetAngularVelocity(float AngularVelocityIn) { AngularVelocity = AngularVelocityIn; }
	float GetAngularVelocity() const { return AngularVelocity; }

	void AddAngularVelocity(float AngularVelocityIn) { AngularVelocity += AngularVelocityIn; }

	void SetAngularPosition(float AngularPositionIn) { AngularPosition = AngularPositionIn; }
	float GetAngularPosition() const { return AngularPosition; }

	void SetRPM(float InRPM) { AngularVelocity = RPMToOmega(InRPM); }
	float GetRPM() const { return OmegaToRPM(AngularVelocity); }

	/**
	 * Transmit torque between this module and its Parent and Children. DriveTorque passed down to children, LoadTorque passed from child to parent
	 */
	void TransmitTorque(const FSimModuleTree& BlockSystem, float PushedTorque, float BrakeTorque = 0.f, float GearingRatio = 1.0f, float ClutchSlip = 1.0f);

	/**
	 * Integrate angular velocity using specified DeltaTime & Inertia value, Note the inertia should be the combined inertia of all the connected pieces otherwise things will rotate at different rates
	 */
	void IntegrateAngularVelocity(float DeltaTime, float Inertia, float MaxRotationVel=MAX_FLT);

	/**
	 * Cast an ISimulationModuleBase to a FTorqueSimModule if compatible class
	 */
	static FTorqueSimModule* CastToTorqueInterface(ISimulationModuleBase* SimModule)
	{
		if (SimModule && SimModule->IsBehaviourType(eSimModuleTypeFlags::TorqueBased))
		{
			return static_cast<FTorqueSimModule*>(SimModule);
		}

		return nullptr;
	}

protected:

	float DriveTorque;
	float LoadTorque;
	float BrakingTorque;
	float AngularVelocity;
	float AngularPosition;
	float CombinedInertia; // Note: ?? Do we want this
};


} // namespace Chaos