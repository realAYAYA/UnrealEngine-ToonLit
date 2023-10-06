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

class FXPBDBendingConstraints final : public FPBDBendingConstraintsBase
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
		int32 InParticleOffset,
		int32 InParticleCount,
		TArray<TVec4<int32>>&& InConstraints,
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints = false)
		: Base(
			InParticles,
			InParticleOffset,
			InParticleCount,
			MoveTemp(InConstraints),
			WeightMaps.FindRef(GetXPBDBendingElementStiffnessString(PropertyCollection, XPBDBendingElementStiffnessName.ToString())),
			WeightMaps.FindRef(GetXPBDBucklingStiffnessString(PropertyCollection, XPBDBucklingStiffnessName.ToString())),
			GetRestAngleMapFromCollection(WeightMaps, PropertyCollection),
			FSolverVec2(GetWeightedFloatXPBDBendingElementStiffness(PropertyCollection, MaxStiffness)),
			(FSolverReal)GetXPBDBucklingRatio(PropertyCollection, 0.f),
			FSolverVec2(GetWeightedFloatXPBDBucklingStiffness(PropertyCollection, MaxStiffness)),
			GetRestAngleValueFromCollection(PropertyCollection),
			(ERestAngleConstructionType)GetXPBDRestAngleType(PropertyCollection, (int32)ERestAngleConstructionType::Use3DRestAngles),
			bTrimKinematicConstraints,
			MaxStiffness)
		, DampingRatio(
			FSolverVec2(GetWeightedFloatXPBDBendingElementDamping(PropertyCollection, MinDamping)).ClampAxes(MinDamping, MaxDamping),
			WeightMaps.FindRef(GetXPBDBendingElementDampingString(PropertyCollection, XPBDBendingElementDampingName.ToString())),
			TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
			ParticleOffset,
			ParticleCount)
		, XPBDBendingElementStiffnessIndex(PropertyCollection)
		, XPBDBendingElementDampingIndex(PropertyCollection)
		, XPBDBucklingRatioIndex(PropertyCollection)
		, XPBDBucklingStiffnessIndex(PropertyCollection)
		, XPBDFlatnessRatioIndex(PropertyCollection)
		, XPBDRestAngleIndex(PropertyCollection)
		, XPBDRestAngleTypeIndex(PropertyCollection)
	{
		Lambdas.Init((FSolverReal)0., Constraints.Num());
		InitColor(InParticles);
	}

	UE_DEPRECATED(5.3, "Use weight map constructor instead.")
	FXPBDBendingConstraints(const FSolverParticles& InParticles,
		int32 InParticleOffset,
		int32 InParticleCount,
		TArray<TVec4<int32>>&& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers,
		const TConstArrayView<FRealSingle>& DampingMultipliers,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints = false)
		: Base(
			InParticles,
			InParticleOffset,
			InParticleCount,
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
		, XPBDBendingElementStiffnessIndex(PropertyCollection)
		, XPBDBendingElementDampingIndex(PropertyCollection)
		, XPBDBucklingRatioIndex(PropertyCollection)
		, XPBDBucklingStiffnessIndex(PropertyCollection)
		, XPBDFlatnessRatioIndex(PropertyCollection)
		, XPBDRestAngleIndex(PropertyCollection)
		, XPBDRestAngleTypeIndex(PropertyCollection)
	{
		Lambdas.Init((FSolverReal)0., Constraints.Num());
		InitColor(InParticles);
	}

	UE_DEPRECATED(5.2, "Use one the other constructors instead.")
	FXPBDBendingConstraints(const FSolverParticles& InParticles,
		int32 InParticleOffset,
		int32 InParticleCount,
		TArray<TVec4<int32>>&& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers,
		const FSolverVec2& InStiffness,
		const FSolverReal InBucklingRatio,
		const FSolverVec2& InBucklingStiffness,
		bool bTrimKinematicConstraints = false)
		: Base(
			InParticles,
			InParticleOffset,
			InParticleCount,
			MoveTemp(InConstraints),
			StiffnessMultipliers,
			BucklingStiffnessMultipliers,
			InStiffness,
			InBucklingRatio,
			InBucklingStiffness,
			bTrimKinematicConstraints,
			MaxStiffness)
		, DampingRatio(FSolverVec2::ZeroVector)
		, XPBDBendingElementStiffnessIndex(ForceInit)
		, XPBDBendingElementDampingIndex(ForceInit)
		, XPBDBucklingRatioIndex(ForceInit)
		, XPBDBucklingStiffnessIndex(ForceInit)
		, XPBDFlatnessRatioIndex(ForceInit)
		, XPBDRestAngleIndex(ForceInit)
		, XPBDRestAngleTypeIndex(ForceInit)
	{
		Lambdas.Init((FSolverReal)0., Constraints.Num());
		InitColor(InParticles);
	}

	FXPBDBendingConstraints(const FSolverParticles& InParticles,
		int32 ParticleOffset,
		int32 ParticleCount,
		TArray<TVec4<int32>>&& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers,
		const TConstArrayView<FRealSingle>& InDampingMultipliers,
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
			InDampingMultipliers,
			TConstArrayView<TVec2<int32>>(ConstraintSharedEdges),
			ParticleOffset,
			ParticleCount)
		, XPBDBendingElementStiffnessIndex(ForceInit)
		, XPBDBendingElementDampingIndex(ForceInit)
		, XPBDBucklingRatioIndex(ForceInit)
		, XPBDBucklingStiffnessIndex(ForceInit)
		, XPBDFlatnessRatioIndex(ForceInit)
		, XPBDRestAngleIndex(ForceInit)
		, XPBDRestAngleTypeIndex(ForceInit)
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

	CHAOS_API void SetProperties(
		const FCollectionPropertyConstFacade& PropertyCollection,
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps);

	UE_DEPRECATED(5.3, "Use SetProperties(const FCollectionPropertyConstFacade&, const TMap<FString, TConstArrayView<FRealSingle>>&, FSolverReal) instead.")
	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		SetProperties(PropertyCollection, TMap<FString, TConstArrayView<FRealSingle>>());
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

	CHAOS_API void Apply(FSolverParticles& Particles, const FSolverReal Dt) const;

	const TArray<int32>& GetConstraintsPerColorStartIndex() const { return ConstraintsPerColorStartIndex; }

private:
	CHAOS_API void InitColor(const FSolverParticles& InParticles);
	CHAOS_API void ApplyHelper(FSolverParticles& Particles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverReal ExpStiffnessValue, const FSolverReal ExpBucklingValue, const FSolverReal DampingRatioValue) const;

	TConstArrayView<FRealSingle> GetRestAngleMapFromCollection(
		const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
		const FCollectionPropertyConstFacade& PropertyCollection)
	{
		const ERestAngleConstructionType ConstructionType = (ERestAngleConstructionType)GetXPBDRestAngleType(PropertyCollection, (int32)ERestAngleConstructionType::Use3DRestAngles);

		switch (ConstructionType)
		{
		default:
		case ERestAngleConstructionType::Use3DRestAngles:
			return TConstArrayView<FRealSingle>(); // Unused
		case ERestAngleConstructionType::FlatnessRatio:
			return WeightMaps.FindRef(GetXPBDFlatnessRatioString(PropertyCollection, XPBDFlatnessRatioName.ToString()));
		case ERestAngleConstructionType::ExplicitRestAngles:
			return WeightMaps.FindRef(GetXPBDRestAngleString(PropertyCollection, XPBDRestAngleName.ToString()));
		}
	}

	FSolverVec2 GetRestAngleValueFromCollection(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		const ERestAngleConstructionType ConstructionType = (ERestAngleConstructionType)GetXPBDRestAngleType(PropertyCollection, (int32)ERestAngleConstructionType::Use3DRestAngles);

		switch (ConstructionType)
		{
		default:
		case ERestAngleConstructionType::Use3DRestAngles:
			return FSolverVec2(0.f); // Unused
		case ERestAngleConstructionType::FlatnessRatio:
			return FSolverVec2(GetWeightedFloatXPBDFlatnessRatio(PropertyCollection, 0.f));
		case ERestAngleConstructionType::ExplicitRestAngles:
			return FSolverVec2(GetWeightedFloatXPBDRestAngle(PropertyCollection, 0.f));
		}
	}

	using Base::Constraints;
	using Base::ParticleOffset;
	using Base::ParticleCount;
	using Base::RestAngles;
	using Base::Stiffness;
	using Base::BucklingStiffness;

	FPBDWeightMap DampingRatio;
	mutable TArray<FSolverReal> Lambdas;
	TArray<int32> ConstraintsPerColorStartIndex; // Constraints are ordered so each batch is contiguous. This is ColorNum + 1 length so it can be used as start and end.

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDBendingElementStiffness, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDBendingElementDamping, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDBucklingRatio, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDBucklingStiffness, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDFlatnessRatio, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDRestAngle, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDRestAngleType, int32);
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
