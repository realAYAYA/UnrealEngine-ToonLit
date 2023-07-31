// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/ParticleHandleFwd.h"

#include "Math/UnrealMathUtility.h"

namespace Chaos
{


	/**
	 * Calculate an inertia scale so that rotation corrections when applying a constraint are no larger than some fraction of the total correction.
	 * It has the net effect of making the object "more round" and also increasing the inertia relative to the mass when the object is not of "regular" proportions.
	 * 
	 * @param ConstraintExtents the Maximum constraint arm distance along each axis. This should be the extents of the object, including all shapes and joint connectors.
	 * @param MaxDistance the constraint error that we want to be stable. Corrections above this may still contain large rotation components.
	 * @param MaxRotationRatio the contribution to the constraint correction from rotation of the object will be less that this fraction of the total error.
	*/
	FVec3f CHAOS_API CalculateInertiaConditioning(const FRealSingle InvM, const FVec3f& InvI, const FVec3f& ConstraintExtents, const FRealSingle MaxDistance, const FRealSingle MaxRotationRatio, const FRealSingle MaxInvInertiaComponentRatio);

	/**
	 * Generate a CoM-relative extent that encompasses all constraints (collision and joints for now) on the particle. For collisions, we approximate the max extend
	 * using the CoM-space bounds. For joints we use the actual joint attachment positions.
	*/
	FVec3f CHAOS_API CalculateParticleConstraintExtents(const FPBDRigidParticleHandle* Rigid);

	/**
	 * Calculate an inertia scale for a particle based on object size and all joint connector offsets that should stabilize the constraints.
	 * @see CalculateInertiaConditioning()
	*/
	FVec3f CHAOS_API CalculateParticleInertiaConditioning(const FPBDRigidParticleHandle* Rigid, const FRealSingle MaxDistance, const FRealSingle MaxRotationRatio, const FRealSingle MaxInvInertiaComponentRatio);

}