// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"

namespace Chaos
{
	/**
		* @brief Settings to control the low-level collision solver behaviour
	*/
	class FPBDCollisionSolverSettings
	{
	public:
		UE_NONCOPYABLE(FPBDCollisionSolverSettings);

		FPBDCollisionSolverSettings()
			: MaxPushOutVelocity(0)
			, NumPositionFrictionIterations(4)
			, NumVelocityFrictionIterations(1)
			, NumPositionShockPropagationIterations(3)
			, NumVelocityShockPropagationIterations(1)
		{
		}

		// Maximum speed at which two objects can depenetrate (actually, how much relative velocity can be added
		// to a contact per frame when depentrating. Stacks and deep penetrations can lead to larger velocities)
		FReal MaxPushOutVelocity;

		// How many of the position iterations should run static/dynamic friction
		int32 NumPositionFrictionIterations;

		// How many of the velocity iterations should run dynamic friction
		// @todo(chaos): if NumVelocityFrictionIterations > 1, then dynamic friction in the velocity phase will be iteration 
		// count dependent (velocity-solve friction is currentlyused by quadratic shapes and RBAN)
		int32 NumVelocityFrictionIterations;

		// How many position iterations should have shock propagation enabled
		int32 NumPositionShockPropagationIterations;

		// How many velocity iterations should have shock propagation enabled
		int32 NumVelocityShockPropagationIterations;
	};

}	// namespace Chaos
