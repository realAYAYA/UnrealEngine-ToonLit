// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Framework/Parallel.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDStiffness.h"
#include "ChaosStats.h"

DECLARE_CYCLE_STAT(TEXT("Chaos PBD Anim Drive Constraint"), STAT_PBD_AnimDriveConstraint, STATGROUP_Chaos);

namespace Chaos::Softs
{

	class FPBDAnimDriveConstraint final
	{
	public:
		FPBDAnimDriveConstraint(
			const int32 InParticleOffset,
			const int32 InParticleCount,
			const TArray<FSolverVec3>& InAnimationPositions,  // Use global indexation (will need adding ParticleOffset)
			const TArray<FSolverVec3>& InOldAnimationPositions,  // Use global indexation (will need adding ParticleOffset)
			const TConstArrayView<FRealSingle>& StiffnessMultipliers,  // Use local indexation
			const TConstArrayView<FRealSingle>& DampingMultipliers  // Use local indexation
		)
			: AnimationPositions(InAnimationPositions)
			, OldAnimationPositions(InOldAnimationPositions)
			, ParticleOffset(InParticleOffset)
			, ParticleCount(InParticleCount)
			, Stiffness(FSolverVec2::UnitVector, StiffnessMultipliers, InParticleCount)
			, Damping(FSolverVec2::UnitVector, DampingMultipliers, InParticleCount)
		{
		}

		~FPBDAnimDriveConstraint() {}

		// Return the stiffness input values used by the constraint
		FSolverVec2 GetStiffness() const { return Stiffness.GetWeightedValue(); }

		// Return the damping input values used by the constraint
		FSolverVec2 GetDamping() const { return Damping.GetWeightedValue(); }

		inline void SetProperties(const FSolverVec2& InStiffness, const FSolverVec2& InDamping)
		{
			Stiffness.SetWeightedValue(InStiffness);
			Damping.SetWeightedValue(InDamping);
		}

		// Set stiffness offset and range, as well as the simulation stiffness exponent
		inline void ApplyProperties(const FSolverReal Dt, const int32 NumIterations)
		{
			Stiffness.ApplyValues(Dt, NumIterations);
			Damping.ApplyValues(Dt, NumIterations);
		}

		inline void Apply(FSolverParticles& InParticles, const FSolverReal Dt) const
		{
			SCOPE_CYCLE_COUNTER(STAT_PBD_AnimDriveConstraint);

			if (Stiffness.HasWeightMap())
			{
				if (Damping.HasWeightMap())
				{
					PhysicsParallelFor(ParticleCount, [this, &InParticles, &Dt](int32 Index)  // TODO: profile needed for these parallel loop based on particle count
					{
						const FSolverReal ParticleStiffness = Stiffness[Index];
						const FSolverReal ParticleDamping = Damping[Index];
						ApplyHelper(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
					});
				}
				else
				{
					const FSolverReal ParticleDamping = (FSolverReal)Damping;
					PhysicsParallelFor(ParticleCount, [this, &InParticles, ParticleDamping, &Dt](int32 Index)
					{
						const FSolverReal ParticleStiffness = Stiffness[Index];
						ApplyHelper(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
					});
				}
			}
			else
			{
				const FSolverReal ParticleStiffness = (FSolverReal)Stiffness;
				if (Damping.HasWeightMap())
				{
					PhysicsParallelFor(ParticleCount, [this, &InParticles, &ParticleStiffness, &Dt](int32 Index)
					{
						const FSolverReal ParticleDamping = Damping[Index];
						ApplyHelper(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
					});
				}
				else
				{
					const FSolverReal ParticleDamping = (FSolverReal)Damping;
					PhysicsParallelFor(ParticleCount, [this, &InParticles, &ParticleStiffness, &ParticleDamping, &Dt](int32 Index)
					{
						ApplyHelper(InParticles, ParticleStiffness, ParticleDamping, Dt, Index);
					});
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
			const FSolverVec3& AnimationPosition = AnimationPositions[ParticleIndex];
			const FSolverVec3& OldAnimationPosition = OldAnimationPositions[ParticleIndex];

			const FSolverVec3 ParticleDisplacement = ParticlePosition - Particles.X(ParticleIndex);
			const FSolverVec3 AnimationDisplacement = OldAnimationPosition - AnimationPosition;
			const FSolverVec3 RelativeDisplacement = ParticleDisplacement - AnimationDisplacement;

			ParticlePosition -= InStiffness * (ParticlePosition - AnimationPosition) + InDamping * RelativeDisplacement;
		}

	private:
		const TArray<FSolverVec3>& AnimationPositions;  // Use global index (needs adding ParticleOffset)
		const TArray<FSolverVec3>& OldAnimationPositions;  // Use global index (needs adding ParticleOffset)
		const int32 ParticleOffset;
		const int32 ParticleCount;

		FPBDStiffness Stiffness;
		FPBDStiffness Damping;
	};

}  // End namespace Chaos::Softs
