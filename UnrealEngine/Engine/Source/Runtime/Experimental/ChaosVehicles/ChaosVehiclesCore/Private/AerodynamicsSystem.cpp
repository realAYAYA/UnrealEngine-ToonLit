// Copyright Epic Games, Inc. All Rights Reserved.

#include "AerodynamicsSystem.h"

#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_DISABLE_OPTIMIZATION
#endif

namespace Chaos
{

	FSimpleAerodynamicsSim::FSimpleAerodynamicsSim(const FSimpleAerodynamicsConfig* SetupIn) : TVehicleSystem< FSimpleAerodynamicsConfig>(SetupIn)
		, DownforceCoefficient(SetupIn->DownforceCoefficient)
		, DragCoefficient(SetupIn->DragCoefficient)
		, DensityOfMedium(RealWorldConsts::AirDensity())
	{
		// pre-calculate static values
		EffectiveDragConstant = 0.5f * Setup().AreaMetresSquared * DragCoefficient;
		EffectiveLiftConstant = 0.5f * Setup().AreaMetresSquared * DownforceCoefficient;
	}

	FVector FSimpleAerodynamicsSim::GetCombinedForces(float VelocityIn)
	{
		// -ve as forces applied in opposite direction to velocity
		float CommonSum = -DensityOfMedium * VelocityIn * VelocityIn;
		FVector CombinedForces(EffectiveDragConstant * CommonSum, 0.f, EffectiveLiftConstant * CommonSum);
		return CombinedForces;
	}

} // namespace Chaos

#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_ENABLE_OPTIMIZATION
#endif
