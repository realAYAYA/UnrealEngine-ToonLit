// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosCloth/ChaosClothConstraints.h"
#include "Chaos/PBDSpringConstraints.h"
#include "Chaos/XPBDSpringConstraints.h"
#include "Chaos/PBDBendingConstraints.h"
#include "Chaos/XPBDBendingConstraints.h"
#include "Chaos/PBDAxialSpringConstraints.h"
#include "Chaos/XPBDAxialSpringConstraints.h"
#include "Chaos/PBDVolumeConstraint.h"
#include "Chaos/XPBDLongRangeConstraints.h"
#include "Chaos/PBDSphericalConstraint.h"
#include "Chaos/PBDAnimDriveConstraint.h"
#include "Chaos/PBDShapeConstraints.h"
#include "Chaos/PBDCollisionSpringConstraints.h"
#include "Chaos/PBDTriangleMeshCollisions.h"
#include "Chaos/PBDTriangleMeshIntersections.h"
#include "Chaos/PBDEvolution.h"

namespace Chaos {

FClothConstraints::FClothConstraints()
	: Evolution(nullptr)
	, AnimationPositions(nullptr)
	, OldAnimationPositions(nullptr)
	, AnimationNormals(nullptr)
	, ParticleOffset(0)
	, NumParticles(0)
	, ConstraintInitOffset(INDEX_NONE)
	, ConstraintRuleOffset(INDEX_NONE)
	, PostCollisionConstraintRuleOffset(INDEX_NONE)
	, NumConstraintInits(0)
	, NumConstraintRules(0)
	, NumPostCollisionConstraintRules(0)
{
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FClothConstraints::~FClothConstraints()
{
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FClothConstraints::Initialize(
	Softs::FPBDEvolution* InEvolution,
	const TArray<Softs::FSolverVec3>& InAnimationPositions,
	const TArray<Softs::FSolverVec3>& InOldAnimationPositions,
	const TArray<Softs::FSolverVec3>& InAnimationNormals,
	int32 InParticleOffset,
	int32 InNumParticles)
{
	Evolution = InEvolution;
	AnimationPositions = &InAnimationPositions;
	OldAnimationPositions = &InOldAnimationPositions;
	AnimationNormals = &InAnimationNormals;
	ParticleOffset = InParticleOffset;
	NumParticles = InNumParticles;
}

void FClothConstraints::Enable(bool bEnable)
{
	check(Evolution);
	if (ConstraintInitOffset != INDEX_NONE)
	{
		Evolution->ActivateConstraintInitRange(ConstraintInitOffset, bEnable);
	}
	if (ConstraintRuleOffset != INDEX_NONE)
	{
		Evolution->ActivateConstraintRuleRange(ConstraintRuleOffset, bEnable);
	}
	if (PostCollisionConstraintRuleOffset != INDEX_NONE)
	{
		Evolution->ActivatePostCollisionConstraintRuleRange(PostCollisionConstraintRuleOffset, bEnable);
	}
}

void FClothConstraints::CreateRules()
{
	check(Evolution);
	check(ConstraintInitOffset == INDEX_NONE)
	if (NumConstraintInits)
	{
		ConstraintInitOffset = Evolution->AddConstraintInitRange(NumConstraintInits, false);
	}
	check(ConstraintRuleOffset == INDEX_NONE)
	if (NumConstraintRules)
	{
		ConstraintRuleOffset = Evolution->AddConstraintRuleRange(NumConstraintRules, false);
	}
	check(PostCollisionConstraintRuleOffset == INDEX_NONE);
	if (NumPostCollisionConstraintRules)
	{
		PostCollisionConstraintRuleOffset = Evolution->AddPostCollisionConstraintRuleRange(NumPostCollisionConstraintRules, false);
	}

	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const ConstraintInits = Evolution->ConstraintInits().GetData() + ConstraintInitOffset;
	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const ConstraintRules = Evolution->ConstraintRules().GetData() + ConstraintRuleOffset;
	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const PostCollisionConstraintRules = Evolution->PostCollisionConstraintRules().GetData() + PostCollisionConstraintRuleOffset;

	int32 ConstraintInitIndex = 0;
	int32 ConstraintRuleIndex = 0;
	int32 PostCollisionConstraintRuleIndex = 0;

	if (XEdgeConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				XEdgeConstraints->Init();
				XEdgeConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};

		ConstraintRules[ConstraintRuleIndex++] = 
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				XEdgeConstraints->Apply(Particles, Dt);
			};
	}
	if (EdgeConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				EdgeConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				EdgeConstraints->Apply(Particles, Dt);
			};
	}
	if (XBendingConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				XBendingConstraints->Init();
				XBendingConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				XBendingConstraints->Apply(Particles, Dt);
			};
	}
	if (BendingConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				BendingConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				BendingConstraints->Apply(Particles, Dt);
			};
	}
	if (BendingElementConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				BendingElementConstraints->Init(Particles);
				BendingElementConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				BendingElementConstraints->Apply(Particles, Dt);
			};
	}
	if (XBendingElementConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XBendingElementConstraints->Init(Particles);
			XBendingElementConstraints->ApplyProperties(Dt, Evolution->GetIterations());
		};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XBendingElementConstraints->Apply(Particles, Dt);
		};
	}
	if (XAreaConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				XAreaConstraints->Init();
				XAreaConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				XAreaConstraints->Apply(Particles, Dt);
			};
	}
	if (AreaConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				AreaConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				AreaConstraints->Apply(Particles, Dt);
			};
	}
	if (ThinShellVolumeConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				ThinShellVolumeConstraints->Apply(Particles, Dt);
			};
	}
	if (VolumeConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				VolumeConstraints->Apply(Particles, Dt);
			};
	}
	if (MaximumDistanceConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				MaximumDistanceConstraints->Apply(Particles, Dt);
			};
	}
	if (BackstopConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				BackstopConstraints->Apply(Particles, Dt);
			};
	}
	if (AnimDriveConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				AnimDriveConstraints->ApplyProperties(Dt, Evolution->GetIterations());
			};

		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				AnimDriveConstraints->Apply(Particles, Dt);
			};
	}
	if (ShapeConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				ShapeConstraints->Apply(Particles, Dt);
			};
	}

	if (SelfCollisionInit && SelfCollisionConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal /*Dt*/)
			{
				SelfCollisionInit->Init(Particles);
				SelfCollisionConstraints->Init(Particles, SelfCollisionInit->GetSpatialHash(), SelfCollisionInit->GetVertexGIAColors(), SelfCollisionInit->GetTriangleGIAColors());
			};

		PostCollisionConstraintRules[PostCollisionConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				SelfCollisionConstraints->Apply(Particles, Dt);
			};
	}

	// The following constraints only run once per subframe, so we do their Apply as part of the Init() which modifies P
	// To avoid possible dependency order issues, add them last
	if (SelfCollisionInit && SelfIntersectionConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				SelfIntersectionConstraints->Apply(Particles, SelfCollisionInit->GetContourMinimizationIntersections(), Dt);
			};
	}

	// Long range constraints modify particle P as part of Init. To avoid possible dependency order issues,
	// add them last
	if (LongRangeConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{	
				// Only doing one iteration.
				constexpr int32 NumLRAIterations = 1;
				LongRangeConstraints->ApplyProperties(Dt, NumLRAIterations);
				LongRangeConstraints->Apply(Particles, Dt);  // Run the LRA constraint only once per timestep
			};
	}
	check(ConstraintInitIndex == NumConstraintInits);
	check(ConstraintRuleIndex == NumConstraintRules);
	check(PostCollisionConstraintRuleIndex == NumPostCollisionConstraintRules);
}

void FClothConstraints::SetEdgeConstraints(const TArray<TVec3<int32>>& SurfaceElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, bool bUseXPBDConstraints)
{
	check(Evolution);

	if (bUseXPBDConstraints)
	{
		XEdgeConstraints = MakeShared<Softs::FXPBDSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset, NumParticles,
			SurfaceElements,
			StiffnessMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	else
	{
		EdgeConstraints = MakeShared<Softs::FPBDSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			SurfaceElements,
			StiffnessMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetBendingConstraints(const TArray<TVec2<int32>>& Edges, const TConstArrayView<FRealSingle>& StiffnessMultipliers, bool bUseXPBDConstraints)
{
	check(Evolution);

	if (bUseXPBDConstraints)
	{
		XBendingConstraints = MakeShared<Softs::FXPBDSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset, NumParticles,
			Edges,
			StiffnessMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	else
	{
		BendingConstraints = MakeShared<Softs::FPBDSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			Edges,
			StiffnessMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetBendingConstraints(TArray<TVec4<int32>>&& BendingElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, const TConstArrayView<FRealSingle>& BucklingStiffnessMultipliers, bool bUseXPBDConstraints)
{
	check(Evolution);

	if (bUseXPBDConstraints)
	{
		XBendingElementConstraints = MakeShared<Softs::FXPBDBendingConstraints>(
			Evolution->Particles(),
			ParticleOffset, NumParticles,
			MoveTemp(BendingElements),
			StiffnessMultipliers,
			BucklingStiffnessMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*InBucklingRatio=*/ (Softs::FSolverReal)0.f,
			/*InBucklingStiffnessMultiplier =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	else
	{
		BendingElementConstraints = MakeShared<Softs::FPBDBendingConstraints>(
			Evolution->Particles(),
			ParticleOffset, NumParticles,
			MoveTemp(BendingElements),
			StiffnessMultipliers,
			BucklingStiffnessMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*InBucklingRatio=*/ (Softs::FSolverReal)0.f,
			/*InBucklingStiffnessMultiplier =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);

	}
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetBendingConstraints(TArray<TVec4<int32>>&& BendingElements, Softs::FSolverReal BendingStiffness)
{
	check(Evolution);

	BendingElementConstraints = MakeShared<Softs::FPBDBendingConstraints>(Evolution->Particles(), MoveTemp(BendingElements), BendingStiffness);
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetAreaConstraints(const TArray<TVec3<int32>>& SurfaceElements, const TConstArrayView<FRealSingle>& StiffnessMultipliers, bool bUseXPBDConstraints)
{
	check(Evolution);

	if (bUseXPBDConstraints)
	{
		XAreaConstraints = MakeShared<Softs::FXPBDAxialSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			SurfaceElements,
			StiffnessMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	else
	{
		AreaConstraints = MakeShared<Softs::FPBDAxialSpringConstraints>(
			Evolution->Particles(),
			ParticleOffset,
			NumParticles,
			SurfaceElements,
			StiffnessMultipliers,
			/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
			/*bTrimKinematicConstraints =*/ true);
	}
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetVolumeConstraints(const TArray<TVec2<int32>>& DoubleBendingEdges, Softs::FSolverReal VolumeStiffness)
{
	check(Evolution);

	ThinShellVolumeConstraints = MakeShared<Softs::FPBDSpringConstraints>(
		Evolution->Particles(),
		ParticleOffset,
		NumParticles,
		DoubleBendingEdges,
		TConstArrayView<FRealSingle>(),
		VolumeStiffness,
		/*bTrimKinematicConstraints =*/ true);
	++NumConstraintRules;
}

void FClothConstraints::SetVolumeConstraints(TArray<TVec3<int32>>&& SurfaceElements, Softs::FSolverReal VolumeStiffness)
{
	check(Evolution);
	check(VolumeStiffness > 0.f && VolumeStiffness <= 1.f);

	VolumeConstraints = MakeShared<Softs::FPBDVolumeConstraint>(Evolution->Particles(), MoveTemp(SurfaceElements), VolumeStiffness);
	++NumConstraintRules;
}

void FClothConstraints::SetLongRangeConstraints(
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
	const TConstArrayView<FRealSingle>& TetherStiffnessMultipliers,
	const TConstArrayView<FRealSingle>& TetherScaleMultipliers,
	const Softs::FSolverVec2& TetherScale)
{
	check(Evolution);
	//  Now that we're only doing a single iteration of Long range constraints, and they're more of a fake constraint to jump start our initial guess, it's not clear that using XPBD makes sense here.
	LongRangeConstraints = MakeShared<Softs::FPBDLongRangeConstraints>(
		Evolution->Particles(),
		ParticleOffset,
		NumParticles,
		Tethers,
		TetherStiffnessMultipliers,
		TetherScaleMultipliers,
		/*InStiffness =*/ Softs::FSolverVec2::UnitVector,
		TetherScale);
	++NumConstraintInits;  // Uses init to both update the property tables and apply the constraint
}

void FClothConstraints::SetMaximumDistanceConstraints(const TConstArrayView<FRealSingle>& MaxDistances)
{
	MaximumDistanceConstraints = MakeShared<Softs::FPBDSphericalConstraint>(
		ParticleOffset,
		NumParticles,
		*AnimationPositions,
		MaxDistances);
	++NumConstraintRules;
}

void FClothConstraints::SetBackstopConstraints(const TConstArrayView<FRealSingle>& BackstopDistances, const TConstArrayView<FRealSingle>& BackstopRadiuses, bool bUseLegacyBackstop)
{
	BackstopConstraints = MakeShared<Softs::FPBDSphericalBackstopConstraint>(
		ParticleOffset,
		NumParticles,
		*AnimationPositions,
		*AnimationNormals,
		BackstopRadiuses,
		BackstopDistances,
		bUseLegacyBackstop);
	++NumConstraintRules;
}

void FClothConstraints::SetAnimDriveConstraints(const TConstArrayView<FRealSingle>& AnimDriveStiffnessMultipliers, const TConstArrayView<FRealSingle>& AnimDriveDampingMultipliers)
{
	AnimDriveConstraints = MakeShared<Softs::FPBDAnimDriveConstraint>(
		ParticleOffset,
		NumParticles,
		*AnimationPositions,
		*OldAnimationPositions,
		AnimDriveStiffnessMultipliers,
		AnimDriveDampingMultipliers);
	++NumConstraintInits;  // Uses init to update the property tables
	++NumConstraintRules;
}

void FClothConstraints::SetShapeTargetConstraints(Softs::FSolverReal ShapeTargetStiffness)
{
	// TODO: Review this constraint. Currently does nothing more than the anim drive with less controls
	check(ShapeTargetStiffness > 0.f && ShapeTargetStiffness <= 1.f);

	ShapeConstraints = MakeShared<Softs::FPBDShapeConstraints>(
		ParticleOffset,
		NumParticles,
		*AnimationPositions,
		*AnimationPositions,
		ShapeTargetStiffness);
	++NumConstraintRules;
}

void FClothConstraints::SetSelfCollisionConstraints(const FTriangleMesh& TriangleMesh, TSet<TVec2<int32>>&& DisabledCollisionElements, Softs::FSolverReal SelfCollisionThickness, Softs::FSolverReal SelfCollisionFrictionCoefficient, bool bGlobalIntersectionAnalysis, bool bContourMinimization)
{
	SelfCollisionInit = MakeShared<Softs::FPBDTriangleMeshCollisions>(
		ParticleOffset,
		NumParticles,
		TriangleMesh,
		bGlobalIntersectionAnalysis,
		bContourMinimization);

	SelfCollisionConstraints = MakeShared<Softs::FPBDCollisionSpringConstraints>(
		ParticleOffset,
		NumParticles,
		TriangleMesh,
		AnimationPositions,
		MoveTemp(DisabledCollisionElements),
		SelfCollisionThickness,
		Softs::FPBDCollisionSpringConstraintsBase::BackCompatStiffness,
		SelfCollisionFrictionCoefficient);

	++NumConstraintInits;
	++NumPostCollisionConstraintRules;

	SelfIntersectionConstraints = MakeShared<Softs::FPBDTriangleMeshIntersections>(
		ParticleOffset,
		NumParticles,
		TriangleMesh);
	++NumConstraintInits;
}

void FClothConstraints::SetEdgeProperties(const Softs::FSolverVec2& EdgeStiffness)
{
	if (EdgeConstraints)
	{
		EdgeConstraints->SetProperties(EdgeStiffness);
	}
	if (XEdgeConstraints)
	{
		XEdgeConstraints->SetProperties(EdgeStiffness);
	}
}

void FClothConstraints::SetBendingProperties(const Softs::FSolverVec2& BendingStiffness, Softs::FSolverReal BucklingRatio, const Softs::FSolverVec2& BucklingStiffness)
{
	if (BendingConstraints)
	{
		BendingConstraints->SetProperties(BendingStiffness);
	}
	if (XBendingConstraints)
	{
		XBendingConstraints->SetProperties(BendingStiffness);
	}
	if (BendingElementConstraints)
	{
		BendingElementConstraints->SetProperties(BendingStiffness, BucklingRatio, BucklingStiffness);
	}
	if (XBendingElementConstraints)
	{
		XBendingElementConstraints->SetProperties(BendingStiffness, BucklingRatio, BucklingStiffness);
	}
}

void FClothConstraints::SetAreaProperties(const Softs::FSolverVec2& AreaStiffness)
{
	if (AreaConstraints)
	{
		AreaConstraints->SetProperties(AreaStiffness);
	}
	if (XAreaConstraints)
	{
		XAreaConstraints->SetProperties(AreaStiffness);
	}
}

void FClothConstraints::SetThinShellVolumeProperties(Softs::FSolverReal VolumeStiffness)
{
	if (ThinShellVolumeConstraints)
	{
		ThinShellVolumeConstraints->SetProperties(VolumeStiffness);
	}
}

void FClothConstraints::SetVolumeProperties(Softs::FSolverReal VolumeStiffness)
{
	if (VolumeConstraints)
	{
		VolumeConstraints->SetStiffness(VolumeStiffness);
	}
}

void FClothConstraints::SetLongRangeAttachmentProperties(const Softs::FSolverVec2& TetherStiffness, const Softs::FSolverVec2& TetherScale)
{
	if (LongRangeConstraints)
	{
		LongRangeConstraints->SetStiffness(TetherStiffness);
		LongRangeConstraints->SetScale(TetherScale);
	}
}

void FClothConstraints::SetMaximumDistanceProperties(Softs::FSolverReal MaxDistancesMultiplier)
{
	if (MaximumDistanceConstraints)
	{
		MaximumDistanceConstraints->SetSphereRadiiMultiplier(MaxDistancesMultiplier);
	}
}

void FClothConstraints::SetAnimDriveProperties(const Softs::FSolverVec2& AnimDriveStiffness, const Softs::FSolverVec2& AnimDriveDamping)
{
	if (AnimDriveConstraints)
	{
		AnimDriveConstraints->SetProperties(AnimDriveStiffness, AnimDriveDamping);
	}
}

void FClothConstraints::SetSelfCollisionProperties(Softs::FSolverReal SelfCollisionThickness, Softs::FSolverReal SelfCollisionFrictionCoefficient, bool bGlobalIntersectionAnalysis, bool bContourMinimization)
{
	if (SelfCollisionInit)
	{
		SelfCollisionInit->SetGlobalIntersectionAnalysis(bGlobalIntersectionAnalysis);
		SelfCollisionInit->SetContourMinimization(bContourMinimization);
	}
	if (SelfCollisionConstraints)
	{
		SelfCollisionConstraints->SetThickness(SelfCollisionThickness);
		SelfCollisionConstraints->SetFrictionCoefficient(SelfCollisionFrictionCoefficient);
	}
}

void FClothConstraints::SetBackstopProperties(bool bEnabled, Softs::FSolverReal BackstopDistancesMultiplier)
{
	if (BackstopConstraints)
	{
		BackstopConstraints->SetEnabled(bEnabled);
		BackstopConstraints->SetSphereRadiiMultiplier(BackstopDistancesMultiplier);
	}
}

}  // End namespace Chaos
