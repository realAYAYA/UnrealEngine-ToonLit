// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/EngineModule.h"
#include "SimModule/SimModuleTree.h"
#include "VehicleUtility.h"

#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_DISABLE_OPTIMIZATION
#endif

namespace Chaos
{

	void FEngineSimModule::Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem)
	{
		if (!EngineStarted)
		{
			return;
		}

		// TODO: Engine braking effect
		DriveTorque = GetEngineTorque(Inputs.Throttle, GetRPM());
		
		float BrakeTorque = 0.f;
		TransmitTorque(VehicleModuleSystem, DriveTorque, BrakeTorque);
		IntegrateAngularVelocity(DeltaTime, Setup().EngineInertia);

		// clamp RPM
		AngularVelocity = FMath::Clamp(AngularVelocity, -MaxEngineSpeed, MaxEngineSpeed);

		// we don't let the engine stall
		if (AngularVelocity < EngineIdleSpeed)
		{
			AngularVelocity = EngineIdleSpeed;
		}

	}

	bool FEngineSimModule::GetDebugString(FString& StringOut) const
	{
		FTorqueSimModule::GetDebugString(StringOut);
		StringOut += FString::Format(TEXT("Drive {0}, Brake {1}, Load {2} RPM {3}"), { DriveTorque, BrakingTorque, LoadTorque, GetRPM() });
		return true;
	}


	float FEngineSimModule::GetEngineTorque(float ThrottlePosition, float EngineRPM)
	{
		if (EngineStarted)
		{
			return ThrottlePosition * GetTorqueFromRPM(EngineRPM);
		}

		return 0.f;
	}

	float FEngineSimModule::GetTorqueFromRPM(float RPM, bool LimitToIdle)
	{
		//return Setup().MaxTorque; // TODO: Fix this - engine RPM running rampant

		if (!EngineStarted || (FMath::Abs(RPM - Setup().MaxRPM) < 1.0f) || Setup().MaxRPM == 0)
		{
			return 0.0f;
		}

		if (LimitToIdle)
		{
			RPM = FMath::Clamp(RPM, (float)Setup().IdleRPM, (float)Setup().MaxRPM);
		}

		return Setup().TorqueCurve.GetValue(RPM, Setup().MaxRPM, Setup().MaxTorque);
	}

} // namespace Chaos

#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_ENABLE_OPTIMIZATION
#endif
