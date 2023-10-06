// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if !COMPILE_WITHOUT_UNREAL_SUPPORT
#include "Chaos/PBDCollisionSpringConstraintsBase.h"
#include "Chaos/CollectionPropertyFacade.h"

namespace Chaos::Softs
{

class FPBDCollisionSpringConstraints : public FPBDCollisionSpringConstraintsBase
{
	typedef FPBDCollisionSpringConstraintsBase Base;
	using Base::Barys;
	using Base::Constraints;

public:
	static constexpr FSolverReal MinFrictionCoefficient = (FSolverReal)0.;
	static constexpr FSolverReal MaxFrictionCoefficient = (FSolverReal)10.;

	static bool IsEnabled(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		return IsSelfCollisionStiffnessEnabled(PropertyCollection, false);
	}

	FPBDCollisionSpringConstraints(
		const int32 InOffset,
		const int32 InNumParticles,
		const FTriangleMesh& InTriangleMesh,
		const TArray<FSolverVec3>* InRestPositions,
		TSet<TVec2<int32>>&& InDisabledCollisionElements,
		const FCollectionPropertyConstFacade& PropertyCollection)
		: Base(
			InOffset,
			InNumParticles,
			InTriangleMesh,
			InRestPositions,
			MoveTemp(InDisabledCollisionElements),
			(FSolverReal)FMath::Max(GetSelfCollisionThickness(PropertyCollection, Base::BackCompatThickness), 0.f),
			(FSolverReal)FMath::Clamp(GetSelfCollisionStiffness(PropertyCollection, Base::BackCompatStiffness), 0.f, 1.f),
			FMath::Clamp((FSolverReal)GetSelfCollisionFriction(PropertyCollection, Base::BackCompatFrictionCoefficient), MinFrictionCoefficient, MaxFrictionCoefficient))
		, SelfCollisionThicknessIndex(PropertyCollection)
		, SelfCollisionStiffnessIndex(PropertyCollection)
		, SelfCollisionFrictionIndex(PropertyCollection)
	{}

	FPBDCollisionSpringConstraints(
		const int32 InOffset,
		const int32 InNumParticles,
		const FTriangleMesh& InTriangleMesh,
		const TArray<FSolverVec3>* InRestPositions,
		TSet<TVec2<int32>>&& InDisabledCollisionElements,
		const FSolverReal InThickness = Base::BackCompatThickness,
		const FSolverReal InStiffness = Base::BackCompatStiffness,
		const FSolverReal InFrictionCoefficient = Base::BackCompatFrictionCoefficient)
		: Base(
			InOffset,
			InNumParticles,
			InTriangleMesh,
			InRestPositions,
			MoveTemp(InDisabledCollisionElements),
			InThickness,
			InStiffness,
			InFrictionCoefficient)
		, SelfCollisionThicknessIndex(ForceInit)
		, SelfCollisionStiffnessIndex(ForceInit)
		, SelfCollisionFrictionIndex(ForceInit)
	{}

	virtual ~FPBDCollisionSpringConstraints() override {}

	using Base::Init;

	void SetProperties(const FCollectionPropertyConstFacade& PropertyCollection)
	{
		if (IsSelfCollisionThicknessMutable(PropertyCollection))
		{
			Thickness = (FSolverReal)FMath::Max(GetSelfCollisionThickness(PropertyCollection), 0.f);
		}
		if (IsSelfCollisionStiffnessMutable(PropertyCollection))
		{
			Stiffness = (FSolverReal)FMath::Clamp(GetSelfCollisionStiffness(PropertyCollection), 0.f, 1.f);
		}
		if (IsSelfCollisionFrictionMutable(PropertyCollection))
		{
			FrictionCoefficient = FMath::Clamp((FSolverReal)GetSelfCollisionFriction(PropertyCollection), MinFrictionCoefficient, MaxFrictionCoefficient);
		}
	}

	void Apply(FSolverParticles& Particles, const FSolverReal Dt, const int32 ConstraintIndex) const
	{
		const int32 i = ConstraintIndex;
		const TVector<int32, 4>& Constraint = Constraints[i];
		const int32 i1 = Constraint[0];
		const int32 i2 = Constraint[1];
		const int32 i3 = Constraint[2];
		const int32 i4 = Constraint[3];
		const FSolverVec3 Delta = Base::GetDelta(Particles, i);
		if (Particles.InvM(i1) > 0)
		{
			Particles.P(i1) += Particles.InvM(i1) * Delta;
		}
		if (Particles.InvM(i2) > (FSolverReal)0.)
		{
			Particles.P(i2) -= Particles.InvM(i2) * Barys[i][0] * Delta;
		}
		if (Particles.InvM(i3) > (FSolverReal)0.)
		{
			Particles.P(i3) -= Particles.InvM(i3) * Barys[i][1] * Delta;
		}
		if (Particles.InvM(i4) > (FSolverReal)0.)
		{
			Particles.P(i4) -= Particles.InvM(i4) * Barys[i][2] * Delta;
		}
	}

	void Apply(FSolverParticles& InParticles, const FSolverReal Dt) const
	{
		for (int32 i = 0; i < Constraints.Num(); ++i)
		{
			Apply(InParticles, Dt, i);
		}
	}

	void Apply(FSolverParticles& InParticles, const FSolverReal Dt, const TArray<int32>& InConstraintIndices) const
	{
		for (int32 i : InConstraintIndices)
		{
			Apply(InParticles, Dt, i);
		}
	}

private:
	using Base::Thickness;
	using Base::Stiffness;
	using Base::FrictionCoefficient;

	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(SelfCollisionThickness, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(SelfCollisionStiffness, float);
	UE_CHAOS_DECLARE_PROPERTYCOLLECTION_NAME(SelfCollisionFriction, float);
};

}  // End namespace Chaos::Softs

#endif
