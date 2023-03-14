// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/PBDSoftsSolverParticles.h"
#include "Chaos/PBDStiffness.h"
#include "Containers/Array.h"

namespace Chaos::Softs
{

class FPBDAxialSpringConstraintsBase
{
public:
	FPBDAxialSpringConstraintsBase(
		const FSolverParticles& Particles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const TArray<TVec3<int32>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const FSolverVec2& InStiffness,
		bool bTrimKinematicConstraints)
		: Constraints(TrimConstraints(InConstraints, 
			[&Particles, bTrimKinematicConstraints](int32 Index0, int32 Index1, int32 Index2)
			{
				return bTrimKinematicConstraints && Particles.InvM(Index0) == (FSolverReal)0. && Particles.InvM(Index1) == (FSolverReal)0. && Particles.InvM(Index2) == (FSolverReal)0.;
			}))
		, Stiffness(InStiffness, StiffnessMultipliers, TConstArrayView<TVec3<int32>>(Constraints), ParticleOffset, ParticleCount)
	{
		Init(Particles);
	}

	virtual ~FPBDAxialSpringConstraintsBase() {}

	void SetProperties(const FSolverVec2& InStiffness) { Stiffness.SetWeightedValue(InStiffness); }

	void ApplyProperties(const FSolverReal Dt, const int32 NumIterations) { Stiffness.ApplyValues(Dt, NumIterations); }

protected:
	inline FSolverVec3 GetDelta(const FSolverParticles& Particles, const int32 ConstraintIndex, const FSolverReal ExpStiffnessValue) const
	{
		const TVec3<int32>& Constraint = Constraints[ConstraintIndex];
		const int32 i1 = Constraint[0];
		const int32 i2 = Constraint[1];
		const int32 i3 = Constraint[2];
		const FSolverReal PInvMass = Particles.InvM(i3) * ((FSolverReal)1. - Barys[ConstraintIndex]) + Particles.InvM(i2) * Barys[ConstraintIndex];
		if (Particles.InvM(i1) == (FSolverReal)0. && PInvMass == (FSolverReal)0.)
		{
			return FSolverVec3((FSolverReal)0.);
		}
		const FSolverVec3& P1 = Particles.P(i1);
		const FSolverVec3& P2 = Particles.P(i2);
		const FSolverVec3& P3 = Particles.P(i3);
		const FSolverVec3 P = (P2 - P3) * Barys[ConstraintIndex] + P3;
		const FSolverVec3 Difference = P1 - P;
		const FSolverReal Distance = Difference.Size();
		if (UNLIKELY(Distance <= (FSolverReal)UE_SMALL_NUMBER))
		{
			return FSolverVec3((FSolverReal)0.);
		}
		const FSolverVec3 Direction = Difference / Distance;
		const FSolverVec3 Delta = (Distance - Dists[ConstraintIndex]) * Direction;
		const FSolverReal CombinedInvMass = PInvMass + Particles.InvM(i1);
		checkSlow(CombinedInvMass > (FSolverReal)1e-7);
		return ExpStiffnessValue * Delta / CombinedInvMass;
	}

private:
	FSolverReal FindBary(const FSolverParticles& Particles, const int32 i1, const int32 i2, const int32 i3)
	{
		const FSolverVec3& P1 = Particles.X(i1);
		const FSolverVec3& P2 = Particles.X(i2);
		const FSolverVec3& P3 = Particles.X(i3);
		const FSolverVec3& P32 = P3 - P2;
		const FSolverReal Bary = FSolverVec3::DotProduct(P32, P3 - P1) / P32.SizeSquared();
		return FMath::Clamp(Bary, (FSolverReal)0., (FSolverReal)1.);
	}

	template<typename Predicate>
	TArray<TVec3<int32>> TrimConstraints(const TArray<TVec3<int32>>& InConstraints, Predicate TrimPredicate)
	{
		TSet<TVec3<int32>> TrimmedConstraints;
		TrimmedConstraints.Reserve(InConstraints.Num());

		for (const TVec3<int32>& Constraint : InConstraints)
		{
			const int32 Index0 = Constraint[0];
			const int32 Index1 = Constraint[1];
			const int32 Index2 = Constraint[2];

			if (!TrimPredicate(Index0, Index1, Index2))
			{
				TrimmedConstraints.Add(
					Index0 <= Index1 ?
						Index1 <= Index2 ? TVec3<int32>(Index0, Index1, Index2) :
						Index0 <= Index2 ? TVec3<int32>(Index0, Index2, Index1) :
										   TVec3<int32>(Index2, Index0, Index1) :
					// Index1 < Index0
						Index0 <= Index2 ? TVec3<int32>(Index1, Index0, Index2) :
						Index1 <= Index2 ? TVec3<int32>(Index1, Index2, Index0) :
										   TVec3<int32>(Index2, Index1, Index0));
			}
		}
		return TrimmedConstraints.Array();
	}

	void Init(const FSolverParticles& Particles)
	{
		Barys.Reset(Constraints.Num());
		Dists.Reset(Constraints.Num());

		for (TVec3<int32>& Constraint : Constraints)
		{
			int32 i1 = Constraint[0];
			int32 i2 = Constraint[1];
			int32 i3 = Constraint[2];
			// Find Bary closest to 0.5
			const FSolverReal Bary1 = FindBary(Particles, i1, i2, i3);
			const FSolverReal Bary2 = FindBary(Particles, i2, i3, i1);
			const FSolverReal Bary3 = FindBary(Particles, i3, i1, i2);
			FSolverReal Bary = Bary1;
			const FSolverReal Bary1dist = FGenericPlatformMath::Abs(Bary1 - (FSolverReal)0.5);
			const FSolverReal Bary2dist = FGenericPlatformMath::Abs(Bary2 - (FSolverReal)0.5);
			const FSolverReal Bary3dist = FGenericPlatformMath::Abs(Bary3 - (FSolverReal)0.5);
			if (Bary3dist < Bary2dist && Bary3dist < Bary1dist)
			{
				Constraint[0] = i3;
				Constraint[1] = i1;
				Constraint[2] = i2;
				Bary = Bary3;
			}
			else if (Bary2dist < Bary1dist && Bary2dist < Bary3dist)
			{
				Constraint[0] = i2;
				Constraint[1] = i3;
				Constraint[2] = i1;
				Bary = Bary2;
			}
			// Reset as they may have changed
			i1 = Constraint[0];
			i2 = Constraint[1];
			i3 = Constraint[2];
			const FSolverVec3& P1 = Particles.X(i1);
			const FSolverVec3& P2 = Particles.X(i2);
			const FSolverVec3& P3 = Particles.X(i3);
			const FSolverVec3 P = (P2 - P3) * Bary + P3;
			Barys.Add(Bary);
			Dists.Add((P1 - P).Size());
		}
	}

protected:
	TArray<TVec3<int32>> Constraints;
	TArray<FSolverReal> Barys;
	TArray<FSolverReal> Dists;
	FPBDStiffness Stiffness;
};

}  // End namespace Chaos::Softs
