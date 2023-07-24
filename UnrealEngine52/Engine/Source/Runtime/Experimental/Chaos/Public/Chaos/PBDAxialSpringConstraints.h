// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

// HEADER_UNIT_SKIP - Internal

#include "Chaos/PBDAxialSpringConstraintsBase.h"
#include "Chaos/CollectionPropertyFacade.h"

namespace Chaos::Softs
{

class CHAOS_API FPBDAxialSpringConstraints : public FPBDAxialSpringConstraintsBase
{
	typedef FPBDAxialSpringConstraintsBase Base;
	using Base::Barys;
	using Base::Constraints;

public:
	FPBDAxialSpringConstraints(
		const FSolverParticles& Particles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const TArray<TVec3<int32>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const FSolverVec2& InStiffness,
		bool bTrimKinematicConstraints)
		: Base(Particles, ParticleOffset, ParticleCount, InConstraints, StiffnessMultipliers, InStiffness, bTrimKinematicConstraints)
	{
		InitColor(Particles, ParticleOffset, ParticleCount);
	}

	virtual ~FPBDAxialSpringConstraints() override {}

	void Apply(FSolverParticles& InParticles, const FSolverReal Dt) const;

protected:
	using Base::Stiffness;

private:
	void InitColor(const FSolverParticles& InParticles, const int32 ParticleOffset, const int32 ParticleCount);
	void ApplyHelper(FSolverParticles& InParticles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverReal ExpStiffnessValue) const;

	TArray<int32> ConstraintsPerColorStartIndex; // Constraints are ordered so each batch is contiguous. This is ColorNum + 1 length so it can be used as start and end.
};

class CHAOS_API FPBDAreaSpringConstraints final : public FPBDAxialSpringConstraints
{
public:
	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return IsAreaSpringStiffnessEnabled(PropertyCollection, false);
	}

	FPBDAreaSpringConstraints(
		const FSolverParticles& Particles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const TArray<TVec3<int32>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints)
		: FPBDAxialSpringConstraints(
			Particles,
			ParticleOffset,
			ParticleCount,
			InConstraints,
			StiffnessMultipliers,
			FSolverVec2(GetWeightedFloatAreaSpringStiffness(PropertyCollection, 1.f)),
			bTrimKinematicConstraints)
	{}

	virtual ~FPBDAreaSpringConstraints() override = default;

	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		if (IsAreaSpringStiffnessMutable(PropertyCollection))
		{
			Stiffness.SetWeightedValue(FSolverVec2(GetWeightedFloatAreaSpringStiffness(PropertyCollection)));
		}
	}

private:
	using FPBDAxialSpringConstraints::Stiffness;

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(AreaSpringStiffness, float);
};

}  // End namespace Chaos::Softs

#if !defined(CHAOS_AXIAL_SPRING_ISPC_ENABLED_DEFAULT)
#define CHAOS_AXIAL_SPRING_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if !INTEL_ISPC || UE_BUILD_SHIPPING
static constexpr bool bChaos_AxialSpring_ISPC_Enabled = INTEL_ISPC && CHAOS_AXIAL_SPRING_ISPC_ENABLED_DEFAULT;
#else
extern CHAOS_API bool bChaos_AxialSpring_ISPC_Enabled;
#endif
