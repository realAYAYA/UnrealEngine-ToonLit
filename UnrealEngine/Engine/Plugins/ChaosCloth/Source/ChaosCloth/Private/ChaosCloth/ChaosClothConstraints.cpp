// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosCloth/ChaosClothConstraints.h"
#include "ChaosCloth/ChaosWeightMapTarget.h"
#include "ChaosCloth/ChaosClothingPatternData.h"
#include "Chaos/PBDSpringConstraints.h"
#include "Chaos/XPBDSpringConstraints.h"
#include "Chaos/XPBDStretchBiasElementConstraints.h"
#include "Chaos/XPBDAnisotropicSpringConstraints.h"
#include "Chaos/PBDBendingConstraints.h"
#include "Chaos/XPBDBendingConstraints.h"
#include "Chaos/XPBDAnisotropicBendingConstraints.h"
#include "Chaos/PBDAxialSpringConstraints.h"
#include "Chaos/XPBDAxialSpringConstraints.h"
#include "Chaos/PBDVolumeConstraint.h"
#include "Chaos/XPBDLongRangeConstraints.h"
#include "Chaos/SoftsMultiResConstraints.h"
#include "Chaos/PBDSphericalConstraint.h"
#include "Chaos/PBDAnimDriveConstraint.h"
#include "Chaos/PBDShapeConstraints.h"
#include "Chaos/PBDCollisionSpringConstraints.h"
#include "Chaos/PBDSelfCollisionSphereConstraints.h"
#include "Chaos/PBDTriangleMeshCollisions.h"
#include "Chaos/PBDTriangleMeshIntersections.h"
#include "Chaos/PBDEvolution.h"
#include "Chaos/PBDSoftBodyCollisionConstraint.h"
#include "Chaos/SoftsExternalForces.h"
#include "Chaos/SoftsEvolution.h"
#include "Chaos/VelocityField.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "Chaos/Deformable/GaussSeidelMainConstraint.h"
#include "Chaos/Deformable/GaussSeidelCorotatedCodimensionalConstraints.h"
#include "PhysicsProxy/PerSolverFieldSystem.h"
#include "Utils/ClothingMeshUtils.h"
#include "HAL/IConsoleManager.h"

namespace Chaos {

namespace ClothingSimulationClothConsoleVariables
{
// These are defined in ChaosClothingSimulationCloth.cpp
extern TAutoConsoleVariable<bool> CVarLegacyDisablesAccurateWind;
extern TAutoConsoleVariable<float> CVarGravityMultiplier;
}

bool bEnableGS = false;

#if !UE_BUILD_SHIPPING
    static FAutoConsoleVariableRef CVarClothbEnableGS(TEXT("p.Chaos.Cloth.EnableGaussSeidel"), bEnableGS, TEXT("Use Gauss Seidel constraints instead of XPBD [def: false]"));

	bool bDisplayResidual = false;

	static FAutoConsoleVariableRef CVarClothbDisplayResidual(TEXT("p.Chaos.Cloth.DisplayResidual"), bDisplayResidual, TEXT("Diplay residual norms for the first 10 iters [def: false]"));

	int32 MaxResidualIters = 10;

	static FAutoConsoleVariableRef CVarClothMaxResidualIters(TEXT("p.Chaos.Cloth.MaxResidualIters"), MaxResidualIters, TEXT("Max number of iterations to diaplay residuals [def: 10]"));

	bool bWriteResidual2File = false;

	static FAutoConsoleVariableRef CVarClothbWriteResidual2File(TEXT("p.Chaos.Cloth.WriteResidual2File"), bWriteResidual2File, TEXT("Write residual to file [def: false]"));

	bool bReplaceBiasElementsWithCorotatedCod = false;

	static FAutoConsoleVariableRef CVarClothReplaceBiasElementsWithCorotatedCod(TEXT("p.Chaos.Cloth.ReplaceBiasElementsWithCorotatedCod"), bReplaceBiasElementsWithCorotatedCod, TEXT("Replace existing aniso bias element constraint with gauss seidel corotated codimensional [def: false]"));

	Softs::FSolverReal YoungsModulus = 10000.f;

	static FAutoConsoleVariableRef CVarClothYoungsModulus(TEXT("p.Chaos.Cloth.YoungsModulus"), YoungsModulus, TEXT("Youngs modulus [def: 1e4]"));

	bool bClothDoQuasistatics = false;

	static FAutoConsoleVariableRef CVarClothDoQuasistatics(TEXT("p.Chaos.Cloth.DoQuasistatics"), bClothDoQuasistatics, TEXT("Do cloth quasistatics [def: false]"));

	bool bEnableCG = false;

	static FAutoConsoleVariableRef CVarClothEnableCG(TEXT("p.Chaos.Cloth.EnableCG"), bEnableCG, TEXT("Use conjugate gradient instead of nonlinear gauss seidel [def: false]"));

	bool bMakeSandwich = false;

	static FAutoConsoleVariableRef CVarClothMakeSandwich(TEXT("p.Chaos.Cloth.MakeSandwich"), bMakeSandwich, TEXT("MakeSandwich solver of xpbd - Newton/GS [def: false]"));

	int32 UpperBreadIters = 3;

	static FAutoConsoleVariableRef CVarClothUpperBreadIters(TEXT("p.Chaos.Cloth.UpperBreadIters"), UpperBreadIters, TEXT("Upper Bread Iters for the sandwich solver [def: 3]"));

	int32 MiddleBreadIters = 3;

	static FAutoConsoleVariableRef CVarClothMiddleBreadIters(TEXT("p.Chaos.Cloth.MiddleBreadIters"), MiddleBreadIters, TEXT("Middle Bread Iters for the sandwich solver [def: 3]"));

	bool bWriteFinalResiduals = false;

	static FAutoConsoleVariableRef CVarClothWriteFinalResiduals(TEXT("p.Chaos.Cloth.WriteFinalResiduals"), bWriteFinalResiduals, TEXT("Write final residuals at each timestep to a file [def: false]"));

	bool bUseSOR = true;

	static FAutoConsoleVariableRef CVarClothUseSOR(TEXT("p.Chaos.Cloth.UseSOR"), bUseSOR, TEXT("Use SOR acceleration for Gauss Seidel [def: true]"));

	Softs::FSolverReal SOROmega = 1.7f;

	static FAutoConsoleVariableRef CVarClothSOROmega(TEXT("p.Chaos.Cloth.SOROmega"), SOROmega, TEXT("SOR omega coefficient for acceleration [def: 1.7]"));

#endif

class FClothConstraints::FRuleCreator
{
public:
	explicit FRuleCreator(FClothConstraints* InConstraints)
		: Constraints(InConstraints)
	{
		check(Constraints);
		Constraints->Evolution->AllocatePreSubstepParallelInitRange(Constraints->ParticleRangeId, Constraints->NumPreSubstepInits);
		Constraints->Evolution->AllocatePBDExternalForceRulesRange(Constraints->ParticleRangeId, Constraints->NumExternalForceRules);
		Constraints->Evolution->AllocatePostInitialGuessParallelInitRange(Constraints->ParticleRangeId, Constraints->NumConstraintInits);
		Constraints->Evolution->AllocatePreSubstepConstraintRulesRange(Constraints->ParticleRangeId, Constraints->NumPreSubstepConstraintRules);
		Constraints->Evolution->AllocatePerIterationPBDConstraintRulesRange(Constraints->ParticleRangeId, Constraints->NumConstraintRules);
		Constraints->Evolution->AllocatePerIterationCollisionPBDConstraintRulesRange(Constraints->ParticleRangeId, Constraints->NumCollisionConstraintRules);
		Constraints->Evolution->AllocatePerIterationPostCollisionsPBDConstraintRulesRange(Constraints->ParticleRangeId, Constraints->NumPostCollisionConstraintRules);
		Constraints->Evolution->AllocateUpdateLinearSystemRulesRange(Constraints->ParticleRangeId, Constraints->NumUpdateLinearSystemRules);
		Constraints->Evolution->AllocateUpdateLinearSystemCollisionsRulesRange(Constraints->ParticleRangeId, Constraints->NumUpdateLinearSystemCollisionsRules);
		Constraints->Evolution->AllocatePostSubstepConstraintRulesRange(Constraints->ParticleRangeId, Constraints->NumPostprocessingConstraintRules);

		PreSubstepParallelInits = Constraints->Evolution->GetPreSubstepParallelInitRange(Constraints->ParticleRangeId);
		ExternalForceRules = Constraints->Evolution->GetPBDExternalForceRulesRange(Constraints->ParticleRangeId);
		PostInitialGuessParallelInits = Constraints->Evolution->GetPostInitialGuessParallelInitRange(Constraints->ParticleRangeId);
		PreSubstepConstraintRules = Constraints->Evolution->GetPreSubstepConstraintRulesRange(Constraints->ParticleRangeId);
		PerIterationConstraintRules = Constraints->Evolution->GetPerIterationPBDConstraintRulesRange(Constraints->ParticleRangeId);
		PerIterationCollisionConstraintRules = Constraints->Evolution->GetPerIterationCollisionPBDConstraintRulesRange(Constraints->ParticleRangeId);
		PerIterationPostCollisionsConstraintRules = Constraints->Evolution->GetPerIterationPostCollisionsPBDConstraintRulesRange(Constraints->ParticleRangeId);
		UpdateLinearSystemRules = Constraints->Evolution->GetUpdateLinearSystemRulesRange(Constraints->ParticleRangeId);
		UpdateLinearSystemCollisionsRules = Constraints->Evolution->GetUpdateLinearSystemCollisionsRulesRange(Constraints->ParticleRangeId);
		PostSubstepConstraintRules = Constraints->Evolution->GetPostSubstepConstraintRulesRange(Constraints->ParticleRangeId);
	}

	~FRuleCreator()
	{
		if (Constraints)
		{
			check(PreSubstepInitsIndex == Constraints->NumPreSubstepInits);
			check(ExternalForceRulesIndex == Constraints->NumExternalForceRules);
			check(PostInitialGuessInitsIndex == Constraints->NumConstraintInits);
			check(PreSubstepConstraintRulesIndex == Constraints->NumPreSubstepConstraintRules);
			check(ConstraintRuleIndex == Constraints->NumConstraintRules);
			check(CollisionConstraintRulesIndex == Constraints->NumCollisionConstraintRules);
			check(PostCollisionConstraintRulesIndex == Constraints->NumPostCollisionConstraintRules);
			check(UpdateLinearSystemRulesIndex == Constraints->NumUpdateLinearSystemRules);
			check(UpdateLinearSystemCollisionsRulesIndex == Constraints->NumUpdateLinearSystemCollisionsRules);
			check(PostSubstepConstraintRulesIndex == Constraints->NumPostprocessingConstraintRules);
		}
	}

	void PreSubstepParallelInitRule(Softs::FEvolution::ParallelInitFunc&& Rule)
	{
		PreSubstepParallelInits[PreSubstepInitsIndex++] = MoveTemp(Rule);
	}

	void AddExternalForceRule(Softs::FEvolution::PBDConstraintRuleFunc&& Rule)
	{
		ExternalForceRules[ExternalForceRulesIndex++] = MoveTemp(Rule);
	}

	void AddPostInitialGuessParallelInitRule(Softs::FEvolution::ParallelInitFunc&& Rule)
	{
		PostInitialGuessParallelInits[PostInitialGuessInitsIndex++] = MoveTemp(Rule);
	}

	void AddPreSubstepConstraintRule(Softs::FEvolution::ConstraintRuleFunc&& Rule)
	{
		PreSubstepConstraintRules[PreSubstepConstraintRulesIndex++] = MoveTemp(Rule);
	}

	void AddPerIterationPBDConstraintRule(Softs::FEvolution::PBDConstraintRuleFunc&& Rule)
	{
		PerIterationConstraintRules[ConstraintRuleIndex++] = MoveTemp(Rule);
	}

	void AddPerIterationCollisionPBDConstraintRule(Softs::FEvolution::PBDCollisionConstraintRuleFunc&& Rule)
	{
		PerIterationCollisionConstraintRules[CollisionConstraintRulesIndex++] = MoveTemp(Rule);
	}

	void AddPerIterationPostCollisionsPBDConstraintRule(Softs::FEvolution::PBDConstraintRuleFunc&& Rule)
	{
		PerIterationPostCollisionsConstraintRules[PostCollisionConstraintRulesIndex++] = MoveTemp(Rule);
	}

	void AddUpdateLinearSystemRule(Softs::FEvolution::UpdateLinearSystemFunc&& Rule)
	{
		UpdateLinearSystemRules[UpdateLinearSystemRulesIndex++] = MoveTemp(Rule);
	}

	void AddUpdateLinearSystemCollisionsRule(Softs::FEvolution::UpdateLinearSystemCollisionsFunc&& Rule)
	{
		UpdateLinearSystemCollisionsRules[UpdateLinearSystemCollisionsRulesIndex++] = MoveTemp(Rule);
	}

	void AddPostSubstepConstraintRule(Softs::FEvolution::ConstraintRuleFunc&& Rule)
	{
		PostSubstepConstraintRules[PostSubstepConstraintRulesIndex++] = MoveTemp(Rule);
	}

	template<typename ConstraintType>
	void AddExternalForceRule_Apply(ConstraintType* const Constraint)
	{
		AddExternalForceRule(
			[Constraint](Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt)
		{
			Constraint->Apply(Particles, Dt);
		});
	}

	template<typename ConstraintType>
	void AddUpdateLinearSystemRule_UpdateLinearSystem(ConstraintType* const Constraint)
	{
		AddUpdateLinearSystemRule([Constraint](const Softs::FSolverParticlesRange& Particles, const FSolverReal Dt, Softs::FEvolutionLinearSystem& LinearSystem)
		{
			Constraint->UpdateLinearSystem(Particles, Dt, LinearSystem);
		});
	}

	template<bool bInitParticles = false, typename ConstraintType>
	void AddXPBDParallelInitRule(ConstraintType* const Constraint)
	{
		if constexpr (bInitParticles)
		{
			AddPostInitialGuessParallelInitRule(
				[Constraint, Evolution = Constraints->Evolution](const Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt, const Softs::ESolverMode SolverMode)
			{
				Constraint->ApplyProperties(Dt, Evolution->GetIterations());
				if ((SolverMode & Softs::ESolverMode::PBD) != Softs::ESolverMode::None)\
				{
					Constraint->Init(Particles);
				}
			});
		}
		else
		{
			AddPostInitialGuessParallelInitRule(
				[Constraint, Evolution = Constraints->Evolution](const Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt, const Softs::ESolverMode SolverMode)
			{
				Constraint->ApplyProperties(Dt, Evolution->GetIterations());
				if ((SolverMode & Softs::ESolverMode::PBD) != Softs::ESolverMode::None)\
				{
					Constraint->Init();
				}
			});
		}
	}

	template<typename ConstraintType>
	void AddPBDParallelInitRule(ConstraintType* const Constraint)
	{
		AddPostInitialGuessParallelInitRule(
			[Constraint, Evolution = Constraints->Evolution](const Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt, const Softs::ESolverMode SolverMode)
		{
			if ((SolverMode & Softs::ESolverMode::PBD) != Softs::ESolverMode::None)
			{
				Constraint->ApplyProperties(Dt, Evolution->GetIterations()); 
			}
		});
	}

	template<bool bPostCollisions, typename ConstraintType>
	void AddPerIterationPBDConstraintRule_Apply(ConstraintType* const Constraint)
	{
		if constexpr (bPostCollisions)
		{
			AddPerIterationPostCollisionsPBDConstraintRule(
				[Constraint](Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt)
			{
				Constraint->Apply(Particles, Dt);
			});
		}
		else
		{
			AddPerIterationPBDConstraintRule(
				[Constraint](Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt)
			{
				Constraint->Apply(Particles, Dt);
			});
		}
	}

	template<typename ConstraintType>
	void AddPerIterationCollisionPBDConstraintRule_Apply(ConstraintType* const Constraint)
	{
		AddPerIterationCollisionPBDConstraintRule(
			[Constraint](Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt, const TArray<Softs::FSolverCollisionParticlesRange>& CollisionParticles)
		{
			Constraint->Apply(Particles, Dt, CollisionParticles);
		}
		);
	}

	template<typename ConstraintType>
	void AddUpdateLinearSystemCollisionsRule_UpdateLinearSystem(ConstraintType* const Constraint)
	{
		AddUpdateLinearSystemCollisionsRule(
			[Constraint](const Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt, const TArray<Softs::FSolverCollisionParticlesRange>& CollisionParticles, Softs::FEvolutionLinearSystem& LinearSystem)
		{
			Constraint->UpdateLinearSystem(Particles, Dt, CollisionParticles, LinearSystem);
		}
		);
	}

private:

	FClothConstraints* const Constraints = nullptr;
	TArrayView<Softs::FEvolution::ParallelInitFunc> PreSubstepParallelInits;
	TArrayView<Softs::FEvolution::PBDConstraintRuleFunc> ExternalForceRules;
	TArrayView<Softs::FEvolution::ParallelInitFunc> PostInitialGuessParallelInits;
	TArrayView<Softs::FEvolution::ConstraintRuleFunc> PreSubstepConstraintRules;
	TArrayView<Softs::FEvolution::PBDConstraintRuleFunc> PerIterationConstraintRules;
	TArrayView<Softs::FEvolution::PBDCollisionConstraintRuleFunc> PerIterationCollisionConstraintRules;
	TArrayView<Softs::FEvolution::PBDConstraintRuleFunc> PerIterationPostCollisionsConstraintRules;
	TArrayView<Softs::FEvolution::UpdateLinearSystemFunc> UpdateLinearSystemRules;
	TArrayView<Softs::FEvolution::UpdateLinearSystemCollisionsFunc> UpdateLinearSystemCollisionsRules;
	TArrayView<Softs::FEvolution::ConstraintRuleFunc> PostSubstepConstraintRules;

	int32 PreSubstepInitsIndex = 0;
	int32 ExternalForceRulesIndex = 0;
	int32 PostInitialGuessInitsIndex = 0;
	int32 PreSubstepConstraintRulesIndex = 0;
	int32 ConstraintRuleIndex = 0;
	int32 CollisionConstraintRulesIndex = 0;
	int32 PostCollisionConstraintRulesIndex = 0;
	int32 UpdateLinearSystemRulesIndex = 0;
	int32 UpdateLinearSystemCollisionsRulesIndex = 0;
	int32 PostSubstepConstraintRulesIndex = 0;
};

FClothConstraints::FClothConstraints()
	: Evolution(nullptr), PBDEvolution(nullptr)
	, AnimationPositions(nullptr)
	, AnimationNormals(nullptr)
	, AnimationVelocities(nullptr)
	, ParticleOffset(0)
	, ParticleRangeId(0)
	, NumParticles(0)
	, NumConstraintInits(0)
	, NumConstraintRules(0)
	, NumPostCollisionConstraintRules(0)
	, NumPostprocessingConstraintRules(0)

	, PerSolverField(nullptr)
	, Normals(nullptr)
	, LastSubframeCollisionTransformsCCD(nullptr)
	, CollisionParticleCollided(nullptr)
	, CollisionContacts(nullptr)
	, CollisionNormals(nullptr)
	, CollisionPhis(nullptr)
	, NumPreSubstepInits(0)
	, NumExternalForceRules(0)
	, NumPreSubstepConstraintRules(0)
	, NumCollisionConstraintRules(0)
	, NumUpdateLinearSystemRules(0)
	, NumUpdateLinearSystemCollisionsRules(0)

	, OldAnimationPositions_Deprecated(nullptr)
	, ConstraintInitOffset(INDEX_NONE)
	, ConstraintRuleOffset(INDEX_NONE)
	, PostCollisionConstraintRuleOffset(INDEX_NONE)
	, PostprocessingConstraintRuleOffset(INDEX_NONE)
{
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FClothConstraints::~FClothConstraints()
{
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FClothConstraints::Initialize(
	Softs::FEvolution* InEvolution,
	FPerSolverFieldSystem* InPerSolverField,
	const TArray<Softs::FSolverVec3>& InInterpolatedAnimationPositions,
	const TArray<Softs::FSolverVec3>& InInterpolatedAnimationNormals,
	const TArray<Softs::FSolverVec3>& InAnimationVelocities,
	const TArray<Softs::FSolverVec3>& InNormals,
	const TArray<Softs::FSolverRigidTransform3>& InLastSubframeCollisionTransformsCCD,
	TArray<bool>& InCollisionParticleCollided,
	TArray<Softs::FSolverVec3>& InCollisionContacts,
	TArray<Softs::FSolverVec3>& InCollisionNormals,
	TArray<Softs::FSolverReal>& InCollisionPhis,
	int32 InParticleRangeId)
{
	PBDEvolution = nullptr;
	Evolution = InEvolution;
	PerSolverField = InPerSolverField;
	AnimationPositions = &InInterpolatedAnimationPositions;
	AnimationNormals = &InInterpolatedAnimationNormals;
	AnimationVelocities = &InAnimationVelocities;
	Normals = &InNormals;
	LastSubframeCollisionTransformsCCD = &InLastSubframeCollisionTransformsCCD;
	CollisionParticleCollided = &InCollisionParticleCollided;
	CollisionContacts = &InCollisionContacts;
	CollisionNormals = &InCollisionNormals;
	CollisionPhis = &InCollisionPhis;
	ParticleOffset = 0;
	ParticleRangeId = InParticleRangeId;
	NumParticles = Evolution->GetSoftBodyParticleNum(ParticleRangeId);
}

void FClothConstraints::Initialize(
	Softs::FPBDEvolution* InEvolution,
	const TArray<Softs::FSolverVec3>& InInterpolatedAnimationPositions,
	const TArray<Softs::FSolverVec3>& /*InOldAnimationPositions*/, // deprecated
	const TArray<Softs::FSolverVec3>& InInterpolatedAnimationNormals,
	const TArray<Softs::FSolverVec3>& InAnimationVelocities,
	int32 InParticleOffset,
	int32 InNumParticles)
{
	Evolution = nullptr;
	PBDEvolution = InEvolution;
	AnimationPositions = &InInterpolatedAnimationPositions;
	OldAnimationPositions_Deprecated = nullptr;
	AnimationNormals = &InInterpolatedAnimationNormals;
	AnimationVelocities = &InAnimationVelocities;
	ParticleOffset = InParticleOffset;
	ParticleRangeId = InParticleOffset;
	NumParticles = InNumParticles;
}

void FClothConstraints::Enable(bool bEnable)
{
	if (ConstraintInitOffset != INDEX_NONE)
	{
		check(PBDEvolution);
		PBDEvolution->ActivateConstraintInitRange(ConstraintInitOffset, bEnable);
	}
	if (ConstraintRuleOffset != INDEX_NONE)
	{
		check(PBDEvolution);
		PBDEvolution->ActivateConstraintRuleRange(ConstraintRuleOffset, bEnable);
	}
	if (PostCollisionConstraintRuleOffset != INDEX_NONE)
	{
		check(PBDEvolution);
		PBDEvolution->ActivatePostCollisionConstraintRuleRange(PostCollisionConstraintRuleOffset, bEnable);
	}
	if (PostprocessingConstraintRuleOffset != INDEX_NONE)
	{
		check(PBDEvolution);
		PBDEvolution->ActivateConstraintPostprocessingsRange(PostprocessingConstraintRuleOffset, bEnable);
	}
}

void FClothConstraints::AddRules(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const FTriangleMesh& TriangleMesh,
	const TArray<TConstArrayView<FRealSingle>>& WeightMapArray,
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
	Softs::FSolverReal MeshScale,
	bool bEnabled)
{
	// Build new weight map container from the old legacy weight map enum
	TMap<FString, TConstArrayView<FRealSingle>> WeightMaps;

	const UEnum* const ChaosWeightMapTargetEnum = StaticEnum<EChaosWeightMapTarget>();
	const int32 NumWeightMaps = (int32)ChaosWeightMapTargetEnum->GetMaxEnumValue() + 1;

	WeightMaps.Reserve(NumWeightMaps);

	for (int32 EnumIndex = 0; EnumIndex < ChaosWeightMapTargetEnum->NumEnums(); ++EnumIndex)
	{
		const int32 TargetIndex = (int32)ChaosWeightMapTargetEnum->GetValueByIndex(EnumIndex);
		const FString WeightMapName = ChaosWeightMapTargetEnum->GetNameByIndex(EnumIndex).ToString();

		WeightMaps.Add(WeightMapName, WeightMapArray[TargetIndex]);
	}

	// Call new AddRules function
	AddRules(ConfigProperties, TriangleMesh, nullptr, WeightMaps, TMap<FString, const TSet<int32>*>(), TMap<FString, const TSet<int32>*>(),
		TMap<FString, TConstArrayView<int32>>(), Tethers, MeshScale, bEnabled);
}

void FClothConstraints::AddRules(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const FTriangleMesh& TriangleMesh,
	const FClothingPatternData* PatternData,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
	Softs::FSolverReal MeshScale, bool bEnabled)
{
	AddRules(ConfigProperties, TriangleMesh, PatternData, WeightMaps, TMap<FString, const TSet<int32>*>(), TMap<FString, const TSet<int32>*>(),
		TMap<FString, TConstArrayView<int32>>(), Tethers, MeshScale, bEnabled);
}

//only counts the number of rules that GS/CG currently cupports
void FClothConstraints::GetGSNumRules()
{
#if !UE_BUILD_SHIPPING
	check(PBDEvolution);

	NumConstraintInits = 1;
	NumConstraintRules = 0;
	if (XStretchBiasConstraints)
	{
		NumConstraintInits += 1;
	}
	if (XBendingElementConstraints)
	{
		NumConstraintInits += 1;
	}
	if (XEdgeConstraints)
	{
		NumConstraintInits += 1;
	}
	if (XAnisoBendingElementConstraints)
	{
		NumConstraintInits += 1;
	}

	if (bMakeSandwich)
	{
		NumConstraintRules = 2;
	}
	else
	{
		NumConstraintRules = 1;
	}
#endif
}


void FClothConstraints::CreateGSRules()
{
#if !UE_BUILD_SHIPPING
	check(PBDEvolution);
	check(ConstraintInitOffset == INDEX_NONE)
	GetGSNumRules();

	ConstraintInitOffset = PBDEvolution->AddConstraintInitRange(NumConstraintInits, false);

	check(ConstraintRuleOffset == INDEX_NONE)

	ConstraintRuleOffset = PBDEvolution->AddConstraintRuleRange(NumConstraintRules, false);

	check(PostprocessingConstraintRuleOffset == INDEX_NONE)

	int32 PostProcessingConstraintRule = 0;
	if (bWriteFinalResiduals)
	{
		PostProcessingConstraintRule = 1;
	}

	if (PostProcessingConstraintRule)
	{
		PostprocessingConstraintRuleOffset = PBDEvolution->AddConstraintPostprocessingsRange(PostProcessingConstraintRule, false);
	}

	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const ConstraintInits = PBDEvolution->ConstraintInits().GetData() + ConstraintInitOffset;
	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const ConstraintRules = PBDEvolution->ConstraintRules().GetData() + ConstraintRuleOffset;
	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const PostprocessingRules = PBDEvolution->ConstraintPostprocessings().GetData() + PostprocessingConstraintRuleOffset;

	int32 ConstraintInitIndex = 0;
	int32 ConstraintRuleIndex = 0;

	int32 ConstraintPostProcessingIndex = 0;

	GSMainConstraint = MakeShared<Chaos::Softs::FGaussSeidelMainConstraint<Softs::FSolverReal, Softs::FSolverParticles>>(PBDEvolution->Particles(), bClothDoQuasistatics, bUseSOR, SOROmega, 100);

	if (bClothDoQuasistatics)
	{
		PBDEvolution->SetQuasistatics(bClothDoQuasistatics);
		const Softs::FSolverVec3& GravityTerm = PBDEvolution->GetGravity(0);
		GSMainConstraint->ExternalForce[0] = 0.f;
		GSMainConstraint->ExternalForce[1] = 0.f;
		GSMainConstraint->ExternalForce[2] = GravityTerm[2];
	}

	ConstraintInits[ConstraintInitIndex++] =
		[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
	{
		this->GSMainConstraint->Init(Dt, InParticles);
	};

	if (XStretchBiasConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
		{
			XStretchBiasConstraints->Init();
			XStretchBiasConstraints->ApplyProperties(Dt, Evolution->GetIterations());
		};
	}

	if (XBendingElementConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
		{
			this->XBendingElementConstraints->Init(InParticles);
			this->XBendingElementConstraints->ApplyProperties(Dt, Evolution->GetIterations());
		};
	}

	if (XAnisoBendingElementConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
		{
			this->XAnisoBendingElementConstraints->Init(InParticles);
			this->XAnisoBendingElementConstraints->ApplyProperties(Dt, Evolution->GetIterations());
		};
	}

	if (XEdgeConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
		{
			this->XEdgeConstraints->ApplyProperties(Dt, Evolution->GetIterations());
		};
	}

	if (bMakeSandwich)
	{
		if (XStretchBiasConstraints)
		{
			ConstraintRules[ConstraintRuleIndex++] =
				[this, PreIterXPBDNums = UpperBreadIters, bbDisplayResidual = bDisplayResidual](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				for (int32 i = 0; i < PreIterXPBDNums; i++)
				{
					XStretchBiasConstraints->Apply(Particles, Dt);
					if (this->XBendingElementConstraints)
					{
						this->XBendingElementConstraints->Apply(Particles, Dt);
					}
					if (bbDisplayResidual)
					{
						TArray<Chaos::Softs::FSolverVec3> Residual = this->GSMainConstraint->ComputeNewtonResiduals(Particles, Dt);
					}
				}
			};
		}


		//middle layer of sandwich:
		if (bEnableCG)
		{
			ConstraintRules[ConstraintRuleIndex++] =
				[this, GSIterNums = MiddleBreadIters](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
			{
				for (int32 i = 0; i < GSIterNums; i++)
				{
					this->GSMainConstraint->ApplyCG(InParticles, Dt);
				}
			};
		}
		else
		{
			ConstraintRules[ConstraintRuleIndex++] =
				[this, GSIterNums = MiddleBreadIters](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
			{
				for (int32 i = 0; i < GSIterNums; i++)
				{
					this->GSMainConstraint->Apply(InParticles, Dt, MaxResidualIters, bWriteResidual2File);
				}
			};
		}

	}
	else
	{
		if (bEnableCG)
		{
			ConstraintRules[ConstraintRuleIndex++] =
				[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
			{
				this->GSMainConstraint->ApplyCG(InParticles, Dt);
			};
		}
		else
		{
			ConstraintRules[ConstraintRuleIndex++] =
				[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
			{
				this->GSMainConstraint->Apply(InParticles, Dt, MaxResidualIters, bWriteResidual2File);
			};
		}
	}



	if (XStretchBiasConstraints)
	{
		if (bReplaceBiasElementsWithCorotatedCod)
		{
			TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;

			const TArray<TVec3<int32>> CodimensionalMesh = XStretchBiasConstraints->GetConstraints();

			GSCorotatedCodimensionalConstraint = MakeShared<Chaos::Softs::FGaussSeidelCorotatedCodimensionalConstraints<Softs::FSolverReal, Softs::FSolverParticles>>(PBDEvolution->Particles(), CodimensionalMesh, false, YoungsModulus);

			GSMainConstraint->AddStaticConstraints(XStretchBiasConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
			const int32 StaticIndex = GSMainConstraint->AddStaticConstraintResidualAndHessianRange(1);

			GSMainConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
			{
				this->GSCorotatedCodimensionalConstraint->AddHyperelasticResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
			};
		}
		else
		{
			TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;
			GSMainConstraint->AddStaticConstraints(XStretchBiasConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
			const int32 StaticIndex = GSMainConstraint->AddStaticConstraintResidualAndHessianRange(1);

			GSMainConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
			{
				this->XStretchBiasConstraints->AddStretchBiasElementResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
			};

			if (bEnableCG)
			{
				const int32 ForceDifferentialRange = GSMainConstraint->AddAddInternalForceDifferentialsRange(1);

				GSMainConstraint->InternalForceDifferentials()[ForceDifferentialRange] = [this](const Softs::FSolverParticles& Particles, const TArray<Softs::FSolverVec3>& Deltax, TArray<Softs::FSolverVec3>& ndf)
				{
					this->XStretchBiasConstraints->AddInternalForceDifferential(Particles, Deltax, ndf);
				};
			}

		}

		XStretchBiasConstraints->InitializeDmInvAndMeasures(PBDEvolution->Particles());

	}

	if (XAnisoBendingElementConstraints)
	{
		TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;
		GSMainConstraint->AddStaticConstraints(XAnisoBendingElementConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
		const int32 StaticIndex = GSMainConstraint->AddStaticConstraintResidualAndHessianRange(1);

		GSMainConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
		{
			this->XAnisoBendingElementConstraints->AddAnisotropicBendingResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
		};

		if (bEnableCG)
		{
			const int32 ForceDifferentialRange = GSMainConstraint->AddAddInternalForceDifferentialsRange(1);

			GSMainConstraint->InternalForceDifferentials()[ForceDifferentialRange] = [this](const Softs::FSolverParticles& Particles, const TArray<Softs::FSolverVec3>& Deltax, TArray<Softs::FSolverVec3>& ndf)
			{
				this->XAnisoBendingElementConstraints->AddInternalForceDifferential(Particles, Deltax, ndf);
			};
		}
	}

	if (XBendingElementConstraints)
	{

		TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;
		GSMainConstraint->AddStaticConstraints(XBendingElementConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
		const int32 StaticIndex = GSMainConstraint->AddStaticConstraintResidualAndHessianRange(1);

		GSMainConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
		{
			this->XBendingElementConstraints->AddBendingResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
		};


		if (bEnableCG)
		{
			const int32 ForceDifferentialRange = GSMainConstraint->AddAddInternalForceDifferentialsRange(1);

			GSMainConstraint->InternalForceDifferentials()[ForceDifferentialRange] = [this](const Softs::FSolverParticles& Particles, const TArray<Softs::FSolverVec3>& Deltax, TArray<Softs::FSolverVec3>& ndf)
			{
				this->XBendingElementConstraints->AddInternalForceDifferential(Particles, Deltax, ndf);
			};
		}
	}

	GSMainConstraint->InitStaticColor(PBDEvolution->Particles());

	if (bWriteFinalResiduals)
	{
		FString file = FPaths::ProjectDir();
		file.Append(TEXT("/DebugOutput/NewtonResidual.txt"));
		FFileHelper::SaveStringToFile(FString(TEXT("Newton Norm\r\n")), *file);
		PostprocessingRules[ConstraintPostProcessingIndex++] =
			[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
		{
			this->GSMainConstraint->ComputeNewtonResiduals(InParticles, Dt, true, nullptr);
		};
	}

#endif
}

void FClothConstraints::AddRules(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const FTriangleMesh& TriangleMesh,
	const FClothingPatternData* PatternData,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const TMap<FString, const TSet<int32>*>& VertexSets,
	const TMap<FString, const TSet<int32>*>& FaceSets,
	const TMap<FString, TConstArrayView<int32>>& FaceIntMaps,
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
	Softs::FSolverReal MeshScale, bool bEnabled,
	const FTriangleMesh* MultiResCoarseLODMesh,
	const int32 MultiResCoarseLODParticleRangeId,
	const TSharedPtr<Softs::FMultiResConstraints>& FineLODMultiResConstraint)
{
	// Self collisions
	CreateSelfCollisionConstraints(ConfigProperties, WeightMaps, VertexSets, FaceSets, FaceIntMaps, TriangleMesh);

	// Edge constraints
	CreateStretchConstraints(ConfigProperties, WeightMaps, TriangleMesh, PatternData);

	// Bending constraints
	CreateBendingConstraints(ConfigProperties, WeightMaps, TriangleMesh, PatternData);

	// Area constraints
	CreateAreaConstraints(ConfigProperties, WeightMaps, TriangleMesh);

	// Long range constraints
	CreateLongRangeConstraints(ConfigProperties, WeightMaps, Tethers, MeshScale);

	// Max distances
	CreateMaxDistanceConstraints(ConfigProperties, WeightMaps, MeshScale);

	// Backstop Constraints
	CreateBackstopConstraints(ConfigProperties, WeightMaps, MeshScale);

	// Animation Drive Constraints
	CreateAnimDriveConstraints(ConfigProperties, WeightMaps);

	if (Evolution)
	{
		// External Forces
		CreateExternalForces(ConfigProperties, WeightMaps);

		// Velocity Field
		CreateVelocityAndPressureField(ConfigProperties, WeightMaps, TriangleMesh);

		// PerSolverField
		if (PerSolverField)
		{
			++NumExternalForceRules;
		}

		// Body collisions
		CreateCollisionConstraint(ConfigProperties, MeshScale);

		//Multires Springs
		CreateMultiresConstraint(ConfigProperties, WeightMaps, TriangleMesh, MultiResCoarseLODMesh, MultiResCoarseLODParticleRangeId);
	}

	// Commit rules to solver
	if (!bEnableGS)
	{
		if (Evolution)
		{
			CreateForceBasedRules(FineLODMultiResConstraint);
		}
		else
		{
			CreatePBDRules();
		}
	} 
	else
	{
		CreateGSRules();
	}

	if (PBDEvolution)
	{
		// Enable or disable constraints as requested
		Enable(bEnabled);
	}
}

void FClothConstraints::CreateSelfCollisionConstraints(const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const TMap<FString, const TSet<int32>*>& VertexSets,
	const TMap<FString, const TSet<int32>*>& FaceSets,
	const TMap<FString, TConstArrayView<int32>>& FaceIntMaps, const FTriangleMesh& TriangleMesh)
{
	const bool bUseSelfCollisions = ConfigProperties.GetValue<bool>(TEXT("UseSelfCollisions"));

	if (bUseSelfCollisions)
	{
		SelfCollisionInit = MakeShared<Softs::FPBDTriangleMeshCollisions>(
			ParticleOffset,
			NumParticles,
			FaceSets,
			TriangleMesh,
			ConfigProperties);
		++NumConstraintInits;

		SelfCollisionConstraints = MakeShared<Softs::FPBDCollisionSpringConstraints>(
			ParticleOffset,
			NumParticles,
			TriangleMesh,
			AnimationPositions,
			WeightMaps,
			FaceIntMaps,
			ConfigProperties);
		++NumPostCollisionConstraintRules;

		SelfIntersectionConstraints = MakeShared<Softs::FPBDTriangleMeshIntersections>(
			ParticleOffset,
			NumParticles,
			TriangleMesh);
		++NumPostprocessingConstraintRules;

		if (Evolution)
		{
			++NumPreSubstepConstraintRules; // Contour minimization
			++NumUpdateLinearSystemRules;
		}
		else
		{
			++NumConstraintInits; // Contour minimization
		}
	}
	else if (Softs::FPBDSelfCollisionSphereConstraints::IsEnabled(ConfigProperties))
	{
		SelfCollisionSphereConstraints = MakeShared<Softs::FPBDSelfCollisionSphereConstraints>(
			ParticleOffset,
			NumParticles,
			VertexSets,
			ConfigProperties);
		++NumConstraintInits;
		++NumPostCollisionConstraintRules;
	}
}

void FClothConstraints::CreateStretchConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const FTriangleMesh& TriangleMesh,
	const FClothingPatternData* PatternData)
{
	if (PatternData && PatternData->PatternPositions.Num() && Softs::FXPBDStretchBiasElementConstraints::IsEnabled(ConfigProperties))
	{
		if (Evolution)
		{
			XStretchBiasConstraints = MakeShared<Softs::FXPBDStretchBiasElementConstraints>(
				Evolution->GetSoftBodyParticles(ParticleRangeId),
				TriangleMesh,
				PatternData->WeldedFaceVertexPatternPositions,
				WeightMaps,
				ConfigProperties,
				/*bTrimKinematicConstraints =*/ true);
		}
		else
		{
			XStretchBiasConstraints = MakeShared<Softs::FXPBDStretchBiasElementConstraints>(
				PBDEvolution->Particles(),
				ParticleOffset,
				NumParticles,
				TriangleMesh,
				PatternData->WeldedFaceVertexPatternPositions,
				WeightMaps,
				ConfigProperties,
				/*bTrimKinematicConstraints =*/ true);
		}

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
	else if (Softs::FXPBDEdgeSpringConstraints::IsEnabled(ConfigProperties))
	{
		if (Evolution)
		{
			XEdgeConstraints = MakeShared<Softs::FXPBDEdgeSpringConstraints>(
				Evolution->GetSoftBodyParticles(ParticleRangeId),
				TriangleMesh.GetSurfaceElements(),
				WeightMaps,
				ConfigProperties);
			++NumUpdateLinearSystemRules;
		}
		else
		{
			XEdgeConstraints = MakeShared<Softs::FXPBDEdgeSpringConstraints>(
				PBDEvolution->Particles(),
				ParticleOffset,
				NumParticles,
				TriangleMesh.GetSurfaceElements(),
				WeightMaps,
				ConfigProperties);
		}

		++NumConstraintInits;
		++NumConstraintRules;
	}
	else if (Softs::FPBDEdgeSpringConstraints::IsEnabled(ConfigProperties))
	{
		if (Evolution)
		{
			EdgeConstraints = MakeShared<Softs::FPBDEdgeSpringConstraints>(
				Evolution->GetSoftBodyParticles(ParticleRangeId),
				TriangleMesh.GetSurfaceElements(),
				WeightMaps,
				ConfigProperties,
				/*bTrimKinematicConstraints =*/ true);
		}
		else
		{
			EdgeConstraints = MakeShared<Softs::FPBDEdgeSpringConstraints>(
				PBDEvolution->Particles(),
				ParticleOffset,
				NumParticles,
				TriangleMesh.GetSurfaceElements(),
				WeightMaps,
				ConfigProperties,
				/*bTrimKinematicConstraints =*/ true);
		}

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
	if (PatternData && PatternData->PatternPositions.Num() && Softs::FXPBDAnisotropicSpringConstraints::IsEnabled(ConfigProperties))
	{
		if (Evolution)
		{
			XAnisoSpringConstraints = MakeShared<Softs::FXPBDAnisotropicSpringConstraints>(
				Evolution->GetSoftBodyParticles(ParticleRangeId),
				TriangleMesh,
				PatternData->WeldedFaceVertexPatternPositions,
				WeightMaps,
				ConfigProperties);
			++NumUpdateLinearSystemRules;
		}
		else
		{
			XAnisoSpringConstraints = MakeShared<Softs::FXPBDAnisotropicSpringConstraints>(
				PBDEvolution->Particles(),
				ParticleOffset,
				NumParticles,
				TriangleMesh,
				PatternData->WeldedFaceVertexPatternPositions,
				WeightMaps,
				ConfigProperties);
		}

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
		++NumConstraintRules; // 2X because edge and axial spring applies are split
	}
}

void FClothConstraints::CreateBendingConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const FTriangleMesh& TriangleMesh,
	const FClothingPatternData* PatternData)
{
	if (PatternData && PatternData->PatternPositions.Num() && Softs::FXPBDAnisotropicBendingConstraints::IsEnabled(ConfigProperties))
	{
		if (Evolution)
		{
			XAnisoBendingElementConstraints = MakeShared<Softs::FXPBDAnisotropicBendingConstraints>(
				Evolution->GetSoftBodyParticles(ParticleRangeId),
				TriangleMesh,
				PatternData->WeldedFaceVertexPatternPositions,
				WeightMaps,
				ConfigProperties);
		}
		else
		{
			XAnisoBendingElementConstraints = MakeShared<Softs::FXPBDAnisotropicBendingConstraints>(
				PBDEvolution->Particles(),
				ParticleOffset, NumParticles,
				TriangleMesh,
				PatternData->WeldedFaceVertexPatternPositions,
				WeightMaps,
				ConfigProperties);
		}

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
	else if (Softs::FXPBDBendingConstraints::IsEnabled(ConfigProperties))
	{
		TArray<Chaos::TVec4<int32>> BendingElements = TriangleMesh.GetUniqueAdjacentElements();

		if (Evolution)
		{
			XBendingElementConstraints = MakeShared<Softs::FXPBDBendingConstraints>(
				Evolution->GetSoftBodyParticles(ParticleRangeId),
				MoveTemp(BendingElements),
				WeightMaps,
				ConfigProperties);
		}
		else
		{
			XBendingElementConstraints = MakeShared<Softs::FXPBDBendingConstraints>(
				PBDEvolution->Particles(),
				ParticleOffset, NumParticles,
				MoveTemp(BendingElements),
				WeightMaps,
				ConfigProperties);
		}

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
	else if (Softs::FPBDBendingConstraints::IsEnabled(ConfigProperties))
	{
		TArray<Chaos::TVec4<int32>> BendingElements = TriangleMesh.GetUniqueAdjacentElements();

		if (Evolution)
		{
			BendingElementConstraints = MakeShared<Softs::FPBDBendingConstraints>(
				Evolution->GetSoftBodyParticles(ParticleRangeId),
				MoveTemp(BendingElements),
				WeightMaps,
				ConfigProperties,
				/*bTrimKinematicConstraints =*/ true);
		}
		else
		{
			BendingElementConstraints = MakeShared<Softs::FPBDBendingConstraints>(
				PBDEvolution->Particles(),
				ParticleOffset, NumParticles,
				MoveTemp(BendingElements),
				WeightMaps,
				ConfigProperties,
				/*bTrimKinematicConstraints =*/ true);
		}

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
	else if (Softs::FXPBDBendingSpringConstraints::IsEnabled(ConfigProperties))
	{
		const TArray<Chaos::TVec2<int32>> CrossEdges = TriangleMesh.GetUniqueAdjacentPoints();

		if (Evolution)
		{
			XBendingConstraints = MakeShared<Softs::FXPBDBendingSpringConstraints>(
				Evolution->GetSoftBodyParticles(ParticleRangeId),
				CrossEdges,
				WeightMaps,
				ConfigProperties);
			++NumUpdateLinearSystemRules;
		}
		else
		{
			XBendingConstraints = MakeShared<Softs::FXPBDBendingSpringConstraints>(
				PBDEvolution->Particles(),
				ParticleOffset, NumParticles,
				CrossEdges,
				WeightMaps,
				ConfigProperties);
		}

		++NumConstraintInits;
		++NumConstraintRules;
	}
	else if (Softs::FPBDBendingSpringConstraints::IsEnabled(ConfigProperties))
	{
		const TArray<Chaos::TVec2<int32>> CrossEdges = TriangleMesh.GetUniqueAdjacentPoints();

		if (Evolution)
		{
			BendingConstraints = MakeShared<Softs::FPBDBendingSpringConstraints>(
				Evolution->GetSoftBodyParticles(ParticleRangeId),
				CrossEdges,
				WeightMaps,
				ConfigProperties,
				/*bTrimKinematicConstraints =*/ true);
		}
		else
		{
			BendingConstraints = MakeShared<Softs::FPBDBendingSpringConstraints>(
				PBDEvolution->Particles(),
				ParticleOffset,
				NumParticles,
				CrossEdges,
				WeightMaps,
				ConfigProperties,
				/*bTrimKinematicConstraints =*/ true);
		}

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
}

void FClothConstraints::CreateAreaConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const FTriangleMesh& TriangleMesh)
{
	if (Softs::FXPBDAreaSpringConstraints::IsEnabled(ConfigProperties))
	{
		if (Evolution)
		{
			XAreaConstraints = MakeShared<Softs::FXPBDAreaSpringConstraints>(
				Evolution->GetSoftBodyParticles(ParticleRangeId),
				TriangleMesh.GetSurfaceElements(),
				WeightMaps,
				ConfigProperties,
				/*bTrimKinematicConstraints =*/ true);
		}
		else
		{
			XAreaConstraints = MakeShared<Softs::FXPBDAreaSpringConstraints>(
				PBDEvolution->Particles(),
				ParticleOffset,
				NumParticles,
				TriangleMesh.GetSurfaceElements(),
				WeightMaps,
				ConfigProperties,
				/*bTrimKinematicConstraints =*/ true);
		}

		++NumConstraintInits;
		++NumConstraintRules;
	}
	else if (Softs::FPBDAreaSpringConstraints::IsEnabled(ConfigProperties))
	{
		if (Evolution)
		{
			AreaConstraints = MakeShared<Softs::FPBDAreaSpringConstraints>(
				Evolution->GetSoftBodyParticles(ParticleRangeId),
				TriangleMesh.GetSurfaceElements(),
				WeightMaps,
				ConfigProperties,
				/*bTrimKinematicConstraints =*/ true);
		}
		else
		{
			AreaConstraints = MakeShared<Softs::FPBDAreaSpringConstraints>(
				PBDEvolution->Particles(),
				ParticleOffset,
				NumParticles,
				TriangleMesh.GetSurfaceElements(),
				WeightMaps,
				ConfigProperties,
				/*bTrimKinematicConstraints =*/ true);
		}

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
}

void FClothConstraints::CreateLongRangeConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& Tethers,
	Softs::FSolverReal MeshScale)
{
	if (Softs::FPBDLongRangeConstraints::IsEnabled(ConfigProperties))
	{
		if (Evolution)
		{
			//  Now that we're only doing a single iteration of Long range constraints, and they're more of a fake constraint to jump start our initial guess, it's not clear that using XPBD makes sense here.
			LongRangeConstraints = MakeShared<Softs::FPBDLongRangeConstraints>(
				Evolution->GetSoftBodyParticles(ParticleRangeId),
				Tethers,
				WeightMaps,
				ConfigProperties,
				MeshScale);
			++NumPreSubstepConstraintRules;
		}
		else
		{
			LongRangeConstraints = MakeShared<Softs::FPBDLongRangeConstraints>(
				PBDEvolution->Particles(),
				ParticleOffset,
				NumParticles,
				Tethers,
				WeightMaps,
				ConfigProperties,
				MeshScale);
			++NumConstraintInits;
		}
	}
}

void FClothConstraints::CreateMaxDistanceConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	Softs::FSolverReal MeshScale)
{
	if (ConfigProperties.GetValue("UseLegacyConfig", false))
	{
		FString MaxDistanceString = TEXT("MaxDistance");
		MaxDistanceString = ConfigProperties.GetStringValue(MaxDistanceString, MaxDistanceString);  // Uses the same string for both the default weight map name and the property name
		const TConstArrayView<FRealSingle> MaxDistances = WeightMaps.FindRef(MaxDistanceString);
		if (MaxDistances.Num() != NumParticles)
		{
			return;  // Legacy configs disable the constraint when the weight map is missing
		}
	}

	if (Softs::FPBDSphericalConstraint::IsEnabled(ConfigProperties))
	{
		MaximumDistanceConstraints = MakeShared<Softs::FPBDSphericalConstraint>(
			ParticleOffset,
			NumParticles,
			*AnimationPositions,
			WeightMaps,
			ConfigProperties,
			MeshScale);

		++NumConstraintRules;
	}
}

void FClothConstraints::CreateBackstopConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	Softs::FSolverReal MeshScale)
{
	if (ConfigProperties.GetValue("UseLegacyConfig", false))
	{
		FString BackstopRadiusString = TEXT("BackstopRadius");
		FString BackstopDistanceString = TEXT("BackstopDistance");
		BackstopRadiusString = ConfigProperties.GetStringValue(BackstopRadiusString, BackstopRadiusString);        // Uses the same string for both the default weight map name and the property name
		BackstopDistanceString = ConfigProperties.GetStringValue(BackstopDistanceString, BackstopDistanceString);  //
		const TConstArrayView<FRealSingle> BackstopRadiuses = WeightMaps.FindRef(BackstopRadiusString);
		const TConstArrayView<FRealSingle> BackstopDistances = WeightMaps.FindRef(BackstopDistanceString);

		if (BackstopRadiuses.Num() != NumParticles || BackstopDistances.Num() != NumParticles)
		{
			return;  // Legacy configs disable the constraint when the weight maps are missing
		}
	}

	if (Softs::FPBDSphericalBackstopConstraint::IsEnabled(ConfigProperties))
	{
		BackstopConstraints = MakeShared<Softs::FPBDSphericalBackstopConstraint>(
			ParticleOffset,
			NumParticles,
			*AnimationPositions,
			*AnimationNormals,
			WeightMaps,
			ConfigProperties,
			MeshScale);

		++NumConstraintRules;
	}
}

void FClothConstraints::CreateAnimDriveConstraints(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps)
{
	if (Softs::FPBDAnimDriveConstraint::IsEnabled(ConfigProperties))
	{
		check(AnimationVelocities); // Legacy code didn't use to have AnimationVelocities

		AnimDriveConstraints = MakeShared<Softs::FPBDAnimDriveConstraint>(
			ParticleOffset,
			NumParticles,
			*AnimationPositions,
			*AnimationVelocities,
			WeightMaps,
			ConfigProperties);

		++NumConstraintInits;  // Uses init to update the property tables
		++NumConstraintRules;
	}
}

void FClothConstraints::CreateVelocityAndPressureField(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const FTriangleMesh& TriangleMesh)
{
	if (Evolution)
	{
		// Always create velocity field--we allow turning it on via blueprints
		constexpr Softs::FSolverReal WorldScale = (Softs::FSolverReal)100.;
		VelocityAndPressureField = MakeShared<Softs::FVelocityAndPressureField>(
			Evolution->GetSoftBodyParticles(ParticleRangeId),
			&TriangleMesh,
			ConfigProperties,
			WeightMaps,
			WorldScale
			);
		++NumExternalForceRules;
	}
}

void FClothConstraints::CreateExternalForces(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps)
{
	if (Evolution)
	{
		// Always create external forces
		check(Normals);
		ExternalForces = MakeShared<Softs::FExternalForces>(
			Evolution->GetSoftBodyParticles(ParticleRangeId),
			*Normals,
			WeightMaps,
			ConfigProperties
			);

		++NumExternalForceRules;
		++NumUpdateLinearSystemRules;
	}
}

void FClothConstraints::CreateCollisionConstraint(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	Softs::FSolverReal MeshScale)
{
	if (Evolution)
	{
		// Always create collision constraint
		check(LastSubframeCollisionTransformsCCD);
		CollisionConstraint = MakeShared<Softs::FPBDSoftBodyCollisionConstraint>(
			*LastSubframeCollisionTransformsCCD,
			ConfigProperties,
			MeshScale,
			CollisionParticleCollided,
			CollisionContacts,
			CollisionNormals,
			CollisionPhis
			);
		++NumCollisionConstraintRules;
		++NumUpdateLinearSystemCollisionsRules;
	}
}

void FClothConstraints::CreateMultiresConstraint(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const FTriangleMesh& TriangleMesh,
	const FTriangleMesh* MultiResCoarseLODMesh,
	const int32 MultiResCoarseLODParticleRangeId)
{
	if (Evolution && MultiResCoarseLODMesh && MultiResCoarseLODParticleRangeId != INDEX_NONE)
	{
		if (Softs::FMultiResConstraints::IsEnabled(ConfigProperties))
		{
			TArray<TVec4<Softs::FSolverReal>> CoarseToFinePositionBaryCoordsAndDist;
			TArray<TVec3<int32>> CoarseToFineSourceMeshVertIndices;
			const Softs::FSolverParticlesRange& FineParticles = Evolution->GetSoftBodyParticles(ParticleRangeId);
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(BuildMultiResTransitionData);
				// TODO: Cache this data in the asset.
				const FPointWeightMap* const MaxDistances = nullptr;  // No need to update the vertex contribution on the transition maps
				constexpr bool bUseSmoothTransitions = false;         // Smooth transitions are only used at rendering for now and not during LOD transitions
				constexpr bool bUseMultipleInfluences = false;        // Multiple influences must not be used for LOD transitions
				constexpr float SkinningKernelRadius = 0.f;           // KernelRadius is only required when using multiple influences
				static_assert(std::is_same<float, Softs::FSolverReal>::value);
				static_assert(sizeof(Softs::FSolverVec3) == sizeof(FVector3f));

				TConstArrayView<FVector3f> FinePositions((const FVector3f*)FineParticles.XArray().GetData(), FineParticles.Size());
				TArray<uint32> FineIndices;
				FineIndices.Reserve(3 * TriangleMesh.GetNumElements());
				for (const TVec3<int32>& Element : TriangleMesh.GetElements())
				{
					FineIndices.Add(Element[0]);
					FineIndices.Add(Element[1]);
					FineIndices.Add(Element[2]);
				}

				const ClothingMeshUtils::ClothMeshDesc FineLodDesc(FinePositions, TConstArrayView<uint32>(FineIndices));

				const Softs::FSolverParticlesRange& CoarseParticles = Evolution->GetSoftBodyParticles(MultiResCoarseLODParticleRangeId);
				TConstArrayView<FVector3f> CoarsePositions((const FVector3f*)CoarseParticles.XArray().GetData(), CoarseParticles.Size());
				TConstArrayView<Softs::FSolverReal> CoarseInvM = CoarseParticles.GetInvM();
				TArray<uint32> CoarseIndices;
				CoarseIndices.Reserve(3 * MultiResCoarseLODMesh->GetNumElements());
				for (const TVec3<int32>& Element : MultiResCoarseLODMesh->GetElements())
				{
					if (CoarseInvM[Element[0]] != 0.f || CoarseInvM[Element[1]] != 0.f || CoarseInvM[Element[2]] != 0.f)
					{
						CoarseIndices.Add(Element[0]);
						CoarseIndices.Add(Element[1]);
						CoarseIndices.Add(Element[2]);
					}
				}

				if (CoarseIndices.IsEmpty())
				{
					return;
				}

				const ClothingMeshUtils::ClothMeshDesc CoarseLodDesc(CoarsePositions, TConstArrayView<uint32>(CoarseIndices));
				TArray<FMeshToMeshVertData> TransitionData;
				ClothingMeshUtils::GenerateMeshToMeshVertData(TransitionData, FineLodDesc, CoarseLodDesc, MaxDistances, bUseSmoothTransitions, bUseMultipleInfluences, SkinningKernelRadius);

				CoarseToFinePositionBaryCoordsAndDist.Reserve(TransitionData.Num());
				CoarseToFineSourceMeshVertIndices.Reserve(TransitionData.Num());
				for (const FMeshToMeshVertData& Data : TransitionData)
				{
					CoarseToFinePositionBaryCoordsAndDist.Emplace(Data.PositionBaryCoordsAndDist.X, Data.PositionBaryCoordsAndDist.Y, Data.PositionBaryCoordsAndDist.Z, Data.PositionBaryCoordsAndDist.W);
					CoarseToFineSourceMeshVertIndices.Emplace(Data.SourceMeshVertIndices[0], Data.SourceMeshVertIndices[1], Data.SourceMeshVertIndices[2]);
				}
			}

			MultiResConstraints = MakeShared<Softs::FMultiResConstraints>(
				FineParticles,
				MultiResCoarseLODParticleRangeId,
				*MultiResCoarseLODMesh,
				MoveTemp(CoarseToFinePositionBaryCoordsAndDist),
				MoveTemp(CoarseToFineSourceMeshVertIndices),
				WeightMaps,
				ConfigProperties);
			++NumConstraintRules;
			++NumConstraintInits;
		}
	}
}

void FClothConstraints::CreateForceBasedRules(const TSharedPtr<Softs::FMultiResConstraints>& FineLODMultiResConstraint)
{
	if (FineLODMultiResConstraint)
	{
		++NumPostprocessingConstraintRules;
	}

	FRuleCreator RuleCreator(this);

	if (ExternalForces)
	{
		RuleCreator.AddExternalForceRule_Apply(ExternalForces.Get());
		RuleCreator.AddUpdateLinearSystemRule_UpdateLinearSystem(ExternalForces.Get());
	}
	if (VelocityAndPressureField)
	{
		RuleCreator.AddExternalForceRule_Apply(VelocityAndPressureField.Get());

		// TODO Linear System
	}

	if (PerSolverField)
	{
		RuleCreator.AddExternalForceRule(
			[this](Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PerSolverField);
			const TArray<FVector>& LinearVelocities = PerSolverField->GetOutputResults(EFieldCommandOutputType::LinearVelocity);
			const TArray<FVector>& LinearForces = PerSolverField->GetOutputResults(EFieldCommandOutputType::LinearForce);
			const FVector* const LinearVelocitiesView = LinearVelocities.IsEmpty() ? nullptr :
				Particles.GetConstArrayView(LinearVelocities).GetData();
			const FVector* const LinearForcesView = LinearForces.IsEmpty() ? nullptr :
				Particles.GetConstArrayView(LinearForces).GetData();
			if (!LinearVelocitiesView && !LinearForcesView)
			{
				return;
			}

			Softs::FSolverVec3* const Acceleration = Particles.GetAcceleration().GetData();
			const Softs::FSolverReal* const InvM = Particles.GetInvM().GetData();
			for (int32 Index = 0; Index < Particles.GetRangeSize(); ++Index)
			{
				if (InvM[Index] != (Softs::FSolverReal)0.)
				{
					if (LinearForcesView)
					{
						Acceleration[Index] += Softs::FSolverVec3(LinearForcesView[Index]) * InvM[Index];
					}
					if (LinearVelocitiesView)
					{
						Acceleration[Index] += Softs::FSolverVec3(LinearVelocitiesView[Index]) / Dt;
					}
				}
			}
		});

		// TODO: Linear System
	}

	if (MultiResConstraints)
	{
		constexpr bool bInitParticles = false;
		RuleCreator.AddXPBDParallelInitRule<bInitParticles>(MultiResConstraints.Get());
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(MultiResConstraints.Get());
	}

	if (FineLODMultiResConstraint)
	{
		RuleCreator.AddPostSubstepConstraintRule(
			[FineLODMultiResConstraint, this](Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt, const Softs::ESolverMode SolverMode)
		{
			FineLODMultiResConstraint->UpdateFineTargets(Particles);
		});
	}

	if (XStretchBiasConstraints)
	{
		constexpr bool bInitParticles = false;
		RuleCreator.AddXPBDParallelInitRule<bInitParticles>(XStretchBiasConstraints.Get());
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(XStretchBiasConstraints.Get());
	}
	if (XEdgeConstraints)
	{
		constexpr bool bInitParticles = false;
		RuleCreator.AddXPBDParallelInitRule<bInitParticles>(XEdgeConstraints.Get());
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(XEdgeConstraints.Get());
		RuleCreator.AddUpdateLinearSystemRule_UpdateLinearSystem(XEdgeConstraints.Get());
	}
	if (EdgeConstraints)
	{
		RuleCreator.AddPBDParallelInitRule(EdgeConstraints.Get());
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(EdgeConstraints.Get());
	}
	if (XAnisoSpringConstraints)
	{
		constexpr bool bInitParticles = false;
		RuleCreator.AddXPBDParallelInitRule<bInitParticles>(XAnisoSpringConstraints.Get());
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(&XAnisoSpringConstraints->GetEdgeConstraints());
		RuleCreator.AddUpdateLinearSystemRule(
			[this](const Softs::FSolverParticlesRange& Particles, const FSolverReal Dt, Softs::FEvolutionLinearSystem& LinearSystem)
		{
			XAnisoSpringConstraints->GetEdgeConstraints().UpdateLinearSystem(Particles, Dt, LinearSystem);
			XAnisoSpringConstraints->GetAxialConstraints().UpdateLinearSystem(Particles, Dt, LinearSystem);
		});
	}

	if (XBendingConstraints)
	{
		constexpr bool bInitParticles = false;
		RuleCreator.AddXPBDParallelInitRule<bInitParticles>(XBendingConstraints.Get());
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(XBendingConstraints.Get());
		RuleCreator.AddUpdateLinearSystemRule_UpdateLinearSystem(XBendingConstraints.Get());
	}
	if (BendingConstraints)
	{
		RuleCreator.AddPBDParallelInitRule(BendingConstraints.Get());
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(BendingConstraints.Get());
	}
	if (BendingElementConstraints)
	{
		// These are PBD, not XPBD constraints, but the difference is that XPBD constraints have an Init method and PBD (usually) do not.
		// BendingElementConstraints do have an Init function, so this will add a call to that.
		// TODO: clean up the names of these methods to be more explicit.
		constexpr bool bInitParticles = true;
		RuleCreator.AddXPBDParallelInitRule<bInitParticles>(BendingElementConstraints.Get());
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(BendingElementConstraints.Get());
	}
	if (XBendingElementConstraints)
	{
		constexpr bool bInitParticles = true;
		RuleCreator.AddXPBDParallelInitRule<bInitParticles>(XBendingElementConstraints.Get());
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(XBendingElementConstraints.Get());
	}
	if (XAnisoBendingElementConstraints)
	{
		constexpr bool bInitParticles = true;
		RuleCreator.AddXPBDParallelInitRule<bInitParticles>(XAnisoBendingElementConstraints.Get());
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(XAnisoBendingElementConstraints.Get());
	}
	if (XAreaConstraints)
	{
		constexpr bool bInitParticles = false;
		RuleCreator.AddXPBDParallelInitRule<bInitParticles>(XAreaConstraints.Get());
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(XAreaConstraints.Get());
	}
	if (AreaConstraints)
	{
		RuleCreator.AddPBDParallelInitRule(AreaConstraints.Get());
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(AreaConstraints.Get());
	}
	if (XAnisoSpringConstraints)
	{
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(&XAnisoSpringConstraints->GetAxialConstraints());
	}
	if (MaximumDistanceConstraints)
	{
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(MaximumDistanceConstraints.Get());
	}
	if (BackstopConstraints)
	{
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(BackstopConstraints.Get());
	}
	if (AnimDriveConstraints)
	{
		RuleCreator.AddPBDParallelInitRule(AnimDriveConstraints.Get());
		constexpr bool bPostCollisions = false;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(AnimDriveConstraints.Get());
	}

	if (CollisionConstraint)
	{
		RuleCreator.AddPerIterationCollisionPBDConstraintRule_Apply(CollisionConstraint.Get());
		RuleCreator.AddUpdateLinearSystemCollisionsRule_UpdateLinearSystem(CollisionConstraint.Get());
	}

	if (SelfCollisionInit && SelfCollisionConstraints)
	{
		RuleCreator.AddPostInitialGuessParallelInitRule(
		[this](const Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt, const Softs::ESolverMode SolverMode)
		{
			if (bSkipSelfCollisionInit)
			{
				return;
			}
			SelfCollisionInit->Init(Particles, SelfCollisionConstraints->GetThicknessWeighted());
			SelfCollisionConstraints->Init(Particles, Dt, SelfCollisionInit->GetCollidableSubMesh(), SelfCollisionInit->GetDynamicSpatialHash(), SelfCollisionInit->GetKinematicColliderSpatialHash(), SelfCollisionInit->GetVertexGIAColors(), SelfCollisionInit->GetTriangleGIAColors());
		});

		constexpr bool bPostCollisions = true;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(SelfCollisionConstraints.Get());
		RuleCreator.AddUpdateLinearSystemRule_UpdateLinearSystem(SelfCollisionConstraints.Get());
	}

	if (SelfCollisionSphereConstraints)
	{
		RuleCreator.AddPostInitialGuessParallelInitRule(
			[this](const Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt, const Softs::ESolverMode SolverMode)
		{
			if (bSkipSelfCollisionInit)
			{
				return;
			}
			SelfCollisionSphereConstraints->Init(Particles);
		});

		constexpr bool bPostCollisions = true;
		RuleCreator.AddPerIterationPBDConstraintRule_Apply<bPostCollisions>(SelfCollisionSphereConstraints.Get());
	}

	if (SelfCollisionInit && SelfIntersectionConstraints)
	{
		RuleCreator.AddPreSubstepConstraintRule(
			[this](Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt, const Softs::ESolverMode SolverMode)
		{
			if (bSkipSelfCollisionInit)
			{
				return;
			}
			SelfIntersectionConstraints->Apply(Particles, SelfCollisionInit->GetContourMinimizationIntersections(), Dt);
		});

		RuleCreator.AddPostSubstepConstraintRule(
			[this](Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt, const Softs::ESolverMode SolverMode)
		{
			if (bSkipSelfCollisionInit)
			{
				return;
			}
			const int32 NumContourIterations = SelfCollisionInit->GetNumContourMinimizationPostSteps();
			for (int32 Iter = 0; Iter < NumContourIterations; ++Iter)
			{
				SelfCollisionInit->PostStepInit(Particles);
				SelfIntersectionConstraints->Apply(Particles, SelfCollisionInit->GetPostStepContourMinimizationIntersections(), Dt);
			}
		});
	}
	if (LongRangeConstraints)
	{
		RuleCreator.AddPreSubstepConstraintRule(
			[this](Softs::FSolverParticlesRange& Particles, const Softs::FSolverReal Dt, const Softs::ESolverMode SolverMode)
		{
			if ((SolverMode & Softs::ESolverMode::PBD) != Softs::ESolverMode::None)
			{
				// Only doing one iteration.
				constexpr int32 NumLRAIterations = 1;
				LongRangeConstraints->ApplyProperties(Dt, NumLRAIterations);
				LongRangeConstraints->Apply(Particles, Dt);  // Run the LRA constraint only once per timestep
			}
		});
	}
}


void FClothConstraints::CreatePBDRules()
{
	check(PBDEvolution);
	check(ConstraintInitOffset == INDEX_NONE)
#if !UE_BUILD_SHIPPING
	if (bDisplayResidual)
	{
		NumConstraintRules++;
	}
	if (bWriteFinalResiduals || bDisplayResidual)
	{
		NumConstraintInits++;
	}
#endif
	if (NumConstraintInits)
	{
		ConstraintInitOffset = PBDEvolution->AddConstraintInitRange(NumConstraintInits, false);
	}
	check(ConstraintRuleOffset == INDEX_NONE)

	if (NumConstraintRules)
	{
		ConstraintRuleOffset = PBDEvolution->AddConstraintRuleRange(NumConstraintRules, false);
	}
	check(PostCollisionConstraintRuleOffset == INDEX_NONE);
	if (NumPostCollisionConstraintRules)
	{
		PostCollisionConstraintRuleOffset = PBDEvolution->AddPostCollisionConstraintRuleRange(NumPostCollisionConstraintRules, false);
	}
	check(PostprocessingConstraintRuleOffset == INDEX_NONE);
#if !UE_BUILD_SHIPPING	
	if (bWriteFinalResiduals)
	{
		NumPostprocessingConstraintRules += 1;
	}
#endif
	if (NumPostprocessingConstraintRules)
	{
		PostprocessingConstraintRuleOffset = PBDEvolution->AddConstraintPostprocessingsRange(NumPostprocessingConstraintRules, false);
	}

	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const ConstraintInits = PBDEvolution->ConstraintInits().GetData() + ConstraintInitOffset;
	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const ConstraintRules = PBDEvolution->ConstraintRules().GetData() + ConstraintRuleOffset;
	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const PostCollisionConstraintRules = PBDEvolution->PostCollisionConstraintRules().GetData() + PostCollisionConstraintRuleOffset;
	TFunction<void(Softs::FSolverParticles&, const Softs::FSolverReal)>* const PostprocessingConstraintRules = PBDEvolution->ConstraintPostprocessings().GetData() + PostprocessingConstraintRuleOffset;

	int32 ConstraintInitIndex = 0;
	int32 ConstraintRuleIndex = 0;
	int32 PostCollisionConstraintRuleIndex = 0;
	int32 PostprocessingConstraintRuleIndex = 0;
	#if !UE_BUILD_SHIPPING	
	if (bDisplayResidual || bWriteFinalResiduals)
	{
		GSMainConstraint = MakeShared<Chaos::Softs::FGaussSeidelMainConstraint<Softs::FSolverReal, Softs::FSolverParticles>>(PBDEvolution->Particles(), false, false, (Softs::FSolverReal)1.2, 100);

		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
		{
			this->GSMainConstraint->Init(Dt, InParticles);
		};
		if (bDisplayResidual)
		{

			ConstraintRules[ConstraintRuleIndex++] =
				[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
			{
				if (this->GSMainConstraint->DebugResidual && this->GSMainConstraint->PassedIters < MaxResidualIters)
				{
					UE_LOG(LogTemp, Warning, TEXT("Current Iteration As From Evolution: %d"), this->Evolution->GetIterations());
					this->GSMainConstraint->ComputeNewtonResiduals(InParticles, Dt, bWriteResidual2File);
					this->GSMainConstraint->PassedIters += 1;
				}
			};
		}

		if (XStretchBiasConstraints)
		{
			TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;
			GSMainConstraint->AddStaticConstraints(XStretchBiasConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
			int32 StaticIndex = GSMainConstraint->AddStaticConstraintResidualAndHessianRange(1);

			GSMainConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
			{
				this->XStretchBiasConstraints->AddStretchBiasElementResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
			};
		}

		if (XAnisoBendingElementConstraints)
		{
			TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;
			GSMainConstraint->AddStaticConstraints(XAnisoBendingElementConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
			int32 StaticIndex = GSMainConstraint->AddStaticConstraintResidualAndHessianRange(1);

			GSMainConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
			{
				this->XAnisoBendingElementConstraints->AddAnisotropicBendingResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
			};
		}


		if (XBendingElementConstraints)
		{
			TArray<TArray<int32>> IncidentElements, IncidentElementsLocal;
			GSMainConstraint->AddStaticConstraints(XBendingElementConstraints->GetConstraintsArray(), IncidentElements, IncidentElementsLocal);
			int32 StaticIndex = GSMainConstraint->AddStaticConstraintResidualAndHessianRange(1);

			GSMainConstraint->StaticConstraintResidualAndHessian()[StaticIndex] = [this](const Softs::FSolverParticles& Particles, const int32 ElementIndex, const int32 ElementIndexLocal, const Softs::FSolverReal Dt, TVec3<Softs::FSolverReal>& ParticleResidual, Chaos::PMatrix<Softs::FSolverReal, 3, 3>& ParticleHessian)
			{
				this->XBendingElementConstraints->AddBendingResidualAndHessian(Particles, ElementIndex, ElementIndexLocal, Dt, ParticleResidual, ParticleHessian);
			};
		}

		GSMainConstraint->InitStaticColor(PBDEvolution->Particles());

		if (bWriteFinalResiduals)
		{
			FString file = FPaths::ProjectDir();
			file.Append(TEXT("/DebugOutput/NewtonResidual.txt"));
			FFileHelper::SaveStringToFile(FString(TEXT("Newton Norm\r\n")), *file);
			PostprocessingConstraintRules[PostprocessingConstraintRuleIndex++] =
				[this](Softs::FSolverParticles& InParticles, const Softs::FSolverReal Dt)
			{
				this->GSMainConstraint->ComputeNewtonResiduals(InParticles, Dt, true, nullptr);
			};
		}
	}
#endif

	if (XStretchBiasConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
		{
			XStretchBiasConstraints->Init();
			XStretchBiasConstraints->ApplyProperties(Dt, PBDEvolution->GetIterations());
		};

		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XStretchBiasConstraints->Apply(Particles, Dt);
		};
	}
	if (XEdgeConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				XEdgeConstraints->Init();
				XEdgeConstraints->ApplyProperties(Dt, PBDEvolution->GetIterations());
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
				EdgeConstraints->ApplyProperties(Dt, PBDEvolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				EdgeConstraints->Apply(Particles, Dt);
			};
	}
	if (XAnisoSpringConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
		{
			XAnisoSpringConstraints->Init();
			XAnisoSpringConstraints->ApplyProperties(Dt, PBDEvolution->GetIterations());
		};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XAnisoSpringConstraints->GetEdgeConstraints().Apply(Particles, Dt);
		};
	}
	if (XBendingConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				XBendingConstraints->Init();
				XBendingConstraints->ApplyProperties(Dt, PBDEvolution->GetIterations());
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
				BendingConstraints->ApplyProperties(Dt, PBDEvolution->GetIterations());
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
				BendingElementConstraints->ApplyProperties(Dt, PBDEvolution->GetIterations());
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
			XBendingElementConstraints->ApplyProperties(Dt, PBDEvolution->GetIterations());
		};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XBendingElementConstraints->Apply(Particles, Dt);
		};
	}
	if (XAnisoBendingElementConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XAnisoBendingElementConstraints->Init(Particles);
			XAnisoBendingElementConstraints->ApplyProperties(Dt, PBDEvolution->GetIterations());
		};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XAnisoBendingElementConstraints->Apply(Particles, Dt);
		};
	}
	if (XAreaConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& /*Particles*/, const Softs::FSolverReal Dt)
			{
				XAreaConstraints->Init();
				XAreaConstraints->ApplyProperties(Dt, PBDEvolution->GetIterations());
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
				AreaConstraints->ApplyProperties(Dt, PBDEvolution->GetIterations());
			};
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				AreaConstraints->Apply(Particles, Dt);
			};
	}
	if (XAnisoSpringConstraints)
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			XAnisoSpringConstraints->GetAxialConstraints().Apply(Particles, Dt);
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
				AnimDriveConstraints->ApplyProperties(Dt, PBDEvolution->GetIterations());
			};

		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				AnimDriveConstraints->Apply(Particles, Dt);
			};
	}
	if (ShapeConstraints_Deprecated) // TODO: Remove 5.6
	{
		ConstraintRules[ConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				ShapeConstraints_Deprecated->Apply(Particles, Dt);
			};
	}

	if (SelfCollisionInit && SelfCollisionConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				if (bSkipSelfCollisionInit)
				{
					return;
				}
				// Thickness * 2 to account for collision radius for both particles
				SelfCollisionInit->Init(Particles, SelfCollisionConstraints->GetThicknessWeighted());
				SelfCollisionConstraints->Init(Particles, Dt, SelfCollisionInit->GetCollidableSubMesh(), SelfCollisionInit->GetDynamicSpatialHash(), SelfCollisionInit->GetKinematicColliderSpatialHash(), SelfCollisionInit->GetVertexGIAColors(), SelfCollisionInit->GetTriangleGIAColors());
			};

		PostCollisionConstraintRules[PostCollisionConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				SelfCollisionConstraints->Apply(Particles, Dt);
			};

	}

	if (SelfCollisionSphereConstraints)
	{
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal /*Dt*/)
			{
				if (bSkipSelfCollisionInit)
				{
					return;
				}
				SelfCollisionSphereConstraints->Init(Particles);
			};

		PostCollisionConstraintRules[PostCollisionConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				SelfCollisionSphereConstraints->Apply(Particles, Dt);
			};
	}

	// The following constraints only run once per subframe, so we do their Apply as part of the Init() which modifies P
	// To avoid possible dependency order issues, add them last
	if (SelfCollisionInit && SelfIntersectionConstraints)
	{
		// The following constraints only run once per subframe, so we do their Apply as part of the Init() which modifies P
		// To avoid possible dependency order issues, add them last
		ConstraintInits[ConstraintInitIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
			{
				if (bSkipSelfCollisionInit)
				{
					return;
				}
				SelfIntersectionConstraints->Apply(Particles, SelfCollisionInit->GetContourMinimizationIntersections(), Dt);
			};

		PostprocessingConstraintRules[PostprocessingConstraintRuleIndex++] =
			[this](Softs::FSolverParticles& Particles, const Softs::FSolverReal Dt)
		{
			if (bSkipSelfCollisionInit)
			{
				return;
			}
			const int32 NumContourIterations = SelfCollisionInit->GetNumContourMinimizationPostSteps();
			for (int32 Iter = 0; Iter < NumContourIterations; ++Iter)
			{
				SelfCollisionInit->PostStepInit(Particles);
				SelfIntersectionConstraints->Apply(Particles, SelfCollisionInit->GetPostStepContourMinimizationIntersections(), Dt);
			}
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

void FClothConstraints::UpdateFromSolver(const FSolverVec3& SolverGravity, bool bPerClothGravityOverrideEnabled,
	const FSolverVec3& FictitiousAngularVelocity, const FSolverVec3& ReferenceSpaceLocation,
	const FSolverVec3& InSolverWindVelocity, const FSolverReal LegacyWindAdaptation)
{
	if (ExternalForces)
	{
		ExternalForces->SetWorldGravityMultiplier((FSolverReal)ClothingSimulationClothConsoleVariables::CVarGravityMultiplier.GetValueOnAnyThread());
		ExternalForces->SetSolverGravityProperties(SolverGravity, bPerClothGravityOverrideEnabled);
		ExternalForces->SetFictitiousForcesData(FictitiousAngularVelocity, ReferenceSpaceLocation);
		ExternalForces->SetSolverWind(InSolverWindVelocity, LegacyWindAdaptation);
	}
	SolverWindVelocity = InSolverWindVelocity;
}

void FClothConstraints::Update(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	const TMap<FString, const TSet<int32>*>& VertexSets,
	const TMap<FString, const TSet<int32>*>& FaceSets,
	const TMap<FString, TConstArrayView<int32>>& FaceIntMaps,
	Softs::FSolverReal MeshScale,
	Softs::FSolverReal MaxDistancesScale)
{
	if (EdgeConstraints)
	{
		EdgeConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (XEdgeConstraints)
	{
		XEdgeConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (XStretchBiasConstraints)
	{
		XStretchBiasConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (XAnisoSpringConstraints)
	{
		XAnisoSpringConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (BendingConstraints)
	{
		BendingConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (XBendingConstraints)
	{
		XBendingConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (BendingElementConstraints)
	{
		BendingElementConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (XBendingElementConstraints)
	{
		XBendingElementConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (XAnisoBendingElementConstraints)
	{
		XAnisoBendingElementConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (AreaConstraints)
	{
		AreaConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (XAreaConstraints)
	{
		XAreaConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (LongRangeConstraints)
	{
		LongRangeConstraints->SetProperties(ConfigProperties, WeightMaps, MeshScale);
	}
	if (MaximumDistanceConstraints)
	{
		MaximumDistanceConstraints->SetProperties(ConfigProperties, WeightMaps, MeshScale * MaxDistancesScale);
	}
	if (BackstopConstraints)
	{
		BackstopConstraints->SetProperties(ConfigProperties, WeightMaps, MeshScale);
	}
	if (AnimDriveConstraints)
	{
		AnimDriveConstraints->SetProperties(ConfigProperties, WeightMaps);
	}
	if (SelfCollisionConstraints)
	{
		SelfCollisionConstraints->SetProperties(ConfigProperties, WeightMaps, FaceIntMaps);
	}
	if (SelfCollisionInit)
	{
		SelfCollisionInit->SetProperties(ConfigProperties, FaceSets);
	}
	if (SelfCollisionSphereConstraints)
	{
		SelfCollisionSphereConstraints->SetProperties(ConfigProperties, VertexSets);
	}
	if (MultiResConstraints)
	{
		MultiResConstraints->SetProperties(ConfigProperties, WeightMaps);
	}

	bool bUsePointBasedWindModel = false;
	if (ExternalForces)
	{
		ExternalForces->SetProperties(ConfigProperties, WeightMaps);
		bUsePointBasedWindModel = ExternalForces->UsePointBasedWindModel();
	}
	if (VelocityAndPressureField)
	{
		constexpr FSolverReal WorldScale = 100.f;
		const bool bPointBasedWindDisablesAccurateWind = ClothingSimulationClothConsoleVariables::CVarLegacyDisablesAccurateWind.GetValueOnAnyThread();
		const bool bEnableAerodynamics = !(bUsePointBasedWindModel && bPointBasedWindDisablesAccurateWind);
		VelocityAndPressureField->SetPropertiesAndWind(
			ConfigProperties,
			WeightMaps,
			WorldScale,
			bEnableAerodynamics,
			SolverWindVelocity
		);
	}
	if (CollisionConstraint)
	{
		static IConsoleVariable* const WriteCCDContacts = IConsoleManager::Get().FindConsoleVariable(TEXT("p.Chaos.PBDEvolution.WriteCCDContacts"));
		const bool bWriteCCDContacts = WriteCCDContacts ? WriteCCDContacts->GetBool() : false;
		CollisionConstraint->SetProperties(ConfigProperties);
		CollisionConstraint->SetWriteDebugContacts(bWriteCCDContacts);
	}
}

// Deprecated
void FClothConstraints::Update(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
	Softs::FSolverReal MeshScale,
	Softs::FSolverReal MaxDistancesScale)
{
	Update(ConfigProperties, WeightMaps, TMap<FString, const TSet<int32>*>(), TMap<FString, const TSet<int32>*>(), TMap<FString, TConstArrayView<int32>>(), MeshScale, MaxDistancesScale);
}

// Deprecated
void FClothConstraints::Update(
	const Softs::FCollectionPropertyConstFacade& ConfigProperties,
	Softs::FSolverReal MeshScale,
	Softs::FSolverReal MaxDistancesScale)
{
	Update(ConfigProperties, TMap<FString, TConstArrayView<FRealSingle>>(), TMap<FString, const TSet<int32>*>(), TMap<FString, const TSet<int32>*>(), TMap<FString, TConstArrayView<int32>>(), MeshScale, MaxDistancesScale);
}

}  // End namespace Chaos
