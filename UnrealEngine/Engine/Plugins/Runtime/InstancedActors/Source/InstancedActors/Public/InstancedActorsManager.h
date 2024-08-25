// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "InstancedActorsTypes.h"
#include "InstancedActorsIndex.h"
#include "ActorPartition/PartitionActor.h"
#include "Containers/BitArray.h"
#include "Templates/SharedPointer.h"

#include "Engine/ActorInstanceManagerInterface.h"
#include "Elements/SMInstance/SMInstanceManager.h"

#include "InstancedActorsManager.generated.h"


class UInstancedActorsModifierVolumeComponent;
class UInstancedActorsSubsystem;
struct FInstancedActorsSettings;
struct FMassEntityManager;
class UInstancedActorsData;
class UInstancedStaticMeshComponent;

namespace UE::InstancedActors::CVars
{
	extern INSTANCEDACTORS_API bool bEnablePersistence;
}

DECLARE_STATS_GROUP(TEXT("InstanceActor Rendering"), STATGROUP_InstancedActorsRendering, STATCAT_Advanced);

// @todo consider renaming to AStaticInstancedActorsManager. Will require renaming of the instance data type as well along
//	with providing base classes to be used in generic code. 

/**
 * Regional manager of 'instanced actors'.
 *
 * Uses Mass to provide lightweight and efficient instancing of items in the distance, with server-authoritative actor
 * spawning around players. AInstancedActorsManager's also provide replication and persistence for their managed instances.
 *
 * Spawned and populated *offline* by UInstancedActorsSubsystem::InstanceActor. Offline population ensures client & server
 * both load the same stable instance data and can commonly refer to instances by index as such.
 */
UCLASS(Config = Mass)
class INSTANCEDACTORS_API AInstancedActorsManager : public APartitionActor, public ISMInstanceManager, public IActorInstanceManagerInterface
{
	GENERATED_BODY()

public:
	AInstancedActorsManager();

	// Adds modifiers already registered with InInstancedActorSubsystem and either calls InitializeModifyAndSpawnEntities to spawn
	// entities immediately, or schedules deferred call by InInstancedActorSubsystem if IA.DeferSpawnEntities is enabled.
	//
	// Called either in BeginPlay if InInstancedActorSubsystem was already initialized or latently once it is, in
	// UInstancedActorsSubsystem::Initialize
	void OnAddedToSubsystem(UInstancedActorsSubsystem& InInstancedActorSubsystem, FInstancedActorsManagerHandle InManagerHandle);

	// Initializes all PerActorClassInstanceData, applies pre-spawn modifiers, spawns entities then applies post-spawn modifiers.
	//
	// Called either directly in OnAddedToSubsystem or deferred and time-sliced in UInstancedActorsSubsystem::Tick if
	// IA.DeferSpawnEntities is enabled.
	void InitializeModifyAndSpawnEntities();

	// Returns true if InstanceTransforms have been consumed to spawn Mass entities in InitializeModifyAndSpawnEntities
	bool HasSpawnedEntities() const { return bHasSpawnedEntities; }

#if WITH_EDITOR
	/** Adds an instance of ActorClass at InstanceTransform location to instance data */
	FInstancedActorsInstanceHandle AddActorInstance(TSubclassOf<AActor> ActorClass, FTransform InstanceTransform, bool bWorldSpace = true, const FInstancedActorsTagSet& InstanceTags = FInstancedActorsTagSet());

	/**
	 * Removes all instance data for InstanceHandle.
	 *
	 * This simply adds this instance to a FreeList which incurs extra cost to process at runtime before instance spawning.
	 *
	 * Note: By default UInstancedActorsSubsystem::RemoveActorInstance will destroy empty managers on
	 * last instance removal, implicitly clearing this for subsequent instance adds which would create
	 * a fresh manager.
	 *
	 * @see IA.CompactInstances console command
	 */
	bool RemoveActorInstance(const FInstancedActorsInstanceHandle& InstanceToRemove);
#endif

	// Searches PerActorClassInstanceData, returning the IAD with matching UInstancedActorsData::ID, if any (nullptr otherwise)
	UInstancedActorsData* FindInstanceDataByID(uint16 InstanceDataID) const;

	// Returns the full set of instance data for this manager
	TConstArrayView<TObjectPtr<UInstancedActorsData>> GetAllInstanceData() const { return PerActorClassInstanceData; }

	/**
	 * Removes all instances as if they were never present i.e: these removals are not persisted as
	 * if made by a player.
	 * Prior to entity spawning this simply invalidates InstanceTransforms entries, post entity spawning this
	 * destroys spawned entities.
	 *
	 * Note: Any RuntimeRemoveInstances that have already been removed are safely skipped.
	 */
	void RuntimeRemoveAllInstances();

	// Returns the current valid instance count (i.e: NumInstances - FreeList.Num()) sum for all instance datas
	int32 GetNumValidInstances() const;

	bool HasAnyValidInstances() const;

	// Returns true if InstanceHandle refers to this manager and we have current information for an
	// instance at InstanceHandle.InstanceIndex in InstanceHandle.InstancedActorData
	bool IsValidInstance(const FInstancedActorsInstanceHandle& InstanceHandle) const;

	// Returns world space cumulative instance bounds. Only valid after BeginPlay.
	FBox GetInstanceBounds() const { return InstanceBounds; }

	/**
	 * Iteration callback for ForEachInstance
	 * @param InstanceHandle	Handle to the current instance in the iteration
	 * @param InstanceTransform If entities have been spawned, this will be taken from the Mass transform fragment, else from UInstancedActorsData::InstanceTransforms
	 * @param InterationContext Provides useful functionality while iterating instances like safe instance deletion
	 * @return Return true to continue iteration to subsequent instances, false to break iteration.
	 */
	using FInstanceOperationFunc = TFunctionRef<bool(const FInstancedActorsInstanceHandle& InstanceHandle, const FTransform& InstanceTransform, FInstancedActorsIterationContext& IterationContext)>;

	/**
	 * Predicate taking a UInstancedActorsData and returns true if IAD matches search criteria, false otherwise.
	 * @param InstanceHandle	Handle to the current instance in the iteration
	 * @param InstanceTransform If entities have been spawned, this will be taken from the Mass transform fragment, else from UInstancedActorsData::InstanceTransforms
	 * @param InterationContext Provides useful functionality while iterating instances like safe instance deletion
	 * @return Return true to continue iteration to subsequent instances, false to break iteration.
	 */
	using FInstancedActorDataPredicateFunc = TFunctionRef<bool(const UInstancedActorsData& InstancedActorData)>;

	/**
	 * Call InOperation for each valid instance in this manager. Prior to entity spawning in BeginPlay, this iterates valid UInstancedActorsData::InstanceTransforms.
	 * Once entities have been spawned, UInstancedActorsData::Entities are iterated.
	 * @param Operation Function to call for each instance found within QueryBounds
	 * @return false if InOperation ever returned false to break iteration, true otherwise.
	 */
	bool ForEachInstance(FInstanceOperationFunc Operation) const;
	bool ForEachInstance(FInstanceOperationFunc Operation, FInstancedActorsIterationContext& IterationContext, TOptional<FInstancedActorDataPredicateFunc> InstancedActorDataPredicate = TOptional<FInstancedActorDataPredicateFunc>()) const;

	/**
	 * Call InOperation for each valid instance in this manager whose location falls within QueryBounds. Prior to entity spawning in BeginPlay, this iterates valid
	 * UInstancedActorsData::InstanceTransforms. Once entities have been spawned, UInstancedActorsData::Entities are iterated.
	 * @param QueryBounds A world space FBox or FSphere to test instance locations against using QueryBounds.IsInside(InstanceLocation)
	 * @param InOperation Function to call for each instance found within QueryBounds
	 * @return false if InOperation ever returned false to break iteration, true otherwise.
	 */
	template <typename TBoundsType>
	bool ForEachInstance(const TBoundsType& QueryBounds, FInstanceOperationFunc InOperation) const;
	template <typename TBoundsType>
	bool ForEachInstance(const TBoundsType& QueryBounds, FInstanceOperationFunc InOperation, FInstancedActorsIterationContext& IterationContext, TOptional<FInstancedActorDataPredicateFunc> InstancedActorDataPredicate = TOptional<FInstancedActorDataPredicateFunc>()) const;

	// Outputs instance metrics to Ar
	void AuditInstances(FOutputDevice& Ar, bool bDebugDraw = false, float DebugDrawDuration = 10.0f) const;

	// Called by IA.CompactInstances console command to fully remove FreeList instances
	void CompactInstances(FOutputDevice& Ar);

	void AddModifierVolume(UInstancedActorsModifierVolumeComponent& ModifierVolume);
	void RemoveModifierVolume(UInstancedActorsModifierVolumeComponent& ModifierVolume);

	// Request the persistant data system to re-save this managers persistent data
	void RequestPersistentDataSave();

	// Helper function to deduce appropriate instanced static mesh bounds for ActorClass
	static FBox CalculateBounds(TSubclassOf<AActor> ActorClass);

	// Returns the Mass entity manager used to spawn entities. Valid only after BeginPlay
	FORCEINLINE TSharedPtr<FMassEntityManager> GetMassEntityManager() const { return MassEntityManager; }
	FORCEINLINE FMassEntityManager& GetMassEntityManagerChecked() const
	{
		check(MassEntityManager.IsValid());
		return *MassEntityManager;
	}

	// Returns the Instanced Actor Subsystem this Manager is registered with. Valid only after BeginPlay
	FORCEINLINE UInstancedActorsSubsystem* GetInstancedActorSubsystem() const { return InstancedActorSubsystem; }
	FORCEINLINE UInstancedActorsSubsystem& GetInstancedActorSubsystemChecked() const
	{
		check(InstancedActorSubsystem);
		return *InstancedActorSubsystem;
	}

	//~ Begin APartitionActor Overrides
#if WITH_EDITOR
	virtual uint32 GetDefaultGridSize(UWorld* InWorld) const override;
	virtual FGuid GetGridGuid() const override;
	void SetGridGuid(const FGuid& InGuid);
#endif
	//~ End APartitionActor Overrides

	static void UpdateInstanceStats(int32 InstanceCount, EInstancedActorsBulkLOD LODMode, bool Increment);

	/**
	 * Registers Components as related to InstanceData for IActorInstanceManagerInterface-related purposes.
	 */
	void RegisterInstanceDatasComponents(const UInstancedActorsData& InstanceData, TConstArrayView<TObjectPtr<UInstancedStaticMeshComponent>> Components);

	/**
	 * Unregisters Component from IActorInstanceManagerInterface-related tracking. Note that the function will assert
	 * whether the Component has been registered in the first place
	 */
	void UnregisterInstanceDatasComponent(UInstancedStaticMeshComponent& Component);

	virtual void CreateISMComponents(const FInstancedActorsVisualizationDesc& VisualizationDesc, FConstSharedStruct SharedSettings
		, TArray<TObjectPtr<UInstancedStaticMeshComponent>>& OutComponents, const bool bEditorPreviewISMCs = false);

	//~ Begin UObject Overrides
	virtual void Serialize(FStructuredArchive::FRecord Record) override;
	//~ End UObject Overrides

protected:
	// mz@todo a temp glue to be removed once InstancedActors get moved out of original project for good. It's virtual and I don't like it.
	virtual void RequestActorSave(AActor* Actor) {}

	//~ Begin AActor Overrides
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool IsHLODRelevant() const override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual bool IsUserManaged() const override { return true; }
	virtual FBox GetStreamingBounds() const override;
#endif
	//~ End AActor Overrides

	//~ Begin IActorInstanceManagerInterface Overrides
	virtual int32 ConvertCollisionIndexToInstanceIndex(int32 InIndex, const UPrimitiveComponent* RelevantComponent) const /*override*/;
	virtual AActor* FindActor(const FActorInstanceHandle& Handle) /*override*/;
	virtual AActor* FindOrCreateActor(const FActorInstanceHandle& Handle) /*override*/;
	virtual UClass* GetRepresentedClass(const int32 InstanceIndex) const /*override*/;
	virtual ULevel* GetLevelForInstance(const int32 InstanceIndex) const /*override*/;
	virtual FTransform GetTransform(const FActorInstanceHandle& Handle) const /*override*/;
	//~ End IActorInstanceManagerInterface Overrides

	// Called by Serialize for SaveGame archives to save / load IAD persistence data
	// @param Record		The archive record to read / write IAD save data to
	// @param InstanceData	The InstanceData to serialize from / to. May be nullptr when loading if Record's IAD has been removed since saving.
	//						In this case, we still need to read Record to seek the archive past this IAD record consistently.
	// @param TimeDelta		Real time in seconds since serialization (0 when saving)
	void SerializeInstancePersistenceData(FStructuredArchive::FRecord Record, UInstancedActorsData* InstanceData, int64 TimeDelta) const;

	// Attempts to run any 'pending' modifiers in ModifierVolumes where are appropriate to run givem HasSpawnedEntities
	// Called in BeginPlay prior to, and then again after SpawnEntities. Also called in AddModifierVolume.
	// @see UInstancedActorsModifierBase::bRequiresSpawnedEntities
	void TryRunPendingModifiers();

	// Called when persistent data has been applied / restored
	void OnPersistentDataRestored();

	// Calculate cumulative local space instance bounds for all PerActorClassInstanceData
	FBox CalculateLocalInstanceBounds() const;

#if WITH_EDITOR
	// Helper function to create and initialize per-actor-class UInstancedActorsData's, optionally further partitioned by InstanceTags
	UInstancedActorsData& GetOrCreateActorInstanceData(TSubclassOf<AActor> ActorClass, const FInstancedActorsTagSet& InstanceTags);

	// Used to set the right properties on the editor ISMCs so we can do per-instance selection.
	virtual void PreRegisterAllComponents() override;
#endif

	UPROPERTY(Transient)
	TObjectPtr<UInstancedActorsSubsystem> InstancedActorSubsystem;

	UPROPERTY(Transient)
	FInstancedActorsManagerHandle ManagerHandle;

	TSharedPtr<FMassEntityManager> MassEntityManager;

	/** Saved Actor Guid. Initialized from the actor name in constructor */
	UPROPERTY(VisibleAnywhere, Category=InstancedActors)
	FGuid SavedActorGuid = FGuid();

	/** True if SpawnEntities has been called to spawn entities. Reset in EndPlay */
	UPROPERTY(Transient)
	bool bHasSpawnedEntities = false;

	// Incremented in GetOrCreateActorInstanceData to provide IAD's with a stable, unique identifier
	// within this IAM.
	// @see UInstancedActorsData::ID
	UPROPERTY()
	uint16 NextInstanceDataID = 0;

	// Per-actor-class instance data populated by AddActorInstance
	UPROPERTY(Instanced, VisibleAnywhere, Category=InstancedActors)
	TArray<TObjectPtr<UInstancedActorsData>> PerActorClassInstanceData;

	// World space cumulative instance bounds, calculated in BeginPlay
	UPROPERTY(Transient)
	FBox InstanceBounds = FBox(ForceInit);

	// Modifier volumes added via AddModifierVolume
	TArray<TWeakObjectPtr<UInstancedActorsModifierVolumeComponent>> ModifierVolumes;

	// A bit flag per volume in ModifierVolumes for whether the volume has pending Modifiers to run on this manager
	// i.e if PendingModifierVolumeModifiers[VolumeIndex] has any true flags.
	// Some modifiers can run prior to entity spawning for efficiency purposes, others must be held 'pending'
	// later execution after entities have spawned.
	TBitArray<> PendingModifierVolumes;

	// A set of bit flags per volume in ModifierVolumes, matching each modifiers Modifiers list, marking
	// whether the Modifier has yet to run on this manager or not (true = needs running).
	//
	// Some modifiers can run prior to entity spawning for efficiency purposes, others must be held 'pending'
	// later execution after entities have spawned.
	TArray<TBitArray<>> PendingModifierVolumeModifiers;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UInstancedStaticMeshComponent>, int32> ISMComponentToInstanceDataMap;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid ManagerGridGuid;

	// Set this to false to be able to move the instances contained by this IAM. The property is not saved and will reset.
	UPROPERTY(EditAnywhere, Transient, meta = (DisplayPriority = 1), Category = InstancedActors)
	bool bLockInstanceLocation = true;
#endif

	//~ Begin ISMInstanceManager Overrides
	virtual FText GetSMInstanceDisplayName(const FSMInstanceId& InstanceId) const override;
	virtual FText GetSMInstanceTooltip(const FSMInstanceId& InstanceId) const override;
	virtual bool CanEditSMInstance(const FSMInstanceId& InstanceId) const override;
	virtual bool CanMoveSMInstance(const FSMInstanceId& InstanceId, const ETypedElementWorldType WorldType) const override;
	virtual bool GetSMInstanceTransform(const FSMInstanceId& InstanceId, FTransform& OutInstanceTransform, bool bWorldSpace = false) const override;
	virtual bool SetSMInstanceTransform(const FSMInstanceId& InstanceId, const FTransform& InstanceTransform, bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false) override;
	virtual void NotifySMInstanceMovementStarted(const FSMInstanceId& InstanceId) override;
	virtual void NotifySMInstanceMovementOngoing(const FSMInstanceId& InstanceId) override;
	virtual void NotifySMInstanceMovementEnded(const FSMInstanceId& InstanceId) override;
	virtual void NotifySMInstanceSelectionChanged(const FSMInstanceId& InstanceId, const bool bIsSelected) override;
	virtual bool DeleteSMInstances(TArrayView<const FSMInstanceId> InstanceIds) override;
	virtual bool DuplicateSMInstances(TArrayView<const FSMInstanceId> InstanceIds, TArray<FSMInstanceId>& OutNewInstanceIds) override;
	//~ End ISMInstanceManager Overrides

	/**
	 * Try to extract the actor from the provided handle or from associated Mass Entity.
	 * When unable to retrieve it returns nullptr but also the associated EntityView so caller could reuse it to create the actor.
	 */
	AActor* FindActorInternal(const FActorInstanceHandle& Handle, FMassEntityView& OutEntityView, bool bEnsureOnMissingInstanceDataOrMassEntity);

	FInstancedActorsInstanceHandle ActorInstanceHandleFromFSMInstanceId(const FSMInstanceId& InstanceId) const;
};
