// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/TransmissionModule.h"
#include "SimModule/SimModuleTree.h"
#include "VehicleUtility.h"

#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_DISABLE_OPTIMIZATION
#endif

namespace Chaos
{

	void FTransmissionSimModule::Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem)
	{
		if (Setup().TransmissionType == FTransmissionSettings::ETransType::Automatic)
		{
			// not currently changing gear, also don't want to change up because the wheels are spinning up due to having no load
			if (!IsCurrentlyChangingGear() && AllowedToChangeGear)
			{
				// in Automatic & currently in neutral and not currently changing gear then automatically change up to 1st
				if (CurrentGear == 0)
				{
					ChangeUp();
				}

				float EngineRPM = GetRPM();

				if (EngineRPM >= Setup().ChangeUpRPM)
				{
					if (CurrentGear > 0)
					{
						ChangeUp();
					}
					else
					{
						ChangeDown();
					}
				}
				else if (EngineRPM <= Setup().ChangeDownRPM && FMath::Abs(CurrentGear) > 1) // don't change down to neutral
				{
					if (CurrentGear > 0)
					{
						ChangeDown();
					}
					else
					{
						ChangeUp();
					}
				}
			}
		}
		else
		{
			if (Inputs.ChangeUp)
			{
				ChangeUp();
			}
			else if (Inputs.ChangeDown)
			{
				ChangeDown();
			}
			else if (Inputs.SetGearNumber)
			{
				TargetGear = Inputs.SetGearNumber;
			}
		}

		if (CurrentGear != TargetGear)
		{
			CurrentGearChangeTime -= DeltaTime;
			if (CurrentGearChangeTime <= 0.f)
			{
				CurrentGearChangeTime = 0.f;
				CurrentGear = TargetGear;
			}
		}

		float GearRatio = GetGearRatio(CurrentGear);

		// if there IS a selected gear then connect the parent and child building blocks transmitting their torque
		if (FMath::Abs(GearRatio) > SMALL_NUMBER)
		{
			float BrakeTorque = 0.0f;
			TransmitTorque(VehicleModuleSystem, DriveTorque, BrakeTorque, GearRatio);
		}

	}

	float FTransmissionSimModule::GetGearRatio(int32 InGear) const
	{
		CorrectGearInputRange(InGear);

		if (InGear > 0) // a forwards gear
		{
			return Setup().ForwardRatios[InGear - 1] * Setup().FinalDriveRatio;
		}
		else if (InGear < 0) // a reverse gear
		{
			return -Setup().ReverseRatios[FMath::Abs(InGear) - 1] * Setup().FinalDriveRatio;
		}
		else
		{
			return 0.f; // neutral has no ratio
		}
	}

	void FTransmissionSimModule::SetGear(int32 InGear, bool Immediate /*= false*/)
	{
		CorrectGearInputRange(InGear);

		TargetGear = InGear;

		if (TargetGear != CurrentGear)
		{
			if (Immediate || Setup().GearChangeTime == 0.f)
			{
				CurrentGear = TargetGear;
			}
			else
			{
				CurrentGear = 0;	// go through neutral for GearChangeTime time period
				CurrentGearChangeTime = Setup().GearChangeTime;
			}
		}
	}

	bool FTransmissionSimModule::GetDebugString(FString& StringOut) const
	{
		FTorqueSimModule::GetDebugString(StringOut);
		StringOut += FString::Format(TEXT("CurrentGear {0}, Ratio {1}")
			, { CurrentGear, GetGearRatio(CurrentGear) });
		return true;
	}


} // namespace Chaos

#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_ENABLE_OPTIMIZATION
#endif
