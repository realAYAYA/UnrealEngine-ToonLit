// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Island/IslandManager.h"
#include "Chaos/Island/SolverIsland.h"
#include "Chaos/Island/IslandGraph.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/ConstraintHandle.h"
#include "Chaos/PBDConstraintContainer.h"
#include "Chaos/PBDRigidParticles.h"
#include "Chaos/PBDRigidsSOAs.h"
#include "ChaosLog.h"

#include "Framework/Threading.h"
#include "Misc/App.h"

//PRAGMA_DISABLE_OPTIMIZATION

namespace Chaos
{

/** Cvar to enable/disable the island sleeping */
bool bChaosSolverSleepEnabled = true;
FAutoConsoleVariableRef CVarChaosSolverSleepEnabled(TEXT("p.Chaos.Solver.SleepEnabled"), bChaosSolverSleepEnabled, TEXT(""));

/** Cvar to override the sleep counter threshold if necessary */
int32 ChaosSolverCollisionDefaultSleepCounterThresholdCVar = 20;
FAutoConsoleVariableRef CVarChaosSolverCollisionDefaultSleepCounterThreshold(TEXT("p.ChaosSolverCollisionDefaultSleepCounterThreshold"), ChaosSolverCollisionDefaultSleepCounterThresholdCVar, TEXT("Default counter threshold for sleeping.[def:20]"));

/** Cvar to override the sleep linear threshold if necessary */
FRealSingle ChaosSolverCollisionDefaultLinearSleepThresholdCVar = 0.001f; // .001 unit mass cm
FAutoConsoleVariableRef CVarChaosSolverCollisionDefaultLinearSleepThreshold(TEXT("p.ChaosSolverCollisionDefaultLinearSleepThreshold"), ChaosSolverCollisionDefaultLinearSleepThresholdCVar, TEXT("Default linear threshold for sleeping.[def:0.001]"));

/** Cvar to override the sleep angular threshold if necessary */
FRealSingle ChaosSolverCollisionDefaultAngularSleepThresholdCVar = 0.0087f;  //~1/2 unit mass degree
FAutoConsoleVariableRef CVarChaosSolverCollisionDefaultAngularSleepThreshold(TEXT("p.ChaosSolverCollisionDefaultAngularSleepThreshold"), ChaosSolverCollisionDefaultAngularSleepThresholdCVar, TEXT("Default angular threshold for sleeping.[def:0.0087]"));

bool bChaosSolverValidateGraph = (CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED != 0);
FAutoConsoleVariableRef CVarChaosSolverValidateGraph(TEXT("p.Chaos.Solver.ValidateGraph"), bChaosSolverValidateGraph, TEXT(""));

bool bChaosSolverPersistentGraph = true;
FAutoConsoleVariableRef CVarChaosSolverPersistentGraph(TEXT("p.Chaos.Solver.PersistentGraph"), bChaosSolverPersistentGraph, TEXT(""));

extern int32 GSingleThreadedPhysics;

	
/** Check if a particle is dynamic or sleeping */
FORCEINLINE bool IsDynamicParticle(const FGeometryParticleHandle* ParticleHandle)
{
	return (ParticleHandle->ObjectState() == EObjectStateType::Dynamic) ||
		   (ParticleHandle->ObjectState() == EObjectStateType::Sleeping);
}

/** Check if a particle is not moving */
bool IsStationaryParticle(const FGeometryParticleHandle* ParticleHandle)
{
	if (ParticleHandle->ObjectState() == EObjectStateType::Kinematic)
	{
		// For kinematic particles check whether their current mode and target will cause the particle to move
		const FKinematicGeometryParticleHandle* KinematicParticle = ParticleHandle->CastToKinematicParticle();
		if (KinematicParticle->KinematicTarget().GetMode() == EKinematicTargetMode::Position)
		{
			return (KinematicParticle->X() - KinematicParticle->KinematicTarget().GetTargetPosition()).IsZero()
					&& (KinematicParticle->R() * KinematicParticle->KinematicTarget().GetTargetRotation().Inverse()).IsIdentity();
		}
		// For all other modes (Velocity, None and Reset) check that the velocity is non-zero
		else
		{
			return KinematicParticle->V().IsZero() && KinematicParticle->W().IsZero();
		}
	}
	else
	{
		return (ParticleHandle->ObjectState() == EObjectStateType::Static) ||
			   (ParticleHandle->ObjectState() == EObjectStateType::Sleeping);
	}
}

/** Check if a particle is dynamic or sleeping */
inline const FChaosPhysicsMaterial* GetPhysicsMaterial(const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>& ParticleMaterialAttributes,
														    const THandleArray<FChaosPhysicsMaterial>& SolverPhysicsMaterials, FPBDRigidParticleHandle* RigidParticleHandle)
{
	const FChaosPhysicsMaterial* PhysicsMaterial = RigidParticleHandle->AuxilaryValue(ParticleMaterialAttributes).Get();
	if (!PhysicsMaterial && RigidParticleHandle->ShapesArray().Num())
	{
		if (FPerShapeData* PerShapeData = RigidParticleHandle->ShapesArray()[0].Get())
		{
			if (PerShapeData->GetMaterials().Num())
			{
				PhysicsMaterial = SolverPhysicsMaterials.Get(PerShapeData->GetMaterials()[0].InnerHandle);
			}
		}
	}
	return PhysicsMaterial;
}

/** Check if an island is sleeping or not given a linear/angular velocities and sleeping thresholds */
inline bool IsIslandSleeping(const FReal MaxLinearSpeed2, const FReal MaxAngularSpeed2,
								  const FReal LinearSleepingThreshold, const FReal AngularSleepingThreshold,
								  const int32 CounterThreshold, int32& SleepCounter)
{
	const FReal MaxLinearSpeed = FMath::Sqrt(MaxLinearSpeed2);
	const FReal MaxAngularSpeed = FMath::Sqrt(MaxAngularSpeed2);

	if (MaxLinearSpeed < LinearSleepingThreshold && MaxAngularSpeed < AngularSleepingThreshold)
	{
		if (SleepCounter >= CounterThreshold)
		{
			return true;
		}
		else
		{
			SleepCounter++;
		}
	}
	else
	{
		SleepCounter = 0;
	}
	return false;
}
	
/** Compute sleeping thresholds given a solver island  */
inline bool ComputeSleepingThresholds(FPBDIsland* Island,
	const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>& PerParticleMaterialAttributes,
	const THandleArray<FChaosPhysicsMaterial>& SolverPhysicsMaterials, FReal& LinearSleepingThreshold,
	FReal& AngularSleepingThreshold, FReal& MaxLinearSpeed2, FReal& MaxAngularSpeed2, int32& SleepCounterThreshold)
{
	LinearSleepingThreshold = FLT_MAX;  
	AngularSleepingThreshold = FLT_MAX; 
	MaxLinearSpeed2 = 0.f; 
	MaxAngularSpeed2 = 0.f;
	SleepCounterThreshold = 0;

	bool bHaveSleepThreshold = false;
	for (FPBDIslandParticle& IslandParticle : Island->GetParticles())
	{
		FGeometryParticleHandle* ParticleHandle = IslandParticle.GetParticle();
		if (ParticleHandle)
		{
			if (FPBDRigidParticleHandle* PBDRigid = ParticleHandle->CastToRigidParticle())
			{
				// Should we change this condition to be if (!IsStationaryParticle(ParticleHandle))
				// to be in sync with what is done for the graph island sleeping flag?
				if	(IsDynamicParticle(ParticleHandle) && !PBDRigid->Sleeping())
				{
					// If any body in the island is not allowed to sleep, the whole island cannot sleep
					// @todo(chaos): if this is a common thing, we should set a flag on the island when it has a particle
					// with this property enabled and skip the sleep check altogether
					if (PBDRigid->SleepType() == ESleepType::NeverSleep)
					{
						return false;
					}

					bHaveSleepThreshold = true;

					MaxLinearSpeed2 = FMath::Max(PBDRigid->VSmooth().SizeSquared(), MaxLinearSpeed2);
					MaxAngularSpeed2 = FMath::Max(PBDRigid->WSmooth().SizeSquared(), MaxAngularSpeed2);

					const FChaosPhysicsMaterial* PhysicsMaterial = GetPhysicsMaterial(PerParticleMaterialAttributes, SolverPhysicsMaterials, PBDRigid);

					const FReal LocalSleepingLinearThreshold = PhysicsMaterial ? PhysicsMaterial->SleepingLinearThreshold :
										ChaosSolverCollisionDefaultLinearSleepThresholdCVar;
					const FReal LocalAngularSleepingThreshold = PhysicsMaterial ? PhysicsMaterial->SleepingAngularThreshold :
										ChaosSolverCollisionDefaultAngularSleepThresholdCVar;
					const int32 LocalSleepCounterThreshold = PhysicsMaterial ? PhysicsMaterial->SleepCounterThreshold :
										ChaosSolverCollisionDefaultSleepCounterThresholdCVar;

					LinearSleepingThreshold = FMath::Min(LinearSleepingThreshold, LocalSleepingLinearThreshold);
					AngularSleepingThreshold = FMath::Min(AngularSleepingThreshold, LocalAngularSleepingThreshold);
					SleepCounterThreshold = FMath::Max(SleepCounterThreshold, LocalSleepCounterThreshold);
				}
			}
		}
	}
	return bHaveSleepThreshold;
}

FPBDIslandManager::FPBDIslandManager() 
	: Islands()
	, IslandGraph(MakeUnique<GraphType>())
	, MaxParticleIndex(INDEX_NONE)
	, bIslandsPopulated(false)
	, bEndTickCalled(true)
{
	IslandGraph->SetOwner(this);
}

FPBDIslandManager::~FPBDIslandManager()
{
}

void FPBDIslandManager::Reset()
{
	RemoveConstraints();
	RemoveParticles();

	Islands.Reset();
}

void FPBDIslandManager::AddConstraintContainer(FPBDConstraintContainer& ConstraintContainer)
{
	const int32 ContainerId = ConstraintContainer.GetContainerId();

	if (ConstraintContainers.Num() <= ContainerId)
	{
		ConstraintContainers.SetNum(ContainerId + 1);
	}

	ConstraintContainers[ContainerId] = &ConstraintContainer;
}

void FPBDIslandManager::InitializeGraph(const TParticleView<FPBDRigidParticles>& PBDRigids)
{
	// If we hit this, there's a missing call to EndTick since the previous tick
	ensure(bEndTickCalled);
	bEndTickCalled = false;

	// @todo(chaos): make this handle when the user sleeps all particles in an island. Currently this
	// will put the island to sleep as expected, but only after removing all the constraints from the island.
	// The result is that the particles in the island will have no collisions on the first frame after waking.
	// See unit test: DISABLED_TestConstraintGraph_ParticleSleep_Manual

	MaxParticleIndex = 0;
	ReserveParticles(PBDRigids.Num());

	// Reset the island merge tracking data - adding particles may cause islands to merge (e.g., if a particle has switched from kinematic to dynamic
	// @todo(chaos): put this into a method on the graph. Or maybe this isn't necessary since Merge/Split should always leave a valid empty state...
	for (auto& GraphIsland : IslandGraph->GraphIslands)
	{
		GraphIsland.ChildrenIslands.Reset();
		GraphIsland.ParentIsland = INDEX_NONE;
	}

	// We update the valid/steady state of the nodes in case any state changed. We don't need to do this for newly added particles, 
	// so we do it before AddParticles and AddConstraints
	// @todo(chaos): can we do this when the properties change instead?
	for(int32 NodeIndex = 0, NumNodes = IslandGraph->GraphNodes.GetMaxIndex(); NodeIndex < NumNodes; ++NodeIndex)
	{
		if (IslandGraph->GraphNodes.IsValidIndex(NodeIndex))
		{
			FGeometryParticleHandle* ParticleHandle = IslandGraph->GraphNodes[NodeIndex].NodeItem;

			IslandGraph->UpdateNode(NodeIndex, IsDynamicParticle(ParticleHandle), IsStationaryParticle(ParticleHandle));
		}
	}

	if (!bChaosSolverPersistentGraph)
	{
		// We are adding all the particles from the solver in case some were just created/activated
		for (auto& RigidParticle : PBDRigids)
		{
			AddParticle(RigidParticle.Handle());
		}

		// NOTE: This only removes constraints from awake islands and it leaves the sleeping ones. This is important for 
		// semi-persistent collisions because we do not run collision detection on sleeping particles, and we need the graph
		// to keep it's sleeping edges so that we know what collisions to wake when an island is awakened. The constraint
		// management system also uses the sleeping state as a lock to prevent constraint destruction.
		IslandGraph->RemoveAllAwakeEdges();
	}
}

void FPBDIslandManager::EndTick()
{
	// Islands are transient for use during the solver phase - clear them at the end of the frame
	for (auto& Island : Islands)
	{
		Island->ClearParticles();
		Island->ClearConstraints();
	}

	// We can no longer use the Island's particle and constraint lists
	bIslandsPopulated = false;

	bEndTickCalled = true;
}

void FPBDIslandManager::RemoveParticles()
{
	RemoveConstraints();

	IslandGraph->ResetNodes();

	for (auto& Island : Islands)
	{
		Island->ClearParticles();
	}
}

void FPBDIslandManager::RemoveConstraints()
{
	IslandGraph->ResetEdges();

	for(auto& Island : Islands)
	{
		Island->ClearConstraints();
		Island->ClearParticles();
	}
}

int32 FPBDIslandManager::ReserveParticles(const int32 NumParticles)
{
	const int32 MaxIndex = IslandGraph->MaxNumNodes();
	IslandGraph->ReserveNodes(NumParticles);
	
	Islands.Reserve(NumParticles);
	IslandIndexing.Reserve(NumParticles);
	
	return FMath::Max(0,NumParticles - MaxIndex);
}

void FPBDIslandManager::ReserveConstraints(const int32 NumConstraints)
{
	IslandGraph->ReserveEdges(NumConstraints);
}

// Callback from the IslandGraph to allow us to store the node index
void FPBDIslandManager::GraphNodeAdded(FGeometryParticleHandle* ParticleHandle, const int32 NodeIndex)
{
	check(ParticleHandle->ConstraintGraphIndex() == INDEX_NONE);

	ParticleHandle->SetConstraintGraphIndex(NodeIndex);
}

void FPBDIslandManager::GraphNodeRemoved(FGeometryParticleHandle* ParticleHandle)
{
	check(ParticleHandle->ConstraintGraphIndex() != INDEX_NONE);

	ParticleHandle->SetConstraintGraphIndex(INDEX_NONE);
}

// Callback from the IslandGraph to allow us to store the edge index
void FPBDIslandManager::GraphEdgeAdded(const FConstraintHandleHolder& ConstraintHandle, const int32 EdgeIndex)
{
	check(ConstraintHandle->GetConstraintGraphIndex() == INDEX_NONE);

	ConstraintHandle->SetConstraintGraphIndex(EdgeIndex);
}

void FPBDIslandManager::GraphEdgeRemoved(const FConstraintHandleHolder& ConstraintHandle)
{
	check(ConstraintHandle->GetConstraintGraphIndex() != INDEX_NONE);

	ConstraintHandle->SetConstraintGraphIndex(INDEX_NONE);
}

int32 FPBDIslandManager::AddParticle(FGeometryParticleHandle* ParticleHandle)
{
#if CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED
	ensure(!FConstGenericParticleHandle(ParticleHandle)->Disabled());
#endif

	return TryAddParticleIfDynamic(ParticleHandle,  /*IslandIndex*/INDEX_NONE);
}

int32 FPBDIslandManager::TryAddParticle(FGeometryParticleHandle* ParticleHandle, const int32 IslandIndex)
{
	if (ParticleHandle)
	{
		if (!ParticleHandle->IsInConstraintGraph())
		{
			MaxParticleIndex = FMath::Max(MaxParticleIndex, ParticleHandle->UniqueIdx().Idx);

			const bool bIsDynamic = IsDynamicParticle(ParticleHandle);
			const bool bIsStationary = IsStationaryParticle(ParticleHandle);
			IslandGraph->AddNode(ParticleHandle, bIsDynamic, IslandIndex, bIsStationary);
		}

		return ParticleHandle->ConstraintGraphIndex();
	}
	return INDEX_NONE;
}

int32 FPBDIslandManager::TryAddParticleIfDynamic(FGeometryParticleHandle* ParticleHandle, const int32 IslandIndex)
{
	if (ParticleHandle)
	{
		if (IsDynamicParticle(ParticleHandle))
		{
			return TryAddParticle(ParticleHandle, IslandIndex);
		}

		return ParticleHandle->ConstraintGraphIndex();
	}
	return INDEX_NONE;
}

int32 FPBDIslandManager::TryAddParticleIfNotDynamic(FGeometryParticleHandle* ParticleHandle, const int32 IslandIndex)
{
	if (ParticleHandle)
	{
		if (!IsDynamicParticle(ParticleHandle))
		{
			return TryAddParticle(ParticleHandle, IslandIndex);
		}

		return ParticleHandle->ConstraintGraphIndex();
	}
	return INDEX_NONE;
}

void FPBDIslandManager::AddConstraint(const uint32 ContainerId, FConstraintHandle* ConstraintHandle, const TVec2<FGeometryParticleHandle*>& ConstrainedParticles)
{
	if (ConstraintHandle)
	{
		if (ConstraintHandle->GetConstraintGraphIndex() != INDEX_NONE)
		{
			// Already in the graph
			return;
		}

		// Are the particles dynamic (including asleep)?
		const bool bDynamicParticle0 = ConstrainedParticles[0] && IsDynamicParticle(ConstrainedParticles[0]);
		const bool bDynamicParticle1 = ConstrainedParticles[1] && IsDynamicParticle(ConstrainedParticles[1]);
		
		// We only add the constraint if one particle is non-kinematic
		// @todo(chaos): we can relax the above restriction now if necessary, but so far no need
		if (bDynamicParticle0 || bDynamicParticle1)
		{
			// If either of the particles is kinematic, it will not have been added by AddParticle, so we must add it here
			const int32 NodeIndex0 = TryAddParticleIfNotDynamic(ConstrainedParticles[0], /*IslandIndex*/INDEX_NONE);
			const int32 NodeIndex1 = TryAddParticleIfNotDynamic(ConstrainedParticles[1], /*IslandIndex*/INDEX_NONE);

			// All required particles should be in the graph at this point
			check((NodeIndex0 != INDEX_NONE) || (ConstrainedParticles[0] == nullptr));
			check((NodeIndex1 != INDEX_NONE) || (ConstrainedParticles[1] == nullptr));

			// Add the constraint to the graph
			const int32 EdgeIndex = IslandGraph->AddEdge(ConstraintHandle, ContainerId, NodeIndex0, NodeIndex1);

			// If we were added to a sleeping island, make sure the constraint is flagged as sleeping.
			// Adding constraints between 2 sleeping particles does not wake them because we need to handle
			// streaming which may amortize particle and constraint creation over multiple ticks.
			const FGraphEdge& GraphEdge = IslandGraph->GraphEdges[EdgeIndex];
			if (GraphEdge.IslandIndex != INDEX_NONE)
			{
				const bool bIsSleeping = IslandGraph->GraphIslands[GraphEdge.IslandIndex].bIsSleeping;
				if (bIsSleeping)
				{
					ConstraintHandle->SetIsSleeping(bIsSleeping);
				}
			}
		}
	}
}

void FPBDIslandManager::RemoveParticle(FGeometryParticleHandle* ParticleHandle)
{
	if (ParticleHandle)
	{
		IslandGraph->RemoveNode(ParticleHandle);

#if CHAOS_CONSTRAINTHANDLE_DEBUG_DETAILED_ENABLED
		DebugCheckParticleNotInGraph(ParticleHandle);
#endif
	}
}

void FPBDIslandManager::RemoveConstraint(const uint32 ContainerId, FConstraintHandle* ConstraintHandle)
{
	if (ConstraintHandle)
	{
		const int32 EdgeIndex = ConstraintHandle->GetConstraintGraphIndex();
		if (IslandGraph->GraphEdges.IsValidIndex(EdgeIndex))
		{
			IslandGraph->RemoveEdge(EdgeIndex);
		}

#if CHAOS_CONSTRAINTHANDLE_DEBUG_DETAILED_ENABLED
		DebugCheckConstraintNotInGraph(ConstraintHandle);
#endif
	}
}

void FPBDIslandManager::RemoveConstraints(const uint32 ContainerId)
{
	for (int32 EdgeIndex = 0; EdgeIndex < IslandGraph->GraphEdges.Num(); ++EdgeIndex)
	{
		if (IslandGraph->GraphEdges.IsValidIndex(EdgeIndex))
		{
			FGraphEdge& GraphEdge = IslandGraph->GraphEdges[EdgeIndex];
			if (GraphEdge.ItemContainer == ContainerId)
			{
				IslandGraph->RemoveEdge(EdgeIndex);
			}
		}
	}
}

void FPBDIslandManager::RemoveParticleConstraints(FGeometryParticleHandle* ParticleHandle, const uint32 ContainerId)
{
	if (ParticleHandle)
	{
		// Find the graph node for this particle
		if (const int32* PNodeIndex = IslandGraph->ItemNodes.Find(ParticleHandle))
		{
			const int32 NodeIndex = *PNodeIndex;
			if (IslandGraph->GraphNodes.IsValidIndex(NodeIndex))
			{
				const FGraphNode& GraphNode = IslandGraph->GraphNodes[NodeIndex];

				// Loop over the edges attached to this node and remove the constraints in the specified container
				// NOTE: Can't use ranged-for iterator when removing items, but we can just loop and remove since it's a sparse array
				for (int32 NodeEdgeIndex = 0, NumNodeEdges = GraphNode.NodeEdges.GetMaxIndex(); NodeEdgeIndex < NumNodeEdges; ++NodeEdgeIndex)
				{
					if (GraphNode.NodeEdges.IsValidIndex(NodeEdgeIndex))
					{
						const int32 EdgeIndex = GraphNode.NodeEdges[NodeEdgeIndex];
						if (IslandGraph->GraphEdges.IsValidIndex(EdgeIndex))
						{
							const FGraphEdge& GraphEdge = IslandGraph->GraphEdges[EdgeIndex];

							// Remove the constraint if it is the right type (same ContainerId)
							if (GraphEdge.ItemContainer == ContainerId)
							{
#if CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED
								// All constraints should know their graph index
								ensure(GraphEdge.EdgeItem->GetConstraintGraphIndex() != INDEX_NONE);
#endif

								RemoveConstraint(ContainerId, GraphEdge.EdgeItem.Get());
							}
						}
					}
				}

				// Note: we may have left the particle in the island without any constraints on it. Is that ok?
				// It should be fine as long as the node knows that it is still in it/them, which it does.
			}
		}

#if CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED
		DebugParticleConstraintsNotInGraph(ParticleHandle, ContainerId);
#endif
	}
}

int32 FPBDIslandManager::GetParticleLevel(FGeometryParticleHandle* ParticleHandle) const
{
	return IslandGraph->GetNodeItemLevel(ParticleHandle);
}

void FPBDIslandManager::ResetIslands(const TParticleView<FPBDRigidParticles>& PBDRigids)
{
	// @todo 
}

inline bool SolverIslandSortPredicate(const TUniquePtr<FPBDIsland>& SolverIslandL, const TUniquePtr<FPBDIsland>& SolverIslandR)
{
	return SolverIslandL->GetNumConstraints() < SolverIslandR->GetNumConstraints();
}

void FPBDIslandManager::GraphIslandAdded(const int32 IslandIndex)
{
	FGraphIsland& GraphIsland = IslandGraph->GraphIslands[IslandIndex];
	check(GraphIsland.IslandItem == nullptr);

	// An graph island was created, so we should assign an island manager island to it
	Islands.Reserve(IslandGraph->MaxNumIslands());
	if (!Islands.IsValidIndex(IslandIndex))
	{
		Islands.EmplaceAt(IslandIndex, MakeUnique<FPBDIsland>(ConstraintContainers.Num()));
	}

	// NOTE: The index may be reused and we may already have an island so we need to reset the island
	Islands[IslandIndex]->Reuse();
	GraphIsland.IslandItem = Islands[IslandIndex].Get();

}

void FPBDIslandManager::InitIslands()
{
	Islands.Reserve(IslandGraph->MaxNumIslands());
	IslandIndexing.SetNum(IslandGraph->MaxNumIslands(),false);
	int32 LocalIsland = 0;

	// Sync of the solver islands first and reserve the required space
	for (int32 IslandIndex = 0, NumIslands = IslandGraph->MaxNumIslands(); IslandIndex < NumIslands; ++IslandIndex)
	{
		if (IslandGraph->GraphIslands.IsValidIndex(IslandIndex))
		{
			FGraphIsland& GraphIsland = IslandGraph->GraphIslands[IslandIndex];

			// NOTE: We do not check !IsSleeping() here because we need to sync the sleep state of islands
			// that were just put to sleep this tick. See PopulateIslands.

			// If the island item is not set on the graph we check if the solver island
			// at the right index is already there. If not we create a new solver island.
			check(Islands.IsValidIndex(IslandIndex));
			FPBDIsland* Island = Islands[IslandIndex].Get();
			
			// We then transfer the persistent flag and the graph dense index to the solver island
			Island->SetIsPersistent(GraphIsland.bIsPersistent);
			Island->SetIsSleeping(GraphIsland.bIsSleeping);
			Island->SetIsSleepingChanged(GraphIsland.bIsSleeping != GraphIsland.bWasSleeping);
			Island->SetIslandIndex(LocalIsland);

			// We update the IslandIndexing to retrieve the graph sparse and persistent index from the dense one.
			IslandIndexing[LocalIsland] = IslandIndex;
			LocalIsland++;

			Island->ReserveParticles(GraphIsland.NumNodes);

			// Reset of the sleep counter if the island is :
			// - Non persistent since we are starting incrementing the counter once the island is persistent and if values below the threshold
			// - Sleeping since as soon as it wakes up we could start incrementing the counter as well
			// - Just woken because we may not have been asleep for a whole frame (this probably makes the previous check unnecessary)
			if (!Island->IsPersistent() || Island->IsSleeping() || Island->IsSleepingChanged())
			{ 
				Island->SetSleepCounter(0);
			}

			// We reset the persistent flag to be true on the island graph for next frame. It may get set to false again
			// if any particles are added or removed
			// @todo(chaos): It does not feel right to reset this here. Should probably be in EndTick? (but See AdvanceClustering which
			// is currently before that, and generates new particles that get added to the graph).
			GraphIsland.bIsPersistent = true;
		}
		else if (Islands.IsValidIndex(IslandIndex))
		{
			Islands.RemoveAt(IslandIndex);
		}
	}

	IslandIndexing.SetNum(LocalIsland, false);
}

void FPBDIslandManager::SyncIslands(FPBDRigidsSOAs& Particles)
{
	// Update of the sync and sleep state of all particles in all awake islands
	for(auto& Island : Islands)
	{
		if(!Island->IsSleeping() || Island->IsSleepingChanged())
		{
			Island->UpdateSyncState(Particles);
			Island->PropagateSleepState(Particles);
		}
	}

#if CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED
	DebugCheckIslands();
#endif

	// We can now safely use the GetIslandParticles and GetIslandConstraints accessors
	bIslandsPopulated = true;
}

void FPBDIslandManager::GraphNodeLevelAssigned(FGraphNode& GraphNode)
{
	// Add the particle to its island. This will be called in the order that levels are assigned
	// which allows us to skip the sorting-by-level step. NOTE: We only add dynamic particles to 
	// islands at this stage (kinematics are added to the solver when we gather data).
	if (GraphNode.bValidNode)
	{
		// We have a dynamic particle - add it to its island
		check(GraphNode.IslandIndex != INDEX_NONE);
		FPBDIsland* Island = IslandGraph->GraphIslands[GraphNode.IslandIndex].IslandItem;

#if CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED
		check(!Island->DebugContainsParticle(GraphNode.NodeItem));
#endif

		Island->AddParticle(GraphNode.NodeItem);
	}
}

void FPBDIslandManager::GraphEdgeLevelAssigned(FGraphEdge& GraphEdge)
{
	check(GraphEdge.IslandIndex != INDEX_NONE);
	FPBDIsland* Island = IslandGraph->GraphIslands[GraphEdge.IslandIndex].IslandItem;

#if CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED
	ensure(GraphEdge.bValidEdge);
	check(!Island->DebugContainsConstraint(GraphEdge.EdgeItem));
#endif

	// Persistent ordering between ticks is now built-in because the graph is persistent so the constraint order will 
	// not change from tick to tick even with parallel collision detection (though the order will be different between 
	// machines unles deterministic mode is set on the evolution). As a result this sort key is not required but for 
	// now we still calculate it because we can disable persistent islands on a cvar (bChaosSolverPersistentGraph).
	// 
	// For non-persistent graph mode:
	// The sub sort key is for consistent ordering of constraints when both Level and Color are the same.
	// Without this the ordering within a level/color would depend on the order they were added to the graph
	// which depends on collision detection order, which is multi-threaded and not deterministic (by default).
	// The sort key is based on Particle's NodeIndex because particles remain in the graph with the same index 
	// as long as they are active. We need to pack the key into 32 bits so we truncate the particle indices. 
	// This should matter only very rarely. If we hit a conflict we may see a slight non-deterministic behaviour 
	// difference because the solve  order may vary from frame to frame, but it will still solve ok.
	const uint32 SubSortKey = (((uint32(GraphEdge.FirstNode) & 0xFFFF) << 16) | (uint32(GraphEdge.SecondNode) & 0xFFFF));

	Island->AddConstraint(GraphEdge.EdgeItem, GraphEdge.LevelIndex, GraphEdge.ColorIndex, SubSortKey);
}

void FPBDIslandManager::UpdateIslands(FPBDRigidsSOAs& Particles)
{
	// Assign all particles and constraints to islands, merging and splitting islands as required
	IslandGraph->UpdateGraph();

#if CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED
	DebugCheckParticlesInGraph(Particles.GetNonDisabledDynamicView());
#endif

	// Initializes our set of islands to match the state of the graph islands, ready to receive the particles and constraints.
	InitIslands();

	// Reeset all particle and constraint levels, colors etc
	IslandGraph->InitSorting();

	// Assign a level to every constraint and particle.
	// NOTE: This will call GraphNodeLevelAssigned/GraphEdgeLevelAssigned for every
	// particle/constraint in level order. We use this to add constraints and particles
	// to their islands in level-sorted order, so we don't need to sort again below
	IslandGraph->ComputeLevels();

	// Assign colors
	// @todo(chaos): re-enable graph coloring, although it's probably not worth it!
	//IslandGraph->ComputeColors();

	// Sync the state of all particles and constraints (primarily sleep state)
	SyncIslands(Particles);

	// Sort each island's constraints based on level, color, etc
	// This is not required when we have a persistent graph because we already added constraints to islands in 
	// Level-order, and the sub-level order does not change from tick to tick when persistence is enabled.
	if (!bChaosSolverPersistentGraph)
	{
		for (TUniquePtr<FPBDIsland>& Island : Islands)
		{
			if (!Island->IsSleeping())
			{
				Island->SortConstraints();
			}
		}
	}

	ValidateIslands();
}

void FPBDIslandManager::ValidateIslands() const
{
	if (!bChaosSolverValidateGraph)
	{
		return;
	}

	// Check that nodes joined by an edge are in the same island as the edge
	for (const FGraphEdge& Edge : IslandGraph->GraphEdges)
	{
		const FGraphNode* Node0 = (Edge.FirstNode != INDEX_NONE) ? &IslandGraph->GraphNodes[Edge.FirstNode] : nullptr;
		const FGraphNode* Node1 = (Edge.SecondNode != INDEX_NONE) ? &IslandGraph->GraphNodes[Edge.SecondNode] : nullptr;

		ensure((Node0 == nullptr) || !Node0->bValidNode || (Node0->IslandIndex == Edge.IslandIndex));
		ensure((Node1 == nullptr) || !Node1->bValidNode || (Node1->IslandIndex == Edge.IslandIndex));
	}
}

bool FPBDIslandManager::SleepInactive(const int32 IslandIndex,
	const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>& PerParticleMaterialAttributes,
	const THandleArray<FChaosPhysicsMaterial>& SolverPhysicsMaterials)
{
	// Only the persistent islands could start sleeping
	const int32 GraphIndex = GetGraphIndex(IslandIndex);
	if (!bChaosSolverSleepEnabled || !Islands.IsValidIndex(GraphIndex) ||
		(Islands.IsValidIndex(GraphIndex) && !Islands[GraphIndex]->IsPersistent()) )
	{
		return false;
	}

	FReal LinearSleepingThreshold = FLT_MAX,  AngularSleepingThreshold = FLT_MAX, MaxLinearSpeed2 = 0.f, MaxAngularSpeed2 = 0.f;
	int32 SleepCounterThreshold = 0, NumDynamicParticles = 0;

	// Compute of the linear/angular velocities + thresholds to make islands sleeping 
	if (ComputeSleepingThresholds(Islands[GraphIndex].Get(), PerParticleMaterialAttributes,
		SolverPhysicsMaterials, LinearSleepingThreshold, AngularSleepingThreshold,
		MaxLinearSpeed2, MaxAngularSpeed2, SleepCounterThreshold))
	{
		int32 SleepCounter = Islands[GraphIndex]->GetSleepCounter();
		const bool bSleepingIsland = IsIslandSleeping(MaxLinearSpeed2, MaxAngularSpeed2, LinearSleepingThreshold,
			AngularSleepingThreshold, SleepCounterThreshold, SleepCounter);
		
		Islands[GraphIndex]->SetSleepCounter(SleepCounter);

		return bSleepingIsland;
	}
	return false;
}

void FPBDIslandManager::SleepIsland(FPBDRigidsSOAs& Particles, const int32 IslandIndex)
{
	// @todo(chaos): this function will not work when called manually at the start of the frame based on user request.
	// Currently this is not required so not important. See disabled unit test: TestConstraintGraph_SleepIsland
	ensure(bIslandsPopulated);

	SetIslandSleeping(Particles, IslandIndex, true);
}

void FPBDIslandManager::SetIslandSleeping(FPBDRigidsSOAs& Particles, const int32 IslandIndex, const bool bIsSleeping)
{
	// NOTE: This Sleep function is called from the graph update to set the state of the 
	// island and all particles in it.
	const int32 GraphIndex = GetGraphIndex(IslandIndex);
	if (Islands.IsValidIndex(GraphIndex))
	{
		const bool bWasSleeping = Islands[GraphIndex]->IsSleeping();
		if (bWasSleeping != bIsSleeping)
		{
			IslandGraph->GraphIslands[GraphIndex].bIsSleeping = bIsSleeping;
			Islands[GraphIndex]->SetIsSleeping(bIsSleeping);
			Islands[GraphIndex]->SetIsSleepingChanged(true);
			Islands[GraphIndex]->PropagateSleepState(Particles);
		}

		// Reset the sleep counter with every wake call, even if already awake so that we don't immediately go back to sleep
		if (!bIsSleeping)
		{
			Islands[GraphIndex]->SetSleepCounter(0);
		}
	}
}

TArray<int32> FPBDIslandManager::FindParticleIslands(const FGeometryParticleHandle* ParticleHandle) const
{
	TArray<int32> IslandIndices;

	// Initially build the list of sparse island indices based on the connected edges (for kinematics)
	if (ParticleHandle)
	{
		// Find the graph node for this particle
		if (const int32* PNodeIndex = IslandGraph->ItemNodes.Find(ParticleHandle))
		{
			const int32 NodeIndex = *PNodeIndex;
			if (IslandGraph->GraphNodes.IsValidIndex(NodeIndex))
			{
				const FGraphNode& GraphNode = IslandGraph->GraphNodes[NodeIndex];

				if (GraphNode.bValidNode)
				{
					// A dynamic particle is only in one island
					if ((GraphNode.IslandIndex != INDEX_NONE) && IslandGraph->GraphIslands.IsValidIndex(GraphNode.IslandIndex))
					{
						IslandIndices.Add(GraphNode.IslandIndex);
					}
				}
				else
				{
					// A non-dynamic particle can be in many islands, depending on the connections to dynamics
					for (int32 EdgeIndex : GraphNode.NodeEdges)
					{
						if (IslandGraph->GraphEdges.IsValidIndex(EdgeIndex))
						{
							const FGraphEdge& GraphEdge = IslandGraph->GraphEdges[EdgeIndex];
							if ((GraphEdge.IslandIndex != INDEX_NONE) && IslandGraph->GraphIslands.IsValidIndex(GraphEdge.IslandIndex))
							{
								IslandIndices.Add(GraphEdge.IslandIndex);
							}
						}
					}
				}
			}
		}

		// Map the islands into non-sparse indices
		for (int32 IslandIndicesIndex = 0; IslandIndicesIndex < IslandIndices.Num(); ++IslandIndicesIndex)
		{
			const int32 GraphIslandIndex = IslandIndices[IslandIndicesIndex];
			const int32 SolverIslandIndex = IslandGraph->GraphIslands[GraphIslandIndex].IslandItem->GetIslandIndex();
			IslandIndices[IslandIndicesIndex] = SolverIslandIndex;
		}
	}

	return IslandIndices;
}

int32 FPBDIslandManager::GetConstraintIsland(const FConstraintHandle* ConstraintHandle) const
{
	const int32 EdgeIndex = ConstraintHandle->GetConstraintGraphIndex();
	if (IslandGraph->GraphEdges.IsValidIndex(EdgeIndex))
	{
		int32 GraphIslandIndex = IslandGraph->GraphEdges[EdgeIndex].IslandIndex;
		if (GraphIslandIndex != INDEX_NONE)
		{
			const int32 SolverIslandIndex = IslandGraph->GraphIslands[GraphIslandIndex].IslandItem->GetIslandIndex();
			return SolverIslandIndex;
		}
	}
	return INDEX_NONE;
}

bool FPBDIslandManager::IslandNeedsResim(const int32 IslandIndex) const
{
	return Islands[GetGraphIndex(IslandIndex)]->NeedsResim();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//
//
// BEGIN DEBUG STUFF
//
//
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

#if CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED
bool FPBDIslandManager::DebugCheckParticlesInGraph(const TParticleView<FPBDRigidParticles>& Particles) const
{
	// Make sure that all particles that should be in the graph actually are.
	// We used to loop over all particles every trick and re-add them, but now we only add/remove when necessary.
	bool bSuccess = true;

	// All particles in the graph should be enabled
	for (const FGraphNode& Node : IslandGraph->GraphNodes)
	{
		FPBDRigidParticleHandle* Rigid = Node.NodeItem->CastToRigidParticle();
		if (Rigid != nullptr)
		{
			bSuccess = bSuccess && !Rigid->Disabled();
		}
		ensure(bSuccess);
	}

	// All dynamic particles should be in the graph
	for (const auto& ParticleHandle : Particles)
	{
		FConstGenericParticleHandle Particle = ParticleHandle.Handle();
		if (!Particle->IsDynamic())
		{
			continue;
		}

		int32* PNodeIndex = IslandGraph->ItemNodes.Find(Particle->Handle());
		bSuccess = bSuccess && (PNodeIndex != nullptr);
		if (ensure(bSuccess))
		{
			const int32 NodeIndex = *PNodeIndex;
			bSuccess = bSuccess && IslandGraph->GraphNodes.IsValidIndex(NodeIndex);
			if (ensure(bSuccess))
			{
				const FGraphNode& GraphNode = IslandGraph->GraphNodes[NodeIndex];
				bSuccess = bSuccess && (GraphNode.NodeItem == Particle->Handle());
				ensure(bSuccess);
			}
		}
	}

	for (const FGraphEdge& GraphEdge : IslandGraph->GraphEdges)
	{
		const int32 FirstNodeIndex = GraphEdge.FirstNode;
		const int32 SecondNodeIndex = GraphEdge.SecondNode;

		// Every constraint should reference at least one particle
		bSuccess = bSuccess && ((FirstNodeIndex != INDEX_NONE) || (SecondNodeIndex != INDEX_NONE));
		ensure(bSuccess);

		// Every particle referenced by a constraint should be in the graph (including kinematics and statics)
		// The node must point to the correct particle
		if (FirstNodeIndex != INDEX_NONE)
		{
			bSuccess = bSuccess && IslandGraph->GraphNodes.IsValidIndex(FirstNodeIndex);
			if (ensure(bSuccess))
			{
				bSuccess = bSuccess && (IslandGraph->GraphNodes[FirstNodeIndex].NodeItem == GraphEdge.EdgeItem->GetConstrainedParticles()[0]);
				ensure(bSuccess);
			}
		}
		if (SecondNodeIndex != INDEX_NONE)
		{
			bSuccess = bSuccess && IslandGraph->GraphNodes.IsValidIndex(SecondNodeIndex);
			if (ensure(bSuccess))
			{
				bSuccess = bSuccess && (IslandGraph->GraphNodes[SecondNodeIndex].NodeItem == GraphEdge.EdgeItem->GetConstrainedParticles()[1]);
				ensure(bSuccess);
			}
		}
	}

	return bSuccess;
}

bool FPBDIslandManager::DebugCheckParticleNotInGraph(const FGeometryParticleHandle* ParticleHandle) const
{
	bool bSuccess = true;
	for (const auto& Node : IslandGraph->GraphNodes)
	{
		if (Node.NodeItem == ParticleHandle)
		{
			bSuccess = false;
			ensure(false);
		}
	}

	for (const auto& ItemNode : IslandGraph->ItemNodes)
	{
		if (ItemNode.Key == ParticleHandle)
		{
			bSuccess = false;
			ensure(false);
		}
	}

	for (const auto& Edge : IslandGraph->GraphEdges)
	{
		if (Edge.EdgeItem.GetParticle0() == ParticleHandle)
		{
			bSuccess = false;
			ensure(false);
		}
		if (Edge.EdgeItem.GetParticle1() == ParticleHandle)
		{
			bSuccess = false;
			ensure(false);
		}
	}
	return bSuccess;
}

bool FPBDIslandManager::DebugParticleConstraintsNotInGraph(const FGeometryParticleHandle* ParticleHandle, const int32 ContainerId) const
{
	bool bSuccess = true;
	for (const auto& Edge : IslandGraph->GraphEdges)
	{
		if (Edge.ItemContainer == ContainerId)
		{
			if ((Edge.EdgeItem.GetParticle0() == ParticleHandle) || (Edge.EdgeItem.GetParticle1() == ParticleHandle))
			{
				bSuccess = false;
				ensure(false);
			}
		}
	}
	return bSuccess;
}

bool FPBDIslandManager::DebugCheckConstraintNotInGraph(const FConstraintHandle* ConstraintHandle) const
{
	bool bSuccess = true;
	for (const auto& Edge : IslandGraph->GraphEdges)
	{
		if (Edge.EdgeItem.Get() == ConstraintHandle)
		{
			bSuccess = false;
			ensure(false);
		}
	}

	for (const auto& ItemEdge : IslandGraph->ItemEdges)
	{
		if (ItemEdge.Key.Get() == ConstraintHandle)
		{
			bSuccess = false;
			ensure(false);
		}
	}

	for (int32 IslandIndex = 0; IslandIndex < NumIslands(); ++IslandIndex)
	{
		const FPBDIsland* Island = GetIsland(IslandIndex);
		for (int32 ContainerIndex = 0; ContainerIndex < Island->GetNumConstraintContainers(); ++ContainerIndex)
		{
			for (const FPBDIslandConstraint& Constraint : Island->GetConstraints(ContainerIndex))
			{
				if (Constraint.GetConstraint() == ConstraintHandle)
				{
					bSuccess = false;
					ensure(false);
				}
			}
		}
	}
	return bSuccess;
}

bool FPBDIslandManager::DebugCheckIslandConstraints() const
{
	bool bSuccess = true;

	for (const auto& GraphEdge : IslandGraph->GraphEdges)
	{
		const FConstraintHandleHolder& ConstraintHolder = GraphEdge.EdgeItem;

		// NOTE: The check for both null here is for unit tests that do not use particles...not ideal...
		if ((ConstraintHolder.GetParticle0() != nullptr) || (ConstraintHolder.GetParticle1() != nullptr))
		{
			// Make sure that if we have an IslandIndex it is a valid one
			const bool bIslandIndexOK = (GraphEdge.IslandIndex == INDEX_NONE) || IslandGraph->GraphIslands.IsValidIndex(GraphEdge.IslandIndex);
			bSuccess = bSuccess && bIslandIndexOK;
			ensure(bIslandIndexOK);

			// At least one particle on each constraint must be dynamic if we are in an awake island.
			// If we do not have a dynamic, we must have no island or it must be asleep
			const bool bInAwakeIsland = (GraphEdge.IslandIndex != INDEX_NONE) && IslandGraph->GraphIslands.IsValidIndex(GraphEdge.IslandIndex) && !IslandGraph->GraphIslands[GraphEdge.IslandIndex].bIsSleeping;
			const bool bIsDynamic0 = (ConstraintHolder.GetParticle0() != nullptr) ? FConstGenericParticleHandle(ConstraintHolder.GetParticle0())->IsDynamic() : false;
			const bool bIsDynamic1 = (ConstraintHolder.GetParticle1() != nullptr) ? FConstGenericParticleHandle(ConstraintHolder.GetParticle1())->IsDynamic() : false;
			
			if (bInAwakeIsland)
			{
				// If the island is awake, both particle should be too
				bSuccess = bSuccess && (bIsDynamic0 || bIsDynamic1);
				ensure(bIsDynamic0 || bIsDynamic1);

				// If the island is awake, constraint should be too
				if (ConstraintHolder->SupportsSleeping())
				{
					const bool bIsAsleep = ConstraintHolder->IsSleeping();
					ensure(!bIsAsleep);
				}
			}
			else
			{
				// If the island is asleep, make sure the constraint is too
				if (ConstraintHolder->SupportsSleeping())
				{
					const bool bIsAsleep = ConstraintHolder->IsSleeping();
					ensure(bIsAsleep);
				}
			}

			if (!bIsDynamic0 && !bIsDynamic1)
			{
				bSuccess = bSuccess && !bInAwakeIsland;
				ensure(!bInAwakeIsland);
			}
		}
	}

	return bSuccess;
}

void FPBDIslandManager::DebugCheckParticleIslands(const FGeometryParticleHandle* ParticleHandle) const
{
	// Check that every particle is included in the islands they think and no others
	const int32* PNodeIndex = IslandGraph->ItemNodes.Find(ParticleHandle);
	if (PNodeIndex != nullptr)
	{
		const int32 NodeIndex = *PNodeIndex;
		const auto& Node = IslandGraph->GraphNodes[NodeIndex];
		ensure(Node.NodeItem == ParticleHandle);

		// Find the islands containing this node (either directly, or referenced by an constraint)
		TArray<int32> NodeSolverIslands;
		for (int32 IslandIndex = 0; IslandIndex < NumIslands(); ++IslandIndex)
		{
			const FPBDIsland* Island = GetIsland(IslandIndex);
			for (const FPBDIslandParticle& IslandParticle : Island->GetParticles())
			{
				const FGeometryParticleHandle* IslandParticleHandle = IslandParticle.GetParticle();

				if (IslandParticleHandle == ParticleHandle)
				{
					NodeSolverIslands.AddUnique(IslandIndex);
				}
			}
			for (int32 ContainerIndex = 0; ContainerIndex < Island->GetNumConstraintContainers(); ++ContainerIndex)
			{
				for (const FPBDIslandConstraint& IslandConstraint : Island->GetConstraints(ContainerIndex))
				{
					if ((IslandConstraint.GetConstraint()->GetConstrainedParticles()[0] == ParticleHandle) || (IslandConstraint.GetConstraint()->GetConstrainedParticles()[1] == ParticleHandle))
					{
						NodeSolverIslands.AddUnique(IslandIndex);
					}
				}
			}
		}

		// NOTE: Particles are always assigneed to an island, but if we do not have any constraints, the node may not be in the Island
		if (FConstGenericParticleHandle(ParticleHandle)->IsDynamic())
		{
			// Dynamic particles should be in 1 island
			ensure(Node.IslandIndex != INDEX_NONE);
		}
		else
		{
			// There's not much we can check for non-dynamic particles. They should not have an IslandIndex because that's not used
			// for kinematics, but they may or may not be in an island. E.g., if the kinematic is connected to dynamic it will
			// be in one or more islands, but if it has no connections or is connected only to another kinematic it will not be in any islands.
			ensure(Node.IslandIndex == INDEX_NONE);
		}
	}
}

void FPBDIslandManager::DebugCheckConstraintIslands(const FConstraintHandle* ConstraintHandle, const int32 EdgeIslandIndex) const
{
	// Find all the islands containing this edge
	TArray<int32> EdgeSolverIslands;
	for (int32 SolverIslandIndex = 0; SolverIslandIndex < NumIslands(); ++SolverIslandIndex)
	{
		const FPBDIsland* Island = GetIsland(SolverIslandIndex);
		for (int32 ContainerIndex = 0; ContainerIndex < Island->GetNumConstraintContainers(); ++ContainerIndex)
		{
			for (const FPBDIslandConstraint& IslandConstraint : Island->GetConstraints(ContainerIndex))
			{
				// Make sure all the constraints are in the correct list according to type
				ensure(ContainerIndex == IslandConstraint.GetConstraint()->GetContainerId());

				if (IslandConstraint.GetConstraint() == ConstraintHandle)
				{
					EdgeSolverIslands.AddUnique(SolverIslandIndex);
				}
			}
		}
	}

	if (EdgeIslandIndex != INDEX_NONE)
	{
		if (IslandGraph->GraphIslands[EdgeIslandIndex].bIsSleeping)
		{
			// Make sure the constraint and its island agree on sleepiness
			if (ConstraintHandle->SupportsSleeping())
			{
				ensure(ConstraintHandle->IsSleeping());
			}

			// If we are sleeping, we usually don't appear in the solver islands (because we don't need to solve sleepers)
			// but we do create solver islands for 1 tick after the island goes to sleep so that we can sync the sleep state
			// (see PopulateIslands and SyncSleepState). 
			if (EdgeSolverIslands.Num() > 0)
			{
				ensure(EdgeSolverIslands.Num() == 1);
				const int32 EdgeIsland = IslandIndexing[EdgeSolverIslands[0]];
				ensure(EdgeIslandIndex == EdgeIsland);
			}
		}
		else
		{
			// Make sure the constraint and its island agree on sleepiness
			if (ConstraintHandle->SupportsSleeping())
			{
				ensure(!ConstraintHandle->IsSleeping());
			}

			// We are awake, make sure our island search produced the correct island (and only that one)
			ensure(EdgeSolverIslands.Num() == 1);
			if (EdgeSolverIslands.Num() > 0)
			{
				const int32 EdgeIsland = IslandIndexing[EdgeSolverIslands[0]];
				ensure(EdgeIslandIndex == EdgeIsland);
			}
		}
	}
	else
	{
		// We are not yet added to any islands
		ensure(EdgeSolverIslands.Num() == 0);
	}
}

void FPBDIslandManager::DebugCheckIslands() const
{
	// Check that the solver island state matches the graph island state
	for (int32 IslandIndex = 0; IslandIndex < IslandGraph->GraphIslands.GetMaxIndex(); ++IslandIndex)
	{
		if (IslandGraph->GraphIslands.IsValidIndex(IslandIndex))
		{
			if (IslandGraph->GraphIslands[IslandIndex].bIsSleeping != IslandGraph->GraphIslands[IslandIndex].IslandItem->IsSleeping())
			{
				ensure(false);
			}

			const int32 SolverIslandIndex = IslandGraph->GraphIslands[IslandIndex].IslandItem->GetIslandIndex();
			if (IslandIndexing[SolverIslandIndex] != IslandIndex)
			{
				ensure(false);
			}
		}
	}

	// Make sure islands only contain valid constraints
	DebugCheckIslandConstraints();

	// Check that every particle is included in the islands they think and no others
	for (const auto& Node : IslandGraph->GraphNodes)
	{
		DebugCheckParticleIslands(Node.NodeItem);
	}

	// Check that every constraint is included in the island they think and no others
	for (const auto& Edge : IslandGraph->GraphEdges)
	{
		DebugCheckConstraintIslands(Edge.EdgeItem, Edge.IslandIndex);
	}
}
#endif

}