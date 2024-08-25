// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Island/IslandManager.h"
#include "Chaos/ChaosDebugDraw.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/ParticleIterator.h"
#include "Chaos/PBDConstraintContainer.h"
#include "Chaos/PBDRigidsSOAs.h"
#include "Chaos/PerParticleGravity.h"
#include "Chaos/PhysicsMaterialUtilities.h"

#include "ChaosStats.h"


// Extra check for debug mode
// NOTE: Should be disabled for checkin (except debug builds)
#ifndef CHAOS_CONSTRAINTGRAPH_CHECK_ENABLED
#define CHAOS_CONSTRAINTGRAPH_CHECK_ENABLED (UE_BUILD_DEBUG)
#endif

#if CHAOS_CONSTRAINTGRAPH_CHECK_ENABLED
#define CHAOS_CONSTRAINTGRAPH_CHECK(X) check(X)
#else
#define CHAOS_CONSTRAINTGRAPH_CHECK(X)
#endif

#define CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(X) \
	bIsValid = bIsValid && ensure(X); \
	if (!bIsValid) { return false; }


DECLARE_CYCLE_STAT(TEXT("IslandManager::UpdateParticles"), STAT_IslandManager_UpdateParticles, STATGROUP_ChaosIslands);
DECLARE_CYCLE_STAT(TEXT("IslandManager::Merge"), STAT_IslandManager_MergeIslands, STATGROUP_ChaosIslands);
DECLARE_CYCLE_STAT(TEXT("IslandManager::Split"), STAT_IslandManager_SplitIslands, STATGROUP_ChaosIslands);
DECLARE_CYCLE_STAT(TEXT("IslandManager::Levels"), STAT_IslandManager_AssignLevels, STATGROUP_ChaosIslands);
DECLARE_CYCLE_STAT(TEXT("IslandManager::Finalize"), STAT_IslandManager_Finalize, STATGROUP_ChaosIslands);
DECLARE_CYCLE_STAT(TEXT("IslandManager::Validate"), STAT_IslandManager_Validate, STATGROUP_ChaosIslands);

namespace Chaos::CVars
{
	extern int32 ChaosSolverCollisionPositionShockPropagationIterations;
	extern int32 ChaosSolverCollisionVelocityShockPropagationIterations;
	extern bool bChaosSolverPersistentGraph;
	extern FRealSingle SmoothedPositionLerpRate;

	bool bChaosConstraintGraphValidate = (CHAOS_CONSTRAINTGRAPH_CHECK_ENABLED != 0);
	FAutoConsoleVariableRef CVarChaosConstraintGraphValidate(TEXT("p.Chaos.ConstraintGraph.Validate"), bChaosConstraintGraphValidate, TEXT("Enable per-tick ConstraintGraph validation checks/assertions"));

	/** Cvar to enable/disable the island sleeping */
	bool bChaosSolverSleepEnabled = true;
	FAutoConsoleVariableRef CVarChaosSolverSleepEnabled(TEXT("p.Chaos.Solver.Sleep.Enabled"), bChaosSolverSleepEnabled, TEXT(""));

	/** Cvar to override the sleep counter threshold if necessary */
	int32 ChaosSolverCollisionDefaultSleepCounterThreshold = 20;
	FAutoConsoleVariableRef CVarChaosSolverCollisionDefaultSleepCounterThreshold(TEXT("p.Chaos.Solver.Sleep.Defaults.SleepCounterThreshold"), ChaosSolverCollisionDefaultSleepCounterThreshold, TEXT("Default counter threshold for sleeping.[def:20]"));

	/** Cvar to override the sleep linear threshold if necessary */
	FRealSingle ChaosSolverCollisionDefaultLinearSleepThreshold = 0.001f; // .001 unit mass cm
	FAutoConsoleVariableRef CVarChaosSolverCollisionDefaultLinearSleepThreshold(TEXT("p.Chaos.Solver.Sleep.Defaults.LinearSleepThreshold"), ChaosSolverCollisionDefaultLinearSleepThreshold, TEXT("Default linear threshold for sleeping.[def:0.001]"));

	/** Cvar to override the sleep angular threshold if necessary */
	FRealSingle ChaosSolverCollisionDefaultAngularSleepThreshold = 0.0087f;  //~1/2 unit mass degree
	FAutoConsoleVariableRef CVarChaosSolverCollisionDefaultAngularSleepThreshold(TEXT("p.Chaos.Solver.Sleep.Defaults.AngularSleepThreshold"), ChaosSolverCollisionDefaultAngularSleepThreshold, TEXT("Default angular threshold for sleeping.[def:0.0087]"));

	/** The size of object for which the angular sleep threshold is defined. Large objects reduce the threshold propertionally. 0 means do not apply size scale. */
	// E.g., if ChaosSolverCollisionAngularSleepThresholdSize=100, an objects with a bounds of 500 will have 1/5x the sleep threshold.
	// We are effectively converting the angular threshold into a linear threshold calculated at the object extents.
	// @todo(chaos): male this a project setting or something
	FRealSingle ChaosSolverCollisionAngularSleepThresholdSize = 0;
	FAutoConsoleVariableRef CVarChaosSolverCollisionAngularSleepThresholdSize(TEXT("p.Chaos.Solver.Sleep.AngularSleepThresholdSize"), ChaosSolverCollisionAngularSleepThresholdSize, TEXT("Scales the angular threshold based on size (0 to disable size based scaling)"));

	/* Cvar to increase the sleep counter threshold for floating particles */
	int32 IsolatedParticleSleepCounterThresholdMultiplier = 1;
	FAutoConsoleVariableRef CVarChaosSolverIsolatedParticleSleepCounterThresholdMultiplier(TEXT("p.Chaos.Solver.Sleep.IsolatedParticle.CounterMultiplier"), IsolatedParticleSleepCounterThresholdMultiplier, TEXT("A multiplier applied to SleepCounterThreshold for floating particles"));

	/* Cvar to adjust the sleep linear threshold for floating particles */
	FRealSingle IsolatedParticleSleepLinearThresholdMultiplier = 1.0f;
	FAutoConsoleVariableRef CVarChaosSolverIsolatedParticleSleepLinearThresholdMultiplier(TEXT("p.Chaos.Solver.Sleep.IsolatedParticle.LinearMultiplier"), IsolatedParticleSleepLinearThresholdMultiplier, TEXT("A multiplier applied to SleepLinearThreshold for floating particles"));

	/* Cvar to adjust the sleep angular threshold for floating particles */
	FRealSingle IsolatedParticleSleepAngularThresholdMultiplier = 1.0f;
	FAutoConsoleVariableRef CVarChaosSolverIsolatedParticleSleepAngularThresholdMultiplier(TEXT("p.Chaos.Solver.Sleep.IsolatedParticle.AngularMultiplier"), IsolatedParticleSleepAngularThresholdMultiplier, TEXT("A multiplier applied to SleepAngularThreshold for floating particles"));
}


namespace Chaos::Private
{
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 
	// Utility Functions
	// 
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////


	bool IsParticleDynamic(const FGeometryParticleHandle* Particle)
	{
		return (Particle != nullptr) && ((Particle->ObjectState() == EObjectStateType::Dynamic) || (Particle->ObjectState() == EObjectStateType::Sleeping));
	}

	bool IsParticleSleeping(const FGeometryParticleHandle* Particle)
	{
		return (Particle != nullptr) && (Particle->ObjectState() == EObjectStateType::Sleeping);
	}

	bool WasParticleAsleep(const FGeometryParticleHandle* Particle)
	{
		if (const FPBDRigidParticleHandle* Rigid = Particle->CastToRigidParticle())
		{
			return Rigid->WasSleeping();
		}
		return false;
	}

	bool IsParticleMoving(const FGeometryParticleHandle* Particle)
	{
		bool bIsStationary = true;
		if (Particle != nullptr)
		{
			if (Particle->ObjectState() == EObjectStateType::Kinematic)
			{
				// For kinematic particles check whether their current mode and target will cause the particle to move
				// For all other modes (Velocity, None and Reset) check that the velocity is non-zero
				const FKinematicGeometryParticleHandle* Kinematic = Particle->CastToKinematicParticle();
				const FKinematicTarget& KinematicTarget = Kinematic->KinematicTarget();
				if (KinematicTarget.GetMode() == EKinematicTargetMode::Position)
				{
					bIsStationary = (Kinematic->GetX() - KinematicTarget.GetTargetPosition()).IsZero() && (Kinematic->GetR() * KinematicTarget.GetTargetRotation().Inverse()).IsIdentity();
				}
				else
				{
					bIsStationary = Kinematic->GetV().IsZero() && Kinematic->GetW().IsZero();
				}
			}
			else
			{
				bIsStationary = (Particle->ObjectState() == EObjectStateType::Static) || (Particle->ObjectState() == EObjectStateType::Sleeping);
			}
		}
		return !bIsStationary;
	}

	bool IsParticleNeedsResim(const FGeometryParticleHandle* Particle)
	{
		return (Particle != nullptr) && (Particle->SyncState() != ESyncState::InSync);
	}

	bool GetIslandParticleSleepThresholds(
		const FGeometryParticleHandle* Particle,
		const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>* PhysicsMaterials,
		const TArrayCollectionArray<TUniquePtr<FChaosPhysicsMaterial>>* PerParticlePhysicsMaterials,
		const THandleArray<FChaosPhysicsMaterial>* SimMaterials,
		FRealSingle& OutSleepLinearThreshold,
		FRealSingle& OutSleepAngularThreshold,
		int& OutSleepCounterThreshold)
	{
		OutSleepLinearThreshold = 0;
		OutSleepAngularThreshold = 0;
		OutSleepCounterThreshold = TNumericLimits<int32>::Max();

		const FPBDRigidParticleHandle* Rigid = Particle->CastToRigidParticle();
		if ((Rigid != nullptr) && (Rigid->SleepType() != ESleepType::NeverSleep))
		{
			const FRealSingle ParticleSleepThresholdMultiplier = Rigid->SleepThresholdMultiplier();

			const FChaosPhysicsMaterial* PhysicsMaterial = Private::GetFirstPhysicsMaterial(Rigid, PhysicsMaterials, PerParticlePhysicsMaterials, SimMaterials);
			if (PhysicsMaterial != nullptr)
			{
				OutSleepLinearThreshold = ParticleSleepThresholdMultiplier * FRealSingle(PhysicsMaterial->SleepingLinearThreshold);
				OutSleepAngularThreshold = ParticleSleepThresholdMultiplier * FRealSingle(PhysicsMaterial->SleepingAngularThreshold);
				OutSleepCounterThreshold = PhysicsMaterial->SleepCounterThreshold;
			}
			else
			{
				OutSleepLinearThreshold = ParticleSleepThresholdMultiplier * CVars::ChaosSolverCollisionDefaultLinearSleepThreshold;
				OutSleepAngularThreshold = ParticleSleepThresholdMultiplier * CVars::ChaosSolverCollisionDefaultAngularSleepThreshold;
				OutSleepCounterThreshold = CVars::ChaosSolverCollisionDefaultSleepCounterThreshold;
			}

			// Adjust angular threshold for size. It is equivalent to converting the angular threshold into a linear
			// movement threshold at the extreme points on the particle.
			const FRealSingle AngularSleepThresholdSize = CVars::ChaosSolverCollisionAngularSleepThresholdSize;
			if ((AngularSleepThresholdSize > 0) && Rigid->HasBounds())
			{
				const FRealSingle RigidSize = FRealSingle(Rigid->LocalBounds().Extents().GetMax());
				if (RigidSize > AngularSleepThresholdSize)
				{
					const FRealSingle ThresholdScale = AngularSleepThresholdSize / RigidSize;
					OutSleepAngularThreshold *= ThresholdScale;
				}
			}

			return true;
		}

		return false;
	}

	bool GetIsolatedParticleSleepThresholds(
		const FGeometryParticleHandle* Particle,
		const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>* PhysicsMaterials,
		const TArrayCollectionArray<TUniquePtr<FChaosPhysicsMaterial>>* PerParticlePhysicsMaterials,
		const THandleArray<FChaosPhysicsMaterial>* SimMaterials,
		FRealSingle& OutSleepLinearThreshold,
		FRealSingle& OutSleepAngularThreshold,
		int& OutSleepCounterThreshold)
	{
		if (GetIslandParticleSleepThresholds(Particle, PhysicsMaterials, PerParticlePhysicsMaterials, SimMaterials, OutSleepLinearThreshold, OutSleepAngularThreshold, OutSleepCounterThreshold))
		{
			// Sleep thresholds are tuned to hide minor collision jitter which isn't a problem for floating particles
			// so we scale the thesholds to make sleeping harder. E.g., to avoid going to sleep at the apex of an
			// upwards ballistic trajectory, or when a floating oscillating body reverses rotation, etc.
			OutSleepCounterThreshold *= FMath::Max(1, CVars::IsolatedParticleSleepCounterThresholdMultiplier);
			OutSleepLinearThreshold *= FMath::Max(0.0f, CVars::IsolatedParticleSleepLinearThresholdMultiplier);
			OutSleepAngularThreshold *= FMath::Max(0.0f, CVars::IsolatedParticleSleepAngularThresholdMultiplier);

			return true;
		}

		return false;
	}

	bool GetParticleDisableThresholds(
		const FGeometryParticleHandle* Particle,
		const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>* PhysicsMaterials,
		const TArrayCollectionArray<TUniquePtr<FChaosPhysicsMaterial>>* PerParticlePhysicsMaterials,
		const THandleArray<FChaosPhysicsMaterial>* SimMaterials,
		FRealSingle& OutDisableLinearThreshold,
		FRealSingle& OutDisableAngularThreshold)
	{
		OutDisableLinearThreshold = 0;
		OutDisableAngularThreshold = 0;

		const FPBDRigidParticleHandle* Rigid = Particle->CastToRigidParticle();
		if ((Rigid != nullptr) && (Rigid->SleepType() != ESleepType::NeverSleep))
		{
			const FChaosPhysicsMaterial* PhysicsMaterial = Private::GetFirstPhysicsMaterial(Rigid, PhysicsMaterials, PerParticlePhysicsMaterials, SimMaterials);
			if (PhysicsMaterial != nullptr)
			{
				OutDisableLinearThreshold = FRealSingle(PhysicsMaterial->DisabledLinearThreshold);
				OutDisableAngularThreshold = FRealSingle(PhysicsMaterial->DisabledAngularThreshold);
				return true;
			}
		}

		return false;
	}

	template<typename TRigidParticleHandle>
	void InitParticleSleepMetrics(TRigidParticleHandle& Rigid, FReal Dt)
	{
		if (Dt > UE_SMALL_NUMBER)
		{
			Rigid.SetVSmooth(Rigid.GetV());
			Rigid.SetWSmooth(Rigid.GetW());
		}
	}

	template<typename TRigidParticleHandle>
	void UpdateParticleSleepMetrics(TRigidParticleHandle& Rigid, FReal Dt)
	{
		if (Dt > UE_SMALL_NUMBER)
		{
			const FReal SmoothRate = FMath::Clamp(CVars::SmoothedPositionLerpRate, 0.0f, 1.0f);
			const FVec3 VImp = FVec3::CalculateVelocity(Rigid.GetX(), Rigid.GetP(), Dt);
			const FVec3 WImp = FRotation3::CalculateAngularVelocity(Rigid.GetR(), Rigid.GetQ(), Dt);
			Rigid.SetVSmooth(FMath::Lerp(Rigid.VSmooth(), VImp, SmoothRate));
			Rigid.SetWSmooth(FMath::Lerp(Rigid.WSmooth(), WImp, SmoothRate));
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 
	// FPBDIslandParticle
	// 
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////


	FPBDIslandParticle::FPBDIslandParticle()
	{
	}

	FPBDIslandParticle::FPBDIslandParticle(FGeometryParticleHandle* InParticle)
	{
		Reuse(InParticle);
	}

	FPBDIslandParticle::~FPBDIslandParticle()
	{
		Trash();
	}

	int32 FPBDIslandParticle::GetIslandId() const
	{
		if (Island != nullptr)
		{
			return Island->GetIslandId();
		}
		return INDEX_NONE;
	}

	void FPBDIslandParticle::Reuse(FGeometryParticleHandle* InParticle)
	{
		Particle = InParticle;

		if (Particle != nullptr)
		{
			Particle->SetConstraintGraphNode(this);
		}
	}

	void FPBDIslandParticle::Trash()
	{
		check((Particle == nullptr) || (Particle->GetConstraintGraphNode() == this));
		check(ArrayIndex == INDEX_NONE);
		check(Edges.IsEmpty());
		check(Island == nullptr);
		check(IslandArrayIndex == INDEX_NONE);
		check(ArrayIndex == INDEX_NONE);

		if (Particle != nullptr)
		{
			Particle->SetConstraintGraphNode(nullptr);
			Particle = nullptr;
		}

		Level = 0;
		Flags.Reset();
		VisitEpoch = INDEX_NONE;
		ResimFrame = INDEX_NONE;
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 
	// FPBDIslandConstraint
	// 
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////


	FPBDIslandConstraint::FPBDIslandConstraint()
	{
	}

	FPBDIslandConstraint::FPBDIslandConstraint(const int32 InContainerId, FConstraintHandle* InConstraint)
	{
		Reuse(InContainerId, InConstraint);
	}

	FPBDIslandConstraint::~FPBDIslandConstraint()
	{
		Trash();
	}

	int32 FPBDIslandConstraint::GetIslandId() const
	{
		if (Island != nullptr)
		{
			return Island->GetIslandId();
		}
		return INDEX_NONE;
	}

	void FPBDIslandConstraint::Reuse(const int32 InContainerId, FConstraintHandle* InConstraint)
	{
		ContainerIndex = InContainerId;
		Constraint = InConstraint;

		if (Constraint != nullptr)
		{
			Constraint->SetConstraintGraphEdge(this);

			// Initialize edge state to match constraint state
			Flags.bIsSleeping = Constraint->IsSleeping();
		}
	}

	void FPBDIslandConstraint::Trash()
	{
		check((Constraint == nullptr) || (Constraint->GetConstraintGraphEdge() == this));
		check(Nodes[0] == nullptr);
		check(Nodes[1] == nullptr);
		check(NodeArrayIndices[0] == INDEX_NONE);
		check(NodeArrayIndices[1] == INDEX_NONE);
		check(Island == nullptr);
		check(IslandArrayIndex == INDEX_NONE);
		check(ArrayIndex == INDEX_NONE);

		if (Constraint != nullptr)
		{
			Constraint->SetConstraintGraphEdge(nullptr);
			Constraint = nullptr;
		}

		VisitEpoch = INDEX_NONE;
		ContainerIndex = INDEX_NONE;
		Level = INDEX_NONE;
		Flags.Reset();
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 
	// FPBDIsland
	// 
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////


	FPBDIsland::FPBDIsland()
	{
		Reuse();
	}

	FPBDIsland::~FPBDIsland()
	{
		Trash();
	}

	void FPBDIsland::Reuse()
	{
		Flags.bIsSleepAllowed = true;
		Flags.bIsSleeping = true;
	}

	void FPBDIsland::Trash()
	{
		check(Nodes.IsEmpty());
		check(NumEdges == 0);
		check(ArrayIndex == INDEX_NONE);

		MergeSet = nullptr;
		MergeSetIslandIndex = INDEX_NONE;
		SleepCounter = 0;
		DisableCounter = 0;
		ResimFrame = INDEX_NONE;
		Flags.Reset();
	}

	void FPBDIsland::UpdateSyncState()
	{
		Flags.bNeedsResim = false;
		ResimFrame = INDEX_NONE;

		// If any of our particles want a resim, the whole island is resimmed
		for (TArray<FPBDIslandConstraint*>& Edges : ContainerEdges)
		{
			for (FPBDIslandConstraint* Edge : Edges)
			{
				for (int32 NodeIndex = 0; NodeIndex < 2; ++NodeIndex)
				{
					if (FPBDIslandParticle* EdgeNode = Edge->Nodes[NodeIndex])
					{
						Flags.bNeedsResim = ((Flags.bNeedsResim | EdgeNode->Flags.bNeedsResim) != 0);
						ResimFrame = (ResimFrame == INDEX_NONE) ? EdgeNode->ResimFrame : FMath::Min(ResimFrame, EdgeNode->ResimFrame);
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 
	// FPBDIslandMergeSet
	// 
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////


	FPBDIslandMergeSet::FPBDIslandMergeSet()
	{
		Reuse();
	}

	FPBDIslandMergeSet::~FPBDIslandMergeSet()
	{
		Trash();
	}

	void FPBDIslandMergeSet::Reuse()
	{
	}

	void FPBDIslandMergeSet::Trash()
	{
		check(ArrayIndex == INDEX_NONE);

		Islands.Reset();
		NumEdges = 0;
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 
	// FPBDIslandManager
	// 
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////


	FPBDIslandManager::FPBDIslandManager(FPBDRigidsSOAs& InParticles)
		: Particles(InParticles)
		, PhysicsMaterials(nullptr)
		, PerParticlePhysicsMaterials(nullptr)
		, SimMaterials(nullptr)
		, Nodes(1000)
		, Edges(1000)
		, Islands(1000)
		, MergeSets(1000)
	{
	}

	FPBDIslandManager::~FPBDIslandManager()
	{
	}

	int32 FPBDIslandManager::GetNextVisitEpoch()
	{
		// Don't wrap to negative because INDEX_NONE is used as "uninitialized epoch"
		if (NextVisitEpoch == TNumericLimits<int32>::Max())
		{
			NextVisitEpoch = 0;
		}
		return NextVisitEpoch++;
	}

	void FPBDIslandManager::SetIsDeterministic(const bool bInIsDeterministic)
	{
		bIsDeterministic = bInIsDeterministic;
		ApplyDeterminism();
	}

	void FPBDIslandManager::SetAssignLevels(const bool bInAssignLevels)
	{
		bAssignLevels = bInAssignLevels;
	}

	void FPBDIslandManager::AddConstraintContainer(const FPBDConstraintContainer& Container)
	{
		if (ConstraintContainers.Num() < Container.GetContainerId() + 1)
		{
			ConstraintContainers.SetNumZeroed(Container.GetContainerId() + 1, EAllowShrinking::No);
		}
		ConstraintContainers[Container.GetContainerId()] = &Container;
	}

	void FPBDIslandManager::RemoveConstraintContainer(const FPBDConstraintContainer& Container)
	{
		for (int32 EdgeIndex = Edges.Num() - 1; EdgeIndex >= 0; --EdgeIndex)
		{
			if (Edges[EdgeIndex]->ContainerIndex == Container.GetContainerId())
			{
				RemoveConstraint(Edges[EdgeIndex]->Constraint);
			}
		}

		ConstraintContainers[Container.GetContainerId()] = nullptr;
	}

	void FPBDIslandManager::SetMaterialContainers(const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>* InPhysicsMaterials, const TArrayCollectionArray<TUniquePtr<FChaosPhysicsMaterial>>* InPerParticlePhysicsMaterials, const THandleArray<FChaosPhysicsMaterial>* InSimMaterials)
	{
		PhysicsMaterials = InPhysicsMaterials;
		PerParticlePhysicsMaterials = InPerParticlePhysicsMaterials;
		SimMaterials = InSimMaterials;
	}

	void FPBDIslandManager::SetGravityForces(const FPerParticleGravity* InGravity)
	{
		Gravity = InGravity;
	}

	void FPBDIslandManager::SetDisableCounterThreshold(const int32 InDisableCounterThreshold)
	{
		DisableCounterThreshold = InDisableCounterThreshold;
	}

	int32 FPBDIslandManager::GetNumConstraintContainers() const
	{
		return ConstraintContainers.Num();
	}

	void FPBDIslandManager::Reset()
	{
		for (int32 EdgeIndex = Edges.Num() - 1; EdgeIndex >= 0; --EdgeIndex)
		{
			RemoveConstraint(Edges[EdgeIndex]->Constraint);
		}
		check(Edges.IsEmpty());

		for (int32 NodeIndex = Nodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
		{
			RemoveParticle(Nodes[NodeIndex]->Particle);
		}
		check(Nodes.IsEmpty());

		MergeSets.Reset();
		Islands.Reset();
		Edges.Reset();
		Nodes.Reset();
	}

	int32 FPBDIslandManager::ReserveParticles(const int32 InNumParticles)
	{
		const int32 NumNodes = Nodes.Num();
		Nodes.Reserve(InNumParticles);
		return FMath::Max(0, InNumParticles - NumNodes);
	}

	int32 FPBDIslandManager::GetParticleLevel(FGeometryParticleHandle* Particle) const
	{
		if (const FPBDIslandParticle* Node = GetGraphNode(Particle))
		{
			return Node->Level;
		}
		return INDEX_NONE;
	}

	void FPBDIslandManager::UpdateParticleMaterial(FGeometryParticleHandle* Particle)
	{
		if (FPBDIslandParticle* Node = GetGraphNode(Particle))
		{
			if (Node->Flags.bIsDynamic)
			{
				UpdateGraphNodeSleepSettings(Node);
			}
		}
	}

	const FPBDIsland* FPBDIslandManager::GetParticleIsland(const FGeometryParticleHandle* Particle) const
	{
		if (const FPBDIslandParticle* Node = GetGraphNode(Particle))
		{
			return Node->Island;
		}
		return nullptr;
	}

	const FPBDIsland* FPBDIslandManager::GetConstraintIsland(const FConstraintHandle* Constraint) const
	{
		if (const FPBDIslandConstraint* Edge = GetGraphEdge(Constraint))
		{
			return Edge->Island;
		}
		return nullptr;
	}

	int32 FPBDIslandManager::GetIslandIndex(const FPBDIsland* Island) const
	{
		if (Island != nullptr)
		{
			return Island->GetArrayIndex();
		}
		return INDEX_NONE;
	}

	int32 FPBDIslandManager::GetIslandArrayIndex(const FPBDIslandConstraint* Edge) const
	{
		if (Edge != nullptr)
		{
			return Edge->IslandArrayIndex;
		}
		return INDEX_NONE;
	}

	TArray<const FPBDIsland*> FPBDIslandManager::FindParticleIslands(const FGeometryParticleHandle* Particle) const
	{
		TArray<const FPBDIsland*> ParticleIslands;

		if (const FPBDIslandParticle* Node = GetGraphNode(Particle))
		{
			if (Node->Flags.bIsDynamic)
			{
				// Dynamic particles are in one island
				if (Node->Island != nullptr)
				{
					ParticleIslands.Add(Node->Island);
				}
			}
			else
			{
				// Kinematic particles are in many islands and we need to check the edges
				for (FPBDIslandConstraint* Edge : Node->Edges)
				{
					if (Edge->Island != nullptr)
					{
						ParticleIslands.AddUnique(Edge->Island);
					}
				}
			}
		}

		return ParticleIslands;
	}

	TArray<const FGeometryParticleHandle*> FPBDIslandManager::FindParticlesInIslands(const TArray<const FPBDIsland*> InIslands) const
	{
		TArray<const FGeometryParticleHandle*> IslandParticles;

		for (const FPBDIsland* Island : InIslands)
		{
			for (const TArray<FPBDIslandConstraint*>& ContainerEdges : Island->ContainerEdges)
			{
				for (const FPBDIslandConstraint* Edge : ContainerEdges)
				{
					if (Edge->Nodes[0] != nullptr)
					{
						IslandParticles.AddUnique(Edge->Nodes[0]->GetParticle());
					}
					if (Edge->Nodes[1] != nullptr)
					{
						IslandParticles.AddUnique(Edge->Nodes[1]->GetParticle());
					}
				}
			}
		}

		return IslandParticles;
	}

	TArray<const FConstraintHandle*> FPBDIslandManager::FindConstraintsInIslands(const TArray<const FPBDIsland*> InIslands, int32 ContainerId) const
	{
		TArray<const FConstraintHandle*> IslandConstraints;

		for (const FPBDIsland* Island : InIslands)
		{
			for (const FPBDIslandConstraint* Edge : Island->ContainerEdges[ContainerId])
			{
				IslandConstraints.Add(Edge->GetConstraint());
			}
		}

		return IslandConstraints;
	}

	void FPBDIslandManager::WakeParticleIslands(FGeometryParticleHandle* Particle)
	{
		// We want to wake for at least one frame (i.e., prevent re-sleeping this frame)
		const bool bIsSleepAllowed = false;

		// When we explicitly wake we reset sleep counters etc
		if (FPBDIslandParticle* Node = GetGraphNode(Particle))
		{
			// Make sure the node sleep state matches the particle state
			Node->Flags.bIsSleeping = FConstGenericParticleHandle(Particle)->IsSleeping();

			// NOTE: We could check Flags.bIsDynamic here, but checking the Island pointer means we 
			// are automatically handling the case where a kinematic with constraints on it 
			// was made dynamic right before calling WakeParticleIsland
			if (Node->Island != nullptr)
			{
				// This is a dynamic particle and we know its island
				EnqueueIslandCheckSleep(Node->Island, bIsSleepAllowed);
			}
			else
			{
				// This is a kinematic particle, visit our edges to find the islands we are in
				for (const FPBDIslandConstraint* Edge : Node->Edges)
				{
					EnqueueIslandCheckSleep(Edge->Island, bIsSleepAllowed);
				}
			}
		}

		// If we are an isolated particle we keep our own sleep counter
		// NOTE: We won't even be in the graph if we are dynamic but have no constraints
		if (FPBDRigidParticleHandle* Rigid = Particle->CastToRigidParticle())
		{
			Rigid->SetSleepCounter(0);
		}
	}

	void FPBDIslandManager::SleepParticle(FGeometryParticleHandle* Particle)
	{
		// When we explicitly sleep we must check the island for sleeping before we update constraints
		// so that constraint sleep state matched their particles' sleep state.
		const bool bIsSleepAllowed = true;
		if (FPBDIslandParticle* Node = GetGraphNode(Particle))
		{
			Node->Flags.bIsSleeping = true;

			if ((Node->Island != nullptr) && !Node->Island->Flags.bIsSleeping)
			{
				// Set the checksleep flag for processing in See UpdateParticlesCheckSleep
				EnqueueIslandCheckSleep(Node->Island, bIsSleepAllowed);
			}
		}
	}

	void FPBDIslandManager::AddParticle(FGeometryParticleHandle* Particle)
	{
		// Particles get auto-added via AddConstraint (see CreateGraphEdge, GetOrCreateGraphNode)
		// But must be explicitly removed when destroyed.
	}

	void FPBDIslandManager::RemoveParticle(FGeometryParticleHandle* Particle)
	{
		if (FPBDIslandParticle* Node = GetGraphNode(Particle))
		{
			// Islands are awakened when we disable a particle in it
			WakeNodeIslands(Node);

			// Remove all the constraints attached to the particle
			RemoveParticleConstraints(Particle);

			// Remove from our island
			RemoveNodeFromIsland(Node);

			// Destroy the node
			DestroyGraphNode(Node);

			// We should not be in the graph anymore
			check(GetGraphNode(Particle) == nullptr);
		}
	}

	void FPBDIslandManager::UpdateParticles()
	{
		SCOPE_CYCLE_COUNTER(STAT_IslandManager_UpdateParticles);

		// Process state changed from registered particles.
		// To reduce cache-misses it would be nice to iterate over transient particle handles 
		// here but the view we need (Particles.GetActiveDynamicMovingKinematicParticlesView()) 
		// actually holds non-transient handles so we lose the benefits.
		// We would also have to visit nodes that are not represented in that view
		for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
		{
			if (NodeIndex < Nodes.Num() - 1)
			{
				// Prefetch the next handle
				FPlatformMisc::Prefetch(Nodes[NodeIndex + 1]->Particle);
			}

			FPBDIslandParticle* Node = Nodes[NodeIndex];
			UpdateGraphNode(Node);
		}

		// The above loop may leave orphaned nodes in the Nodes array when dyanmics are made
		// kinematic and we have removed any kinematic-kinematic constraints. We don't 
		// handle node removal above because updating one node to kinematic can result 
		// in removal of many nodes which is awkward while iterating
		for (int32 NodeIndex = Nodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
		{
			FPBDIslandParticle* Node = Nodes[NodeIndex];
			if (Node->Edges.IsEmpty() && (Node->Island == nullptr))
			{
				// We have an orphaned node - it must be removed from the graph here because
				// all other loops are over islands and won't visit this node
				DestroyGraphNode(Node);
			}
		}
	}

	void FPBDIslandManager::AddConstraint(const int32 ContainerId, FConstraintHandle* Constraint, const TVec2<FGeometryParticleHandle*>& ConstrainedParticles)
	{
		if (GetGraphEdge(Constraint) == nullptr)
		{
			// Create the edge representing the constraint. Also creates the nodes if necessary
			if (FPBDIslandConstraint* Edge = CreateGraphEdge(ContainerId, Constraint, ConstrainedParticles))
			{
				// Put the edge/nodes into an island and flag islands for merging if necessary
				// NOTE: We do not wake the island here. It will be awake already if one of the
				// particles is awake. It both particles are asleep we do not want to wake them
				// here - it would break support for bStartAwake==false for connected bodies.
				AssignEdgeIsland(Edge);
			}
		}
	}

	void FPBDIslandManager::RemoveConstraint(FConstraintHandle* Constraint)
	{
		if (FPBDIslandConstraint* Edge = GetGraphEdge(Constraint))
		{
			FPBDIslandParticle* Node0 = Edge->Nodes[0];
			FPBDIslandParticle* Node1 = Edge->Nodes[1];

			// Wake the island when we remove a constraint
			const bool bIsSleepAllowed = true;
			EnqueueIslandCheckSleep(Edge->Island, bIsSleepAllowed);

			// Remove edge from the island
			RemoveEdgeFromIsland(Edge);

			// Destroy the edge (also disconnects it from its nodes)
			DestroyGraphEdge(Edge);
		}
	}

	void FPBDIslandManager::RemoveParticleConstraints(FGeometryParticleHandle* Particle)
	{
		if (FPBDIslandParticle* Node = GetGraphNode(Particle))
		{
			// Reverse loop because we remove as we iterate
			for (int32 EdgeIndex = Node->Edges.Num() - 1; EdgeIndex >= 0; --EdgeIndex)
			{
				FPBDIslandConstraint* Edge = Node->Edges[EdgeIndex];

				// NOTE: Destroys the edge and possibly this and other nodes (if this was their last constraint)
				RemoveConstraint(Edge->Constraint);
			}
		}
	}

	void FPBDIslandManager::RemoveParticleContainerConstraints(FGeometryParticleHandle* Particle, const int32 ContainerId)
	{
		if (FPBDIslandParticle* Node = GetGraphNode(Particle))
		{
			// Reverse loop because we remove as we iterate
			for (int32 EdgeIndex = Node->Edges.Num() - 1; EdgeIndex >= 0; --EdgeIndex)
			{
				FPBDIslandConstraint* Edge = Node->Edges[EdgeIndex];
				if (Edge->ContainerIndex == ContainerId)
				{
					// NOTE: Destroys the edge and possibly this and other nodes (if this was their last constraint)
					RemoveConstraint(Edge->Constraint);
				}
			}
		}
	}

	void FPBDIslandManager::RemoveContainerConstraints(const int32 ContainerId)
	{
		// Reverse loop because we remove as we iterate
		for (int32 EdgeIndex = Edges.Num() - 1; EdgeIndex >= 0; --EdgeIndex)
		{
			FPBDIslandConstraint* Edge = Edges[EdgeIndex];
			if (Edge->ContainerIndex == ContainerId)
			{
				RemoveConstraint(Edge->Constraint);
			}
		}
	}

	void FPBDIslandManager::RemoveAllConstraints()
	{
		// Reverse loop because we remove as we iterate
		for (int32 EdgeIndex = Edges.Num() - 1; EdgeIndex >= 0; --EdgeIndex)
		{
			FPBDIslandConstraint* Edge = Edges[EdgeIndex];
			RemoveConstraint(Edge->Constraint);
		}
	}

	void FPBDIslandManager::WakeConstraintIsland(FConstraintHandle* Constraint)
	{
		if (FPBDIslandConstraint* Edge = GetGraphEdge(Constraint))
		{
			const bool bIsSleepAllowed = false;
			EnqueueIslandCheckSleep(Edge->Island, bIsSleepAllowed);
		}
	}

	void FPBDIslandManager::UpdateIslands()
	{
		ProcessIslands();
	}

	void FPBDIslandManager::UpdateSleep(const FReal Dt)
	{
		ProcessSleep(FRealSingle(Dt));
	}

	void FPBDIslandManager::UpdateDisable(TFunctionRef<void(FPBDRigidParticleHandle*)> ParticleDisableFunctor)
	{
		ProcessDisable(ParticleDisableFunctor);
	}

	void FPBDIslandManager::EndTick()
	{
		ApplyDeterminism();
	}

	int32 FPBDIslandManager::GetParticleLevel(const FPBDIslandParticle* Node) const
	{
		if (Node != nullptr)
		{
			return Node->Level;
		}
		return INDEX_NONE;
	}

	int32 FPBDIslandManager::GetParticleColor(const FPBDIslandParticle* Node) const
	{
		return INDEX_NONE;
	}

	int32 FPBDIslandManager::GetConstraintLevel(const FPBDIslandConstraint* Edge) const
	{
		if (Edge != nullptr)
		{
			return Edge->Level;
		}
		return INDEX_NONE;
	}

	int32 FPBDIslandManager::GetConstraintColor(const FPBDIslandConstraint* Edge) const
	{
		return INDEX_NONE;
	}

	int32 FPBDIslandManager::GetParticleResimFrame(const FGeometryParticleHandle* Particle) const
	{
		if (const FPBDIslandParticle* Node = GetGraphNode(Particle))
		{
			return Node->ResimFrame;
		}
		return INDEX_NONE;
	}

	void FPBDIslandManager::SetParticleResimFrame(FGeometryParticleHandle* Particle, const int32 ResimFrame)
	{
		if (FPBDIslandParticle* Node = GetGraphNode(Particle))
		{
			Node->ResimFrame = ResimFrame;
		}
	}

	void FPBDIslandManager::ResetParticleResimFrame(const int32 ResetFrame)
	{
		for (FPBDIslandParticle* Node : Nodes)
		{
			Node->ResimFrame = ResetFrame;
		}
	}

	FPBDIslandParticle* FPBDIslandManager::GetGraphNode(const FGeometryParticleHandle* Particle) const
	{
		if (Particle != nullptr)
		{
			return Particle->GetConstraintGraphNode();
		}
		return nullptr;
	}

	FPBDIslandParticle* FPBDIslandManager::GetGraphNode(const FTransientGeometryParticleHandle& Particle) const
	{
		return Particle.GetConstraintGraphNode();
	}

	FPBDIslandParticle* FPBDIslandManager::CreateGraphNode(FGeometryParticleHandle* Particle)
	{
		if (Particle != nullptr)
		{
			check(GetGraphNode(Particle) == nullptr);

			FPBDIslandParticle* Node = Nodes.Alloc(Particle);

			// Initial state
			Node->Flags.bIsDynamic = IsParticleDynamic(Particle);
			Node->Flags.bIsSleeping = IsParticleSleeping(Particle);
			Node->Flags.bIsMoving = IsParticleMoving(Particle);
			Node->Flags.bNeedsResim = IsParticleNeedsResim(Particle);
			UpdateGraphNodeSleepSettings(Node);

			return Node;
		}
		return nullptr;
	}

	FPBDIslandParticle* FPBDIslandManager::GetOrCreateGraphNode(FGeometryParticleHandle* Particle)
	{
		FPBDIslandParticle* Node = GetGraphNode(Particle);
		if (Node == nullptr)
		{
			Node = CreateGraphNode(Particle);
		}
		return Node;
	}

	void FPBDIslandManager::DestroyGraphNode(FPBDIslandParticle* Node)
	{
		if (Node != nullptr)
		{
			// We should have already been removed from any island
			check(Node->Island == nullptr);

			Nodes.Free(Node);
		}
	}

	FPBDIslandConstraint* FPBDIslandManager::GetGraphEdge(const FConstraintHandle* Constraint)
	{
		check(Constraint != nullptr);

		return Constraint->GetConstraintGraphEdge();
	}

	const FPBDIslandConstraint* FPBDIslandManager::GetGraphEdge(const FConstraintHandle* Constraint) const
	{
		return const_cast<FPBDIslandManager*>(this)->GetGraphEdge(Constraint);
	}

	FPBDIslandConstraint* FPBDIslandManager::CreateGraphEdge(const int32 ContainerId, FConstraintHandle* Constraint, const TVec2<FGeometryParticleHandle*>& ConstrainedParticles)
	{
		check(Constraint != nullptr);
		check(GetGraphEdge(Constraint) == nullptr);

		// Make sure our particles are represented in the graph
		if (IsParticleDynamic(ConstrainedParticles[0]) || IsParticleDynamic(ConstrainedParticles[1]))
		{
			FPBDIslandParticle* Node0 = GetOrCreateGraphNode(ConstrainedParticles[0]);
			FPBDIslandParticle* Node1 = GetOrCreateGraphNode(ConstrainedParticles[1]);

			// Create the edge and bind it to the nodes
			FPBDIslandConstraint* Edge = Edges.Alloc(ContainerId, Constraint);

			BindEdgeToNodes(Edge, Node0, Node1);

			return Edge;
		}

		return nullptr;
	}

	void FPBDIslandManager::DestroyGraphEdge(FPBDIslandConstraint* Edge)
	{
		if (Edge != nullptr)
		{
			// We should have already been removed from any island
			check(Edge->Island == nullptr);

			// Disconnect from the nodes
			UnbindEdgeFromNodes(Edge);

			Edges.Free(Edge);
		}
	}

	void FPBDIslandManager::BindEdgeToNodes(FPBDIslandConstraint* Edge, FPBDIslandParticle* Node0, FPBDIslandParticle* Node1)
	{
		check(Edge != nullptr);
		check(Edge->Nodes[0] == nullptr);
		check(Edge->Nodes[1] == nullptr);
		check(Edge->Island == nullptr);

		if (Node0 != nullptr)
		{
			Edge->Nodes[0] = Node0;
			Edge->NodeArrayIndices[0] = Node0->Edges.Add(Edge);
		}
		if (Node1 != nullptr)
		{
			Edge->Nodes[1] = Node1;
			Edge->NodeArrayIndices[1] = Node1->Edges.Add(Edge);
		}

		// To retain order between ticks for constraints at the same level we assign a permanent sort key to
		// each constraint. This also allows us to be deterministic as long as constraints are added in the same order.
		Edge->LevelSortKey = NextLevelSortKey++;
	}

	void FPBDIslandManager::UnbindEdgeFromNodes(FPBDIslandConstraint* Edge)
	{
		for (int32 NodeIndex = 0; NodeIndex < 2; ++NodeIndex)
		{
			if (FPBDIslandParticle* Node = Edge->Nodes[NodeIndex])
			{
				const int32 ArrayIndex = Edge->NodeArrayIndices[NodeIndex];
				check(Node->Edges[ArrayIndex] == Edge);

				// Remove the edge from the node
				Node->Edges.RemoveAtSwap(ArrayIndex, 1, EAllowShrinking::No);
				Edge->Nodes[NodeIndex] = nullptr;
				Edge->NodeArrayIndices[NodeIndex] = INDEX_NONE;

				// Update the array index of the edge we swapped in
				if (ArrayIndex < Node->Edges.Num())
				{
					// We don't know which of the edges nodes we are...
					if (Node->Edges[ArrayIndex]->Nodes[0] == Node)
					{
						Node->Edges[ArrayIndex]->NodeArrayIndices[0] = ArrayIndex;
					}
					else if (Node->Edges[ArrayIndex]->Nodes[1] == Node)
					{
						Node->Edges[ArrayIndex]->NodeArrayIndices[1] = ArrayIndex;
					}
				}
			}
		}
	}

	void FPBDIslandManager::UpdateGraphNodeSleepSettings(FPBDIslandParticle* Node)
	{
		check(Node != nullptr);

		if (Node->Flags.bIsDynamic)
		{
			FRealSingle SleepLinearThreshold, SleepAngularThreshold;
			FRealSingle DisableLinearThreshold, DisableAngularThreshold;
			int32 SleepCounterThreshold;
			GetIslandParticleSleepThresholds(Node->Particle, PhysicsMaterials, PerParticlePhysicsMaterials, SimMaterials, SleepLinearThreshold, SleepAngularThreshold, SleepCounterThreshold);
			GetParticleDisableThresholds(Node->Particle, PhysicsMaterials, PerParticlePhysicsMaterials, SimMaterials, DisableLinearThreshold, DisableAngularThreshold);

			Node->SleepLinearThresholdSq = FMath::Square(SleepLinearThreshold);
			Node->SleepAngularThresholdSq = FMath::Square(SleepAngularThreshold);
			Node->SleepCounterThreshold = SleepCounterThreshold;
			Node->DisableLinearThresholdSq = FMath::Square(DisableLinearThreshold);
			Node->DisableAngularThresholdSq = FMath::Square(DisableAngularThreshold);
		}
	}

	void FPBDIslandManager::UpdateGraphNode(FPBDIslandParticle* Node)
	{
		check(Node != nullptr);

		const bool bWasDynamic = Node->Flags.bIsDynamic;
		const bool bWasMoving = Node->Flags.bIsMoving;
		const bool bWasSleeping = Node->Flags.bIsSleeping;
		const bool bIsDynamic = IsParticleDynamic(Node->Particle);
		const bool bIsMoving = IsParticleMoving(Node->Particle);
		const bool bIsSleeping = IsParticleSleeping(Node->Particle);
		const bool bNeedsResim = IsParticleNeedsResim(Node->Particle);

		Node->Flags.bIsDynamic = bIsDynamic;
		Node->Flags.bIsMoving = bIsMoving;
		Node->Flags.bIsSleeping = bIsSleeping;
		Node->Flags.bNeedsResim = bNeedsResim;

		// Did we change between dynamic and kinematic?
		if (bIsDynamic != bWasDynamic)
		{
			// If we have turned dynamic, we need to merge all islands we are in
			// and update our sleep settings (we don't initialize sleep settings for kinematics)
			if (bIsDynamic)
			{
				UpdateGraphNodeSleepSettings(Node);

				for (FPBDIslandConstraint* Edge : Node->Edges)
				{
					// Merge or assign node islands
					MergeNodeIslands(Edge->Nodes[0], Edge->Nodes[1]);
				}
			}

			// If we have turned kinematic, we need to be removed from our island
			// because kinematics are not directly tracked in islands
			if (!bIsDynamic)
			{
				RemoveNodeFromIsland(Node);
			}

			// Update the edge because the particle type has changed
			// NOTE: reverse iteration since we may remove edges as we go
			for (int32 EdgeIndex = Node->Edges.Num() - 1; EdgeIndex >= 0; --EdgeIndex)
			{
				UpdateGraphEdge(Node->Edges[EdgeIndex]);
			}
		}

		// If the particle changed, we need to check the island's sleep state
		// This is to cover the case where any particle is woken, or all particles are manually slept
		if (bIsDynamic && (!bWasDynamic || (bIsSleeping != bWasSleeping) || bIsMoving))
		{
			const bool bIsSleepAllowed = true;
			EnqueueIslandCheckSleep(Node->Island, bIsSleepAllowed);
		}

		// If we have a moving kinematic its island(s) cannot sleep
		if (!bIsDynamic && bIsMoving)
		{
			const bool bIsSleepAllowed = false;
			for (FPBDIslandConstraint* Edge : Node->Edges)
			{
				EnqueueIslandCheckSleep(Edge->Island, bIsSleepAllowed);
			}
		}
	}

	void FPBDIslandManager::UpdateGraphEdge(FPBDIslandConstraint* Edge)
	{
		const bool bIsDynamic0 = (Edge->Nodes[0] != nullptr) && Edge->Nodes[0]->Flags.bIsDynamic;
		const bool bIsDynamic1 = (Edge->Nodes[1] != nullptr) && Edge->Nodes[1]->Flags.bIsDynamic;
		if (!bIsDynamic0 && !bIsDynamic1)
		{
			// If we get here, both particles in the constraint are now kinematic and
			// should have already been removed from the island (see FPBDIslandManager::UpdateGraphNode)
			check((Edge->Nodes[0] == nullptr) || (Edge->Nodes[0]->Island == nullptr));
			check((Edge->Nodes[1] == nullptr) || (Edge->Nodes[1]->Island == nullptr));

			// Remove the edge from its island
			RemoveEdgeFromIsland(Edge);

			// Destroy the edge
			DestroyGraphEdge(Edge);
		}
		else
		{
			// We should be in an island already if at least one of the nodes is dynamic
			check(Edge->Island != nullptr);
		}
	}

	FPBDIsland* FPBDIslandManager::CreateIsland()
	{
		FPBDIsland* Island = Islands.Alloc();
		Island->ContainerEdges.SetNum(GetNumConstraintContainers());
		return Island;
	}

	void FPBDIslandManager::DestroyIsland(FPBDIsland* Island)
	{
		if (Island != nullptr)
		{
			check(Island->Nodes.IsEmpty());
			check(Island->NumEdges == 0);
			check(Island->MergeSetIslandIndex == INDEX_NONE);

			Islands.Free(Island);
		}
	}

	void FPBDIslandManager::AssignNodeIsland(FPBDIslandParticle* Node)
	{
		check(Node != nullptr);
		check(Node->Island == nullptr);

		FPBDIsland* Island = CreateIsland();

		AddNodeToIsland(Node, Island);
	}

	void FPBDIslandManager::AssignEdgeIsland(FPBDIslandConstraint* Edge)
	{
		check(Edge != nullptr);
		check(Edge->Island == nullptr);

		FPBDIsland* Island = MergeNodeIslands(Edge->Nodes[0], Edge->Nodes[1]);

		AddEdgeToIsland(Edge, Island);
	}

	void FPBDIslandManager::AddNodeToIsland(FPBDIslandParticle* Node, FPBDIsland* Island)
	{
		check(Island != nullptr);

		if (Node != nullptr)
		{
			// Only dynamic particles are kept in the island's node array.
			// Kinematics may be in many islands (we need to check the node's edges to visit them).
			if (Node->Flags.bIsDynamic)
			{
				Node->Island = Island;
				Node->IslandArrayIndex = Island->Nodes.Add(Node);
			}

			Island->Flags.bItemsAdded = true;

			const bool bIsSleepAllowed = true;
			EnqueueIslandCheckSleep(Island, bIsSleepAllowed);
		}
	}

	void FPBDIslandManager::RemoveNodeFromIsland(FPBDIslandParticle* Node)
	{
		if ((Node != nullptr) && (Node->Island != nullptr))
		{
			FPBDIsland* Island = Node->Island;
			const int32 ArrayIndex = Node->IslandArrayIndex;
			check(Island->Nodes[ArrayIndex] == Node);

			Island->Nodes.RemoveAtSwap(ArrayIndex, 1, EAllowShrinking::No);
			if (ArrayIndex < Island->Nodes.Num())
			{
				Island->Nodes[ArrayIndex]->IslandArrayIndex = ArrayIndex;
			}

			Island->Flags.bItemsRemoved = true;

			Node->Island = nullptr;
			Node->IslandArrayIndex = INDEX_NONE;
		}
	}

	void FPBDIslandManager::DestroyIslandNodes(FPBDIsland* Island)
	{
		// Destroy any nodes left in the island
		for (int32 IslandNodeIndex = Island->Nodes.Num() - 1; IslandNodeIndex >= 0; --IslandNodeIndex)
		{
			FPBDIslandParticle* Node = Island->Nodes[IslandNodeIndex];
			check(Node->Edges.IsEmpty());

			// If there was a dynamic particle left in the island, we need to transfer the island 
			// sleep state to it for use in ProcessParticlesSleep()
			// @todo(chaos): not great - can we clean this up?
			if (FPBDRigidParticleHandle* Rigid = Node->GetParticle()->CastToRigidParticle())
			{
				Rigid->SetSleepCounter(int8(Island->SleepCounter));
			}

			RemoveNodeFromIsland(Node);
			DestroyGraphNode(Node);
		}
	}

	void FPBDIslandManager::AddEdgeToIsland(FPBDIslandConstraint* Edge, FPBDIsland* Island)
	{
		check(Edge != nullptr);
		check(Island != nullptr);

		const int32 ContainerIndex = Edge->ContainerIndex;
		Edge->IslandArrayIndex = Island->ContainerEdges[ContainerIndex].Add(Edge);
		Edge->Island = Island;

		Island->Flags.bItemsAdded = true;
		Island->NumEdges = Island->NumEdges + 1;

		const bool bIsSleepAllowed = true;
		EnqueueIslandCheckSleep(Island, bIsSleepAllowed);
	}

	void FPBDIslandManager::RemoveEdgeFromIsland(FPBDIslandConstraint* Edge)
	{
		check(Edge != nullptr);

		if (Edge->Island != nullptr)
		{
			FPBDIsland* Island = Edge->Island;

			const int32 ContainerIndex = Edge->ContainerIndex;
			const int32 EdgeIndex = Edge->IslandArrayIndex;
			check(Island->ContainerEdges[ContainerIndex][EdgeIndex] == Edge);

			Island->ContainerEdges[ContainerIndex].RemoveAtSwap(EdgeIndex, 1, EAllowShrinking::No);
			if (EdgeIndex < Island->ContainerEdges[ContainerIndex].Num())
			{
				Island->ContainerEdges[ContainerIndex][EdgeIndex]->IslandArrayIndex = EdgeIndex;
			}

			Island->Flags.bItemsRemoved = true;
			Island->NumEdges = Island->NumEdges - 1;

			Edge->Island = nullptr;
			Edge->IslandArrayIndex = INDEX_NONE;
		}
	}

	void FPBDIslandManager::WakeNodeIslands(const FPBDIslandParticle* Node)
	{
		check(Node != nullptr);

		// We want to wake up this tick, regardless of sleep thresholds etc
		const bool bIsSleepAllowed = false;

		// Dynamic nodes are in one island, kinematic nodes may be in many
		// and we must vist their edges to discover them
		if (Node->Flags.bIsDynamic)
		{
			EnqueueIslandCheckSleep(Node->Island, bIsSleepAllowed);
		}
		else
		{
			for (FPBDIslandConstraint* Edge : Node->Edges)
			{
				EnqueueIslandCheckSleep(Edge->Island, bIsSleepAllowed);
			}
		}
	}

	void FPBDIslandManager::EnqueueIslandCheckSleep(FPBDIsland* Island, const bool bIsSleepAllowed)
	{
		if (Island != nullptr)
		{
			Island->Flags.bCheckSleep = true;
			Island->Flags.bIsSleepAllowed = (Island->Flags.bIsSleepAllowed != 0) && bIsSleepAllowed;
		}
	}

	FPBDIsland* FPBDIslandManager::MergeNodeIslands(FPBDIslandParticle* Node0, FPBDIslandParticle* Node1)
	{
		// NOTE: We only regsiter the islands for merging here. The actual merge takes place later in ProcessMerges.

		FPBDIsland* Island0 = (Node0 != nullptr) ? Node0->Island : nullptr;
		FPBDIsland* Island1 = (Node1 != nullptr) ? Node1->Island : nullptr;

		// Don't try to merge island into self
		if ((Island0 != nullptr) && (Island0 == Island1))
		{
			return Island0;
		}

		FPBDIsland* Island = nullptr;
		if ((Island0 != nullptr) && (Island1 != nullptr))
		{
			Island = MergeIslands(Island0, Island1);
		}
		else
		{
			if (Island0 != nullptr) // && (Island1 == nullptr)
			{
				Island = Island0;
				AddNodeToIsland(Node1, Island);
			}
			else if (Island1 != nullptr) // && (Island0 == nullptr)
			{
				Island = Island1;
				AddNodeToIsland(Node0, Island);
			}
			else // ((Island0 == nullptr) && (Island1 == nullptr))
			{
				// NOTE: Islands are created in the sleeping state and will be set to
				// awake if either node is moving when we add them
				Island = CreateIsland();
				AddNodeToIsland(Node0, Island);
				AddNodeToIsland(Node1, Island);
			}
		}

		return Island;
	}

	FPBDIsland* FPBDIslandManager::MergeIslands(FPBDIsland* Island0, FPBDIsland* Island1)
	{
		// NOTE: We only regsiters the islands for merging here. The actual merge takes place later in ProcessMerges.
		check(Island0 != nullptr);
		check(Island1 != nullptr);

		if (Island0 == Island1)
		{
			return Island0;
		}

		// Add the islands to a merge set, which keeps track of all the islands that have been merged into each other
		// If both islands are already assigned to a merge set, merge the merge sets (if different)
		// Otherwise add the island to the existing or created merge set
		FPBDIslandMergeSet* MergeSet0 = Island0->MergeSet;
		FPBDIslandMergeSet* MergeSet1 = Island1->MergeSet;
		if ((MergeSet0 == nullptr) || (MergeSet0 != MergeSet1))
		{
			if ((MergeSet0 != nullptr) && (MergeSet1 != nullptr))
			{
				CombineMergeSets(MergeSet0, MergeSet1);
			}
			else if (MergeSet0 != nullptr) // && (MergeSet1 == nullptr)
			{
				AddIslandToMergeSet(Island1, MergeSet0);
			}
			else if (MergeSet1 != nullptr) // && (MergeSet0 == nullptr)
			{
				AddIslandToMergeSet(Island0, MergeSet1);
			}
			else // (MergeSet0 == nullptr) && (MergSet1 == nullptr)
			{
				CreateMergeSet(Island0, Island1);
			}
		}

		return (Island0->NumEdges >= Island1->NumEdges) ? Island0 : Island1;
	}

	FPBDIslandMergeSet* FPBDIslandManager::CreateMergeSet(FPBDIsland* Island0, FPBDIsland* Island1)
	{
		FPBDIslandMergeSet* MergeSet = MergeSets.Alloc();

		AddIslandToMergeSet(Island0, MergeSet);
		AddIslandToMergeSet(Island1, MergeSet);

		return MergeSet;
	}

	void FPBDIslandManager::DestroyMergeSet(FPBDIslandMergeSet* MergeSet)
	{
		if (MergeSet != nullptr)
		{
			MergeSets.Free(MergeSet);
		}
	}

	void FPBDIslandManager::AddIslandToMergeSet(FPBDIsland* Island, FPBDIslandMergeSet* MergeSet)
	{
		check(Island != nullptr);
		check(Island->MergeSet == nullptr);

		// Bind the island to the merge set
		Island->MergeSet = MergeSet;
		Island->MergeSetIslandIndex = MergeSet->Islands.Add(Island);
		MergeSet->NumEdges += Island->NumEdges;
	}

	void FPBDIslandManager::RemoveIslandFromMergeSet(FPBDIsland* Island)
	{
		check(Island != nullptr);

		if (Island->MergeSet != nullptr)
		{
			FPBDIslandMergeSet* MergeSet = Island->MergeSet;

			// Remove from the list of islands to merge
			const int32 IslandIndex = Island->MergeSetIslandIndex;
			MergeSet->Islands.RemoveAtSwap(IslandIndex, 1, EAllowShrinking::No);
			if (IslandIndex < MergeSet->Islands.Num())
			{
				MergeSet->Islands[IslandIndex]->MergeSetIslandIndex = IslandIndex;
			}

			// Update edge count
			MergeSet->NumEdges -= Island->NumEdges;

			Island->MergeSet = nullptr;
			Island->MergeSetIslandIndex = INDEX_NONE;
		}
	}

	FPBDIslandMergeSet* FPBDIslandManager::CombineMergeSets(FPBDIslandMergeSet* MergeSetParent, FPBDIslandMergeSet* MergeSetChild)
	{
		check(MergeSetParent != nullptr);
		check(MergeSetChild != nullptr);

		if (MergeSetParent == MergeSetChild)
		{
			return MergeSetParent;
		}

		// Select the largest set as the merge parent
		// (we should merge into the set with the largest island, but this should be good enough)
		if (MergeSetParent->NumEdges < MergeSetChild->NumEdges)
		{
			Swap(MergeSetParent, MergeSetChild);
		}

		// Tell all islands about their new merge set
		int32 ChildIslandIndex = MergeSetParent->Islands.Num();
		for (FPBDIsland* Island : MergeSetChild->Islands)
		{
			Island->MergeSet = MergeSetParent;
			Island->MergeSetIslandIndex = ChildIslandIndex++;
		}

		// Add the islands to their new merge set
		MergeSetParent->Islands.Append(MoveTemp(MergeSetChild->Islands));

		// Update edge count
		MergeSetParent->NumEdges += MergeSetChild->NumEdges;

		// Destroy the emptied merge set
		DestroyMergeSet(MergeSetChild);

		return MergeSetParent;
	}

	FPBDIsland* FPBDIslandManager::GetMergeSetParentIsland(FPBDIslandMergeSet* MergeSet, int32& OutNumNodes, const TArrayView<int32>& OutNumContainerEdges)
	{
		check(MergeSet != nullptr);

		// Initialize outputs
		FPBDIsland* LargestIsland = nullptr;
		OutNumNodes = 0;
		for (int32 ContainerIndex = 0; ContainerIndex < OutNumContainerEdges.Num(); ++ContainerIndex)
		{
			OutNumContainerEdges[ContainerIndex] = 0;
		}

		// Find the largest island and count the nodes and edges from all islands
		for (FPBDIsland* Island : MergeSet->Islands)
		{
			check(Island != nullptr);

			if ((LargestIsland == nullptr) || (Island->NumEdges > LargestIsland->NumEdges))
			{
				LargestIsland = Island;
			}

			OutNumNodes += Island->Nodes.Num();

			for (int32 ContainerIndex = 0; ContainerIndex < Island->ContainerEdges.Num(); ++ContainerIndex)
			{
				OutNumContainerEdges[ContainerIndex] += Island->ContainerEdges[ContainerIndex].Num();
			}
		}

		return LargestIsland;
	}

	void FPBDIslandManager::ProcessIslands()
	{
		ProcessMerges();
		ProcessSplits();
		ProcessWakes();
		AssignLevels();
		FinalizeIslands();
	}

	void FPBDIslandManager::ProcessMerges()
	{
		SCOPE_CYCLE_COUNTER(STAT_IslandManager_MergeIslands);

		int32 NumNodes;
		TArray<int32> NumContainerEdges;
		NumContainerEdges.SetNumUninitialized(GetNumConstraintContainers());

		// Could go wide here...
		for (FPBDIslandMergeSet* MergeSet : MergeSets)
		{
			// Find the island that we will merge into (the biggest one) and count the nodes and edges
			if (FPBDIsland* ParentIsland = GetMergeSetParentIsland(MergeSet, NumNodes, MakeArrayView(NumContainerEdges)))
			{
				// Reserve space for all the nodes and edges
				ParentIsland->Nodes.Reserve(NumNodes);
				for (int32 ContainerIndex = 0; ContainerIndex < NumContainerEdges.Num(); ++ContainerIndex)
				{
					ParentIsland->ContainerEdges[ContainerIndex].Reserve(NumContainerEdges[ContainerIndex]);
				}

				// Merge each child island into the parent island
				for (FPBDIsland* ChildIsland : MergeSet->Islands)
				{
					// Reset the merge set ready for next tick. We only really need to do this on the island
					// that survives the merge, but it helps with error tracking to do it for all (see DestroyIsland)
					ChildIsland->MergeSet = nullptr;
					ChildIsland->MergeSetIslandIndex = INDEX_NONE;

					ProcessIslandMerge(ParentIsland, ChildIsland);
				}
			}
		}

		MergeSets.Reset();
	}

	void FPBDIslandManager::ProcessIslandMerge(FPBDIsland* ParentIsland, FPBDIsland* ChildIsland)
	{
		if (ParentIsland != ChildIsland)
		{
			// Combine island state
			ParentIsland->SleepCounter = FMath::Min(ParentIsland->SleepCounter, ChildIsland->SleepCounter);
			ParentIsland->Flags.bItemsAdded = true;
			ParentIsland->Flags.bItemsRemoved = !!ParentIsland->Flags.bItemsRemoved || !!ChildIsland->Flags.bItemsRemoved;
			ParentIsland->Flags.bIsSleepAllowed = !!ParentIsland->Flags.bIsSleepAllowed && !!ChildIsland->Flags.bIsSleepAllowed;
			ParentIsland->Flags.bIsSleeping = !!ParentIsland->Flags.bIsSleeping && !!ChildIsland->Flags.bIsSleeping;
			ParentIsland->Flags.bWasSleeping = !!ParentIsland->Flags.bWasSleeping || !!ChildIsland->Flags.bWasSleeping;
			ParentIsland->Flags.bCheckSleep = !!ParentIsland->Flags.bCheckSleep || !!ChildIsland->Flags.bCheckSleep;
			ParentIsland->Flags.bNeedsResim = false;	// Calculated in FinalizeIslands

			// Tell all the nodes which island they belong to and what index they will have (after we move the elements)
			int32 NextIslandArrayIndex = ParentIsland->Nodes.Num();
			for (FPBDIslandParticle* Node : ChildIsland->Nodes)
			{
				if (Node->Flags.bIsDynamic)
				{
					Node->Island = ParentIsland;
					Node->IslandArrayIndex = NextIslandArrayIndex++;
				}
			}

			// Tell all the edges which island they belong to and what index they will have (after we move the elements)
			for (int32 ContainerIndex = 0; ContainerIndex < ChildIsland->ContainerEdges.Num(); ++ContainerIndex)
			{
				int32 NextContainerEdgeIndex = ParentIsland->ContainerEdges[ContainerIndex].Num();
				for (FPBDIslandConstraint* Edge : ChildIsland->ContainerEdges[ContainerIndex])
				{
					Edge->Island = ParentIsland;
					Edge->IslandArrayIndex = NextContainerEdgeIndex++;
				}
			}

			// Move the nodes and edges to the parent
			ParentIsland->Nodes.Append(MoveTemp(ChildIsland->Nodes));
			ChildIsland->Nodes.Reset();

			for (int32 ContainerIndex = 0; ContainerIndex < ChildIsland->ContainerEdges.Num(); ++ContainerIndex)
			{
				ParentIsland->NumEdges += ChildIsland->ContainerEdges[ContainerIndex].Num();
				ParentIsland->ContainerEdges[ContainerIndex].Append(MoveTemp(ChildIsland->ContainerEdges[ContainerIndex]));

				ChildIsland->ContainerEdges[ContainerIndex].Reset();
			}
			ChildIsland->NumEdges = 0;

			// Destroy the child island
			DestroyIsland(ChildIsland);
		}
	}

	void FPBDIslandManager::ProcessSplits()
	{
		SCOPE_CYCLE_COUNTER(STAT_IslandManager_SplitIslands);

		// Could go wide here...(though a bit complicated since we create and destroy islands)
		for (int32 IslandIndex = Islands.Num() - 1; IslandIndex >= 0; --IslandIndex)
		{
			FPBDIsland* Island = Islands[IslandIndex];
			if (Island->Flags.bItemsRemoved)
			{
				ProcessIslandSplits(Island);
			}
		}
	}

	void FPBDIslandManager::ProcessIslandSplits(FPBDIsland* Island)
	{
		check(Island != nullptr);
		check(Island->Flags.bItemsRemoved);

		// Reset split tracking indicesRemo
		for (FPBDIslandParticle* Node : Island->Nodes)
		{
			Node->Island = nullptr;
			Node->IslandArrayIndex = INDEX_NONE;
		}
		for (int32 ContainerIndex = 0; ContainerIndex < Island->ContainerEdges.Num(); ++ContainerIndex)
		{
			for (FPBDIslandConstraint* Edge : Island->ContainerEdges[ContainerIndex])
			{
				Edge->Island = nullptr;
				Edge->IslandArrayIndex = INDEX_NONE;
			}
		}
		const int32 VisitEpoch = GetNextVisitEpoch();

		// Extract the nodes and reset the current island. We rebuild below.
		TArray<FPBDIslandParticle*> IslandNodes;
		Swap(IslandNodes, Island->Nodes);
		for (int32 ContainerIndex = 0; ContainerIndex < Island->ContainerEdges.Num(); ++ContainerIndex)
		{
			Island->ContainerEdges[ContainerIndex].Reset();
		}
		Island->NumEdges = 0;

		// The next island to add nodes/edges to
		FPBDIsland* CurrentIsland = Island;

		// Visit all nodes and put connected nodes into an island
		TArray<FPBDIslandParticle*> NodeQueue;
		NodeQueue.Reserve(Island->Nodes.Num());

		// We need to remember this value bacause it gets overwriten when
		// rebuilding the islands below (see AddNodeToIsland)
		const bool bCheckSleep = Island->Flags.bCheckSleep;

		for (FPBDIslandParticle* RootNode : IslandNodes)
		{
			// If we haven't seen this node yet and it is dynamic, start forming a new island
			if ((RootNode->VisitEpoch != VisitEpoch) && RootNode->Flags.bIsDynamic)
			{
				// Create an island if we need one
				if (CurrentIsland == nullptr)
				{
					CurrentIsland = CreateIsland();
				}

				// Add the root node to the island and visit all connected nodes
				AddNodeToIsland(RootNode, CurrentIsland);
				NodeQueue.Add(RootNode);
				RootNode->VisitEpoch = VisitEpoch;

				// Populate the island with all connected nodes and edges
				while (!NodeQueue.IsEmpty())
				{
					FPBDIslandParticle* NextNode = NodeQueue.Pop(EAllowShrinking::No);

					// Visit all the edges connected to the current node
					for (FPBDIslandConstraint* Edge : NextNode->Edges)
					{
						if (Edge->VisitEpoch != VisitEpoch)
						{
							// Add the edge to the current island
							AddEdgeToIsland(Edge, CurrentIsland);
							Edge->VisitEpoch = VisitEpoch;

							// Queue the other connected node for processing
							FPBDIslandParticle* EdgeOtherNode = (NextNode == Edge->Nodes[0]) ? Edge->Nodes[1] : Edge->Nodes[0];
							if ((EdgeOtherNode != nullptr) && EdgeOtherNode->Flags.bIsDynamic && (EdgeOtherNode->VisitEpoch != VisitEpoch))
							{
								AddNodeToIsland(EdgeOtherNode, CurrentIsland);
								NodeQueue.Add(EdgeOtherNode);
								EdgeOtherNode->VisitEpoch = VisitEpoch;
							}
						}
					}
				}

				// Set the island state
				CurrentIsland->SleepCounter = Island->SleepCounter;
				CurrentIsland->Flags.bItemsAdded = true;
				CurrentIsland->Flags.bItemsRemoved = true;
				CurrentIsland->Flags.bIsSleepAllowed = Island->Flags.bIsSleepAllowed;
				CurrentIsland->Flags.bIsSleeping = Island->Flags.bIsSleeping;
				CurrentIsland->Flags.bWasSleeping = Island->Flags.bWasSleeping;
				CurrentIsland->Flags.bCheckSleep = bCheckSleep;
				CurrentIsland->Flags.bNeedsResim = Island->Flags.bNeedsResim;

				// We are done with this island
				CurrentIsland = nullptr;
			}
		}
	}

	void FPBDIslandManager::AssignLevels()
	{
		SCOPE_CYCLE_COUNTER(STAT_IslandManager_AssignLevels);

		// Could go wide here...
		for (FPBDIsland* Island : Islands)
		{
			// Levels will only change when awake and when something has been added or removed
			if (!Island->Flags.bIsSleeping && (!!Island->Flags.bItemsAdded || !!Island->Flags.bItemsRemoved))
			{
				// We only need to assign levels if shock propagation is enabled (but we always sort)
				if (bAssignLevels)
				{
					AssignIslandLevels(Island);
				}
				SortIslandEdges(Island);
			}
		}
	}

	void FPBDIslandManager::AssignIslandLevels(FPBDIsland* Island)
	{
		// Reset levels: 0 means unassigned; 1 means directly attached to a kinematic
		// (default 0 is so that the SortKey work even when we don't assign levels)
		for (FPBDIslandParticle* Node : Island->Nodes)
		{
			Node->Level = 0;
		}
		for (int32 ContainerIndex = 0; ContainerIndex < Island->ContainerEdges.Num(); ++ContainerIndex)
		{
			for (FPBDIslandConstraint* Edge : Island->ContainerEdges[ContainerIndex])
			{
				Edge->Level = 0;
			}
		}

		if (Island->NumEdges < 2)
		{
			return;
		}

		TArray<FPBDIslandParticle*> NodeQueue;
		NodeQueue.Reserve(Island->Nodes.Num());

		// Initialize edge levels and populate the queue with all the level-0 nodes
		for (int32 ContainerIndex = 0; ContainerIndex < Island->ContainerEdges.Num(); ++ContainerIndex)
		{
			for (FPBDIslandConstraint* Edge : Island->ContainerEdges[ContainerIndex])
			{
				check((Edge->Nodes[0] != nullptr) || (Edge->Nodes[1] != nullptr));
				const bool bIsDynamic0 = ((Edge->Nodes[0] != nullptr) && Edge->Nodes[0]->Flags.bIsDynamic);
				const bool bIsDynamic1 = ((Edge->Nodes[1] != nullptr) && Edge->Nodes[1]->Flags.bIsDynamic);

				if (!bIsDynamic0 && (Edge->Nodes[1]->Level == 0))
				{
					Edge->Level = 1;
					Edge->Nodes[1]->Level = 1;
					NodeQueue.Add(Edge->Nodes[1]);
				}
				if (!bIsDynamic1 && (Edge->Nodes[0]->Level == 0))
				{
					Edge->Level = 1;
					Edge->Nodes[0]->Level = 1;
					NodeQueue.Add(Edge->Nodes[0]);
				}
			}
		}

		// Breadth-first visit all nodes, assign a level to the connected node, and enqeue it
		for (int32 NodeIndex = 0; NodeIndex < NodeQueue.Num(); ++NodeIndex)
		{
			FPBDIslandParticle* Node = NodeQueue[NodeIndex];
			for (FPBDIslandConstraint* Edge : Node->Edges)
			{
				if (Edge->Level == 0)
				{
					Edge->Level = Node->Level + 1;

					FPBDIslandParticle* OtherNode = (Node == Edge->Nodes[0]) ? Edge->Nodes[1] : Edge->Nodes[0];
					if ((OtherNode != nullptr) && OtherNode->Flags.bIsDynamic && (OtherNode->Level == 0))
					{
						OtherNode->Level = Node->Level + 1;
						NodeQueue.Add(OtherNode);
					}
				}
			}
		}
	}

	void FPBDIslandManager::SortIslandEdges(FPBDIsland* Island)
	{
		// Sort the constraints based on level, and then add order (within each level)
		for (int32 ContainerIndex = 0; ContainerIndex < Island->ContainerEdges.Num(); ++ContainerIndex)
		{
			Island->ContainerEdges[ContainerIndex].Sort(
				[this](const FPBDIslandConstraint& L, const FPBDIslandConstraint& R)
				{
					return L.GetSortKey() < R.GetSortKey();
				});

			int32 IslandArrayIndex = 0;
			for (FPBDIslandConstraint* Edge : Island->ContainerEdges[ContainerIndex])
			{
				Edge->IslandArrayIndex = IslandArrayIndex++;
			}
		}
	}

	void FPBDIslandManager::ProcessWakes()
	{
		for (FPBDIsland* Island : Islands)
		{
			if (Island->Flags.bCheckSleep)
			{
				// See if we should sleep or wake based on particle state and whether we are allowed to sleep
				// (islands with a moving kinematic are not allowed to sleep)
				bool bIsSleeping = Island->Flags.bIsSleepAllowed;
				if (bIsSleeping)
				{
					for (const FPBDIslandParticle* Node : Island->Nodes)
					{
						bIsSleeping = bIsSleeping && Node->Flags.bIsSleeping;
						if (!bIsSleeping)
						{
							break;
						}
					}
				}

				if (bIsSleeping != Island->Flags.bIsSleeping)
				{
					Island->Flags.bIsSleeping = bIsSleeping;
					Island->SleepCounter = 0;
				}
			}
		}
	}

	void FPBDIslandManager::FinalizeIslands()
	{
		SCOPE_CYCLE_COUNTER(STAT_IslandManager_Finalize);

		for (int32 IslandIndex = Islands.Num() - 1; IslandIndex >= 0; --IslandIndex)
		{
			FPBDIsland* Island = Islands[IslandIndex];

			// If the island is awake or just changed state, make sure all the particles and constraints agree
			// NOTE: also handles the case where we add to a sleeping island (by checking bCheckSleep)
			// @todo(chaos): we should only need to do this when the state changes...?
			if (!Island->Flags.bIsSleeping || (Island->Flags.bIsSleeping != Island->Flags.bWasSleeping) || !!Island->Flags.bCheckSleep)
			{
				PropagateIslandSleep(Island);
				Island->UpdateSyncState();
			}

			// Remove islands without constraints. NOTE: must come after PropagateIslandSleep
			// so that we wake nodes the have been left on their own after all other nodes were removed.
			if (Island->NumEdges == 0)
			{
				// Destroy nodes that are left in the island
				// NOTE: also copies back any island state needed by the particle (sleep counter)
				DestroyIslandNodes(Island);

				DestroyIsland(Island);
				continue;
			}

			// Reset of the sleep counter if the island is:
			// - Not allowed to sleep this tick
			// - Sleeping, since as soon as it wakes up we could start incrementing the counter
			// - Just woken because we may not have been asleep for a whole frame (this probably makes the previous check unnecessary)
			if (!Island->Flags.bIsSleepAllowed || !!Island->Flags.bIsSleeping || (Island->Flags.bIsSleeping != Island->Flags.bWasSleeping))
			{
				Island->SleepCounter = 0;
			}

			Island->Flags.bWasSleeping = Island->Flags.bIsSleeping;
			Island->Flags.bCheckSleep = false;
			Island->Flags.bItemsAdded = false;
			Island->Flags.bItemsRemoved = false;
			Island->ResimFrame = INDEX_NONE;
		}

		Validate();
	}

	void FPBDIslandManager::UpdateExplicitSleep()
	{
		// UpdateExplicitSleep should be called after processing physics inputs that may change the sleep/wake status of particles. 
		// It is required so that the Particle/Constraint/Island sleep states are in sync when we Integrate and Detection Collisions.
		// E.g., If we do not do this and a particle was explicitly put to sleep, it will not be integrated and no collisions will 
		// be detected. If that particle is in an island with other awake particles it will be woken immediately but will not have 
		// moved or have any collisions.

		const auto ShouldIslandSleep = [](FPBDIsland * Island) -> bool
		{
			for (FPBDIslandParticle* Node : Island->GetParticles())
			{
				const bool bIsDynamic = IsParticleDynamic(Node->GetParticle());
				const bool bIsSleeping = IsParticleSleeping(Node->GetParticle());
				if (bIsDynamic && !bIsSleeping)
				{
					return false;
				}
			}
			return true;
		};

		// If we have explicitly made some particles go to sleep (as opposed to them naturally sleeping based on low movement)
		// we must check to see if the whole island can go to sleep. This is primarily to address the issue that we do not
		// update collisions for sleeping particles and destroy collisions in awake islands that are not updated this tick.
		for (FPBDIsland* Island : Islands)
		{
			if (!!Island->Flags.bCheckSleep)
			{
				const bool bIslandShouldSleep = !!Island->Flags.bIsSleepAllowed && ShouldIslandSleep(Island);
				if (bIslandShouldSleep != Island->Flags.bIsSleeping)
				{
					Island->Flags.bIsSleeping = bIslandShouldSleep;
					Island->SleepCounter = 0;
				}

				// NOTE: we only get here if particle sleep state was changed. We need to ensure that
				// constraint sleep state matches the particle sleep state. Ideally we would only
				// do this if we know that they don't match, but that's hard to determine.
				PropagateIslandSleep(Island);

				Island->Flags.bCheckSleep = false;
			}
		}
	}

	void FPBDIslandManager::ProcessSleep(const FRealSingle Dt)
	{
		if (!CVars::bChaosSolverSleepEnabled)
		{
			return;
		}

		// Isloated particles are not kept in any island and need to be handled separately.
		ProcessParticlesSleep(Dt);

		// @todo(chaos): can go wide except for PropagateSleepState
		for (FPBDIsland* Island : Islands)
		{
			if (!Island->Flags.bIsSleeping && !!Island->Flags.bIsSleepAllowed && !Island->Flags.bIsUsingCache)
			{
				// Update the sleep state based on particle movement etc
				ProcessIslandSleep(Island, Dt);

				if (Island->Flags.bIsSleeping)
				{
					PropagateIslandSleep(Island);
				}
			}

			// This gets set to false again next tick if there are moving kinematics in the island
			Island->Flags.bIsSleepAllowed = true;
		}
	}

	void FPBDIslandManager::ProcessParticlesSleep(const FRealSingle Dt)
	{
		// Check the sleepiness of particles that are not in any islands.
		// @todo(chaos): this is very expensive because we have to search for a material in GetParticleSleepThresholds.
		// We should probably cache a particle's sleep (and disable) thresholds somewhere.
		TArray<FGeometryParticleHandle*> SleptParticles;
		TArray<FGeometryParticleHandle*> DisabledParticles;
		for (FTransientPBDRigidParticleHandle& Rigid : Particles.GetActiveDynamicMovingKinematicParticlesView())
		{
			if (Rigid.IsDynamic() && !Rigid.IsInConstraintGraph())
			{
				FRealSingle SleepLinearThreshold, SleepAngularThreshold;
				int32 SleepCounterThreshold;
				GetIsolatedParticleSleepThresholds(Rigid.Handle(), PhysicsMaterials, PerParticlePhysicsMaterials, SimMaterials, SleepLinearThreshold, SleepAngularThreshold, SleepCounterThreshold);

				// Check for sleep
				if ((SleepLinearThreshold > 0) || (SleepAngularThreshold > 0))
				{
					// NOTE: We do not use smoothed velocity for isolated particles (smoothed velocity is used to hide
					// minor collision/joint jitter and that won't be present) so we reset it for isolated particles
					InitParticleSleepMetrics(Rigid, Dt);

					int32 SleepCounter = 0;
					if (SleepCounterThreshold < TNumericLimits<int32>::Max())
					{
						// Isolated particles have a max sleep counter of 127 (to reduce counter space in the particle)
						SleepCounterThreshold = FMath::Min(SleepCounterThreshold, TNumericLimits<int8>::Max());

						// Did we exceed the velocity threshold?
						if ((Rigid.VSmooth().SizeSquared() > FMath::Square(SleepLinearThreshold)) 
							|| (Rigid.WSmooth().SizeSquared() > FMath::Square(SleepAngularThreshold)))
						{
							continue;
						}

						// If we get here we want to sleep
						// Update the counter and sleep if we exceed it
						static_assert(sizeof(decltype(Rigid.SleepCounter())) == 1, "Expected int8 for SleepCounter(). Update clamp below");
						SleepCounter = FMath::Min(int32(Rigid.SleepCounter()) + 1, int32(TNumericLimits<int8>::Max()));
						if (SleepCounter > SleepCounterThreshold)
						{
							SleptParticles.Add(Rigid.Handle());
							SleepCounter = 0;
						}
					}
					Rigid.SetSleepCounter(int8(SleepCounter));
				}
			}
		}

		if (!SleptParticles.IsEmpty())
		{
			Particles.DeactivateParticles(SleptParticles);
		}
	}

	void FPBDIslandManager::ProcessIslandSleep(FPBDIsland* Island, const FRealSingle Dt)
	{
		bool bWithinSleepThreshold = true;
		int32 SleepCounterThreshold = 0;
		for (FPBDIslandParticle* Node : Island->GetParticles())
		{
			// All zeroes means never sleep/disable
			if ((Node->SleepLinearThresholdSq <= 0) && (Node->SleepAngularThresholdSq <= 0))
			{
				bWithinSleepThreshold = false;
				break;
			}

			// Check the particle state against the thresholds
			FPBDRigidParticleHandle* Rigid = Node->GetParticle()->CastToRigidParticle();
			if (Rigid == nullptr)
			{
				continue;
			}
			
			UpdateParticleSleepMetrics(*Rigid, Dt);

			// Did we exceed the velocity threshold?
			if ((Rigid->VSmooth().SizeSquared() > Node->SleepLinearThresholdSq)
				|| (Rigid->WSmooth().SizeSquared() > Node->SleepAngularThresholdSq))
			{
				bWithinSleepThreshold = false;

				// NOTE: We will not sleep if any particle exceeds the threshold, so we could "break" here.
				// However we still want to update the SleepMetrics for all particles because we want to
				// update the smoothed velocity based on current state, so we must continue to remaining particles
				continue;
			}

			// Take the longest sleep time
			SleepCounterThreshold = FMath::Max(SleepCounterThreshold, Node->SleepCounterThreshold);
		}

		int32 SleepCounter = 0;
		if (bWithinSleepThreshold)
		{
			// If we get here we may want to sleep
			SleepCounter = Island->SleepCounter + 1;
			if (SleepCounter > SleepCounterThreshold)
			{
				Island->Flags.bIsSleeping = true;
				SleepCounter = 0;
			}
		}
		Island->SleepCounter = SleepCounter;
	}

	void FPBDIslandManager::PropagateIslandSleep(FPBDIsland* Island)
	{
		PropagateIslandSleepToParticles(Island);
		PropagateIslandSleepToConstraints(Island);
	}

	void FPBDIslandManager::PropagateIslandSleepToParticles(FPBDIsland* Island)
	{
		bool bRebuildViews = false;
		for (FPBDIslandParticle* IslandNode : Island->Nodes)
		{
			if (Island->Flags.bIsSleeping != IslandNode->Flags.bIsSleeping)
			{
				FGeometryParticleHandle* Particle = IslandNode->GetParticle();

				// Put to sleep...
				if (Island->Flags.bIsSleeping)
				{
					Particles.DeactivateParticle(Particle, true);
				}

				// Wake up...
				if (!Island->Flags.bIsSleeping)
				{
					Particles.ActivateParticle(Particle, true);

					// When we wake particles, we have skipped their integrate step which causes some issues:
					//	- we have zero velocity (no gravity or external forces applied)
					//	- the world transforms cached in the ShapesArray will be at the last post-integrate positions
					//	  which doesn't match what the velocity is telling us
					// This causes problems for the solver - essentially we have an "initial overlap" situation.
					// @todo(chaos): We could just run (partial) integrate here for this particle, but we don't know about the Evolution - fix this
					// Better solution: Leave Particle X and P in current state for sleeping particles (rather than setting X = P)
					for (const TUniquePtr<FPerShapeData>& Shape : Particle->ShapesArray())
					{
						Shape->UpdateLeafWorldTransform(Particle);
					}
				}

				IslandNode->Flags.bIsSleeping = Island->Flags.bIsSleeping;
				bRebuildViews = true;
			}
		}

		// Update views if we changed particle state
		if (bRebuildViews)
		{
			Particles.RebuildViews();
		}
	}

	void FPBDIslandManager::PropagateIslandSleepToConstraints(FPBDIsland* Island)
	{
		// Set the constraint sleep state to match
		for (TArray<FPBDIslandConstraint*>& IslandEdges : Island->ContainerEdges)
		{
			for (FPBDIslandConstraint* IslandEdge : IslandEdges)
			{
				if (Island->Flags.bIsSleeping != IslandEdge->Flags.bIsSleeping)
				{
					IslandEdge->GetConstraint()->SetIsSleeping(Island->Flags.bIsSleeping);
					IslandEdge->Flags.bIsSleeping = Island->Flags.bIsSleeping;
				}
			}
		}
	}

	void FPBDIslandManager::ProcessDisable(TFunctionRef<void(FPBDRigidParticleHandle*)> ParticleDisableFunctor)
	{
		// @todo(chaos): parallelize
		if (DisableCounterThreshold == TNumericLimits<int32>::Max())
		{
			return;
		}

		TArray<FPBDRigidParticleHandle*> DisableParticles;

		// Check to see if islolated particles (not in an island) need to be disabled
		// We prefer to iterate over islands because nodes in the graph cache their thresholds
		ProcessParticlesDisable(DisableParticles);

		// Check islands to see if we want to disable the particles in them
		for (FPBDIsland* Island : Islands)
		{
			if (!Island->Flags.bIsSleeping && !Island->IsUsingCache())
			{
				// Update the sleep state based on particle movement etc
				ProcessIslandDisable(Island, DisableParticles);
			}
		}

		// Disable the particles we identified
		for (FPBDRigidParticleHandle* Particle : DisableParticles)
		{
			if (Particle != nullptr)
			{
				ParticleDisableFunctor(Particle);
			}
		}
	}

	void FPBDIslandManager::ProcessParticlesDisable(TArray<FPBDRigidParticleHandle*>& OutDisableParticles)
	{
		// Check all isolated particle (not in any island)
		for (FTransientPBDRigidParticleHandle& Particle : Particles.GetActiveDynamicMovingKinematicParticlesView())
		{
			if (Particle.IsDynamic() && !Particle.IsInConstraintGraph())
			{
				// @todo(chaos): this is very expensive becuase we have to search materials.
				// However this system is only used by Fields via PerParticlePhysicsMaterials, and not exposed to the editor.
				// Maybe we should just skip looking further if no PerParticlePhysicsMaterials is assigned.
				FRealSingle DisableLinearThreshold, DisableAngularThreshold;
				GetParticleDisableThresholds(Particle.Handle(), PhysicsMaterials, PerParticlePhysicsMaterials, SimMaterials, DisableLinearThreshold, DisableAngularThreshold);

				// Check for disable
				if ((DisableLinearThreshold > 0) || (DisableAngularThreshold > 0))
				{
					int32 DisableCounter = 0;

					const FReal VSq = Particle.VSmooth().SizeSquared();
					const FReal WSq = Particle.WSmooth().SizeSquared();
					if ((VSq < FMath::Square(DisableLinearThreshold)) && (WSq < FMath::Square(DisableAngularThreshold)))
					{
						DisableCounter = Particle.DisableCounter() + 1;
						if (DisableCounter > DisableCounterThreshold)
						{
							OutDisableParticles.Add(Particle.Handle());
						}
					}

					Particle.SetDisableCounter(int8(DisableCounter));
				}
			}
		}
	}

	void FPBDIslandManager::ProcessIslandDisable(FPBDIsland* Island, TArray<FPBDRigidParticleHandle*>& OutDisableParticles)
	{
		for (FPBDIslandParticle* Node : Island->GetParticles())
		{
			// All zeroes means never disable
			if ((Node->DisableLinearThresholdSq <= 0) && (Node->DisableAngularThresholdSq <= 0))
			{
				continue;
			}

			if (FPBDRigidParticleHandle* Particle = Node->GetParticle()->CastToRigidParticle())
			{
				int32 DisableCounter = 0;

				// Did we exceed the velocity thresholds?
				const FReal VSq = Particle->GetV().SizeSquared();
				const FReal WSq = Particle->GetW().SizeSquared();
				if ((VSq < Node->DisableLinearThresholdSq) && (WSq < Node->DisableAngularThresholdSq))
				{
					// We are within the velocity thresholds, so see if we should disable
					DisableCounter = Particle->DisableCounter() + 1;
					if (DisableCounter > DisableCounterThreshold)
					{
						OutDisableParticles.Add(Particle);
					}
				}

				Particle->SetDisableCounter(int8(DisableCounter));
			}
		}
	}

	void FPBDIslandManager::ApplyDeterminism()
	{
		// Nothing to do as long as we sorted constraints
	}

	bool FPBDIslandManager::Validate() const
	{
		SCOPE_CYCLE_COUNTER(STAT_IslandManager_Validate);

		if (!CVars::bChaosConstraintGraphValidate)
		{
			return true;
		}

		bool bIsValid = true;

		// Make sure we have processed the merges etc
		CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(MergeSets.IsEmpty());

		// Check various Node properties
		for (FPBDIslandParticle* Node : Nodes)
		{
			// All dynamic nodes should be in an island
			CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(!Node->Flags.bIsDynamic || (Node->Island != nullptr));

			// Kinematic nodes must not be in an island
			CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Node->Flags.bIsDynamic || (Node->Island == nullptr));

			// Nodes without edges should have been removed
			// Actually kinematics with no edges may now remain in the graph until the
			// next UpdateParticles (i.e., the next tick) after their edges were removed
			CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(!Node->Flags.bIsDynamic || (Node->Edges.Num() > 0));

			if (Node->Island != nullptr)
			{
				// Make sure the island exists
				CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Islands[Node->Island->ArrayIndex] == Node->Island);

				// Make sure the island knows about the node
				CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Node->Island->Nodes.Contains(Node));
				CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Node->Island->Nodes[Node->IslandArrayIndex] == Node);

				// Make sure the particle sleep state is correct
				if (Node->Island->Flags.bIsSleeping)
				{
					CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Node->Particle->Sleeping())
				}
			}

			// Make sure the edge points to the node and that we're in the same island
			for (FPBDIslandConstraint* Edge : Node->Edges)
			{
				CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST((Edge->Nodes[0] == Node) || (Edge->Nodes[1] == Node));

				if (Node->Flags.bIsDynamic)
				{
					CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Node->Island == Edge->Island);
				}
			}

			// Particle sleep state should match
			CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Node->Flags.bIsSleeping == IsParticleSleeping(Node->Particle));
		}

		// Check various Edge properties
		for (FPBDIslandConstraint* Edge : Edges)
		{
			// Make sure nodes know about the edge
			CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST((Edge->Nodes[0] == nullptr) || Edge->Nodes[0]->Edges.Contains(Edge));
			CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST((Edge->Nodes[1] == nullptr) || Edge->Nodes[1]->Edges.Contains(Edge));

			// All edges should be in an island (even inactive edges between two kinematics)
			const bool bIsInIsland = (Edge->Island != nullptr);
			CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(bIsInIsland);

			if (bIsInIsland)
			{
				// Make sure the island exists
				CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Islands[Edge->Island->ArrayIndex] == Edge->Island);

				// Make sure the island edge index is correct
				CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Edge->Island->ContainerEdges[Edge->ContainerIndex][Edge->IslandArrayIndex] == Edge);

				// Make sure the island and edge sleep states match
				if (Edge->Constraint->SupportsSleeping())
				{
					CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Edge->Island->Flags.bIsSleeping == Edge->Flags.bIsSleeping);
				}
			}

			// Constraint sleep state should match edge
			if (Edge->Constraint->SupportsSleeping())
			{
				CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Edge->Flags.bIsSleeping == Edge->Constraint->IsSleeping());
			}
		}

		// Check various Island properties
		for (FPBDIsland* Island : Islands)
		{
			// We are done with merges
			CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Island->MergeSet == nullptr);

			// Island wake flag was reset
			CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Island->Flags.bCheckSleep == false);

			// No dupes in the lists
			for (FPBDIslandParticle* Node0 : Island->Nodes)
			{
				int32 Count = 0;
				for (FPBDIslandParticle* Node1 : Island->Nodes)
				{
					if (Node0 == Node1)
					{
						++Count;
					}
				}
				CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Count == 1);
			}
			for (TArray<FPBDIslandConstraint*> IslandContainerEdges : Island->ContainerEdges)
			{
				for (FPBDIslandConstraint* Edge0 : IslandContainerEdges)
				{
					int32 Count = 0;
					for (FPBDIslandConstraint* Edge1 : IslandContainerEdges)
					{
						if (Edge0 == Edge1)
						{
							++Count;
						}
					}
					CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(Count == 1);
				}
			}

			// Edge Count is correct
			int32 NumIslandEdges = 0;
			for (TArray<FPBDIslandConstraint*> IslandContainerEdges : Island->ContainerEdges)
			{
				NumIslandEdges += IslandContainerEdges.Num();
			}
			CHAOS_CONSTRAINTGRAPH_VALIDATE_TEST(NumIslandEdges == Island->NumEdges);
		}

		return true;
	}

#if CHAOS_DEBUG_DRAW
	void FPBDIslandManager::DebugDrawSleepState(const DebugDraw::FChaosDebugDrawSettings* DebugDrawSettings) const
	{
		// Loop over isolated particles
		for (FTransientPBDRigidParticleHandle& Rigid : Particles.GetActiveDynamicMovingKinematicParticlesView())
		{
			if (Rigid.IsDynamic() && !Rigid.IsInConstraintGraph())
			{
				FColor Color = FColor(128, 128, 128);

				if (!Rigid.IsSleeping())
				{
					FRealSingle SleepLinearThreshold, SleepAngularThreshold;
					int32 SleepCounterThreshold;
					GetIsolatedParticleSleepThresholds(Rigid.Handle(), PhysicsMaterials, PerParticlePhysicsMaterials, SimMaterials, SleepLinearThreshold, SleepAngularThreshold, SleepCounterThreshold);

					// Check for sleep
					if ((SleepLinearThreshold > 0) || (SleepAngularThreshold > 0))
					{
						if (SleepCounterThreshold < TNumericLimits<int32>::Max())
						{
							// Isolated particles have a max sleep counter of 127 (to reduce counter space in the particle)
							SleepCounterThreshold = FMath::Min(SleepCounterThreshold, TNumericLimits<int8>::Max());

							// Did we exceed the velocity threshold?
							const bool bIsParticlePreventingSleep = ((Rigid.VSmooth().SizeSquared() > FMath::Square(SleepLinearThreshold)) || (Rigid.WSmooth().SizeSquared() > FMath::Square(SleepAngularThreshold)));
							Color = (bIsParticlePreventingSleep) ? FColor::Red : FColor::Green;
						}
					}
				}

				DebugDraw::DrawParticleShapes(FRigidTransform3::Identity, Rigid.Handle(), Color, DebugDrawSettings);
			}
		}

		// Loop over particles in islands
		for (FPBDIsland* Island : Islands)
		{
			for (FPBDIslandParticle* Node : Island->GetParticles())
			{
				FColor Color = FColor(128, 128, 128);

				if (!Island->Flags.bIsSleeping)
				{
					// All zeroes means never sleep/disable
					if ((Node->SleepLinearThresholdSq > 0) || (Node->SleepAngularThresholdSq > 0))
					{
						// Check the particle state against the thresholds
						if (FPBDRigidParticleHandle* Rigid = Node->GetParticle()->CastToRigidParticle())
						{
							// Did we exceed the velocity threshold?
							const bool bIsParticlePreventingSleep = ((Rigid->VSmooth().SizeSquared() > Node->SleepLinearThresholdSq) || (Rigid->WSmooth().SizeSquared() > Node->SleepAngularThresholdSq));
							Color = (bIsParticlePreventingSleep) ? FColor::Red : FColor::Green;
						}
					}
				}

				DebugDraw::DrawParticleShapes(FRigidTransform3::Identity, Node->GetParticle(), Color, DebugDrawSettings);
			}
		}
	}
#endif

} // namsepace Chaos::Private
