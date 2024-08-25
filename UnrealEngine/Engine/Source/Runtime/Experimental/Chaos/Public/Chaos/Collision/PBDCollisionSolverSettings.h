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
			, DepenetrationVelocity(0)
			, NumPositionFrictionIterations(4)
			, NumVelocityFrictionIterations(0)
			, NumPositionShockPropagationIterations(3)
			, NumVelocityShockPropagationIterations(2)
		{
		}

		// Maximum speed at which two objects can depenetrate (actually, how much relative velocity can be added
		// to a contact per frame when depentrating. Stacks and deep penetrations can lead to larger velocities)
		// A value of zero means unlimited.
		FReal MaxPushOutVelocity;

		// The speed at which initially-overlapping objects depentrate.
		// This value is used when particles do not provide an override for their MaxDepenetrationVelocity.
		FRealSingle DepenetrationVelocity;

		// How many of the position iterations should run static/dynamic friction
		int32 NumPositionFrictionIterations;

		// How many of the velocity iterations should run dynamic friction 
		// This only applies to quadratic shapes if we have static friction enabled (NumPositionFrictionIterations > 0)
		// RBAN is the only system that uses velocity-based friction by default (it also disables static friction)
		// NOTE: velocity-based dynamic friction behaviour is iteration count dependent so generally want (NumVelocityFrictionIterations <= 1)
		// @todo(chaos): fix iteration count dependence of velocity-based friction, or remove it as a feature
		int32 NumVelocityFrictionIterations;

		// How many position iterations should have shock propagation enabled
		int32 NumPositionShockPropagationIterations;

		// How many velocity iterations should have shock propagation enabled
		int32 NumVelocityShockPropagationIterations;
	};

}	// namespace Chaos
