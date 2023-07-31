// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDAxialSpringConstraintsBase.h"
#include "ChaosStats.h"

DECLARE_CYCLE_STAT(TEXT("Chaos XPBD Axial Spring Constraint"), STAT_XPBD_AxialSpring, STATGROUP_Chaos);

namespace Chaos::Softs
{

// Stiffness is in kg/s^2
static const FSolverReal XPBDAxialSpringMinStiffness = (FSolverReal)1e-4; // Stiffness below this will be considered 0 since all of our calculations are actually based on 1 / stiffness.
static const FSolverReal XPBDAxialSpringMaxStiffness = (FSolverReal)1e7;

class FXPBDAxialSpringConstraints final : public FPBDAxialSpringConstraintsBase
{
	typedef FPBDAxialSpringConstraintsBase Base;
	using Base::Barys;
	using Base::Constraints;
	using Base::Dists;
	using Base::Stiffness;

public:
	FXPBDAxialSpringConstraints(
		const FSolverParticles& Particles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const TArray<TVec3<int32>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const FSolverVec2& InStiffness,
		bool bTrimKinematicConstraints)
		: Base(Particles, ParticleOffset, ParticleCount, InConstraints, StiffnessMultipliers, InStiffness, bTrimKinematicConstraints)
	{
		Lambdas.Init(0.f, Constraints.Num());
	}

	virtual ~FXPBDAxialSpringConstraints() override {}

	void ApplyProperties(const FSolverReal Dt, const int32 NumIterations) { Stiffness.ApplyXPBDValues(XPBDAxialSpringMaxStiffness); }

	void Init() const { for (FSolverReal& Lambda : Lambdas) { Lambda = (FSolverReal)0.; } }

	void Apply(FSolverParticles& Particles, const FSolverReal Dt) const
	{
		SCOPE_CYCLE_COUNTER(STAT_XPBD_AxialSpring);
		if (!Stiffness.HasWeightMap())
		{
			const FSolverReal ExpStiffnessValue = (FSolverReal)Stiffness;
			if (ExpStiffnessValue < XPBDAxialSpringMinStiffness)
			{
				return;
			}
			for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
			{
				const TVector<int32, 3>& constraint = Constraints[ConstraintIndex];
				const int32 i1 = constraint[0];
				const int32 i2 = constraint[1];
				const int32 i3 = constraint[2];
				const FSolverVec3 Delta = GetDelta(Particles, Dt, ConstraintIndex, ExpStiffnessValue);
				const FSolverReal Multiplier = (FSolverReal)2. / (FMath::Max(Barys[ConstraintIndex], (FSolverReal)1. - Barys[ConstraintIndex]) + (FSolverReal)1.);
				if (Particles.InvM(i1) > 0)
				{
					Particles.P(i1) -= Multiplier * Particles.InvM(i1) * Delta;
				}
				if (Particles.InvM(i2))
				{
					Particles.P(i2) += Multiplier * Particles.InvM(i2) * Barys[ConstraintIndex] * Delta;
				}
				if (Particles.InvM(i3))
				{
					Particles.P(i3) += Multiplier * Particles.InvM(i3) * ((FSolverReal)1. - Barys[ConstraintIndex]) * Delta;
				}
			}
		}
		else
		{
			for (int32 ConstraintIndex = 0; ConstraintIndex < Constraints.Num(); ++ConstraintIndex)
			{
				const FSolverReal ExpStiffnessValue = Stiffness[ConstraintIndex];
				const TVector<int32, 3>& constraint = Constraints[ConstraintIndex];
				const int32 i1 = constraint[0];
				const int32 i2 = constraint[1];
				const int32 i3 = constraint[2];
				const FSolverVec3 Delta = GetDelta(Particles, Dt, ConstraintIndex, ExpStiffnessValue);
				const FSolverReal Multiplier = (FSolverReal)2. / (FMath::Max(Barys[ConstraintIndex], (FSolverReal)1. - Barys[ConstraintIndex]) + (FSolverReal)1.);
				if (Particles.InvM(i1) > 0)
				{
					Particles.P(i1) -= Multiplier * Particles.InvM(i1) * Delta;
				}
				if (Particles.InvM(i2))
				{
					Particles.P(i2) += Multiplier * Particles.InvM(i2) * Barys[ConstraintIndex] * Delta;
				}
				if (Particles.InvM(i3))
				{
					Particles.P(i3) += Multiplier * Particles.InvM(i3) * ((FSolverReal)1. - Barys[ConstraintIndex]) * Delta;
				}
			}
		}
	}

private:
	FSolverVec3 GetDelta(const FSolverParticles& Particles, const FSolverReal Dt, const int32 InConstraintIndex, const FSolverReal StiffnessValue) const
	{
		const TVector<int32, 3>& Constraint = Constraints[InConstraintIndex];
		const int32 i1 = Constraint[0];
		const int32 i2 = Constraint[1];
		const int32 i3 = Constraint[2];

		const FSolverReal Bary = Barys[InConstraintIndex];
		const FSolverReal PInvMass = Particles.InvM(i3) * ((FSolverReal)1. - Bary) + Particles.InvM(i2) * Bary;
		if (StiffnessValue < XPBDAxialSpringMinStiffness || ( Particles.InvM(i1) == (FSolverReal)0. && PInvMass == (FSolverReal)0.))
		{
			return FSolverVec3((FSolverReal)0.);
		}
		const FSolverReal CombinedInvMass = PInvMass + Particles.InvM(i1);
		ensure(CombinedInvMass > (FSolverReal)SMALL_NUMBER);

		const FSolverVec3& P1 = Particles.P(i1);
		const FSolverVec3& P2 = Particles.P(i2);
		const FSolverVec3& P3 = Particles.P(i3);
		const FSolverVec3 P = (P2 - P3) * Bary + P3;

		const FSolverVec3 Difference = P1 - P;
		const FSolverReal Distance = Difference.Size();
		if (UNLIKELY(Distance <= SMALL_NUMBER))
		{
			return FSolverVec3((FSolverReal)0.);
		}
		const FSolverVec3 Direction = Difference / Distance;
		const FSolverReal Offset = (Distance - Dists[InConstraintIndex]);

		FSolverReal& Lambda = Lambdas[InConstraintIndex];
		const FSolverReal Alpha = (FSolverReal)1 / (StiffnessValue * Dt * Dt);

		const FSolverReal DLambda = (Offset - Alpha * Lambda) / (CombinedInvMass + Alpha);
		const FSolverVec3 Delta = DLambda * Direction;
		Lambda += DLambda;

		return Delta;
	}

private:
	mutable TArray<FSolverReal> Lambdas;
};

}  // End namespace Chaos::Softs
