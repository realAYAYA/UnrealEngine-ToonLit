// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

// HEADER_UNIT_SKIP - Internal

#include "Chaos/Framework/Parallel.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDStiffness.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "ChaosStats.h"

DECLARE_CYCLE_STAT(TEXT("Chaos PBD Anim Drive Constraint"), STAT_PBD_AnimDriveConstraint, STATGROUP_Chaos);

namespace Chaos::Softs
{

	class FPBDAnimDriveConstraint final
	{
	public:
		static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
		{
			return IsAnimDriveStiffnessEnabled(PropertyCollection, false);
		}

		UE_DEPRECATED(5.2, "Use the other constructor supplying AnimationVelocities for correct subframe and damping behavior")
		FPBDAnimDriveConstraint(
			const int32 InParticleOffset,
			const int32 InParticleCount,
			const TArray<FSolverVec3>& InAnimationPositions,  // Use global indexation (will need adding ParticleOffset)
			const TArray<FSolverVec3>& InOldAnimationPositions,  // Use global indexation (will need adding ParticleOffset)
			const TConstArrayView<FRealSingle>& StiffnessMultipliers,  // Use local indexation
			const TConstArrayView<FRealSingle>& DampingMultipliers  // Use local indexation
		)
			: AnimationPositions(InAnimationPositions)
			, OldAnimationPositions_deprecated(InOldAnimationPositions)
			, AnimationVelocities(InOldAnimationPositions) // Unused when using deprecated apply
			, ParticleOffset(InParticleOffset)
			, ParticleCount(InParticleCount)
			, UseDeprecatedApply(true)
			, Stiffness(FSolverVec2::UnitVector, StiffnessMultipliers, InParticleCount)
			, Damping(FSolverVec2::UnitVector, DampingMultipliers, InParticleCount)
		{
		}

		FPBDAnimDriveConstraint(
			const int32 InParticleOffset,
			const int32 InParticleCount,
			const TArray<FSolverVec3>& InAnimationPositions,  // Use global indexation (will need adding ParticleOffset)
			const TArray<FSolverVec3>& InAnimationVelocities,  // Use global indexation (will need adding ParticleOffset)
			const TConstArrayView<FRealSingle>& StiffnessMultipliers,  // Use local indexation
			const TConstArrayView<FRealSingle>& DampingMultipliers,  // Use local indexation
			const FCollectionPropertyConstFacade& PropertyCollection
		)
			: AnimationPositions(InAnimationPositions)
			, OldAnimationPositions_deprecated(InAnimationVelocities) // Unused when not using apply
			, AnimationVelocities(InAnimationVelocities)
			, ParticleOffset(InParticleOffset)
			, ParticleCount(InParticleCount)
			, UseDeprecatedApply(false)
			, Stiffness(
				FSolverVec2(GetWeightedFloatAnimDriveStiffness(PropertyCollection, 1.f)),
				StiffnessMultipliers,
				InParticleCount)
			, Damping(
				FSolverVec2(GetWeightedFloatAnimDriveDamping(PropertyCollection, 1.f)),
				DampingMultipliers,
				InParticleCount)
		{
		}

		FPBDAnimDriveConstraint(
			const int32 InParticleOffset,
			const int32 InParticleCount,
			const TArray<FSolverVec3>& InAnimationPositions,  // Use global indexation (will need adding ParticleOffset)
			const TArray<FSolverVec3>& /*InOldAnimationPositions*/,  // deprecated
			const TArray<FSolverVec3>& InAnimationVelocities,  // Use global indexation (will need adding ParticleOffset)
			const TConstArrayView<FRealSingle>& StiffnessMultipliers,  // Use local indexation
			const TConstArrayView<FRealSingle>& DampingMultipliers  // Use local indexation
		)
			: AnimationPositions(InAnimationPositions)
			, OldAnimationPositions_deprecated(InAnimationVelocities) // Unused when not using apply
			, AnimationVelocities(InAnimationVelocities)
			, ParticleOffset(InParticleOffset)
			, ParticleCount(InParticleCount)
			, UseDeprecatedApply(false)
			, Stiffness(FSolverVec2::UnitVector, StiffnessMultipliers, InParticleCount)
			, Damping(FSolverVec2::UnitVector, DampingMultipliers, InParticleCount)
		{
		}

		~FPBDAnimDriveConstraint() {}

		// Return the stiffness input values used by the constraint
		FSolverVec2 GetStiffness() const { return Stiffness.GetWeightedValue(); }

		// Return the damping input values used by the constraint
		FSolverVec2 GetDamping() const { return Damping.GetWeightedValue(); }

		void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
		{
			if (IsAnimDriveStiffnessMutable(PropertyCollection))
			{
				Stiffness.SetWeightedValue(FSolverVec2(GetWeightedFloatAnimDriveStiffness(PropertyCollection)));
			}
			if (IsAnimDriveDampingMutable(PropertyCollection))
			{
				Damping.SetWeightedValue(FSolverVec2(GetWeightedFloatAnimDriveDamping(PropertyCollection)));
			}
		}

		inline void SetProperties(const FSolverVec2& InStiffness, const FSolverVec2& InDamping)
		{
			Stiffness.SetWeightedValue(InStiffness);
			Damping.SetWeightedValue(InDamping);
		}

		// Set stiffness offset and range, as well as the simulation stiffness exponent
		inline void ApplyProperties(const FSolverReal Dt, const int32 NumIterations)
		{
			Stiffness.ApplyPBDValues(Dt, NumIterations);
			Damping.ApplyPBDValues(Dt, NumIterations);
		}

		inline void Apply(FSolverParticles& InParticles, const FSolverReal Dt) const
		{
			SCOPE_CYCLE_COUNTER(STAT_PBD_AnimDriveConstraint);

			if (Stiffness.HasWeightMap())
			{
				if (Damping.HasWeightMap())
				{
					if (UseDeprecatedApply)
					{
						PhysicsParallelFor(ParticleCount, [this, &InParticles, &Dt](int32 Index)  // TODO: profile needed for these parallel loop based on particle count
							{
								const FSolverReal ParticleStiffness = Stiffness[Index];
								const FSolverReal ParticleDamping = Damping[Index];
								ApplyHelper_Deprecated(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
							});

					}
					else
					{
						PhysicsParallelFor(ParticleCount, [this, &InParticles, &Dt](int32 Index)  // TODO: profile needed for these parallel loop based on particle count
							{
								const FSolverReal ParticleStiffness = Stiffness[Index];
								const FSolverReal ParticleDamping = Damping[Index];
								ApplyHelper(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
							});
					}
				}
				else
				{
					const FSolverReal ParticleDamping = (FSolverReal)Damping;
					if (UseDeprecatedApply)
					{
						PhysicsParallelFor(ParticleCount, [this, &InParticles, ParticleDamping, &Dt](int32 Index)
							{
								const FSolverReal ParticleStiffness = Stiffness[Index];
								ApplyHelper_Deprecated(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
							});

					}
					else
					{
						PhysicsParallelFor(ParticleCount, [this, &InParticles, ParticleDamping, &Dt](int32 Index)
							{
								const FSolverReal ParticleStiffness = Stiffness[Index];
								ApplyHelper(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
							});
					}
				}
			}
			else
			{
				const FSolverReal ParticleStiffness = (FSolverReal)Stiffness;
				if (Damping.HasWeightMap())
				{
					if (UseDeprecatedApply)
					{
						PhysicsParallelFor(ParticleCount, [this, &InParticles, &ParticleStiffness, &Dt](int32 Index)
							{
								const FSolverReal ParticleDamping = Damping[Index];
								ApplyHelper_Deprecated(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
							});
					}
					else
					{
						PhysicsParallelFor(ParticleCount, [this, &InParticles, &ParticleStiffness, &Dt](int32 Index)
							{
								const FSolverReal ParticleDamping = Damping[Index];
								ApplyHelper(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
							});
					}
				}
				else
				{
					const FSolverReal ParticleDamping = (FSolverReal)Damping;
					if (UseDeprecatedApply)
					{
						PhysicsParallelFor(ParticleCount, [this, &InParticles, &ParticleStiffness, &ParticleDamping, &Dt](int32 Index)
							{
								ApplyHelper_Deprecated(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
							});
					}
					else
					{
						PhysicsParallelFor(ParticleCount, [this, &InParticles, &ParticleStiffness, &ParticleDamping, &Dt](int32 Index)
							{
								ApplyHelper(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
							});
					}
				}
			}
		}

	private:
		inline void ApplyHelper(FSolverParticles& Particles, const FSolverReal InStiffness, const FSolverReal InDamping, const FSolverReal Dt, const int32 Index) const
		{
			const int32 ParticleIndex = ParticleOffset + Index;
			if (Particles.InvM(ParticleIndex) == (FSolverReal)0.)
			{
				return;
			}

			FSolverVec3& ParticlePosition = Particles.P(ParticleIndex);

			ParticlePosition -= InStiffness * (ParticlePosition - AnimationPositions[ParticleIndex]);

			const FSolverVec3 ParticleDisplacement = ParticlePosition - Particles.X(ParticleIndex);
			const FSolverVec3 AnimationDisplacement = (AnimationVelocities[ParticleIndex]) * Dt;
			const FSolverVec3 RelativeDisplacement = ParticleDisplacement - AnimationDisplacement;

			ParticlePosition -= InDamping * RelativeDisplacement;
		}

		// This method does not have the correct substepping or damping behavior, but retaining for backwards compatibility until deprecated constructor can be removed
		inline void ApplyHelper_Deprecated(FSolverParticles& Particles, const FSolverReal InStiffness, const FSolverReal InDamping, const FSolverReal Dt, const int32 Index) const
		{
			const int32 ParticleIndex = ParticleOffset + Index;
			if (Particles.InvM(ParticleIndex) == (FSolverReal)0.)
			{
				return;
			}

			FSolverVec3& ParticlePosition = Particles.P(ParticleIndex);
			const FSolverVec3& AnimationPosition = AnimationPositions[ParticleIndex];
			const FSolverVec3& OldAnimationPosition = OldAnimationPositions_deprecated[ParticleIndex];

			const FSolverVec3 ParticleDisplacement = ParticlePosition - Particles.X(ParticleIndex);
			const FSolverVec3 AnimationDisplacement = OldAnimationPosition - AnimationPosition;
			const FSolverVec3 RelativeDisplacement = ParticleDisplacement - AnimationDisplacement;

			ParticlePosition -= InStiffness * (ParticlePosition - AnimationPosition) + InDamping * RelativeDisplacement;
		}

	private:
		const TArray<FSolverVec3>& AnimationPositions;  // Use global index (needs adding ParticleOffset)
		const TArray<FSolverVec3>& OldAnimationPositions_deprecated;  // Use global index (needs adding ParticleOffset). Only used by ApplyHelper_Deprecated until old constructor can be removed.
		const TArray<FSolverVec3>& AnimationVelocities;  // Use global index (needs adding ParticleOffset).
		const int32 ParticleOffset;
		const int32 ParticleCount;
		const bool UseDeprecatedApply;

		FPBDStiffness Stiffness;
		FPBDStiffness Damping;

		UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(AnimDriveStiffness, float);
		UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(AnimDriveDamping, float);
	};

}  // End namespace Chaos::Softs
