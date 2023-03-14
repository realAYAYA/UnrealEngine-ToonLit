// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/Vector.h"
#include "Chaos/PBDSoftsSolverParticles.h"

namespace Chaos::Softs
{

class FPerParticleDampVelocity final
{
public:
	FPerParticleDampVelocity(const FSolverReal InCoefficient = (FSolverReal)0.01)
		: Coefficient(InCoefficient)
	{
	}
	~FPerParticleDampVelocity() {}

	void UpdatePositionBasedState(const FSolverParticles& Particles, const int32 Offset, const int32 Range);

	// Apply damping without first checking for kinematic particles
	inline void ApplyFast(FSolverParticles& Particles, const FSolverReal /*Dt*/, const int32 Index) const
	{
		const FSolverVec3 R = Particles.X(Index) - Xcm;
		const FSolverVec3 Dv = Vcm - Particles.V(Index) + FSolverVec3::CrossProduct(R, Omega);
		Particles.V(Index) += Coefficient * Dv;
	}

private:
	FSolverReal Coefficient;
	FSolverVec3 Xcm, Vcm, Omega;
};

}  // End namespace Chaos::Softs

#if !defined(CHAOS_DAMP_VELOCITY_ISPC_ENABLED_DEFAULT)
#define CHAOS_DAMP_VELOCITY_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if INTEL_ISPC
#if UE_BUILD_SHIPPING
static constexpr bool bChaos_DampVelocity_ISPC_Enabled = CHAOS_DAMP_VELOCITY_ISPC_ENABLED_DEFAULT;
#else
extern CHAOS_API bool bChaos_DampVelocity_ISPC_Enabled;
#endif // UE_BUILD_SHIPPING
#endif