// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentInstanceDataCache.h"
#include "Components/ActorComponent.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "Utils/PCGExtraCapture.h"

#include "PCGComponent.generated.h"

namespace EEndPlayReason { enum Type : int; }

class APCGPartitionActor;
struct FPCGContext;
class UPCGComponent;
class UPCGGraph;
class UPCGGraphInterface;
class UPCGGraphInstance;
class UPCGManagedResource;
class UPCGData;
class UPCGSubsystem;
class ALandscapeProxy;
class FLandscapeProxyComponentDataChangedParams;
class UClass;

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPCGGraphGenerated, UPCGComponent*);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPCGGraphCleaned, UPCGComponent*);
#endif

UENUM(Blueprintable)
enum class EPCGComponentInput : uint8
{
	Actor, /** Generates based on owning actor */
	Landscape,
	Other,
	// More?
	EPCGComponentInput_MAX
};

UENUM(Blueprintable)
enum class EPCGComponentGenerationTrigger : uint8
{
	GenerateOnLoad,
	GenerateOnDemand
};

UENUM(meta = (Bitflags))
enum class EPCGComponentDirtyFlag : uint8
{
	None = 0,
	Actor = 1 << 0,
	Landscape = 1 << 1,
	Input = 1 << 2,
	Data = 1 << 3,
	All = Actor | Landscape | Input | Data
};
ENUM_CLASS_FLAGS(EPCGComponentDirtyFlag);

UCLASS(BlueprintType, ClassGroup = (Procedural), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "PCG"))
class PCG_API UPCGComponent : public UActorComponent
{
	UPCGComponent(const FObjectInitializer& InObjectInitializer);

	GENERATED_BODY()

	friend class UPCGManagedActors;
	friend class UPCGSubsystem;

public:
	/** ~Begin UObject interface */
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditImport() override;
#endif
	/** ~End UObject interface */

	//~Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentCreated() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void OnRegister() override;

protected:
	friend struct FPCGComponentInstanceData;
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;
	//~End UActorComponent Interface

public:
	UPCGData* GetPCGData();
	UPCGData* GetInputPCGData();
	UPCGData* GetActorPCGData();
	UPCGData* GetLandscapePCGData();
	UPCGData* GetLandscapeHeightPCGData();
	UPCGData* GetOriginalActorPCGData();

	bool CanPartition() const;

	UPCGGraph* GetGraph() const;
	UPCGGraphInstance* GetGraphInstance() const { return GraphInstance; }

	void SetGraphLocal(UPCGGraphInterface* InGraph);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = PCG)
	void SetGraph(UPCGGraphInterface* InGraph);

	/** Registers some managed resource to the current component */
	UFUNCTION(BlueprintCallable, Category = PCG)
	void AddToManagedResources(UPCGManagedResource* InResource);

	void ForEachManagedResource(TFunctionRef<void(UPCGManagedResource*)> InFunction);

	/** Transactionable methods to be called from details UI */
	void Generate();
	void Cleanup();

	/** Starts generation from a local (vs. remote) standpoint. Will not be replicated. Will be delayed. */
	UFUNCTION(BlueprintCallable, Category = PCG)
	void GenerateLocal(bool bForce);

	/** Cleans up the generation from a local (vs. remote) standpoint. Will not be replicated. Will be delayed. */
	UFUNCTION(BlueprintCallable, Category = PCG)
	void CleanupLocal(bool bRemoveComponents, bool bSave = false);

	/* Same as CleanupLocal, but without any delayed tasks. All is done immediately. */
	void CleanupLocalImmediate(bool bRemoveComponents);

	/** Networked generation call that also activates the component as needed */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = PCG)
	void Generate(bool bForce);

	/** Networked cleanup call */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = PCG)
	void Cleanup(bool bRemoveComponents, bool bSave = false);

	/** Cancels in-progress generation */
	void CancelGeneration();

	/** Notify properties changed, used in runtime cases, will dirty & trigger a regeneration if needed */
	UFUNCTION(BlueprintCallable, Category = PCG)
	void NotifyPropertiesChangedFromBlueprint();

	/** Retrieves generated data */
	UFUNCTION(BlueprintCallable, Category = PCG)
	const FPCGDataCollection& GetGeneratedGraphOutput() const { return GeneratedGraphOutput; }

	/** Move all generated resources under a new actor, following a template (AActor if not provided), clearing all link to this PCG component. Returns the new actor.*/
	UFUNCTION(BlueprintCallable, Category = PCG)
	AActor* ClearPCGLink(UClass* TemplateActor = nullptr);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGComponentInput InputType = EPCGComponentInput::Actor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bParseActorComponents = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	int Seed = 42;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Properties)
	bool bActivated = true;

	/* In World Partition map, will partition the component in a grid, according to PCGWorldActor settings, dispatching the generation to multiple local components.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = Properties, meta = (EditCondition = "!bIsComponentLocal", DisplayName = "Is Partitioned"))
	bool bIsComponentPartitioned = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Properties, AdvancedDisplay, meta = (EditCondition = "!bIsComponentLocal", EditConditionHides))
	EPCGComponentGenerationTrigger GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnLoad;

	/** Flag to indicate whether this component has run in the editor. Note that for partitionable actors, this will always be false. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, AdvancedDisplay, Category = Properties)
	bool bGenerated = false;

	UPROPERTY(NonPIEDuplicateTransient)
	bool bRuntimeGenerated = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = Properties, meta = (DisplayName = "Regenerate PCG volume in editor"))
	bool bRegenerateInEditor = true;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Transient, AdvancedDisplay, Category = Properties)
	bool bDirtyGenerated = false;

	// Property that will automatically be set on BP templates, to allow for "Generate on add to world" in editor.
	// Set it as a property to automatically transfer it to its child.
	// Don't use it directly, use ShouldGenerateBPPCGAddedToWorld, as there are other conditions checked.
	UPROPERTY()
	bool bForceGenerateOnBPAddedToWorld = false;

	FOnPCGGraphGenerated OnPCGGraphGeneratedDelegate;
	FOnPCGGraphCleaned OnPCGGraphCleanedDelegate;
#endif

	/** Can specify a list of functions from the owner of this component to be called when generation is done, in order. 
	*   Need to take (and only take) a PCGDataCollection as parameter and with "CallInEditor" flag enabled.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = Properties)
	TArray<FName> PostGenerateFunctionNames;

	/** Return if we are currently generating the graph for this component */
	bool IsGenerating() const { return CurrentGenerationTask != InvalidPCGTaskId; }
	bool IsCleaningUp() const { return CurrentCleanupTask != InvalidPCGTaskId; }

	/** Returns task ids to do internal chaining */
	FPCGTaskId GetGenerationTaskId() const { return CurrentGenerationTask; }

#if WITH_EDITOR
	void Refresh();
	void OnRefresh();

	/** Dirty generated data depending on the flag. By default the call is forwarded to the local components.
	    We don't forward if the local component has callbacks that would dirty them too.
		For example: When a tracked actor move, we only want to dirty the impacted local components.*/
	void DirtyGenerated(EPCGComponentDirtyFlag DataToDirtyFlag = EPCGComponentDirtyFlag::None, const bool bDispatchToLocalComponents = true);

	/** Reset last generated bounds to force PCGPartitionActor creation on next refresh */
	void ResetLastGeneratedBounds();

	/** Functions for managing the node inspection cache */
	bool IsInspecting() const { return bIsInspecting; }
	void EnableInspection() { bIsInspecting = true; };
	void DisableInspection();
	void StoreInspectionData(const UPCGNode* InNode, const FPCGDataCollection& InInspectionData);
	const FPCGDataCollection* GetInspectionData(const UPCGNode* InNode) const;

	/** Used by the tracking system to know if the component need to track actors. Not enabled for now, tracking is still done on the component.*/
	bool ShouldTrackActors() const { return false; }

	/** Know if we need to force a generation, in case of BP added to the world in editor */
	bool ShouldGenerateBPPCGAddedToWorld() const;
#endif

	/** Utility function (mostly for tests) to properly set the value of bIsComponentPartitioned.
	*   Will do an immediate cleanup first and then register/unregister the component to the subsystem.
	*   It's your responsibility after to regenerate the graph if you want to.
	*/
	void SetIsPartitioned(bool bIsNowPartitioned);

	bool IsPartitioned() const;
	bool IsLocalComponent() const { return bIsComponentLocal; }

	/* Responsibility of the PCG Partition Actor to mark is local */
	void MarkAsLocalComponent() { bIsComponentLocal = true; }

	/** Updates internal properties from other component, dirties as required but does not trigger Refresh */
	void SetPropertiesFromOriginal(const UPCGComponent* Original);

	UPCGSubsystem* GetSubsystem() const;

	FBox GetGridBounds() const;
	FBox GetLastGeneratedBounds() const { return LastGeneratedBounds; }

	/** Builds the PCG data from a given actor and its PCG component, and places it in a data collection with appropriate tags */
	static FPCGDataCollection CreateActorPCGDataCollection(AActor* Actor, const UPCGComponent* Component, const TFunction<bool(EPCGDataType)>& InDataFilter, bool bParseActor = true);

	/** Builds the canonical PCG data from a given actor and its PCG component if any. */
	static UPCGData* CreateActorPCGData(AActor* Actor, const UPCGComponent* Component, bool bParseActor = true);

protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = PCG, Instanced, meta = (NoResetToDefault))
	TObjectPtr<UPCGGraphInstance> GraphInstance;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UPCGGraph> Graph_DEPRECATED;

private:
	// Note for upgrade: can be safely replaced by bIsComponentPartitioned. Needed a new variable to change the default value. Kept to allow proper value change, cannot be deprecated. Do not use.
	UPROPERTY()
	bool bIsPartitioned = true;
#endif // WITH_EDITORONLY_DATA

private:
	UPCGData* CreatePCGData();
	UPCGData* CreateInputPCGData();
	UPCGData* CreateActorPCGData();
	UPCGData* CreateActorPCGData(AActor* Actor, bool bParseActor = true);
	UPCGData* CreateLandscapePCGData(bool bHeightOnly);
	bool IsLandscapeCachedDataDirty(const UPCGData* Data) const;

	bool ShouldGenerate(bool bForce, EPCGComponentGenerationTrigger RequestedGenerationTrigger) const;

	/* Internal call that allows to delay a Generate/Cleanup call, chain with dependencies and keep track of the task id created. This task id is also returned. */
	FPCGTaskId GenerateInternal(bool bForce, EPCGComponentGenerationTrigger RequestedGenerationTrigger, const TArray<FPCGTaskId>& Dependencies);
	FPCGTaskId CleanupInternal(bool bRemoveComponents, bool bSave, const TArray<FPCGTaskId>& Dependencies);

	/* Internal call to create tasks to generate the component. If there is nothing to do, an invalid task id will be returned. Should only be used by the subsystem. */
	FPCGTaskId CreateGenerateTask(bool bForce, const TArray<FPCGTaskId>& Dependencies);

	/* Internal call to create tasks to cleanup the component. If there is nothing to do, an invalid task id will be returned. Should only be used by the subsystem. */
	FPCGTaskId CreateCleanupTask(bool bRemoveComponents, const TArray<FPCGTaskId>& Dependencies);

	void PostProcessGraph(const FBox& InNewBounds, bool bInGenerated, FPCGContext* Context);
	void CallPostGenerateFunctions(FPCGContext* Context) const;
	void PostCleanupGraph();
	void OnProcessGraphAborted(bool bQuiet = false);
	void CleanupUnusedManagedResources();
	bool MoveResourcesToNewActor(AActor* InNewActor, bool bCreateChild);

	/** Called as part of the RerunConstructionScript, just takes resources as-is, assumes current state is empty + the resources have been retargeted if needed */
	void GetManagedResources(TArray<TObjectPtr<UPCGManagedResource>>& Resources) const;
	void SetManagedResources(const TArray<TObjectPtr<UPCGManagedResource>>& Resources);

	bool GetActorsFromTags(const TMap<FName, bool>& InTagsAndCulling, TSet<TWeakObjectPtr<AActor>>& OutActors);

	void RefreshAfterGraphChanged(UPCGGraphInterface* InGraph, bool bIsStructural, bool bDirtyInputs);
	void OnGraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType);

#if WITH_EDITOR
	// Stub for the other PreEditChange prototype to prevent compile issues from name hiding
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override { Super::PreEditChange(PropertyAboutToChange); }
	virtual void PreEditChange(FEditPropertyChain& PropertyAboutToChange) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;

	bool IsChangingGraphInstanceParameterValue(FEditPropertyChain& InEditPropertyChain) const;

	/** Sets up actor, tracking, landscape and graph callbacks */
	void SetupCallbacksOnCreation();

	void SetupActorCallbacks();
	void TeardownActorCallbacks();
	void SetupTrackingCallbacks();
	void TeardownTrackingCallbacks();
	void RefreshTrackingData();

	void OnActorAdded(AActor* InActor);
	void OnActorDeleted(AActor* InActor);
	void OnActorMoved(AActor* InActor);
	void OnObjectPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent);
	bool ActorIsTracked(AActor* InActor) const;

	void OnActorChanged(AActor* InActor, UObject* InSourceObject, bool bActorTagChange);

	bool PopulateTrackedActorToTagsMap(bool bForce = false);
	bool AddTrackedActor(AActor* InActor, bool bForce = false);
	bool RemoveTrackedActor(AActor* InActor);
	bool UpdateTrackedActor(AActor* InActor);
	bool DirtyTrackedActor(AActor* InActor);
	bool DirtyCacheFromTag(const FName& InTag, const AActor* InActor, bool bIgnoreCull = false);
	void DirtyCacheForAllTrackedTags();

	bool GraphUsesLandscapePin() const;
#endif

	FBox GetGridBounds(const AActor* InActor) const;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UPCGData> CachedPCGData = nullptr;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UPCGData> CachedInputData = nullptr;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UPCGData> CachedActorData = nullptr;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UPCGData> CachedLandscapeData = nullptr;

	UPROPERTY(Transient, NonPIEDuplicateTransient)
	TObjectPtr<UPCGData> CachedLandscapeHeightData = nullptr;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TSet<TSoftObjectPtr<AActor>> GeneratedActors_DEPRECATED;
#endif

	UPROPERTY()
	TArray<TObjectPtr<UPCGManagedResource>> GeneratedResources;

	// When doing a cleanup, locking resource modification. Used as sentinel.
	bool GeneratedResourcesInaccessible = false;

	UPROPERTY()
	FBox LastGeneratedBounds = FBox(EForceInit::ForceInit);

	UPROPERTY()
	FPCGDataCollection GeneratedGraphOutput;

	FPCGTaskId CurrentGenerationTask = InvalidPCGTaskId;
	FPCGTaskId CurrentCleanupTask = InvalidPCGTaskId;

#if WITH_EDITOR
	FPCGTaskId CurrentRefreshTask = InvalidPCGTaskId;
#endif // WITH_EDITOR

	UPROPERTY(VisibleAnywhere, Transient, Category = Properties, meta = (EditCondition = false, EditConditionHides))
	bool bIsComponentLocal = false;

#if WITH_EDITOR
	bool bIsInspecting = false;
	FBox LastGeneratedBoundsPriorToUndo = FBox(EForceInit::ForceInit);
	FPCGTagToSettingsMap CachedTrackedTagsToSettings;

	TMap<FName, bool> CachedTrackedTagsToCulling;

	void SetupLandscapeTracking();
	void TeardownLandscapeTracking();
	void UpdateTrackedLandscape(bool bBoundsCheck = true);
	void OnLandscapeChanged(ALandscapeProxy* Landscape, const FLandscapeProxyComponentDataChangedParams& ChangeParams);
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<TSoftObjectPtr<ALandscapeProxy>> TrackedLandscapes;
#endif

#if WITH_EDITORONLY_DATA
	// Cached tracked actors list is serialized because we can't get it at postload time
	UPROPERTY()
	TSet<TWeakObjectPtr<AActor>> CachedTrackedActors;

	TMap<TWeakObjectPtr<AActor>, TSet<FName>> CachedTrackedActorToTags;
	TMap<TWeakObjectPtr<AActor>, TSet<TObjectPtr<UObject>>> CachedTrackedActorToDependencies;
	bool bActorToTagsMapPopulated = false;

	UPROPERTY(Transient)
	TMap<TObjectPtr<const UPCGNode>, FPCGDataCollection> InspectionCache;
#endif

	mutable FCriticalSection GeneratedResourcesLock;

	// Graph instance
private:
	/** Will set the given graph interface into our owned graph instance. Must not be used on local components.*/
	void SetGraphInterfaceLocal(UPCGGraphInterface* InGraphInterface);

#if WITH_EDITOR
public:
	mutable PCGUtils::FExtraCapture ExtraCapture;
#endif // WITH_EDITOR
};

/** Used to store generated resources data during RerunConstructionScripts */
USTRUCT()
struct FPCGComponentInstanceData : public FActorComponentInstanceData
{
	GENERATED_BODY()
public:
	FPCGComponentInstanceData() = default;
	explicit FPCGComponentInstanceData(const UPCGComponent* InSourceComponent);

protected:
	virtual bool ContainsData() const override;
	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	UPROPERTY()
	TArray<TObjectPtr<UPCGManagedResource>> GeneratedResources;

	UPROPERTY()
	TObjectPtr<const UPCGComponent> SourceComponent;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
