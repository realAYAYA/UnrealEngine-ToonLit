// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ClusterCreationParameters.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Misc/EnumClassFlags.h"

namespace Chaos
{
	using FClusterUnionIndex = int32;
	using FClusterUnionExplicitIndex = int32;

	class FRigidClustering;
	class FPBDRigidsEvolutionGBF;
	struct FClusterCreationParameters;

	enum class EClusterUnionOperation
	{
		Add,
		// AddReleased is the original behavior where if the particle to be added is a cluster, we will release the cluster first
		// and add its children instead.
		AddReleased,
		Remove,
		UpdateChildToParent
	};

	enum class EClusterUnionOperationTiming
	{
		Never,
		Defer,
		Immediate
	};

	enum class EUpdateClusterUnionPropertiesFlags : int32
	{
		None = 0,
		RecomputeMassOrientation = 1 << 0,
		ForceGenerateConnectionGraph = 1 << 1,
		IncrementalGenerateConnectionGraph = 1 << 2,
		UpdateKinematicProperties = 1 << 3,
		All = RecomputeMassOrientation | ForceGenerateConnectionGraph | UpdateKinematicProperties
	};
	ENUM_CLASS_FLAGS(EUpdateClusterUnionPropertiesFlags);

	enum class EClusterUnionConnectivityOperation : int8
	{
		Add,
		Remove
	};

	struct FClusterUnionCreationParameters
	{
		FClusterUnionExplicitIndex ExplicitIndex = INDEX_NONE;
		const FUniqueIdx* UniqueIndex = nullptr;
		uint32 ActorId = 0;
		uint32 ComponentId = 0;
	};

	struct FClusterUnionParticleProperties
	{
		// An auxiliary particle will be removed from the cluster union if FRigidClustering::HandleConnectivityOnReleaseClusterParticle detects an island made up of only auxiliary particles.
		bool bIsAuxiliaryParticle = false;
	};

	struct FClusterUnion
	{
		// The root cluster particle that we created internally to represent the cluster.
		FPBDRigidClusteredParticleHandle* InternalCluster;

		// The thread-safe collision geometry that can be shared between the GT and PT.
		TSharedPtr<FImplicitObject, ESPMode::ThreadSafe> SharedGeometry;

		// All the particles that belong to this cluster.
		TArray<FPBDRigidParticleHandle*> ChildParticles;

		// Additional properties that are only relevant for particles that have been added into a cluster union.
		TMap<FPBDRigidParticleHandle*, FClusterUnionParticleProperties> ChildProperties;

		// The internal index used to reference the cluster union.
		FClusterUnionIndex InternalIndex;

		// An explicit index set by the user if any.
		FClusterUnionExplicitIndex ExplicitIndex;

		// Need to remember the parameters used to create the cluster so we can update it later.
		FClusterCreationParameters Parameters;

		// Other parameters that aren't related to clusters generally but are related to info we need about the cluster union.
		FClusterUnionCreationParameters ClusterUnionParameters;

		// Whether or not the position/rotation needs to be computed the first time a particle is added.
		bool bNeedsXRInitialization = true;

		// Whether or not the anchor got set by the GT and we shouldn't try to recompute its value.
		bool bAnchorLock = false;

		// Whether or not we need to check for connectivity for this cluster union.
		bool bCheckConnectivity = false;

		// Pending particles that need to be added into the connectivity graph.
		TArray<TPair<FPBDRigidParticleHandle*, EClusterUnionConnectivityOperation>> PendingConnectivityOperations;
	};

	struct FClusterUnionChildToParentUpdate
	{
		FTransform ChildToParent;
		bool bLock = false;
	};

	/**
	 * This class is used by Chaos to create internal clusters that will
	 * cause one or more clusters to simulate together as a single rigid
	 * particle.
	 */
	class FClusterUnionManager
	{
	public:

		template<EThreadContext Id>
		static FImplicitObjectTransformed* CreateTransformGeometryForClusterUnion(TThreadRigidParticle<Id>* Child, const FTransform& Frame)
		{
			FImplicitObjectTransformed* TransformedChildGeometry = new TImplicitObjectTransformed<FReal, 3>(Child->Geometry(), Frame);
			if constexpr (Id == EThreadContext::External)
			{
				TransformedChildGeometry->SetSharedObject(Child->GeometrySharedLowLevel());
			}
			else
			{
				TransformedChildGeometry->SetSharedObject(Child->SharedGeometry());
			}
			return TransformedChildGeometry;
		}

		CHAOS_API FClusterUnionManager(FRigidClustering& InClustering, FPBDRigidsEvolutionGBF& InEvolution);

		// Creates a new cluster union with an automatically assigned cluster union index.
		CHAOS_API FClusterUnionIndex CreateNewClusterUnion(const FClusterCreationParameters& Parameters, const FClusterUnionCreationParameters& ClusterUnionParameters = FClusterUnionCreationParameters{});

		// Destroy a given cluster union.
		CHAOS_API void DestroyClusterUnion(FClusterUnionIndex Index);

		// Add a new operation to the queue. Note that we currently only support the pending/flush operations for explicit operations. The behavior is legacy anyway so this should be fine.
		CHAOS_API void AddPendingExplicitIndexOperation(FClusterUnionExplicitIndex Index, EClusterUnionOperation Op, const TArray<FPBDRigidParticleHandle*>& Particles);
		CHAOS_API void AddPendingClusterIndexOperation(FClusterUnionIndex Index, EClusterUnionOperation Op, const TArray<FPBDRigidParticleHandle*>& Particles);

		// Actually performs the change specified in the FClusterUnionOperationData structure.
		CHAOS_API void HandleAddOperation(FClusterUnionIndex ClusterIndex, const TArray<FPBDRigidParticleHandle*>& Particles, bool bReleaseClustersFirst);

		// Removes the specified particles from the specified cluster.
		CHAOS_API void HandleRemoveOperation(FClusterUnionIndex ClusterIndex, const TArray<FPBDRigidParticleHandle*>& Particles, EClusterUnionOperationTiming UpdateClusterPropertiesTiming);

		// Helper function to remove particles given only the particle handle. This will consult the lookup table to find which cluster the particle is in before doing a normal remove operation.
		CHAOS_API void HandleRemoveOperationWithClusterLookup(const TArray<FPBDRigidParticleHandle*>& InParticles, EClusterUnionOperationTiming UpdateClusterPropertiesTiming);

		// Performs the queued child to parent modifications.
		CHAOS_API void HandleUpdateChildToParentOperation(FClusterUnionIndex ClusterIndex, const TArray<FPBDRigidParticleHandle*>& Particles);

		// Will be called at the beginning of every time step to ensure that all the expected cluster unions have been modified.
		CHAOS_API void FlushPendingOperations();

		// Update cluster union properties here if they were deferred. This can be called manually but will otherwise also be handled in FlushPendingOperations.
		CHAOS_API void HandleDeferredClusterUnionUpdateProperties();

		// Access the cluster union externally.
		CHAOS_API FClusterUnion* FindClusterUnionFromExplicitIndex(FClusterUnionExplicitIndex Index);
		CHAOS_API FClusterUnion* FindClusterUnion(FClusterUnionIndex Index);
		CHAOS_API FClusterUnion* FindClusterUnionFromParticle(FPBDRigidParticleHandle* Particle);
		CHAOS_API FClusterUnionIndex FindClusterUnionIndexFromParticle(FPBDRigidParticleHandle* Particle);

		// An extension to FindClusterUnionIndexFromParticle to check whether or not the given particle is the cluster union particle itself.
		CHAOS_API bool IsClusterUnionParticle(FPBDRigidClusteredParticleHandle* Particle);

		// Changes the ChildToParent of a number of particles in a cluster union. bLock will prevent other functions from overriding the ChildToParent (aside from another call to UpdateClusterUnionParticlesChildToParent).
		CHAOS_API void UpdateClusterUnionParticlesChildToParent(FClusterUnionIndex Index, const TArray<FPBDRigidParticleHandle*>& Particles, const TArray<FTransform>& ChildToParent, bool bLock);

		// Update the cluster union's properties after its set of particle changes.
		CHAOS_API void UpdateAllClusterUnionProperties(FClusterUnion& ClusterUnion, EUpdateClusterUnionPropertiesFlags Flags = EUpdateClusterUnionPropertiesFlags::All);

		// Returns all cluster unions. Really meant only to be used for debugging.
		const TMap<FClusterUnionIndex, FClusterUnion>& GetAllClusterUnions() const { return ClusterUnions; }

		// Put in a deferred request to update the properties of the specified cluster union. Flags can limit the scope of the update.
		CHAOS_API void RequestDeferredClusterPropertiesUpdate(FClusterUnionIndex ClusterIndex, EUpdateClusterUnionPropertiesFlags Flags);
	private:
		FRigidClustering& MClustering;
		FPBDRigidsEvolutionGBF& MEvolution;

		using FClusterOpMap = TMap<EClusterUnionOperation, TArray<FPBDRigidParticleHandle*>>;
		template<typename TIndex>
		using TClusterIndexOpMap = TMap<TIndex, FClusterOpMap>;

		TClusterIndexOpMap<FClusterUnionIndex> PendingClusterIndexOperations;
		TClusterIndexOpMap<FClusterUnionExplicitIndex> PendingExplicitIndexOperations;
		TMap<FPBDRigidClusteredParticleHandle*, FClusterUnionChildToParentUpdate> PendingChildToParentUpdates;
		TSet<FPBDRigidClusteredParticleHandle*> PendingParticlesToUndoChildToParentLock;

		template<typename TIndex>
		void AddPendingOperation(TClusterIndexOpMap<TIndex>& OpMap, TIndex Index, EClusterUnionOperation Op, const TArray<FPBDRigidParticleHandle*>& Particles)
		{
			FClusterOpMap& Ops = OpMap.FindOrAdd(Index);
			TArray<FPBDRigidParticleHandle*>& OpData = Ops.FindOrAdd(Op);
			OpData.Append(Particles);
		}

		// All of our actively managed cluster unions. We need to keep track of these
		// so a user could use the index to request modifications to a specific cluster union.
		TMap<FClusterUnionIndex, FClusterUnion> ClusterUnions;

		// The set of cluster unions that we want to defer updating their cluster union properties for.
		TMap<FClusterUnionIndex, EUpdateClusterUnionPropertiesFlags> DeferredClusterUnionsForUpdateProperties;
		
		//
		// There are two ways we can pick a new union index:
		// - If a cluster union gets released/destroyed, that index can be reused.
		// - Otherwise, we use the NextAvailableUnionIndex which is just the max index we've seen + 1.
		//
		CHAOS_API FClusterUnionIndex ClaimNextUnionIndex();
		TArray<FClusterUnionIndex> ReusableIndices;
		FClusterUnionIndex NextAvailableUnionIndex = 1;

		// Using the user's passed in FClusterUnionIndex may result in strange unexpected behavior if the 
		// user creates a cluster with a specified index. Thus we will map all explicitly requested indices
		// (i.e. an index that comes in via FClusterUnionOperationData for the first time) to an automatically
		// generated index (i.e one that would returned via CreateNewClusterUnion).
		TMap<FClusterUnionExplicitIndex, FClusterUnionIndex> ExplicitIndexMap;
		CHAOS_API FClusterCreationParameters DefaultClusterCreationParameters() const;

		// If no cluster index is set but an explicit index is set, map the explicit index to a regular index.
		CHAOS_API FClusterUnionIndex GetOrCreateClusterUnionIndexFromExplicitIndex(FClusterUnionExplicitIndex InIndex);

		// Forcefully recreate the shared geometry on a cluster. Potentially expensive so ideally should be used rarely.
		CHAOS_API TSharedPtr<FImplicitObject, ESPMode::ThreadSafe> ForceRecreateClusterUnionSharedGeometry(const FClusterUnion& Union);

		// Handles updating the cluster union.
		CHAOS_API void DeferredClusterUnionUpdate(FClusterUnion& Union, EUpdateClusterUnionPropertiesFlags Flags);

		// Flush the cluster union's incremental connectivity operations
		CHAOS_API void FlushIncrementalConnectivityGraphOperations(FClusterUnion& ClusterUnion);
	};

}
