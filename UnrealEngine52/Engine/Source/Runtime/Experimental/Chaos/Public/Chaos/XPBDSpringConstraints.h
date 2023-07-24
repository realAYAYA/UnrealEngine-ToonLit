// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDSpringConstraintsBase.h"
#include "ChaosStats.h"
#include "PBDWeightMap.h"
#include "Chaos/CollectionPropertyFacade.h"

namespace Chaos::Softs
{

// Stiffness is in kg/s^2
UE_DEPRECATED(5.2, "Use FXPBDSpringConstraints::MinStiffness instead.")
static const FSolverReal XPBDSpringMinStiffness = (FSolverReal)1e-4; // Stiffness below this will be considered 0 since all of our calculations are actually based on 1 / stiffness.
UE_DEPRECATED(5.2, "Use FXPBDSpringConstraints::MaxStiffness instead.")
static const FSolverReal XPBDSpringMaxStiffness = (FSolverReal)1e7;

class CHAOS_API FXPBDSpringConstraints : public FPBDSpringConstraintsBase
{
	typedef FPBDSpringConstraintsBase Base;

public:
	static constexpr FSolverReal MinStiffness = (FSolverReal)1e-4; // Stiffness below this will be considered 0 since all of our calculations are actually based on 1 / stiffness.
	static constexpr FSolverReal MaxStiffness = (FSolverReal)1e7;
	static constexpr FSolverReal MinDampingRatio = (FSolverReal)0.;
	static constexpr FSolverReal MaxDampingRatio = (FSolverReal)1000.;

	template<int32 Valence, TEMPLATE_REQUIRES(Valence >= 2 && Valence <= 4)>
	UE_DEPRECATED(5.2, "Use the other constructor instead.")
	FXPBDSpringConstraints(
		const FSolverParticles& Particles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const TArray<TVector<int32, Valence>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const FSolverVec2& InStiffness,
		bool bTrimKinematicConstraints = false)
		: Base(
			Particles,
			ParticleOffset,
			ParticleCount,
			InConstraints,
			StiffnessMultipliers,
			InStiffness,
			bTrimKinematicConstraints,
			MaxStiffness)
		, DampingRatio(FSolverVec2::ZeroVector)
	{
		Lambdas.Init((FSolverReal)0., Constraints.Num());
		InitColor(Particles, ParticleOffset, ParticleCount);
	}

	template<int32 Valence, TEMPLATE_REQUIRES(Valence >= 2 && Valence <= 4)>
	FXPBDSpringConstraints(
		const FSolverParticles& Particles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const TArray<TVector<int32, Valence>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const TConstArrayView<FRealSingle>& DampingMultipliers,
		const FSolverVec2& InStiffness,
		const FSolverVec2& InDampingRatio,
		bool bTrimKinematicConstraints = false)
		: Base(
			Particles,
			ParticleOffset,
			ParticleCount,
			InConstraints,
			StiffnessMultipliers,
			InStiffness,
			bTrimKinematicConstraints,
			MaxStiffness)
		, DampingRatio(
			InDampingRatio.ClampAxes(MinDampingRatio, MaxDampingRatio),
			DampingMultipliers,
			TConstArrayView<TVec2<int32>>(Constraints),
			ParticleOffset,
			ParticleCount)
	{
		Lambdas.Init((FSolverReal)0., Constraints.Num());
		InitColor(Particles, ParticleOffset, ParticleCount);
	}

	virtual ~FXPBDSpringConstraints() override {}

	void Init() const { for (FSolverReal& Lambda : Lambdas) { Lambda = (FSolverReal)0.; } }

	// Update stiffness values
	void SetProperties(const FSolverVec2& InStiffness, const FSolverVec2& InDampingRatio = FSolverVec2::ZeroVector)
	{ 
		Stiffness.SetWeightedValue(InStiffness, MaxStiffness);
		DampingRatio.SetWeightedValue(InDampingRatio.ClampAxes(MinDampingRatio, MaxDampingRatio));
	}
	
	// Update stiffness table, as well as the simulation stiffness exponent
	void ApplyProperties(const FSolverReal Dt, const int32 NumIterations)
	{
		Stiffness.ApplyXPBDValues(MaxStiffness);
		DampingRatio.ApplyValues();
	}

	void Apply(FSolverParticles& Particles, const FSolverReal Dt) const;

private:
	void InitColor(const FSolverParticles& InParticles, const int32 ParticleOffset, const int32 ParticleCount);
	void ApplyHelper(FSolverParticles& Particles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverReal StiffnessValue, const FSolverReal DampingRatioValue) const;

	FSolverVec3 GetDelta(const FSolverParticles& Particles, const FSolverReal Dt, const int32 ConstraintIndex, const FSolverReal StiffnessValue, const FSolverReal DampingRatioValue) const
	{
		const TVec2<int32>& Constraint = Constraints[ConstraintIndex];

		const int32 i1 = Constraint[0];
		const int32 i2 = Constraint[1];

		if (StiffnessValue < MinStiffness || (Particles.InvM(i2) == (FSolverReal)0. && Particles.InvM(i1) == (FSolverReal)0.))
		{
			return FSolverVec3((FSolverReal)0.);
		}

		const FSolverReal CombinedInvMass = Particles.InvM(i2) + Particles.InvM(i1);

		const FSolverReal Damping = DampingRatioValue * 2.f * FMath::Sqrt(StiffnessValue / CombinedInvMass);

		const FSolverVec3& P1 = Particles.P(i1);
		const FSolverVec3& P2 = Particles.P(i2);
		FSolverVec3 Direction = P1 - P2;
		const FSolverReal Distance = Direction.SafeNormalize();
		const FSolverReal Offset = Distance - Dists[ConstraintIndex];

		const FSolverVec3& X1 = Particles.X(i1);
		const FSolverVec3& X2 = Particles.X(i2);

		const FSolverVec3 RelativeVelocityTimesDt = P1 - X1 - P2 + X2;


		FSolverReal& Lambda = Lambdas[ConstraintIndex];
		const FSolverReal Alpha = (FSolverReal)1.f / (StiffnessValue * Dt * Dt);
		const FSolverReal Gamma = Alpha * Damping * Dt;

		const FSolverReal DLambda = (Offset - Alpha * Lambda + Gamma * FSolverVec3::DotProduct(Direction, RelativeVelocityTimesDt)) / (((FSolverReal)1.f + Gamma) * CombinedInvMass + Alpha);
		const FSolverVec3 Delta = DLambda * Direction;
		Lambda += DLambda;

		return Delta;
	}

protected:
	using Base::Stiffness;
	FPBDWeightMap DampingRatio;

private:
	using Base::Constraints;
	using Base::Dists;
	mutable TArray<FSolverReal> Lambdas;
	TArray<int32> ConstraintsPerColorStartIndex; // Constraints are ordered so each batch is contiguous. This is ColorNum + 1 length so it can be used as start and end.
};

class CHAOS_API FXPBDEdgeSpringConstraints final : public FXPBDSpringConstraints
{
public:
	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return IsXPBDEdgeSpringStiffnessEnabled(PropertyCollection, false);
	}

	FXPBDEdgeSpringConstraints(
		const FSolverParticles& Particles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const TArray<TVec3<int32>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const TConstArrayView<FRealSingle>& DampingMultipliers,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints = false)
		: FXPBDSpringConstraints(
			Particles,
			ParticleOffset,
			ParticleCount,
			InConstraints,
			StiffnessMultipliers,
			DampingMultipliers,
			FSolverVec2(GetWeightedFloatXPBDEdgeSpringStiffness(PropertyCollection, MaxStiffness)),
			FSolverVec2(GetWeightedFloatXPBDEdgeSpringDamping(PropertyCollection, MinDampingRatio)),
			bTrimKinematicConstraints)
	{}

	virtual ~FXPBDEdgeSpringConstraints() override = default;

	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		if (IsXPBDEdgeSpringStiffnessMutable(PropertyCollection))
		{
			Stiffness.SetWeightedValue(FSolverVec2(GetWeightedFloatXPBDEdgeSpringStiffness(PropertyCollection)), MaxStiffness);
		}
		if (IsXPBDEdgeSpringDampingMutable(PropertyCollection))
		{
			DampingRatio.SetWeightedValue(FSolverVec2(GetWeightedFloatXPBDEdgeSpringDamping(PropertyCollection)).ClampAxes(MinDampingRatio, MaxDampingRatio));
		}
	}

private:
	using FXPBDSpringConstraints::Stiffness;
	using FXPBDSpringConstraints::DampingRatio;

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDEdgeSpringStiffness, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDEdgeSpringDamping, float);
};

class CHAOS_API FXPBDBendingSpringConstraints : public FXPBDSpringConstraints
{
public:
	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return IsXPBDBendingSpringStiffnessEnabled(PropertyCollection, false);
	}

	FXPBDBendingSpringConstraints(
		const FSolverParticles& Particles,
		int32 ParticleOffset,
		int32 ParticleCount,
		const TArray<TVec2<int32>>& InConstraints,
		const TConstArrayView<FRealSingle>& StiffnessMultipliers,
		const TConstArrayView<FRealSingle>& DampingMultipliers,
		const FCollectionPropertyConstFacade& PropertyCollection,
		bool bTrimKinematicConstraints = false)
		: FXPBDSpringConstraints(
			Particles,
			ParticleOffset,
			ParticleCount,
			InConstraints,
			StiffnessMultipliers,
			DampingMultipliers,
			FSolverVec2(GetWeightedFloatXPBDBendingSpringStiffness(PropertyCollection, MaxStiffness)),
			FSolverVec2(GetWeightedFloatXPBDBendingSpringDamping(PropertyCollection, MinDampingRatio)),
			bTrimKinematicConstraints)
	{}

	virtual ~FXPBDBendingSpringConstraints() override = default;

	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		if (IsXPBDBendingSpringStiffnessMutable(PropertyCollection))
		{
			Stiffness.SetWeightedValue(FSolverVec2(GetWeightedFloatXPBDBendingSpringStiffness(PropertyCollection)), MaxStiffness);
		}
		if (IsXPBDBendingSpringDampingMutable(PropertyCollection))
		{
			DampingRatio.SetWeightedValue(FSolverVec2(GetWeightedFloatXPBDBendingSpringDamping(PropertyCollection)).ClampAxes(MinDampingRatio, MaxDampingRatio));
		}
	}

private:
	using FXPBDSpringConstraints::Stiffness;
	using FXPBDSpringConstraints::DampingRatio;

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDBendingSpringStiffness, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(XPBDBendingSpringDamping, float);
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
