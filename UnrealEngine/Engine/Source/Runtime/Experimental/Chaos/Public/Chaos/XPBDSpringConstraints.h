// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSpringConstraintsBase.h"
#include "ChaosStats.h"


namespace Chaos::Softs
{

// Stiffness is in kg/s^2
static const FSolverReal XPBDSpringMinStiffness = (FSolverReal)1e-4; // Stiffness below this will be considered 0 since all of our calculations are actually based on 1 / stiffness.
static const FSolverReal XPBDSpringMaxStiffness = (FSolverReal)1e7; 

class CHAOS_API FXPBDSpringConstraints final : public FPBDSpringConstraintsBase
{
	typedef FPBDSpringConstraintsBase Base;
	using Base::Constraints;
	using Base::Dists;
	using Base::Stiffness;

public:
	template<int32 Valence>
	FXPBDSpringConstraints(
		const FSolverParticles& Particles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const TArray<TVector<int32, Valence>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const FSolverVec2& InStiffness,
		bool bTrimKinematicConstraints = false,
		typename TEnableIf<Valence >= 2 && Valence <= 4>::Type* = nullptr)
		: Base(Particles, ParticleOffset, ParticleCount, InConstraints, StiffnessMultipliers, InStiffness, bTrimKinematicConstraints)
	{
		Lambdas.Init((FSolverReal)0., Constraints.Num());
		InitColor(Particles);
	}

	virtual ~FXPBDSpringConstraints() override {}

	void Init() const { for (FSolverReal& Lambda : Lambdas) { Lambda = (FSolverReal)0.; } }

	// Update stiffness values
	void SetProperties(const FSolverVec2& InStiffness) { Stiffness.SetWeightedValueUnclamped(InStiffness); }
	
	// Update stiffness table, as well as the simulation stiffness exponent
	inline void ApplyProperties(const FSolverReal Dt, const int32 NumIterations) { Stiffness.ApplyXPBDValues(XPBDSpringMaxStiffness); }

	void Apply(FSolverParticles& Particles, const FSolverReal Dt) const;

private:
	void InitColor(const FSolverParticles& InParticles);
	void ApplyHelper(FSolverParticles& Particles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverReal StiffnessValue) const;

	FSolverVec3 GetDelta(const FSolverParticles& Particles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverReal StiffnessValue) const
	{
		const TVec2<int32>& Constraint = Constraints[ConstraintIndex];

		const int32 i1 = Constraint[0];
		const int32 i2 = Constraint[1];

		if (StiffnessValue < XPBDSpringMinStiffness || (Particles.InvM(i2) == (FSolverReal)0. && Particles.InvM(i1) == (FSolverReal)0.))
		{
			return FSolverVec3((FSolverReal)0.);
		}
		const FSolverReal CombinedInvMass = Particles.InvM(i2) + Particles.InvM(i1);

		const FSolverVec3& P1 = Particles.P(i1);
		const FSolverVec3& P2 = Particles.P(i2);
		FSolverVec3 Direction = P1 - P2;
		const FSolverReal Distance = Direction.SafeNormalize();
		const FSolverReal Offset = Distance - Dists[ConstraintIndex];

		FSolverReal& Lambda = Lambdas[ConstraintIndex];
		const FSolverReal Alpha = (FSolverReal)1.f / (StiffnessValue * Dt * Dt);

		const FSolverReal DLambda = (Offset - Alpha * Lambda) / (CombinedInvMass + Alpha);
		const FSolverVec3 Delta = DLambda * Direction;
		Lambda += DLambda;

		return Delta;
	}

private:
	mutable TArray<FSolverReal> Lambdas;
	TArray<int32> ConstraintsPerColorStartIndex; // Constraints are ordered so each batch is contiguous. This is ColorNum + 1 length so it can be used as start and end.
};

}  // End namespace Chaos::Softs

// Support ISPC enable/disable in non-shipping builds
#if !INTEL_ISPC
const bool bChaos_XPBDSpring_ISPC_Enabled = false;
#elif UE_BUILD_SHIPPING
const bool bChaos_XPBDSpring_ISPC_Enabled = true;
#else
extern CHAOS_API bool bChaos_XPBDSpring_ISPC_Enabled;
#endif
