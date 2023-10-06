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

void FPBDSphericalConstraint::SetProperties(
	const FCollectionPropertyConstFacade& PropertyCollection,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	FSolverReal MeshScale)
{
	if (IsMaxDistanceMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedFloatMaxDistance(FVector2f::Max(GetWeightedFloatMaxDistance(PropertyCollection), FVector2f::ZeroVector));
		MaxDistanceBase = WeightedFloatMaxDistance[0];
		MaxDistanceRange = WeightedFloatMaxDistance[1] - MaxDistanceBase;

		if (IsMaxDistanceStringDirty(PropertyCollection))
		{
			SphereRadii = WeightMaps.FindRef(GetMaxDistanceString(PropertyCollection));
		}
	}

	SetScale(MeshScale);
}

void FPBDSphericalConstraint::ApplyHelperISPC(FSolverParticles& Particles, const FSolverReal /*Dt*/) const
{
	check(bRealTypeCompatibleWithISPC);

#if INTEL_ISPC
	if (SphereRadii.Num() == ParticleCount)
	{
		ispc::ApplySphericalConstraintsWithMap(
			(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
			(const ispc::FVector3f*)AnimationPositions.GetData(),
			SphereRadii.GetData(),
			MaxDistanceBase * GetScale(),
			MaxDistanceRange * GetScale(),
			ParticleOffset,
			ParticleCount);
	}
	else
	{
		ispc::ApplySphericalConstraintsWithoutMap(
			(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
			(const ispc::FVector3f*)AnimationPositions.GetData(),
			MaxDistanceBase * GetScale(),
			ParticleOffset,
			ParticleCount);
	}
#endif
}

void FPBDSphericalBackstopConstraint::SetProperties(
	const FCollectionPropertyConstFacade& PropertyCollection,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	FSolverReal MeshScale)
{
	if (IsBackstopRadiusMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedFloatBackstopRadius(FVector2f::Max(GetWeightedFloatBackstopRadius(PropertyCollection), FVector2f::ZeroVector));
		BackstopRadiusBase = (FSolverReal)WeightedFloatBackstopRadius[0];
		BackstopRadiusRange = (FSolverReal)WeightedFloatBackstopRadius[1] - BackstopRadiusBase;

		if (IsBackstopRadiusStringDirty(PropertyCollection))
		{
			SphereRadii = WeightMaps.FindRef(GetBackstopRadiusString(PropertyCollection));
		}

		// Update enable status using the radius property
		SetEnabled(IsBackstopRadiusEnabled(PropertyCollection));
	}

	if (IsBackstopDistanceMutable(PropertyCollection))
	{
		const FSolverVec2 WeightedFloatBackstopDistance(GetWeightedFloatBackstopDistance(PropertyCollection));  // Unlike radius, distance can be negative
		BackstopDistanceBase = WeightedFloatBackstopDistance[0];
		BackstopDistanceRange = WeightedFloatBackstopDistance[1] - BackstopDistanceBase;

		if (IsBackstopDistanceStringDirty(PropertyCollection))
		{
			SphereOffsetDistances = WeightMaps.FindRef(GetBackstopDistanceString(PropertyCollection));
		}
	}

	SetScale(MeshScale);
}

void FPBDSphericalBackstopConstraint::ApplyLegacyHelperISPC(FSolverParticles& Particles, const FSolverReal /*Dt*/) const
{
	check(bRealTypeCompatibleWithISPC);

#if INTEL_ISPC
	if (SphereRadii.Num() == ParticleCount)
	{
		if (SphereOffsetDistances.Num() == ParticleCount)
		{
			ispc::ApplyLegacySphericalBackstopConstraintsWithMaps(
				(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
				(const ispc::FVector3f*)AnimationPositions.GetData(),
				(const ispc::FVector3f*)AnimationNormals.GetData(),
				SphereOffsetDistances.GetData(),
				SphereRadii.GetData(),
				BackstopDistanceBase * GetScale(),
				BackstopDistanceRange * GetScale(),
				BackstopRadiusBase * GetScale(),
				BackstopRadiusRange * GetScale(),
				ParticleOffset,
				ParticleCount);
		}
		else
		{
			ispc::ApplyLegacySphericalBackstopConstraintsWithRadiusMap(
				(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
				(const ispc::FVector3f*)AnimationPositions.GetData(),
				(const ispc::FVector3f*)AnimationNormals.GetData(),
				SphereRadii.GetData(),
				BackstopDistanceBase * GetScale(),
				BackstopRadiusBase * GetScale(),
				BackstopRadiusRange * GetScale(),
				ParticleOffset,
				ParticleCount);
		}
	}
	else if (SphereOffsetDistances.Num() == ParticleCount)
	{
		ispc::ApplyLegacySphericalBackstopConstraintsWithDistanceMap(
			(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
			(const ispc::FVector3f*)AnimationPositions.GetData(),
			(const ispc::FVector3f*)AnimationNormals.GetData(),
			SphereOffsetDistances.GetData(),
			BackstopDistanceBase * GetScale(),
			BackstopDistanceRange * GetScale(),
			BackstopRadiusBase * GetScale(),
			ParticleOffset,
			ParticleCount);
	}
	else
	{
		ispc::ApplyLegacySphericalBackstopConstraintsWithoutMaps(
			(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
			(const ispc::FVector3f*)AnimationPositions.GetData(),
			(const ispc::FVector3f*)AnimationNormals.GetData(),
			BackstopDistanceBase * GetScale(),
			BackstopRadiusBase * GetScale(),
			ParticleOffset,
			ParticleCount);
	}
#endif
}

void FPBDSphericalBackstopConstraint::ApplyHelperISPC(FSolverParticles& Particles, const FSolverReal /*Dt*/) const
{
	check(bRealTypeCompatibleWithISPC);

#if INTEL_ISPC
	if (SphereRadii.Num() == ParticleCount)
	{
		if (SphereOffsetDistances.Num() == ParticleCount)
		{
			ispc::ApplySphericalBackstopConstraintsWithMaps(
				(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
				(const ispc::FVector3f*)AnimationPositions.GetData(),
				(const ispc::FVector3f*)AnimationNormals.GetData(),
				SphereOffsetDistances.GetData(),
				SphereRadii.GetData(),
				BackstopDistanceBase * GetScale(),
				BackstopDistanceRange * GetScale(),
				BackstopRadiusBase * GetScale(),
				BackstopRadiusRange * GetScale(),
				ParticleOffset,
				ParticleCount);
		}
		else
		{
			ispc::ApplySphericalBackstopConstraintsWithRadiusMap(
				(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
				(const ispc::FVector3f*)AnimationPositions.GetData(),
				(const ispc::FVector3f*)AnimationNormals.GetData(),
				SphereRadii.GetData(),
				BackstopDistanceBase * GetScale(),
				BackstopRadiusBase * GetScale(),
				BackstopRadiusRange * GetScale(),
				ParticleOffset,
				ParticleCount);
		}
	}
	else if (SphereOffsetDistances.Num() == ParticleCount)
	{
		ispc::ApplySphericalBackstopConstraintsWithDistanceMap(
			(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
			(const ispc::FVector3f*)AnimationPositions.GetData(),
			(const ispc::FVector3f*)AnimationNormals.GetData(),
			SphereOffsetDistances.GetData(),
			BackstopDistanceBase * GetScale(),
			BackstopDistanceRange * GetScale(),
			BackstopRadiusBase * GetScale(),
			ParticleOffset,
			ParticleCount);
	}
	else
	{
		ispc::ApplySphericalBackstopConstraintsWithoutMaps(
			(ispc::FVector4f*)Particles.GetPAndInvM().GetData(),
			(const ispc::FVector3f*)AnimationPositions.GetData(),
			(const ispc::FVector3f*)AnimationNormals.GetData(),
			BackstopDistanceBase * GetScale(),
			BackstopRadiusBase * GetScale(),
			ParticleOffset,
			ParticleCount);
	}
#endif
}

}  // End namespace Chaos::Softs
