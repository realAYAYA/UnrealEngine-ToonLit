// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSpringConstraintsBase.h"
#include "Chaos/CollectionPropertyFacade.h"

namespace Chaos::Softs
{

class FPBDSpringConstraints : public FPBDSpringConstraintsBase
{
public:
	template<int32 Valence>
	FPBDSpringConstraints(
		const FSolverParticlesRange& Particles,
		const TArray<TVector<int32, Valence>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const FSolverVec2& InStiffness,
		bool bTrimKinematicConstraints = false,
		typename TEnableIf<Valence >= 2 && Valence <= 4>::Type* = nullptr)
		: Base(
			Particles,
			InConstraints,
			StiffnessMultipliers,
			InStiffness,
			bTrimKinematicConstraints)
	{
		InitColor(Particles);
	}

	template<int32 Valence>
	FPBDSpringConstraints(
		const FSolverParticles& Particles,
		int32 InParticleOffset,
		int32 InParticleCount,
		const TArray<TVector<int32, Valence>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const FSolverVec2& InStiffness,
		bool bTrimKinematicConstraints = false,
		typename TEnableIf<Valence >= 2 && Valence <= 4>::Type* = nullptr)
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

	virtual ~FPBDSpringConstraints() override {}

	template<typename SolverParticlesOrRange>
	CHAOS_API void Apply(SolverParticlesOrRange& Particles, const FSolverReal Dt) const;

	const TArray<int32>& GetConstraintsPerColorStartIndex() const { return ConstraintsPerColorStartIndex; }

protected:
	typedef FPBDSpringConstraintsBase Base;
	using Base::Constraints;
	using Base::Stiffness;
	using Base::ParticleOffset;
	using Base::ParticleCount;

private:
	template<typename SolverParticlesOrRange>
	CHAOS_API void InitColor(const SolverParticlesOrRange& InParticles);
	template<typename SolverParticlesOrRange>
	void ApplyHelper(SolverParticlesOrRange& Particles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverReal ExpStiffnessValue) const;

private:
	TArray<int32> ConstraintsPerColorStartIndex; // Constraints are ordered so each batch is contiguous. This is ColorNum + 1 length so it can be used as start and end.
};

class FPBDEdgeSpringConstraints final : public FPBDSpringConstraints
{
public:
	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return IsEdgeSpringStiffnessEnabled(PropertyCollection, false);
	}

	FPBDEdgeSpringConstraints(
		const FSolverParticlesRange& Particles,
		const TArray<TVec3<int32>>& InConstraints,
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints = false)
		: FPBDSpringConstraints(
			Particles,
			InConstraints,
			WeightMaps.FindRef(GetEdgeSpringStiffnessString(PropertyCollection, EdgeSpringStiffnessName.ToString())),
			FSolverVec2(GetWeightedFloatEdgeSpringStiffness(PropertyCollection, 1.f)),
			bTrimKinematicConstraints)
		, EdgeSpringStiffnessIndex(PropertyCollection)
	{}

	FPBDEdgeSpringConstraints(
		const FSolverParticles& Particles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const TArray<TVec3<int32>>& InConstraints,
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints = false)
		: FPBDSpringConstraints(
			Particles,
			ParticleOffset,
			ParticleCount,
			InConstraints,
			WeightMaps.FindRef(GetEdgeSpringStiffnessString(PropertyCollection, EdgeSpringStiffnessName.ToString())),
			FSolverVec2(GetWeightedFloatEdgeSpringStiffness(PropertyCollection, 1.f)),
			bTrimKinematicConstraints)
		, EdgeSpringStiffnessIndex(PropertyCollection)
	{}

	UE_DEPRECATED(5.3, "Use weight map constructor instead.")
	FPBDEdgeSpringConstraints(
		const FSolverParticles& Particles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const TArray<TVec3<int32>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints = false)
		: FPBDSpringConstraints(
			Particles,
			ParticleOffset,
			ParticleCount,
			InConstraints,
			StiffnessMultipliers,
			FSolverVec2(GetWeightedFloatEdgeSpringStiffness(PropertyCollection, 1.f)),
			bTrimKinematicConstraints)
		, EdgeSpringStiffnessIndex(PropertyCollection)
	{}

	virtual ~FPBDEdgeSpringConstraints() override = default;

	CHAOS_API void SetProperties(
		const FCollectionPropertyConstFacade& PropertyCollection,
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps);

	UE_DEPRECATED(5.3, "Use SetProperties(const FCollectionPropertyConstFacade&, const TMap<FString, TConstArrayView<FRealSingle>>&, FSolverReal) instead.")
	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		SetProperties(PropertyCollection, TMap<FString, TConstArrayView<FRealSingle>>());
	}

private:
	using FPBDSpringConstraints::ParticleCount;

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(EdgeSpringStiffness, float);
};

class FPBDBendingSpringConstraints final : public FPBDSpringConstraints
{
public:
	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return IsBendingSpringStiffnessEnabled(PropertyCollection, false);
	}

	FPBDBendingSpringConstraints(
		const FSolverParticlesRange& Particles,
		const TArray<TVec2<int32>>& InConstraints,
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints = false)
		: FPBDSpringConstraints(
			Particles,
			InConstraints,
			WeightMaps.FindRef(GetBendingSpringStiffnessString(PropertyCollection, BendingSpringStiffnessName.ToString())),
			FSolverVec2(GetWeightedFloatBendingSpringStiffness(PropertyCollection, 1.f)),
			bTrimKinematicConstraints)
		, BendingSpringStiffnessIndex(PropertyCollection)
	{}

	FPBDBendingSpringConstraints(
		const FSolverParticles& Particles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const TArray<TVec2<int32>>& InConstraints,
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints = false)
		: FPBDSpringConstraints(
			Particles,
			ParticleOffset,
			ParticleCount,
			InConstraints,
			WeightMaps.FindRef(GetBendingSpringStiffnessString(PropertyCollection, BendingSpringStiffnessName.ToString())),
			FSolverVec2(GetWeightedFloatBendingSpringStiffness(PropertyCollection, 1.f)),
			bTrimKinematicConstraints)
		, BendingSpringStiffnessIndex(PropertyCollection)
	{}

	UE_DEPRECATED(5.3, "Use weight map constructor instead.")
	FPBDBendingSpringConstraints(
		const FSolverParticles& Particles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const TArray<TVec2<int32>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints = false)
		: FPBDSpringConstraints(
			Particles,
			ParticleOffset,
			ParticleCount,
			InConstraints,
			StiffnessMultipliers,
			FSolverVec2(GetWeightedFloatBendingSpringStiffness(PropertyCollection, 1.f)),
			bTrimKinematicConstraints)
		, BendingSpringStiffnessIndex(PropertyCollection)
	{}

	virtual ~FPBDBendingSpringConstraints() override = default;

	CHAOS_API void SetProperties(
		const FCollectionPropertyConstFacade& PropertyCollection,
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps);

	UE_DEPRECATED(5.3, "Use SetProperties(const FCollectionPropertyConstFacade&, const TMap<FString, TConstArrayView<FRealSingle>>&, FSolverReal) instead.")
	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		SetProperties(PropertyCollection, TMap<FString, TConstArrayView<FRealSingle>>());
	}

private:
	using FPBDSpringConstraints::ParticleCount;

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(BendingSpringStiffness, float);
};

}  // End namespace Chaos::Softs

#if !defined(CHAOS_SPRING_ISPC_ENABLED_DEFAULT)
#define CHAOS_SPRING_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if !INTEL_ISPC || UE_BUILD_SHIPPING
static constexpr bool bChaos_Spring_ISPC_Enabled = INTEL_ISPC && CHAOS_SPRING_ISPC_ENABLED_DEFAULT;
#else
extern CHAOS_API bool bChaos_Spring_ISPC_Enabled;
#endif
