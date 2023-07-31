// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDBendingConstraintsBase.h"

namespace Chaos::Softs
{

class CHAOS_API FPBDBendingConstraints : public FPBDBendingConstraintsBase
{
	typedef FPBDBendingConstraintsBase Base;
	using Base::Constraints;

public:
	FPBDBendingConstraints(const FSolverParticles& InParticles,
		int32 ParticleOffset,
		int32 ParticleCount,
		TArray<TVec4<int32>>&& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers,
		const FSolverVec2& InStiffness,
		const FSolverReal InBucklingRatio,
		const FSolverVec2& InBucklingStiffness,
		bool bTrimKinematicConstraints = false)
		:Base(InParticles, ParticleOffset, ParticleCount, MoveTemp(InConstraints), StiffnessMultipliers, BucklingStiffnessMultipliers, InStiffness, InBucklingRatio, InBucklingStiffness, bTrimKinematicConstraints) 
	{
		InitColor(InParticles);
	}

	FPBDBendingConstraints(const FSolverParticles& InParticles, TArray<TVec4<int32>>&& InConstraints, const FSolverReal InStiffness = (FSolverReal)1.)
	    : Base(InParticles, MoveTemp(InConstraints), InStiffness) {}


	virtual ~FPBDBendingConstraints() override {}

	void Apply(FSolverParticles& InParticles, const FSolverReal Dt) const;

private:
	void InitColor(const FSolverParticles& InParticles);
	void ApplyHelper(FSolverParticles& InParticles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverReal ExpStiffnessValue, const FSolverReal ExpBucklingValue) const;

	TArray<int32> ConstraintsPerColorStartIndex; // Constraints are ordered so each batch is contiguous. This is ColorNum + 1 length so it can be used as start and end.
};

}  // End namespace Chaos::Softs

// Support ISPC enable/disable in non-shipping builds
#if !INTEL_ISPC
const bool bChaos_Bending_ISPC_Enabled = false;
#elif UE_BUILD_SHIPPING
const bool bChaos_Bending_ISPC_Enabled = true;
#else
extern CHAOS_API bool bChaos_Bending_ISPC_Enabled;
#endif
