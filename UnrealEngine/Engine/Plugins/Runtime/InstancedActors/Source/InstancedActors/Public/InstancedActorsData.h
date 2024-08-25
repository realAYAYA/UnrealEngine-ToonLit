// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstancedActorsTypes.h"
#include "InstancedActorsReplication.h"
#include "MassEntityTemplate.h"

#include "InstancedActorsData.generated.h"

class AInstancedActorsManager;
struct FInstancedActorsSettings;

// @todo there's a lot of public variables in this class, and properties are mixed with functions. A refactor is coming soon.

/**
 * Instance data for all instances of a given AActor class.
 * Provides 'stable' referral to instances by index via offline population of InstanceTransforms which is then
 * consistently loaded on both client and server.
 * @see AInstancedActorsManager
 */
UCLASS(DefaultToInstanced)
class INSTANCEDACTORS_API UInstancedActorsData : public UObject
{
	GENERATED_BODY()

public:
	friend AInstancedActorsManager;

	// Called early in AInstancedActorsManager::InitializeModifyAndSpawnEntities to intitalize Settings, default visualization & Mass entity template
	void Initialize();

	// Called in AInstancedActorsManager::InitializeModifyAndSpawnEntities to spawn Mass entities for each instance
	void SpawnEntities();

	// Called early in AInstancedActorsManager::EndPlay to reconstruct cooked data state from runtime Mass entities as best we can,
	// then despawn all Mass entities and reset any other runtime instance data
	void DespawnEntities();

	UFUNCTION(BlueprintPure, Category = InstancedActors)
	AInstancedActorsManager* GetManager() const;

	AInstancedActorsManager& GetManagerChecked() const;

	// Returns the 'owning' UInstancedActorsData for EntityHandle by checking it's FInstancedActorsFragment (if any)
	// returning nullptr for unknown entities.
	static UInstancedActorsData* GetInstanceDataForEntity(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle);

	FInstancedActorsInstanceIndex GetInstanceIndexForEntity(const FMassEntityHandle EntityHandle) const;

	FMassEntityHandle GetEntity(FInstancedActorsInstanceIndex InstanceIndex) const;

	void SetSharedInstancedActorDataStruct(FSharedStruct InSharedStruct);
	EInstancedActorsBulkLOD GetBulkLOD() const { return SharedInstancedActorDataStruct.Get<FInstancedActorsDataSharedFragment>().BulkLOD; }

	// Identifying integer, unique within outer IAM, used for identifying matching persistence records
	// Incrementally assigned in GetOrCreateActorInstanceData
	UPROPERTY(VisibleAnywhere, Category=InstancedActors)
	uint16 ID;

	// The fully realized actor type for these instances
	UPROPERTY(VisibleAnywhere, Category=InstancedActors)
	TSubclassOf<AActor> ActorClass;

	// Delimiting tag set to group sets of instances
	UPROPERTY(VisibleAnywhere, Category=InstancedActors)
	FInstancedActorsTagSet Tags;

	template<typename T>
	const std::remove_const_t<T>* GetSettingsPtr() const
	{
		return SharedSettings.GetPtr<const std::remove_const_t<T>>();
	}

	template<typename T>
	const std::remove_const_t<T>& GetSettings() const
	{
		return SharedSettings.Get<const std::remove_const_t<T>>();
	}

	void SetSharedSettings(const FConstSharedStruct& InSharedSettings)
	{
		SharedSettings = InSharedSettings;
	}

protected:
	// Compiled settings for these instances, compiled and cached in BeginPlay.
	// Note: Can also be refreshed on demand in PIE, using IA.RefreshSettings CVar
	FConstSharedStruct SharedSettings;
	/*
	@todo note that at the moment Settings do not contain UObjects so we don't need to track the references. But it's a good
	idea to support UObject properties in settings, so we'll add something like the folowing soon:

		void UInstancedActorsData::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
		{
			Super::AddReferencedObjects(InThis, Collector);

			const UInstancedActorsData* Data = Cast<UInstancedActorsData>(InThis);
			check(Data);
		  if (SharedSettings.IsValid())
		  {
			Collector.AddPropertyReferencesWithStructARO(SharedSettings->GetScriptStruct(), SharedSettings->GetMemory(), Data);
		  }
		}	
	*/

public:
	// Per-instance transforms. This essentially forms the 'instance list' until Entities are spawned from this.
	// Note: This list can contain invalid 'removed' entries with 0'd values which will be skipped by
	//       UInstancedActorsInitializerProcessor
	UPROPERTY()
	TArray<FTransform> InstanceTransforms;

	// InstanceTransforms.Num() - number of invalid transforms
	// Note: This is only valid prior to Entity spawning when InstanceTransforms is also valid, after which, spawned
	// entities may be later removed / culled at runtime, invalidating this number.
	// @todo Bundle this and InstanceTransforms, Bounds etc into a `SourceData` to separate them from runtime entity data
	// @see InstanceTransforms
	UPROPERTY()
	uint16 NumValidInstances = 0;

	// Cumulative mesh bounds for all of InstanceTransfroms
	UPROPERTY()
	FBox Bounds = FBox(ForceInit);

	// InstanceTransforms.Num() cached in PostLoad so we can restore InstanceTransforms to this
	// size in ResetInstanceData
	UPROPERTY(Transient)
	uint16 NumInstances = 0;

	// Runtime spawned mass instances
	// @see AInstancedActorsManager::SpawnInstances
	UPROPERTY(Transient)
	TArray<FMassEntityHandle> Entities;

	// The entity template to spawn Mass entities from
	// @see CreateEntityTemplate
	FMassEntityTemplateID EntityTemplateID;

	// Distance from where the object will be culled out - auto computed from bounding box radius or can be set per via the settings
	float MaxDrawDistance = FLT_MAX;

	// Distance from where the lowest LOD will be rendered - auto computed from bounding box and screen size data stored in the StaticMesh or can be set via the settings
	float LowLODDrawDistance = FLT_MAX;

	// Returns true if InstanceTransforms has been consumed to spawn Mass entities
	bool HasSpawnedEntities() const;

	// Returns the total instance count, including both valid & invalid instances e.g: GetNumFreeInstances() + NumValidInstances
	// Useful for iterating by FInstancedActorsInstanceIndex or allocating same-sized instance data arrays
	int32 GetNumInstances() const;

	// Returns the current invalid instance count, if any (i.e: GetNumInstances() - NumValidInstances)
	int32 GetNumFreeInstances() const;

	// Returns true if InstanceHandle refers to this instance data and we have current information for an
	// instance at InstanceHandle.InstanceIndex
	bool IsValidInstance(const FInstancedActorsInstanceHandle& InstanceHandle) const;

#if WITH_EDITOR
	// Add's an instance of this actor type at Transform
	// @see AInstancedActorsManager::AddActorInstance
	FInstancedActorsInstanceHandle AddInstance(const FTransform& Transform, const bool bWorldSpace = false);

	// Set/update the transform for an instance.
	//
	bool SetInstanceTransform(const FInstancedActorsInstanceHandle& InstanceHandle, const FTransform& Transform, const bool bWorldSpace);

	// Removes all instance data for InstanceHandle.
	//
	// This simply adds this instance to a FreeList which incurs extra cost to process at runtime before instance spawning.
	//
	// Note: By default UInstancedActorsSubsystem::RemoveActorInstance will destroy empty managers on
	// last instance removal, implicitly clearing this for subsequent instance adds which would create
	// a fresh manager.
	//
	// @see IA.CompactInstances console command
	bool RemoveInstance(const FInstancedActorsInstanceHandle& InstanceToRemove);

#endif

	// Removes RuntimeRemoveInstances as if they were never present i.e: these removals are not persisted as
	// if made by a player.
	// Prior to entity spawning this simply invalidates InstanceTransforms entries, post entity spawning this
	// destroys spawned entities.
	//
	// Note: Any RuntimeRemoveInstances that have already been removed are safely skipped.
	void RuntimeRemoveInstances(TConstArrayView<FInstancedActorsInstanceIndex> RuntimeRemoveInstances);

	// Removes all instances as if they were never present i.e: these removals are not persisted as
	// if made by a player.
	// Prior to entity spawning this simply invalidates InstanceTransforms entries, post entity spawning this
	// destroys spawned entities.
	//
	// Note: Any RuntimeRemoveInstances that have already been removed are safely skipped.
	void RuntimeRemoveAllInstances();

	// Authority: Permanently destroy InstanceToDestroy by adding it to the persistent DestroyedInstances list
	// and deleting it's Mass entity (deleting any Mass-spawned actors in turn).
	// DestroyedInstances modifications are then replicated to clients.
	// Non-authority: 'Predict' replicated destruction by removing the instance to prevent ISMC instance addition
	// once actor destruction completes.
	void DestroyInstance(FInstancedActorsInstanceIndex InstanceToDestroy);

	// Authority: `Eject` an instance by unlinking it's Actor from Mass, setting the actor to persist itself and
	// then destroying the manager instance. This effectively transfers the actor to the persistence
	// system.
	// Non-authority: Perform the same unlinking and entity removal locally to release the Actor from Mass.
	virtual void EjectInstanceActor(FInstancedActorsInstanceIndex InstanceToEject, AActor& ActorToEject);

	// Called in UInstancedActorsComponent::OnRep_InstanceHandle on clients to set ReplicatedActor as the current
	// actor for Instance's mass entity. As we set bForceActorRepresentationWhileAvailable in UInstancedActorsVisualizationTrait
	// this then forces the entity to switch to Actor representation on clients.
	void SetReplicatedActor(FInstancedActorsInstanceIndex Instance, AActor& ReplicatedActor);

	// Called in UInstancedActorsComponent::EndPlay on clients to clear / unset the current
	// actor for Instance's mass entity.
	// As we set bForceActorRepresentationWhileAvailable in UInstancedActorsVisualizationTrait
	// this then allows the natural WantedRepresentationType (ISMC) to restore
	void ClearReplicatedActor(FInstancedActorsInstanceIndex Instance, AActor& ExpectedActor);

	// Returns the default visualization auto-created in BeginPlay from ActorClass and used
	// by default for all spawned entities in BeginPlay.
	const FInstancedActorsVisualizationInfo& GetDefaultVisualizationChecked() const { return GetVisualizationChecked(0); }

	// Get runtime visualization info by index for visualizations added with AddVisualization.
	// Asserts if VisualizationIndex doesn't correspond to a valid index, or the visualization at that index is
	// invalid (has been removed)
	// @param VisualizationIndex The index of the visualization previously returned by AddVisualization
	const FInstancedActorsVisualizationInfo& GetVisualizationChecked(uint8 VisualizationIndex) const
	{
		check(InstanceVisualizations.IsValidIndex(VisualizationIndex));
		check(InstanceVisualizationAllocationFlags.IsValidIndex(VisualizationIndex));
		const FInstancedActorsVisualizationInfo& VisualizationInfo = InstanceVisualizations[VisualizationIndex];
		check(InstanceVisualizationAllocationFlags[VisualizationIndex] == true);
		return VisualizationInfo;
	}

	// Get runtime visualization info by index for visualizations added with AddVisualization.
	// If VisualizationIndex doesn't correspond to a valid index, or the visualization at that index is
	// invalid (has been removed) nullptr is returned.
	// @param VisualizationIndex The index of the visualization previously returned by AddVisualization
	const FInstancedActorsVisualizationInfo* GetVisualization(uint8 VisualizationIndex) const
	{
		if (InstanceVisualizations.IsValidIndex(VisualizationIndex))
		{
			check(InstanceVisualizationAllocationFlags.IsValidIndex(VisualizationIndex));
			if (InstanceVisualizationAllocationFlags[VisualizationIndex] == true)
			{
				return &InstanceVisualizations[VisualizationIndex];
			}
		}

		return nullptr;
	}

	// Register additional / alternate VisualizationDesc for instances to switch to, creating ISMC's
	// for each VisualizationDesc.InstancedMeshes
	// @warning No more than 254 visualizations may be registered at any time to allow for uint8 indexing.
	// @return Index handle to refer to the registered visualization
	uint8 AddVisualization(const FInstancedActorsVisualizationDesc& VisualizationDesc);

	// Register additional / alternate VisualizationDesc for instances to switch to, creating ISMC's
	// for each VisualizationDesc.InstancedMeshes
	// @warning No more than 254 visualizations may be registered at any time to allow for uint8 indexing.
	// @return Index handle to refer to the registered visualization
	uint8 AddVisualizationAsync(const FInstancedActorsSoftVisualizationDesc& SoftVisualizationDesc);

	// Iterates all currently 'allocated' visualizations previously added with AddVisualization or AddVisualizationAsync
	// @param InFunction						The function to call for each allocated visualization.
	// @param bSkipAsyncLoadingVisualizations	If true, skips visualizations that are still async loading
	//											(FInstancedActorsVisualizationInfo::IsAsyncLoading() = true) which won't have been
	//											initialized yet i.e: won't yet have a valid Desc, ISMComponents or MassStaticMeshDescIndex
	//											yet.
	void ForEachVisualization(TFunctionRef<bool(uint8 /*VisualizationIndex*/, const FInstancedActorsVisualizationInfo&  /*Visualization*/)> InFunction
		, bool bSkipAsyncLoadingVisualizations = true) const;

	// Switches InstanceToSwitch to use the 'visualization' at NewVisualizationIndex, previoulsy added via
	// AddVisualzation. ISMC instances will be removed from the current /former visualization for these instances
	// and new instances will be added to the ISMC's of the new visualization.
	//
	// Note: This is performed via a mass deferred command to add a 'pending' FInstancedActorsMeshSwitchFragment
	// to the instances entity. UInstancedActorsVisualizationSwitcherProcessor will then process this switch
	// fragment to remove the current visualization's ISMC instances if any and setup NewVisualizationIndex
	// to be instanced instead.
	void SwitchInstanceVisualization(FInstancedActorsInstanceIndex InstanceToSwitch, uint8 NewVisualizationIndex);

	// Remove previously registered instance visualization, deleting all ISMC's
	void RemoveVisualization(uint8 VisualizationIndex);

	// Remove all previously registered instance visualization, deleting all ISMC's
	void RemoveAllVisualizations();

	// Called by UInstancedActorsRepresentationActorManagement when a managed actor is destroyed
	void OnInstancedActorDestroyed(AActor& DestroyedActor, const FMassEntityHandle EntityHandle);

	// Called by UInstancedActorsRepresentationActorManagement when a managed actor is moved
	// @return true if MovedActor was ejected
	bool OnInstancedActorMoved(AActor& MovedActor, const FMassEntityHandle EntityHandle);

	// Called when persistent data has been applied / restored
	void OnPersistentDataRestored();

	FORCEINLINE FInstancedActorsDeltaList& GetMutableInstanceDeltaList() { return InstanceDeltas; }

	FORCEINLINE const FInstancedActorsDeltaList& GetInstanceDeltaList() const { return InstanceDeltas; }

	// Called by FInstancedActorsDeltaList::PostReplicatedAdd and PostReplicatedChanged on InstanceDelta replication
	void OnRep_InstanceDeltas(TConstArrayView<int32> UpdatedInstanceDeltaIndices);

	// Called by FInstancedActorsDeltaList::PreReplicatedRemove on InstanceDelta removal replication (just before the actual array element removal)
	void OnRep_PreRemoveInstanceDeltas(TConstArrayView<int32> RemovedInstanceDeltaIndices);

	// Called on both server and client to apply instance delta changes to mass entities
	// On servers: Called by OnPersistentDataRestored after persistence record has deserialized the delta data
	// On clients: Called by OnRep_InstanceDeltas when new delta data has replicated from the server
	// @param InstanceDeltaIndices Indices into InsanceDeltas.GetInstanceDeltas() of the upated deltas to apply
	void ApplyInstanceDeltas();
	void ApplyInstanceDeltas(TConstArrayView<int32> InstanceDeltaIndices);

	// Called on clients by OnRep_PreRemoveInstanceDeltas to revert instance delta change to mass entities
	// @param InstanceDeltaIndices Indices into InsanceDeltas.GetInstanceDeltas() of the deltas to revert
	void RollbackInstanceDeltas(TConstArrayView<int32> InstanceDeltaIndices);

	// Returns a string useful for identifying this instance data object withing it's owning manager
	// @param bCompact if true, this IAD's pointer is used, otherwise the full manager object path is included
	FString GetDebugName(bool bCompact = false) const;

	virtual void UpdateCullDistance();

	// mz@todo IA: move this section back
	// Server-only. Used by LifecycleComponent & LifecycleProcessor to replicate & persist lifecycle changes
	// @todo Provide generic fragment persistence & replication
	void SetInstanceCurrentLifecyclePhase(FInstancedActorsInstanceIndex InstanceIndex, uint8 InCurrentLifecyclePhaseIndex);

	// mz@todo IA: move this section back
	// Server-only. Used by LifecycleComponent & LifecycleProcessor to remove replicated & persisted lifecycle
	// changes (triggering a rollback to default values on clients)
	// @todo Provide generic fragment persistence & replication
	void RemoveInstanceLifecyclePhaseDelta(FInstancedActorsInstanceIndex InstanceIndex);

	// mz@todo IA: move this section back
	// Server-only. Used by LifecycleComponent & LifecycleProcessor to remove the lifecycle time elapsed
	// delta record for InstanceIndex from InstanceDeltas (if any)
	// @todo Provide generic fragment persistence & replication
	void RemoveInstanceLifecyclePhaseTimeElapsedDelta(FInstancedActorsInstanceIndex InstanceIndex);

	int32 GetInstanceDataID() const { return (int32)ID; }

	FMassEntityHandle GetEntityHandleForIndex(const FInstancedActorsInstanceIndex Index) const
	{
		return Entities.IsValidIndex(Index.GetIndex()) ? Entities[Index.GetIndex()] : FMassEntityHandle();
	}

	FMassEntityManager& GetMassEntityManagerChecked() const;

	int32 GetEntityIndexFromCollisionIndex(const UInstancedStaticMeshComponent& ISMComponent, const int32 CollisionIndex) const;

protected:
	//~ Begin UObject Overrides
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostLoad() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsNameStableForNetworking() const override { return true; }
	//~ End UObject Overrides

	// Called on BeginPlay to create default entity template
	void CreateEntityTemplate(const AActor& ExemplarActor);
	virtual void ModifyEntityTemplate(FMassEntityTemplateData& ModifiedTemplate, const AActor& ExemplarActor);

	// Helper function used in ApplyInstanceDeltas to apply a single delta
	// @see ApplyInstanceDeltas
	virtual void ApplyInstanceDelta(FMassEntityManager& EntityManager, const FInstancedActorsDelta& InstanceDelta, TArray<FInstancedActorsInstanceIndex>& OutEntitiesToRemove);

	// Helper function used in RollbackInstanceDeltas to rollback a single delta
	// @see RollbackInstanceDeltas
	virtual void RollbackInstanceDelta(FMassEntityManager& EntityManager, const FInstancedActorsDelta& InstanceDelta);

	// Unlink InstanceToUnlink's Actor (if any) from Mass by clearing the entities reference to it
	// and disconnecting from any subscribed actor signals, essentially 'forgetting' about the actor.
	//
	// Called by EjectInstanceActor to transfer the actor's management to persistence system and OnInstancedActorDestroyed
	// to allow the Actor to complete death process in the background after we've destroyed the Mass entity (which
	// would have otherwise force-destroyed the associated actor)
	//
	// @return The unlinked actor (if one was currently linked, nullptr otherwise)
	AActor* UnlinkActor(FInstancedActorsInstanceIndex InstanceToUnlink);

	// True if Initialize has ever been called
	bool bHasEverInitialized : 1 = false;

	// True during RuntimeRemoveInstances to prevent recursion via OnInstancedActorDestroyed
	bool bRemovingInstances : 1 = false;

	// Sets of ISMCs which instances can swap between to change 'visualization' e.g: 'with berries', 'without berries'
	//
	// InstanceVisualizations[0] is the default visualization, auto-created in BeginPlay from ActorClass and used
	// by default for all spawned entities in BeginPlay.
	//
	// Additional visualizations can be registered at runtime with AddVisualization, for instances to switch to (@todo)
	// @see AddVisualization, AddVisualizationAsync
	UPROPERTY(Transient, VisibleAnywhere, Category=InstancedActors)
	TArray<FInstancedActorsVisualizationInfo> InstanceVisualizations;

	// Allocation bit flags for InstanceVisualizations to allow for TSpareArray-like removal of visualization without affecting remaining
	// visualization indices. Size-matched to InstanceVisualizations with a bit per visualization defining whether or not that visualization
	// entry is actually allocated.
	// Note: TSparseArray can't be used directly here as it's not a UPROPERTY type.
	TBitArray<> InstanceVisualizationAllocationFlags;

	// Adds or reuses a previously removed entry in InstanceVisualizations
	// @return The 'visualization index` of the new or reused entry in InstanceVisualizations
	// @see AddVisualization, AddVisualizationAsync
	uint8 AllocateVisualization();

	// Creates ISMCs for VisualizationDesc.InstancedMeshes, registers them with Mass and sets
	// FInstancedActorsVisualizationInfo::MassStaticMeshDescIndex with the newly registed ISMC decription index
	// @see AddVisualization, AddVisualizationAsync
	void InitializeVisualization(uint8 AllocatedVisualizationIndex, const FInstancedActorsVisualizationDesc& VisualizationDesc);

#if WITH_EDITORONLY_DATA
	// ISMCs created in GetOrCreateActorInstanceData to match default visualizations ISMComponents for editor only preview of instances
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> EditorPreviewISMComponents;

	// Editor time cached bounds for AssetClass, used during editor-only instance creation and deletion to
	// expand / contract Bounds
	UPROPERTY()
	FBox AssetBounds = FBox(ForceInit);
#endif

private:
	// Represents the shared fragment registered with MassEntityManager, that points back to this UInstancedActorsData instance
	FSharedStruct SharedInstancedActorDataStruct;

	// List of FInstancedActorsDelta's to apply to instances, replicated via fast array replication to clients.
	// Formed at runtime via player actions and persisted / restored by game's persistence system
	// @see Serialize
	UPROPERTY(Replicated, SaveGame, Transient)
	FInstancedActorsDeltaList InstanceDeltas;
};
