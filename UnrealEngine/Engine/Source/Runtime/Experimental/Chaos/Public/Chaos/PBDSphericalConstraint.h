// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDSoftsSolverParticles.h"
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
	FPBDSphericalConstraint(
		const uint32 InParticleOffset,
		const uint32 InParticleCount,
		const TArray<FSolverVec3>& InAnimationPositions,  // Use global indexation (will need adding ParticleOffset)
		const TConstArrayView<FRealSingle>& InSphereRadii  // Use local indexation
	)
		: AnimationPositions(InAnimationPositions)
		, SphereRadii(InSphereRadii)
		, ParticleOffset(InParticleOffset)
		, SphereRadiiMultiplier((FSolverReal)1.)
	{
		check(InSphereRadii.Num() == InParticleCount);
	}
	~FPBDSphericalConstraint() {}

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

	void SetSphereRadiiMultiplier(const FSolverReal InSphereRadiiMultiplier)
	{
		SphereRadiiMultiplier = FMath::Max((FSolverReal)0., InSphereRadiiMultiplier);
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

			const FSolverReal Radius = SphereRadii[Index] * SphereRadiiMultiplier;
			const FSolverVec3& Center = AnimationPositions[ParticleIndex];

			const FSolverVec3 CenterToParticle = Particles.P(ParticleIndex) - Center;
			const FSolverReal DistanceSquared = CenterToParticle.SizeSquared();

			static const FSolverReal DeadZoneSquareRadius = UE_SMALL_NUMBER; // We will not push the particle away in the dead zone
			if (DistanceSquared > FMath::Square(Radius) + DeadZoneSquareRadius)
			{
				const FSolverReal Distance = sqrt(DistanceSquared);
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
	FSolverReal SphereRadiiMultiplier;
};

class CHAOS_API FPBDSphericalBackstopConstraint final
{
public:
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
		, SphereRadiiMultiplier((FSolverReal)1.)
		, bEnabled(true)
		, bUseLegacyBackstop(bInUseLegacyBackstop)
	{
		check(InSphereRadii.Num() == InParticleCount);
		check(InSphereOffsetDistances.Num() == InParticleCount);
	}
	~FPBDSphericalBackstopConstraint() {}

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

	void SetSphereRadiiMultiplier(const FSolverReal InSphereRadiiMultiplier)
	{
		SphereRadiiMultiplier = FMath::Max((FSolverReal)0., InSphereRadiiMultiplier);
	}

	FSolverReal GetSphereRadiiMultiplier() const
	{
		return SphereRadiiMultiplier;
	}

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

			const FSolverReal SphereOffsetDistance = SphereOffsetDistances[Index];
			const FSolverReal Radius = SphereRadii[Index] * SphereRadiiMultiplier;

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

			const FSolverReal SphereOffsetDistance = SphereOffsetDistances[Index];
			const FSolverReal Radius = SphereRadii[Index] * SphereRadiiMultiplier;

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
	FSolverReal SphereRadiiMultiplier;
	bool bEnabled;
	bool bUseLegacyBackstop;
};

}  // End namespace Chaos::Softs
