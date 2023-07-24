// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "ChaosStats.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainersFwd.h"
#include "Chaos/Framework/Parallel.h"

DECLARE_CYCLE_STAT(TEXT("Chaos PBD Spherical Constraints"), STAT_PBD_Spherical, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("Chaos PBD Spherical Backstop Constraints"), STAT_PBD_SphericalBackstop, STATGROUP_Chaos);

#if !defined(CHAOS_SPHERICAL_ISPC_ENABLED_DEFAULT)
#define CHAOS_SPHERICAL_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if !INTEL_ISPC || UE_BUILD_SHIPPING
static constexpr bool bChaos_Spherical_ISPC_Enabled = INTEL_ISPC && CHAOS_SPHERICAL_ISPC_ENABLED_DEFAULT;
#else
extern CHAOS_API bool bChaos_Spherical_ISPC_Enabled;
#endif

namespace Chaos::Softs
{

class CHAOS_API FPBDSphericalConstraint final
{
public:
	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return IsMaxDistanceEnabled(PropertyCollection, false);
	}

	FPBDSphericalConstraint(
		const uint32 InParticleOffset,
		const uint32 InParticleCount,
		const TArray<FSolverVec3>& InAnimationPositions,  // Use global indexation (will need adding ParticleOffset)
		const TConstArrayView<FRealSingle>& InSphereRadii,  // Use local indexation
		const FCollectionPropertyConstFacade& PropertyCollection,
		FSolverReal MeshScale
	)
		: AnimationPositions(InAnimationPositions)
		, SphereRadii(InSphereRadii)
		, ParticleOffset(InParticleOffset)
		, Scale(MeshScale)
	{
		check(InSphereRadii.Num() == InParticleCount);
	}

	FPBDSphericalConstraint(
		const uint32 InParticleOffset,
		const uint32 InParticleCount,
		const TArray<FSolverVec3>& InAnimationPositions,  // Use global indexation (will need adding ParticleOffset)
		const TConstArrayView<FRealSingle>& InSphereRadii  // Use local indexation
	)
		: AnimationPositions(InAnimationPositions)
		, SphereRadii(InSphereRadii)
		, ParticleOffset(InParticleOffset)
		, Scale((FSolverReal)1.)
	{
		check(InSphereRadii.Num() == InParticleCount);
	}
	~FPBDSphericalConstraint() {}

	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection, FSolverReal MeshScale)
	{
		SetScale((FSolverReal)1., MeshScale);
		// TODO: MaxDistance
	}

	void Apply(FSolverParticles& Particles, const FSolverReal Dt) const
	{
		SCOPE_CYCLE_COUNTER(STAT_PBD_Spherical);

		if (bRealTypeCompatibleWithISPC && bChaos_Spherical_ISPC_Enabled)
		{
			ApplyHelperISPC(Particles, Dt);
		}
		else
		{
			ApplyHelper(Particles, Dt);
		}
	}

	void SetScale(FSolverReal MaxDistanceScale, FSolverReal MeshScale)
	{
		Scale = FMath::Max(MaxDistanceScale, (FSolverReal)0.) * MeshScale;
	}

	FSolverReal GetScale() const { return Scale; }

	UE_DEPRECATED(5.2, "Use SetScale instead.")
	void SetSphereRadiiMultiplier(FSolverReal InSphereRadiiMultiplier, FSolverReal MeshScale)
	{
		SetScale(InSphereRadiiMultiplier, MeshScale);
	}

private:
	void ApplyHelper(FSolverParticles& Particles, const FSolverReal Dt) const
	{
		const int32 ParticleCount = SphereRadii.Num();

		PhysicsParallelFor(ParticleCount, [this, &Particles, Dt](int32 Index)  // TODO: profile need for parallel loop based on particle count
		{
			const int32 ParticleIndex = ParticleOffset + Index;

			if (Particles.InvM(ParticleIndex) == 0)
			{
				return;
			}

			const FSolverReal Radius = SphereRadii[Index] * Scale;
			const FSolverVec3& Center = AnimationPositions[ParticleIndex];

			const FSolverVec3 CenterToParticle = Particles.P(ParticleIndex) - Center;
			const FSolverReal DistanceSquared = CenterToParticle.SizeSquared();

			static const FSolverReal DeadZoneSquareRadius = UE_SMALL_NUMBER; // We will not push the particle away in the dead zone
			if (DistanceSquared > FMath::Square(Radius) + DeadZoneSquareRadius)
			{
				const FSolverReal Distance = FMath::Sqrt(DistanceSquared);
				const FSolverVec3 PositionOnSphere = (Radius / Distance) * CenterToParticle;
				Particles.P(ParticleIndex) = Center + PositionOnSphere;
			}
		});
	}

	void ApplyHelperISPC(FSolverParticles& Particles, const FSolverReal Dt) const;

protected:
	const TArray<FSolverVec3>& AnimationPositions;  // Use global indexation (will need adding ParticleOffset)
	const TConstArrayView<FRealSingle> SphereRadii;  // Use local indexation
	const int32 ParticleOffset;
	UE_DEPRECATED(5.2, "Use Scale instead.")
	FSolverReal SphereRadiiMultiplier = 1.f;

private:
	FSolverReal Scale;

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(MaxDistance, float);
};

class CHAOS_API FPBDSphericalBackstopConstraint final
{
public:
	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return IsBackstopDistanceEnabled(PropertyCollection, false) ||
			IsBackstopDistanceAnimatable(PropertyCollection, false);  // Backstop can be re-enabled if animated
	}

	FPBDSphericalBackstopConstraint(
		const int32 InParticleOffset,
		const int32 InParticleCount,
		const TArray<FSolverVec3>& InAnimationPositions,  // Use global indexation (will need adding ParticleOffset)
		const TArray<FSolverVec3>& InAnimationNormals,  // Use global indexation (will need adding ParticleOffset)
		const TConstArrayView<FRealSingle>& InSphereRadii,  // Use local indexation
		const TConstArrayView<FRealSingle>& InSphereOffsetDistances,  // Use local indexation
		const FCollectionPropertyConstFacade& PropertyCollection,
		FSolverReal MeshScale
	)
		: AnimationPositions(InAnimationPositions)
		, AnimationNormals(InAnimationNormals)
		, SphereRadii(InSphereRadii)
		, SphereOffsetDistances(InSphereOffsetDistances)
		, ParticleOffset(InParticleOffset)
		, Scale(MeshScale)
		, bEnabled(true)
		, bUseLegacyBackstop(false)
	{
		check(InSphereRadii.Num() == InParticleCount);
		check(InSphereOffsetDistances.Num() == InParticleCount);
	}

	FPBDSphericalBackstopConstraint(
		const int32 InParticleOffset,
		const int32 InParticleCount,
		const TArray<FSolverVec3>& InAnimationPositions,  // Use global indexation (will need adding ParticleOffset)
		const TArray<FSolverVec3>& InAnimationNormals,  // Use global indexation (will need adding ParticleOffset)
		const TConstArrayView<FRealSingle>& InSphereRadii,  // Use local indexation
		const TConstArrayView<FRealSingle>& InSphereOffsetDistances,  // Use local indexation
		const bool bInUseLegacyBackstop  // Do not include the sphere radius in the distance calculations when this is true
	)
		: AnimationPositions(InAnimationPositions)
		, AnimationNormals(InAnimationNormals)
		, SphereRadii(InSphereRadii)
		, SphereOffsetDistances(InSphereOffsetDistances)
		, ParticleOffset(InParticleOffset)
		, Scale((FSolverReal)1.)
		, bEnabled(true)
		, bUseLegacyBackstop(bInUseLegacyBackstop)
	{
		check(InSphereRadii.Num() == InParticleCount);
		check(InSphereOffsetDistances.Num() == InParticleCount);
	}
	~FPBDSphericalBackstopConstraint() {}

	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection, FSolverReal MeshScale)
	{
		SetScale((FSolverReal)1., MeshScale);
		// TODO: BackstopDistance and BackstopRadius
	}

	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }
	bool IsEnabled() const { return bEnabled; }

	void Apply(FSolverParticles& Particles, const FSolverReal Dt) const
	{
		SCOPE_CYCLE_COUNTER(STAT_PBD_SphericalBackstop);

		if (bEnabled)
		{
			if (bUseLegacyBackstop)
			{
				// SphereOffsetDistances includes the sphere radius
				// This is harder to author, and does not follow the NvCloth specs.
				// However, this is how it's been done in the Unreal Engine PhysX cloth implementation.
				if (bRealTypeCompatibleWithISPC && bChaos_Spherical_ISPC_Enabled)
				{
					ApplyLegacyHelperISPC(Particles, Dt);
				}
				else
				{
					ApplyLegacyHelper(Particles, Dt);
				}
			}
			else
			{
				// SphereOffsetDistances doesn't include the sphere radius
				if (bRealTypeCompatibleWithISPC && bChaos_Spherical_ISPC_Enabled)
				{
					ApplyHelperISPC(Particles, Dt);
				}
				else
				{
					ApplyHelper(Particles, Dt);
				}
			}
		}
	}

	void SetScale(FSolverReal BackstopScale, FSolverReal MeshScale)
	{
		Scale = FMath::Max(BackstopScale, (FSolverReal)0.) * MeshScale;
	}

	FSolverReal GetScale() const { return Scale; }

	UE_DEPRECATED(5.2, "Use SetScale instead.")
	void SetSphereRadiiMultiplier(FSolverReal InSphereRadiiMultiplier, FSolverReal MeshScale = (FSolverReal)1.)
	{
		SetScale(InSphereRadiiMultiplier, MeshScale);
	}

	UE_DEPRECATED(5.2, "Use GetScale() instead.")
	FSolverReal GetSphereRadiiMultiplier() const { return GetScale(); }

	bool UseLegacyBackstop() const
	{
		return bUseLegacyBackstop;
	}

private:
	void ApplyHelper(FSolverParticles& Particles, const FSolverReal Dt) const
	{
		const int32 ParticleCount = SphereRadii.Num();

		PhysicsParallelFor(ParticleCount, [this, &Particles, Dt](int32 Index)  // TODO: profile need for parallel loop based on particle count
		{
			const int32 ParticleIndex = ParticleOffset + Index;

			if (Particles.InvM(ParticleIndex) == 0)
			{
				return;
			}

			const FSolverVec3& AnimationPosition = AnimationPositions[ParticleIndex];
			const FSolverVec3& AnimationNormal = AnimationNormals[ParticleIndex];

			const FSolverReal SphereOffsetDistance = SphereOffsetDistances[Index] * Scale;
			const FSolverReal Radius = SphereRadii[Index] * Scale;

			const FSolverVec3 Center = AnimationPosition - (Radius + SphereOffsetDistance) * AnimationNormal;  // Non legacy version adds radius to the distance
			const FSolverVec3 CenterToParticle = Particles.P(ParticleIndex) - Center;
			const FSolverReal DistanceSquared = CenterToParticle.SizeSquared();

			static const FSolverReal DeadZoneSquareRadius = UE_SMALL_NUMBER;
			if (DistanceSquared < DeadZoneSquareRadius)
			{
				Particles.P(ParticleIndex) = AnimationPosition - SphereOffsetDistance * AnimationNormal;  // Non legacy version adds radius to the distance
			}
			else if (DistanceSquared < FMath::Square(Radius))
			{
				const FSolverVec3 PositionOnSphere = (Radius / sqrt(DistanceSquared)) * CenterToParticle;
				Particles.P(ParticleIndex) = Center + PositionOnSphere;
			}
			// Else the particle is outside the sphere, and there is nothing to do
		});
	}

	void ApplyLegacyHelper(FSolverParticles& Particles, const FSolverReal Dt) const
	{
		const int32 ParticleCount = SphereRadii.Num();

		PhysicsParallelFor(ParticleCount, [this, &Particles, Dt](int32 Index)  // TODO: profile need for parallel loop based on particle count
		{
			const int32 ParticleIndex = ParticleOffset + Index;

			if (Particles.InvM(ParticleIndex) == 0)
			{
				return;
			}

			const FSolverVec3& AnimationPosition = AnimationPositions[ParticleIndex];
			const FSolverVec3& AnimationNormal = AnimationNormals[ParticleIndex];

			const FSolverReal SphereOffsetDistance = SphereOffsetDistances[Index] * Scale;
			const FSolverReal Radius = SphereRadii[Index] * Scale;

			const FSolverVec3 Center = AnimationPosition - SphereOffsetDistance * AnimationNormal;  // Legacy version already includes the radius within the distance
			const FSolverVec3 CenterToParticle = Particles.P(ParticleIndex) - Center;
			const FSolverReal DistanceSquared = CenterToParticle.SizeSquared();

			static const FSolverReal DeadZoneSquareRadius = UE_SMALL_NUMBER;
			if (DistanceSquared < DeadZoneSquareRadius)
			{
				Particles.P(ParticleIndex) = AnimationPosition - (SphereOffsetDistance - Radius) * AnimationNormal;  // Legacy version already includes the radius to the distance
			}
			else if (DistanceSquared < FMath::Square(Radius))
			{
				const FSolverVec3 PositionOnSphere = (Radius / sqrt(DistanceSquared)) * CenterToParticle;
				Particles.P(ParticleIndex) = Center + PositionOnSphere;
			}
			// Else the particle is outside the sphere, and there is nothing to do
		});
	}

	void ApplyLegacyHelperISPC(FSolverParticles& Particles, const FSolverReal Dt) const;
	void ApplyHelperISPC(FSolverParticles& Particles, const FSolverReal Dt) const;

private:
	const TArray<FSolverVec3>& AnimationPositions;  // Positions of spheres, use global indexation (will need adding ParticleOffset)
	const TArray<FSolverVec3>& AnimationNormals; // Sphere offset directions, use global indexation (will need adding ParticleOffset)
	const TConstArrayView<FRealSingle> SphereRadii; // Start at index 0, use local indexation
	const TConstArrayView<FRealSingle> SphereOffsetDistances;  // Sphere position offsets, use local indexation
	const int32 ParticleOffset;
	FSolverReal Scale;
	bool bEnabled;
	bool bUseLegacyBackstop;

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(BackstopDistance, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(BackstopRadius, float);
};

}  // End namespace Chaos::Softs
