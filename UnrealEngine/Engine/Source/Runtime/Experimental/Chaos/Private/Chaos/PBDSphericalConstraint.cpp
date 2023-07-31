// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDSphericalConstraint.h"
#include "Chaos/PBDParticles.h"
#include "Chaos/PBDRigidParticles.h"
#include "Chaos/Framework/Parallel.h"
#include "ChaosStats.h"
#include "ChaosLog.h"
#if INTEL_ISPC
#include "PBDSphericalConstraint.ispc.generated.h"
#endif

#if INTEL_ISPC && !UE_BUILD_SHIPPING
bool bChaos_Spherical_ISPC_Enabled = true;
FAutoConsoleVariableRef CVarChaosSphericalISPCEnabled(TEXT("p.Chaos.Spherical.ISPC"), bChaos_Spherical_ISPC_Enabled, TEXT("Whether to use ISPC optimizations in spherical constraints"));

static_assert(sizeof(ispc::FVector4f) == sizeof(Chaos::Softs::FPAndInvM), "sizeof(ispc::FVector4f) != sizeof(Chaos::Softs::FPAndInvM)");
#endif

namespace Chaos::Softs {

void FPBDSphericalConstraint::ApplyHelperISPC(FSolverParticles& Particles, const FSolverReal /*Dt*/) const
{
	check(bRealTypeCompatibleWithISPC);

	const int32 ParticleCount = SphereRadii.Num();

#if INTEL_ISPC
	ispc::ApplySphericalConstraints(
		(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
		(const ispc::FVector3f*)AnimationPositions.GetData(),
		SphereRadii.GetData(),
		SphereRadiiMultiplier,
		ParticleOffset,
		ParticleCount);
#endif
}
void FPBDSphericalBackstopConstraint::ApplyLegacyHelperISPC(FSolverParticles& Particles, const FSolverReal /*Dt*/) const
{
	check(bRealTypeCompatibleWithISPC);

	const int32 ParticleCount = SphereRadii.Num();

#if INTEL_ISPC
	ispc::ApplyLegacySphericalBackstopConstraints(
		(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
		(const ispc::FVector3f*)AnimationPositions.GetData(),
		(const ispc::FVector3f*)AnimationNormals.GetData(),
		SphereOffsetDistances.GetData(),
		SphereRadii.GetData(),
		SphereRadiiMultiplier,
		ParticleOffset,
		ParticleCount);
#endif
}

void FPBDSphericalBackstopConstraint::ApplyHelperISPC(FSolverParticles& Particles, const FSolverReal /*Dt*/) const
{
	check(bRealTypeCompatibleWithISPC);
	const int32 ParticleCount = SphereRadii.Num();

#if INTEL_ISPC
	ispc::ApplySphericalBackstopConstraints(
		(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
		(const ispc::FVector3f*)AnimationPositions.GetData(),
		(const ispc::FVector3f*)AnimationNormals.GetData(),
		SphereOffsetDistances.GetData(),
		SphereRadii.GetData(),
		SphereRadiiMultiplier,
		ParticleOffset,
		ParticleCount);
#endif
}

}  // End namespace Chaos::Softs
