// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDBendingConstraintsBase.h"
#include "ChaosStats.h"


namespace Chaos::Softs
{

// Stiffness is in kg cm^2 / rad^2 s^2
static const FSolverReal XPBDBendMinStiffness = (FSolverReal)1e-4; // Stiffness below this will be considered 0 since all of our calculations are actually based on 1 / stiffness.
static const FSolverReal XPBDBendMaxStiffness = (FSolverReal)1e7; 

class CHAOS_API FXPBDBendingConstraints final : public FPBDBendingConstraintsBase
{
	typedef FPBDBendingConstraintsBase Base;
	using Base::Constraints;
	using Base::RestAngles;
	using Base::Stiffness;
	using Base::BucklingStiffness;

public:
	FXPBDBendingConstraints(const FSolverParticles& InParticles,
		int32 ParticleOffset,
		int32 ParticleCount,
		TArray<TVec4<int32>>&& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers,
		const FSolverVec2& InStiffness,
		const FSolverReal InBucklingRatio,
		const FSolverVec2& InBucklingStiffness,
		bool bTrimKinematicConstraints = false)
		: Base(InParticles, ParticleOffset, ParticleCount, MoveTemp(InConstraints), StiffnessMultipliers, BucklingStiffnessMultipliers, InStiffness, InBucklingRatio, InBucklingStiffness, bTrimKinematicConstraints)
	{
		Lambdas.Init((FSolverReal)0., Constraints.Num());
		InitColor(InParticles);
	}

	virtual ~FXPBDBendingConstraints() override {}

	void Init(const FSolverParticles& InParticles)
	{ 
		Lambdas.Reset();
		Lambdas.AddZeroed(Constraints.Num());
		FPBDBendingConstraintsBase::Init(InParticles);
	}

	// Update stiffness values
	void SetProperties(const FSolverVec2& InStiffness, const FSolverReal InBucklingRatio, const FSolverVec2& InBucklingStiffness)
	{
		Stiffness.SetWeightedValueUnclamped(InStiffness);
		BucklingRatio = InBucklingRatio;
		BucklingStiffness.SetWeightedValueUnclamped(InBucklingStiffness);
	}

	// Update stiffness table, as well as the simulation stiffness exponent
	void ApplyProperties(const FSolverReal Dt, const int32 NumIterations) { Stiffness.ApplyXPBDValues(XPBDBendMaxStiffness); BucklingStiffness.ApplyXPBDValues(XPBDBendMaxStiffness); }

	void Apply(FSolverParticles& Particles, const FSolverReal Dt) const;

private:
	void InitColor(const FSolverParticles& InParticles);
	void ApplyHelper(FSolverParticles& Particles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverReal ExpStiffnessValue, const FSolverReal ExpBucklingValue) const;

private:
	mutable TArray<FSolverReal> Lambdas;
	TArray<int32> ConstraintsPerColorStartIndex; // Constraints are ordered so each batch is contiguous. This is ColorNum + 1 length so it can be used as start and end.
};

}  // End namespace Chaos::Softs

// Support ISPC enable/disable in non-shipping builds
#if !INTEL_ISPC
const bool bChaos_XPBDBending_ISPC_Enabled = false;
#elif UE_BUILD_SHIPPING
const bool bChaos_XPBDBending_ISPC_Enabled = true;
#else
extern CHAOS_API bool bChaos_XPBDBending_ISPC_Enabled;
#endif
