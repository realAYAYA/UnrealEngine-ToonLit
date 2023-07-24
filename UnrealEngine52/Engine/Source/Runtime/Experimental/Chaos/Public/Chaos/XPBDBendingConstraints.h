// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDBendingConstraintsBase.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "ChaosStats.h"

namespace Chaos::Softs
{

// Stiffness is in kg cm^2 / rad^2 s^2
UE_DEPRECATED(5.2, "Use FXPBDBendingConstraints::MinStiffness instead.")
static const FSolverReal XPBDBendMinStiffness = (FSolverReal)1e-4; // Stiffness below this will be considered 0 since all of our calculations are actually based on 1 / stiffness.
UE_DEPRECATED(5.2, "Use FXPBDBendingConstraints::MaxStiffness instead.")
static const FSolverReal XPBDBendMaxStiffness = (FSolverReal)1e7;

class CHAOS_API FXPBDBendingConstraints final : public FPBDBendingConstraintsBase
{
	typedef FPBDBendingConstraintsBase Base;

public:
	// Stiffness is in kg cm^2 / rad^2 s^2
	static constexpr FSolverReal MinStiffness = (FSolverReal)1e-4; // Stiffness below this will be considered 0 since all of our calculations are actually based on 1 / stiffness.
	static constexpr FSolverReal MaxStiffness = (FSolverReal)1e7;
	static constexpr FSolverReal MinDamping = (FSolverReal)0.;
	static constexpr FSolverReal MaxDamping = (FSolverReal)1000.;

	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return IsXPBDBendingElementStiffnessEnabled(PropertyCollection, false);
	}

	FXPBDBendingConstraints(const FSolverParticles& InParticles,
		int32 ParticleOffset,
		int32 ParticleCount,
		TArray<TVec4<int32>>&& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers,
		const TConstArrayView<FRealSingle>& DampingMultipliers,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints = false)
		: Base(
			InParticles,
			ParticleOffset,
			ParticleCount,
			MoveTemp(InConstraints),
			StiffnessMultipliers,
			BucklingStiffnessMultipliers,
			FSolverVec2(GetWeightedFloatXPBDBendingElementStiffness(PropertyCollection, MaxStiffness)),
			(FSolverReal)GetXPBDBucklingRatio(PropertyCollection, 0.f),
			FSolverVec2(GetWeightedFloatXPBDBucklingStiffness(PropertyCollection, MaxStiffness)),
			bTrimKinematicConstraints,
			MaxStiffness)
		, DampingRatio(
			FSolverVec2(GetWeightedFloatXPBDBendingElementDamping(PropertyCollection, MinDamping)).ClampAxes(MinDamping, MaxDamping),
			DampingMultipliers,
			TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
			ParticleOffset,
			ParticleCount)
	{
		Lambdas.Init((FSolverReal)0., Constraints.Num());
		InitColor(InParticles, ParticleOffset, ParticleCount);
	}

	UE_DEPRECATED(5.2, "Use one the other constructors instead.")
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
		: Base(
			InParticles,
			ParticleOffset,
			ParticleCount,
			MoveTemp(InConstraints),
			StiffnessMultipliers,
			BucklingStiffnessMultipliers,
			InStiffness,
			InBucklingRatio,
			InBucklingStiffness,
			bTrimKinematicConstraints,
			MaxStiffness)
		, DampingRatio(FSolverVec2::ZeroVector)
	{
		Lambdas.Init((FSolverReal)0., Constraints.Num());
		InitColor(InParticles, ParticleOffset, ParticleCount);
	}

	FXPBDBendingConstraints(const FSolverParticles& InParticles,
		int32 ParticleOffset,
		int32 ParticleCount,
		TArray<TVec4<int32>>&& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers,
		const TConstArrayView<FRealSingle>& DampingMultipliers,
		const FSolverVec2& InStiffness,
		const FSolverReal InBucklingRatio,
		const FSolverVec2& InBucklingStiffness,
		const FSolverVec2& InDampingRatio,
		bool bTrimKinematicConstraints = false)
		: Base(
			InParticles,
			ParticleOffset,
			ParticleCount,
			MoveTemp(InConstraints),
			StiffnessMultipliers,
			BucklingStiffnessMultipliers,
			InStiffness,
			InBucklingRatio,
			InBucklingStiffness,
			bTrimKinematicConstraints,
			MaxStiffness)
		, DampingRatio(
			InDampingRatio.ClampAxes((FSolverReal)0., (FSolverReal)1.),
			DampingMultipliers,
			TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
			ParticleOffset,
			ParticleCount)
	{
		Lambdas.Init((FSolverReal)0., Constraints.Num());
		InitColor(InParticles, ParticleOffset, ParticleCount);
	}

	virtual ~FXPBDBendingConstraints() override {}

	void Init(const FSolverParticles& InParticles)
	{ 
		Lambdas.Reset();
		Lambdas.AddZeroed(Constraints.Num());
		FPBDBendingConstraintsBase::Init(InParticles);
	}

	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		if (IsXPBDBendingElementStiffnessMutable(PropertyCollection))
		{
			Stiffness.SetWeightedValue(FSolverVec2(GetWeightedFloatXPBDBendingElementStiffness(PropertyCollection)), MaxStiffness);
		}
		if (IsXPBDBucklingRatioMutable(PropertyCollection))
		{
			BucklingRatio = FMath::Clamp(GetXPBDBucklingRatio(PropertyCollection), (FSolverReal)0., (FSolverReal)1.);
		}
		if (IsXPBDBucklingStiffnessMutable(PropertyCollection))
		{
			BucklingStiffness.SetWeightedValue(FSolverVec2(GetWeightedFloatXPBDBucklingStiffness(PropertyCollection)), MaxStiffness);
		}
		if (IsXPBDBendingElementDampingMutable(PropertyCollection))
		{
			DampingRatio.SetWeightedValue(FSolverVec2(GetWeightedFloatXPBDBendingElementDamping(PropertyCollection)).ClampAxes(MinDamping, MaxDamping));
		}
	}

	void SetProperties(const FSolverVec2& InStiffness, const FSolverReal InBucklingRatio, const FSolverVec2& InBucklingStiffness, const FSolverVec2& InDampingRatio = FSolverVec2::ZeroVector)
	{
		Stiffness.SetWeightedValue(InStiffness, MaxStiffness);
		BucklingRatio = FMath::Clamp(InBucklingRatio, (FSolverReal)0., (FSolverReal)1.);
		BucklingStiffness.SetWeightedValue(InBucklingStiffness, MaxStiffness);
		DampingRatio.SetWeightedValue(InDampingRatio.ClampAxes(MinDamping, MaxDamping));
	}

	// Update stiffness table, as well as the simulation stiffness exponent
	void ApplyProperties(const FSolverReal /*Dt*/, const int32 /*NumIterations*/)
	{
		Stiffness.ApplyXPBDValues(MaxStiffness);
		BucklingStiffness.ApplyXPBDValues(MaxStiffness);
		DampingRatio.ApplyValues();
	}

	void Apply(FSolverParticles& Particles, const FSolverReal Dt) const;

private:
	void InitColor(const FSolverParticles& InParticles, const int32 ParticleOffset, const int32 ParticleCount);
	void ApplyHelper(FSolverParticles& Particles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverReal ExpStiffnessValue, const FSolverReal ExpBucklingValue, const FSolverReal DampingRatioValue) const;

	using Base::Constraints;
	using Base::RestAngles;
	using Base::Stiffness;
	using Base::BucklingStiffness;

	FPBDWeightMap DampingRatio;
	TArray<FSolverReal> DampingMultipliers;
	mutable TArray<FSolverReal> Lambdas;
	TArray<int32> ConstraintsPerColorStartIndex; // Constraints are ordered so each batch is contiguous. This is ColorNum + 1 length so it can be used as start and end.

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDBendingElementStiffness, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDBendingElementDamping, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDBucklingRatio, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDBucklingStiffness, float);
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
