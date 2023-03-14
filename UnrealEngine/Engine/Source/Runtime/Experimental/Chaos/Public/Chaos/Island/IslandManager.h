// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/SparseArray.h"

#include "Chaos/Real.h"
#include "Chaos/Vector.h"
#include "Chaos/Framework/Handles.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "Chaos/Island/IslandGraph.h"
#include "Chaos/Island/SolverIsland.h"

namespace Chaos
{

/** Forward declaration */
template<typename T, int D>
class TPBDRigidParticles;
template<class T>
class TChaosPhysicsMaterial;
template <typename T>
class TSerializablePtr;
template<typename T>
class TArrayCollectionArray;
template <typename T>
class TParticleView;
class FPBDRigidsSOAs;
class FConstraintHandle;
class FChaosPhysicsMaterial;

using FPBDRigidParticles = TPBDRigidParticles<FReal, 3>;

/** Island manager responsible to create the list of solver islands that will be persistent over time */
class CHAOS_API FPBDIslandManager
{
public:
	using GraphType = FIslandGraph<FGeometryParticleHandle*, FConstraintHandleHolder, FPBDIsland*, FPBDIslandManager>;
	using FGraphNode = GraphType::FGraphNode;
	using FGraphEdge = GraphType::FGraphEdge;
	using FGraphIsland = GraphType::FGraphIsland;

	/**
	  * Default island manager constructor
	  */
	FPBDIslandManager();
	
	/**
	* Default island manager destructor
	*/
	~FPBDIslandManager();

	/**
	 * Clear all nodes and edges from the graph
	*/
	void Reset();

	/**
	 * Remove all particles (and their constraints) from the graph
	*/
	void RemoveParticles();

	/**
	* Remove all the constraints from the graph
	*/
	void RemoveConstraints();

	/**
	* Clear the manager and set up the particle-to-graph-node mapping for the specified particles
	* Should be called before AddConstraint.
	* @param Particles List of particles to be used to fill the graph nodes
	*/
	void InitializeGraph(const TParticleView<FPBDRigidParticles>& Particles);

	/**
	 * @brief Called at the end of the frame to perform cleanup
	*/
	void EndTick();

	/**
	  * Reserve space in the graph for NumConstraints additional constraints.
	  * @param NumConstraints Number of constraints to be used to reserved memory space
	  */
	void ReserveConstraints(const int32 NumConstraints);

	/**
	  * Add a constraint to the graph for each constraint in the container.
	  * @param ContainerId Contaner id the constraint is belonging to
	  * @param ConstraintHandle Constraint Handle that will be used for the edge item
	  * @param ConstrainedParticles List of 2 particles handles that are used within the constraint
	  * @return Edge index within the sparse graph edges list
	  */
	void AddConstraint(const uint32 ContainerId, FConstraintHandle* ConstraintHandle, const TVector<FGeometryParticleHandle*, 2>& ConstrainedParticles);

	/**
	 * Utility function for use by the constraint containers to add all their constraints
	 */
	template<typename ConstraintContainerType>
	void AddContainerConstraints(ConstraintContainerType& ConstraintContainer)
	{
		const int32 ContainerId = ConstraintContainer.GetContainerId();
		for (FConstraintHandle* ConstraintHandle : ConstraintContainer.GetConstraintHandles())
		{
			if (ConstraintHandle->IsEnabled())
			{
				AddConstraint(ContainerId, ConstraintHandle, ConstraintHandle->GetConstrainedParticles());
			}
		}
	}

	/**
	  * Remove a constraint from the graph
	  * @param ContainerId Container id the constraint is belonging to
	  * @param ConstraintHandle Constraint Handle that will be used for the edge item
	  * @param ConstrainedParticles List of 2 particles handles that are used within the constraint
	  */
	void RemoveConstraint(const uint32 ContainerId, FConstraintHandle* ConstraintHandle);

	/**
	 * Remove all constraints of the specified type.
	 */
	void RemoveConstraints(const uint32 ContainerId);

	/**
	 * @brief Remove all the constraints of the specified type (ContainerId) belonging to the Particle
	 * @param Particle the particle to remove the constraints from
	 * @param ContainerId Container id the constraint is belonging to
	*/
	void RemoveParticleConstraints(FGeometryParticleHandle* Particle, const uint32 ContainerId);
	
	/**
	  * Preallocate buffers for \p Num particles.
	  * @param NumParticles Number of particles to be used to reserve memory
	  * @return Number of new slots created.
	  */
	int32 ReserveParticles(const int32 NumParticles);

	/**
	  * Add a particle to the graph
	  * @param ParticleHandle Particle to be added to the graph
	  * @param ParentParticleHandle Put the ParticleHandle in the same island as the parent with the same sleep state. May be null.
	  * @return Sparse graph node index
	  */
	int32 AddParticle(FGeometryParticleHandle* ParticleHandle);

	/**
	  * Remove a particle from the graph
	  * @param Particle Particle to be removed from the islands
	  */
	void RemoveParticle(FGeometryParticleHandle* Particle);

	/**
	 * Get the Level for the specified particle (distance to a kinmetic in the graph)
	*/
	int32 GetParticleLevel(FGeometryParticleHandle* Particle) const;


	/**
	  * Call the visitor on all constraints in an awake island. E.g., can be used to check if any constraints
	  * in the graph are expired and should be removed. However, no constraints should be added or removed
	  * from the visitor itself - build a list and remove them after.
	  */
	template<typename VisitorType>
	void VisitConstraintsInAwakeIslands(const int32 ContainerId, const VisitorType& Visitor)
	{
		for (FGraphEdge& GraphEdge : IslandGraph->GraphEdges)
		{
			// If a constraint is not in any island (kinematic-kinematic for example) it is considered asleep.
			if ((GraphEdge.ItemContainer == ContainerId) && (GraphEdge.IslandIndex != INDEX_NONE))
			{
				const FGraphIsland& GraphIsland = IslandGraph->GraphIslands[GraphEdge.IslandIndex];
				if (!GraphIsland.bIsSleeping)
				{
					Visitor(GraphEdge.EdgeItem);
				}
			}
		}
	}

	/**
	  * Clear all the islands
	  */
	void ResetIslands(const TParticleView<FPBDRigidParticles>& PBDRigids);

	/**
	  * Pack all the constraints into their assigned islands in sorted order
	  */
	void UpdateIslands(FPBDRigidsSOAs& Particles);

	/**
	  * @brief Put all particles and constraints in an island to sleep
	  * @param Particles used in the islands
	  * @param IslandIndex Island index to be woken up
	*/
	void SleepIsland(FPBDRigidsSOAs& Particles, const int32 IslandIndex);

	/**
	  * Put particles in inactive islands to sleep.
	  * @param IslandIndex Island index
	  * @param PerParticleMaterialAttributes Material attributes that has been set on each particles
	  * @param SolverPhysicsMaterials Solver materials that could be used to find the sleeping thresholds
	  * @return return true if the island is going to sleep
	  */
	bool SleepInactive(const int32 IslandIndex, const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>& PerParticleMaterialAttributes, const THandleArray<FChaosPhysicsMaterial>& SolverPhysicsMaterials);

	/**
	 * Get the set of islands that the particle belongs to. Will be 0-1 island for dynamics and could be any number for non-dynamics.
	 * This works for sleeping and waking particles (but we must iterate over constraints for this to work).
	 */
	TArray<int32> FindParticleIslands(const FGeometryParticleHandle* ParticleHandle) const;

	/**
	 * Get the island index of the constraint
	*/
	int32 GetConstraintIsland(const FConstraintHandle* ConstraintHandle) const;

	/**
	 * When resim is used, tells us whether we need to resolve island
	 * @param IslandIndex Island index
	 * @return If true the island need to be resim
	 */
	bool IslandNeedsResim(const int32 IslandIndex) const;

	/**
	  * The number of islands in the graph.
	  * @return the number of islands
	  */
	FORCEINLINE int32 NumIslands() const
	{
		return IslandIndexing.Num();
	}
	
	/**
	 * The number of islands in the graph.
	 * @return Get the island graph
	 */
	FORCEINLINE const GraphType* GetIslandGraph() const
	{
		return IslandGraph.Get();
	}
	/**
	* Get the island graph
	* @return Island graph of the manager
	*/
	FORCEINLINE GraphType* GetIslandGraph()
	{
		return IslandGraph.Get();
	}

	/**
	 * Get Max UniqueIdx of the particles.
	 * Used to create arrays large enough to use Particle.UniqueIdx().Idx for indexing.
	 * @return the max index of the particles in the islands
	 */
	inline int32 GetMaxParticleUniqueIdx() const 
	{ 
		return MaxParticleIndex;
	}

	/**
	 * Get the sparse island graph index given a dense index between 0 and NumIslands
	 * @param IslandIndex Island index
	 * @return the max index of the particles in the islands
	 */
	int32 GetGraphIndex(const int32 IslandIndex) const 
	{
		return IslandIndexing.IsValidIndex(IslandIndex) ? IslandIndexing[IslandIndex] : 0;
	}

	/**
	 * Register a constraint container with the island manager. Currently this is only used to determin
	 * how many ContainerIds we need in order to presize arrays etc.
	*/
	void AddConstraintContainer(FPBDConstraintContainer& ConstraintContainer);

	/** Accessors of the graph island */
	const FPBDIsland* GetIsland(const int32 IslandIndex) const {return IslandIndexing.IsValidIndex(IslandIndex) ? Islands[IslandIndexing[IslandIndex]].Get() : nullptr; }
	FPBDIsland* GetIsland(const int32 IslandIndex) {return IslandIndexing.IsValidIndex(IslandIndex) ? Islands[IslandIndexing[IslandIndex]].Get() : nullptr; }

	/** Get the graph nodes */
	// @todo(chaos): remove this - only used by testing
	const TSparseArray<GraphType::FGraphNode>& GetGraphNodes() const { return IslandGraph->GraphNodes; }
	
	/** Get the graph edges */
	// @todo(chaos): remove this - only used by testing
	const TSparseArray<GraphType::FGraphEdge>& GetGraphEdges() const { return IslandGraph->GraphEdges; }

	/** Get the particle nodes */
	// @todo(chaos): remove this - only used by testing
	const TMap<FGeometryParticleHandle*, int32>& GetParticleNodes() const { return IslandGraph->ItemNodes; }

	// Graph Owner API Implementation. Allows us to store edge/node indices on constraints and particles
	void GraphIslandAdded(const int32 IslandIndex);
	void GraphNodeAdded(FGeometryParticleHandle* NodeItem, const int32 NodeIndex);
	void GraphNodeRemoved(FGeometryParticleHandle* NodeItem);
	void GraphNodeLevelAssigned(FGraphNode& GraphNode);
	void GraphEdgeAdded(const FConstraintHandleHolder& EdgeItem, const int32 EdgeIndex);
	void GraphEdgeRemoved(const FConstraintHandleHolder& EdgeItem);
	void GraphEdgeLevelAssigned(FGraphEdge& GraphEdge);

#if CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED
	bool DebugCheckParticlesInGraph(const TParticleView<FPBDRigidParticles>& Particles) const;
	bool DebugCheckParticleNotInGraph(const FGeometryParticleHandle* ParticleHandle) const;
	bool DebugParticleConstraintsNotInGraph(const FGeometryParticleHandle* Particle, const int32 ContainerId) const;
	bool DebugCheckConstraintNotInGraph(const FConstraintHandle* ConstraintHandle) const;
#endif

protected:

#if CHAOS_CONSTRAINTHANDLE_DEBUG_ENABLED
	bool DebugCheckIslandConstraints() const;
	void DebugCheckParticleIslands(const FGeometryParticleHandle* ParticleHandle) const;
	void DebugCheckConstraintIslands(const FConstraintHandle* ConstraintHandle, const int32 IslandIndex) const;
	void DebugCheckIslands() const;
#endif

	// Add a particle to the graph if it is not already in it. Return the node index if present (added or existing)
	int32 TryAddParticle(FGeometryParticleHandle* ParticleHandle, const int32 IslandIndex);

	// Add a particle to the graph if it is dynamic and not already in it. Return the node index if present, even if not dynamic
	int32 TryAddParticleIfDynamic(FGeometryParticleHandle* ParticleHandle, const int32 IslandIndex);

	// Add a particle to the graph if it is kinematic and not already in it. Return the node index if present, even if dynamic
	int32 TryAddParticleIfNotDynamic(FGeometryParticleHandle* ParticleHandle, const int32 IslandIndex);

	void InitIslands();

	/**
	* Sync the Islands with the IslandGraph
	*/
	void SyncIslands(FPBDRigidsSOAs& Particles);


	/**
	* Set the island and all particles' sleep state (called from the graph update).
	*/
	void SetIslandSleeping(FPBDRigidsSOAs& Particles, const int32 IslandIndex, const bool bIsSleeping);

	void ValidateIslands() const;

	TArray<FPBDConstraintContainer*>		ConstraintContainers;

	/** List of solver islands in the manager */
	TSparseArray<TUniquePtr<FPBDIsland>>	Islands;

	/** Island map to make the redirect an index between 0...NumIslands to the persistent island index  */
	TArray<int32>							IslandIndexing;

	/** Global graph to deal with merging and splitting of islands */
	TUniquePtr<GraphType>					IslandGraph;

	/** Maximum particle index that we have in the graph*/
	int32									MaxParticleIndex = INDEX_NONE;

	/** Islands are only populated with constraints and particles during the solver phase. Trying to access them outside that is an error we can trap */
	bool									bIslandsPopulated;

	/** Make sure EndTick is called. Sanity check mostlyhelpful for unit tests */
	bool									bEndTickCalled;
};

}