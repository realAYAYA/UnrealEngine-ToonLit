// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

// HEADER_UNIT_SKIP - Internal

#include "Chaos/PBDAxialSpringConstraintsBase.h"
#include "Chaos/CollectionPropertyFacade.h"

namespace Chaos::Softs
{

class FPBDAxialSpringConstraints : public FPBDAxialSpringConstraintsBase
{
	typedef FPBDAxialSpringConstraintsBase Base;
	using Base::Barys;

public:
	FPBDAxialSpringConstraints(
		const FSolverParticlesRange& Particles,
		const TArray<TVec3<int32>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const FSolverVec2& InStiffness,
		bool bTrimKinematicConstraints)
		: Base(
			Particles,
			InConstraints,
			StiffnessMultipliers,
			InStiffness,
			bTrimKinematicConstraints)
	{
		InitColor(Particles);
	}

	FPBDAxialSpringConstraints(
		const FSolverParticles& Particles,
		int32 InParticleOffset,
		int32 InParticleCount,
		const TArray<TVec3<int32>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const FSolverVec2& InStiffness,
		bool bTrimKinematicConstraints)
		: Base(
			Particles,
			InParticleOffset,
			InParticleCount,
			InConstraints,
			StiffnessMultipliers,
			InStiffness,
			bTrimKinematicConstraints)
	{
		InitColor(Particles);
	}

	virtual ~FPBDAxialSpringConstraints() override {}

	template<typename SolverParticlesOrRange>
	CHAOS_API void Apply(SolverParticlesOrRange& InParticles, const FSolverReal Dt) const;

protected:
	using Base::Constraints;
	using Base::Stiffness;
	using Base::ParticleOffset;
	using Base::ParticleCount;

private:
	template<typename SolverParticlesOrRange>
	CHAOS_API void InitColor(const SolverParticlesOrRange& InParticles);
	template<typename SolverParticlesOrRange>
	void ApplyHelper(SolverParticlesOrRange& InParticles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverReal ExpStiffnessValue) const;

	TArray<int32> ConstraintsPerColorStartIndex; // Constraints are ordered so each batch is contiguous. This is ColorNum + 1 length so it can be used as start and end.
};

class FPBDAreaSpringConstraints final : public FPBDAxialSpringConstraints
{
public:
	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return IsAreaSpringStiffnessEnabled(PropertyCollection, false);
	}

	FPBDAreaSpringConstraints(
		const FSolverParticlesRange& Particles,
		const TArray<TVec3<int32>>& InConstraints,
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints)
		: FPBDAxialSpringConstraints(
			Particles,
			InConstraints,
			WeightMaps.FindRef(GetAreaSpringStiffnessString(PropertyCollection, AreaSpringStiffnessName.ToString())),
			FSolverVec2(GetWeightedFloatAreaSpringStiffness(PropertyCollection, 1.f)),
			bTrimKinematicConstraints)
		, AreaSpringStiffnessIndex(PropertyCollection)
	{}

	FPBDAreaSpringConstraints(
		const FSolverParticles& Particles,
		int32 InParticleOffset,
		int32 InParticleCount,
		const TArray<TVec3<int32>>& InConstraints,
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints)
		: FPBDAxialSpringConstraints(
			Particles,
			InParticleOffset,
			InParticleCount,
			InConstraints,
			WeightMaps.FindRef(GetAreaSpringStiffnessString(PropertyCollection, AreaSpringStiffnessName.ToString())),
			FSolverVec2(GetWeightedFloatAreaSpringStiffness(PropertyCollection, 1.f)),
			bTrimKinematicConstraints)
		, AreaSpringStiffnessIndex(PropertyCollection)
	{}

	UE_DEPRECATED(5.3, "Use weight map constructor instead.")
	FPBDAreaSpringConstraints(
		const FSolverParticles& Particles,
		int32 InParticleOffset,
		int32 InParticleCount,
		const TArray<TVec3<int32>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints)
		: FPBDAxialSpringConstraints(
			Particles,
			InParticleOffset,
			InParticleCount,
			InConstraints,
			StiffnessMultipliers,
			FSolverVec2(GetWeightedFloatAreaSpringStiffness(PropertyCollection, 1.f)),
			bTrimKinematicConstraints)
		, AreaSpringStiffnessIndex(PropertyCollection)
	{}

	virtual ~FPBDAreaSpringConstraints() override = default;

	CHAOS_API void SetProperties(
		const FCollectionPropertyConstFacade& PropertyCollection,
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps);

	UE_DEPRECATED(5.3, "Use SetProperties(const FCollectionPropertyConstFacade&, const TMap<FString, TConstArrayView<FRealSingle>>&, FSolverReal) instead.")
	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		SetProperties(PropertyCollection, TMap<FString, TConstArrayView<FRealSingle>>());
	}

private:
	using FPBDAxialSpringConstraints::Constraints;
	using FPBDAxialSpringConstraints::Stiffness;
	using FPBDAxialSpringConstraints::ParticleOffset;
	using FPBDAxialSpringConstraints::ParticleCount;

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
