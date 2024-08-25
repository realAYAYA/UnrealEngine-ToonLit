// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "Graph/PCGStackContext.h"
#include "Utils/PCGExtraCapture.h"

#include "ComponentInstanceDataCache.h"
#include "Components/ActorComponent.h"
#include "Containers/Map.h"
#include "UObject/ObjectKey.h"

#include "PCGComponent.generated.h"

namespace EEndPlayReason { enum Type : int; }

class APCGPartitionActor;
class FPCGActorAndComponentMapping;
class FPCGStackContext;
class UPCGComponent;
class UPCGData;
class IPCGGenSourceBase;
class UPCGGraph;
class UPCGGraphInstance;
class UPCGGraphInterface;
class UPCGManagedResource;
class UPCGSchedulingPolicyBase;
class UPCGSubsystem;
class ALandscapeProxy;
class FLandscapeProxyComponentDataChangedParams;
class UClass;
struct FPCGContext;

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPCGGraphStartGenerating, UPCGComponent*);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPCGGraphCancelled, UPCGComponent*);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPCGGraphGenerated, UPCGComponent*);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPCGGraphCleaned, UPCGComponent*);
#endif

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPCGGraphStartGeneratingExternal, UPCGComponent*, PCGComponent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPCGGraphCancelledExternal, UPCGComponent*, PCGComponent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPCGGraphGeneratedExternal, UPCGComponent*, PCGComponent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPCGGraphCleanedExternal, UPCGComponent*, PCGComponent);

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
	GenerateOnLoad    UMETA(ToolTip = "Generates only when the component is loaded into the level."),
	GenerateOnDemand  UMETA(ToolTip = "Generates only when requested (e.g. via Blueprint)."),
	GenerateAtRuntime UMETA(ToolTip = "Generates only when scheduled by the Runtime Generation Scheduler.")
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
	friend class FPCGActorAndComponentMapping;

public:
	/** ~Begin UObject interface */
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	virtual bool IsEditorOnly() const override;

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditImport() override;
#endif
	/** ~End UObject interface */

	//~Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

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

	/** If this is a local component returns self, otherwise returns the original component. */
	UPCGComponent* GetOriginalComponent();

	UPCGSchedulingPolicyBase* GetRuntimeGenSchedulingPolicy() const { return SchedulingPolicy; }

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

	/** Requests the component to generate only on the specified grid level (all grid levels if EPCGHiGenGrid::Uninitialized). */
	void GenerateLocal(EPCGComponentGenerationTrigger RequestedGenerationTrigger, bool bForce, EPCGHiGenGrid Grid = EPCGHiGenGrid::Uninitialized, const TArray<FPCGTaskId>& Dependencies = {});

	FPCGTaskId GenerateLocalGetTaskId(bool bForce);
	FPCGTaskId GenerateLocalGetTaskId(EPCGComponentGenerationTrigger RequestedGenerationTrigger, bool bForce, EPCGHiGenGrid Grid = EPCGHiGenGrid::Uninitialized);

	/** Cleans up the generation from a local (vs. remote) standpoint. Will not be replicated. Will be delayed. */
	UFUNCTION(BlueprintCallable, Category = PCG)
	void CleanupLocal(bool bRemoveComponents, bool bSave = false);

	/**
	 * Same as CleanupLocal, but without any delayed tasks. All is done immediately. If 'bCleanupLocalComponents' is true and the
	 * component is partitioned, we will forward the calls to its registered local components.
	 */
	void CleanupLocalImmediate(bool bRemoveComponents, bool bCleanupLocalComponents = false);

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

	uint32 GetGenerationGridSize() const { return GenerationGridSize; }
	void SetGenerationGridSize(uint32 InGenerationGridSize) { GenerationGridSize = InGenerationGridSize; }
	EPCGHiGenGrid GetGenerationGrid() const;

	/** Store data with a resource key that identifies the pin. */
	void StoreOutputDataForPin(const FString& InResourceKey, const FPCGDataCollection& InData);

	/** Lookup data using a resource key that identifies the pin. */
	const FPCGDataCollection* RetrieveOutputDataForPin(const FString& InResourceKey);

	/** Clear any data stored for any pins. */
	void ClearPerPinGeneratedOutput();

	/** Set the runtime generation scheduling policy type. */
	void SetSchedulingPolicyClass(TSubclassOf<UPCGSchedulingPolicyBase> InSchedulingPolicyClass);

	/** Get the runtime generation radius for the given grid size. */
	double GetGenerationRadiusFromGrid(EPCGHiGenGrid Grid) const;

	/** Compute the runtime cleanup radius for the given grid size. */
	double GetCleanupRadiusFromGrid(EPCGHiGenGrid Grid) const;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayPriority = 600))
	int Seed = 42;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayPriority = 100))
	bool bActivated = true;

	/** 
	 * Will partition the component in a grid, dispatching the generation to multiple local components. Grid size is determined by the
	 * PCGWorldActor unless the graph has Hierarchical Generation enabled, in which case grid sizes are determined by the graph.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "!bIsComponentLocal", DisplayName = "Is Partitioned", DisplayPriority = 500))
	bool bIsComponentPartitioned = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (EditCondition = "!bIsComponentLocal", EditConditionHides, DisplayPriority = 200))
	EPCGComponentGenerationTrigger GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnLoad;

	/** When Generation Trigger is OnDemand, we can still force the component to generate on drop. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Settings|Advanced" , meta = (EditCondition = "GenerationTrigger == EPCGComponentGenerationTrigger::GenerateOnDemand"))
	bool bGenerateOnDropWhenTriggerOnDemand = false;

	/** Manual overrides for the graph generation radii and cleanup radius multiplier. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RuntimeGeneration, meta = (EditCondition = "GenerationTrigger == EPCGComponentGenerationTrigger::GenerateAtRuntime", EditConditionHides))
	bool bOverrideGenerationRadii = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RuntimeGeneration, meta = (EditCondition = "GenerationTrigger == EPCGComponentGenerationTrigger::GenerateAtRuntime && bOverrideGenerationRadii", EditConditionHides))
	FPCGRuntimeGenerationRadii GenerationRadii;

	/** A Scheduling Policy dictates the order in which instances of this component will be scheduled. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, NoClear, Category = RuntimeGeneration, meta = (EditCondition = "GenerationTrigger == EPCGComponentGenerationTrigger::GenerateAtRuntime", EditConditionHides))
	TSubclassOf<UPCGSchedulingPolicyBase> SchedulingPolicyClass = nullptr;

	/** This is the instanced UPCGSchedulingPolicy object which holds scheduling parameters and calculates priorities. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Instanced, Category = RuntimeGeneration, meta = (EditCondition = "GenerationTrigger == EPCGComponentGenerationTrigger::GenerateAtRuntime", EditConditionHides))
	TObjectPtr<UPCGSchedulingPolicyBase> SchedulingPolicy;

#if WITH_EDITORONLY_DATA
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Editing Settings", meta = (DisplayName = "Regenerate PCG Volume In Editor", DisplayPriority = 400))
	bool bRegenerateInEditor = true;

	/** Even if the graph has external dependencies, the component won't react to them. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Editing Settings", meta = (DisplayPriority = 450))
	bool bOnlyTrackItself = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Transient, Category = Debug, meta = (NoResetToDefault))
	bool bDirtyGenerated = false;

	// Property that will automatically be set on BP templates, to allow for "Generate on add to world" in editor.
	// Set it as a property to automatically transfer it to its child.
	// Don't use it directly, use ShouldGenerateBPPCGAddedToWorld, as there are other conditions checked.
	UPROPERTY()
	bool bForceGenerateOnBPAddedToWorld = false;

	FOnPCGGraphStartGenerating OnPCGGraphStartGeneratingDelegate;
	FOnPCGGraphCancelled OnPCGGraphCancelledDelegate;
	FOnPCGGraphGenerated OnPCGGraphGeneratedDelegate;
	FOnPCGGraphCleaned OnPCGGraphCleanedDelegate;
#endif // WITH_EDITORONLY_DATA

	/** Event dispatched when a graph begins generation on this component. */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "On Graph Started Generating"))
	FOnPCGGraphStartGeneratingExternal OnPCGGraphStartGeneratingExternal;
	/** Event dispatched when a graph cancels generation on this component. */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "On Graph Cancelled"))
	FOnPCGGraphCancelledExternal OnPCGGraphCancelledExternal;
	/** Event dispatched when a graph completes its generation on this component. */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "On Graph Generated"))
	FOnPCGGraphGeneratedExternal OnPCGGraphGeneratedExternal;
	/** Event dispatched when a graph cleans on this component. */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "On Graph Cleaned"))
	FOnPCGGraphCleanedExternal OnPCGGraphCleanedExternal;

	/** Flag to indicate whether this component has run in the editor. Note that for partitionable actors, this will always be false. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Debug, NonTransactional, meta = (NoResetToDefault))
	bool bGenerated = false;

	UPROPERTY(NonPIEDuplicateTransient)
	bool bRuntimeGenerated = false;

	/** Can specify a list of functions from the owner of this component to be called when generation is done, in order.
	*   Need to take (and only take) a PCGDataCollection as parameter and with "CallInEditor" flag enabled.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayPriority = 700))
	TArray<FName> PostGenerateFunctionNames;

	/** Return if we are currently generating the graph for this component */
	bool IsGenerating() const { return CurrentGenerationTask != InvalidPCGTaskId; }
	bool IsCleaningUp() const { return CurrentCleanupTask != InvalidPCGTaskId; }

	/** Returns task ids to do internal chaining */
	FPCGTaskId GetGenerationTaskId() const { return CurrentGenerationTask; }

#if WITH_EDITOR
	/** Schedules refresh of the component. If bCancelExistingRefresh is true, any existing refresh is cancelled and a new one is scheduled. */
	void Refresh(EPCGChangeType ChangeType = EPCGChangeType::None, bool bCancelExistingRefresh = false);

	void OnRefresh(bool bForceRefresh);

	void StartGenerationInProgress();
	void StopGenerationInProgress();
	bool IsGenerationInProgress();

	/** Returns current refresh task ID. */
	bool IsRefreshInProgress() const { return CurrentRefreshTask != InvalidPCGTaskId; }

	/** Dirty generated data depending on the flag. By default the call is forwarded to the local components.
	    We don't forward if the local component has callbacks that would dirty them too.
		For example: When a tracked actor move, we only want to dirty the impacted local components.*/
	void DirtyGenerated(EPCGComponentDirtyFlag DataToDirtyFlag = EPCGComponentDirtyFlag::None, const bool bDispatchToLocalComponents = true);

	/** Reset last generated bounds to force PCGPartitionActor creation on next refresh */
	void ResetLastGeneratedBounds();

	/** Functions for managing the node inspection cache */
	bool IsInspecting() const;
	void EnableInspection();
	void DisableInspection();
	void StoreInspectionData(const FPCGStack* InStack, const UPCGNode* InNode, const FPCGDataCollection& InInputData, const FPCGDataCollection& InOutputData, bool bUsedCache);
	const FPCGDataCollection* GetInspectionData(const FPCGStack& InStack) const;
	void ClearInspectionData(bool bClearPerNodeExecutionData = true);

	/** Whether a task for the given node and stack was executed during the last execution. */
	bool WasNodeExecuted(const UPCGNode* InNode, const FPCGStack& Stack) const;

	/** Called at execution time each time a node has been executed. */
	void NotifyNodeExecuted(const UPCGNode* InNode, const FPCGStack* InStack, bool bNodeUsedCache);

	/* Retrieves the executed nodes information */
	TMap<TObjectKey<const UPCGNode>, TSet<FPCGStack>> GetExecutedNodeStacks() const;

	/** Retrieve the inactive pin bitmask for the given node and stack in the last execution. */
	uint64 GetNodeInactivePinMask(const UPCGNode* InNode, const FPCGStack& Stack) const;

	/** Whether the given node was culled by a dynamic branch in the given stack. */
	void NotifyNodeDynamicInactivePins(const UPCGNode* InNode, const FPCGStack* InStack, uint64 InactivePinBitmask) const;

	/** Did the given node produce one or more data items in the given stack during the last execution. */
	bool HasNodeProducedData(const UPCGNode* InNode, const FPCGStack& Stack) const;

	bool IsObjectTracked(const UObject* InObject, bool& bOutIsCulled) const;

	/** Know if we need to force a generation, in case of BP added to the world in editor */
	bool ShouldGenerateBPPCGAddedToWorld() const;

	/** Changes the transient state (preview, normal, load on preview) - public only because it needs to be accessed by APCGPartitionActor */
	void ChangeTransientState(EPCGEditorDirtyMode NewEditingMode);

	/** Get execution stack information. */
	bool GetStackContext(FPCGStackContext& OutStackContext) const;

	/** To be called by an element to notify the component that this settings have a dynamic dependency. */
	void RegisterDynamicTracking(const UPCGSettings* InSettings, const TArrayView<TPair<FPCGSelectionKey, bool>>& InDynamicKeysAndCulling);

	/** For duration of the current/next generation, any change triggers from this change origin will be discarded. */
	void StartIgnoringChangeOriginDuringGeneration(UObject* InChangeOriginToIgnore);
	void StopIgnoringChangeOriginDuringGeneration(UObject* InChangeOriginToIgnore);
	bool IsIgnoringChangeOrigin(UObject* InChangeOrigin);
	void ResetIgnoredChangeOrigins(bool bLogIfAnyPresent);
#endif

	/** Utility function (mostly for tests) to properly set the value of bIsComponentPartitioned.
	*   Will do an immediate cleanup first and then register/unregister the component to the subsystem.
	*   It's your responsibility after to regenerate the graph if you want to.
	*/
	void SetIsPartitioned(bool bIsNowPartitioned);

	bool IsPartitioned() const;
	bool IsLocalComponent() const { return bIsComponentLocal; }

	/** Returns true if the component is managed by the runtime generation system. Nothing else should generate or cleanup this component. */
	bool IsManagedByRuntimeGenSystem() const { return GenerationTrigger == EPCGComponentGenerationTrigger::GenerateAtRuntime; }

	/* Responsibility of the PCG Partition Actor to mark is local */
	void MarkAsLocalComponent() { bIsComponentLocal = true; }

	/** Updates internal properties from other component, dirties as required but does not trigger Refresh */
	void SetPropertiesFromOriginal(const UPCGComponent* Original);

	/** Returns whether the component (or resources) should be marked as dirty following interaction/refresh based on the current editing mode */
	bool IsInPreviewMode() const { return CurrentEditingMode == EPCGEditorDirtyMode::Preview; }

	UFUNCTION(BlueprintCallable, Category="PCG|Advanced")
	void SetEditingMode(EPCGEditorDirtyMode InEditingMode, EPCGEditorDirtyMode InSerializedEditingMode);

	/** Returns the current editing mode */
	UFUNCTION(BlueprintCallable, Category="PCG|Advanced")
	EPCGEditorDirtyMode GetEditingMode() const { return CurrentEditingMode; }

	UFUNCTION(BlueprintCallable, Category = "PCG|Advanced")
	EPCGEditorDirtyMode GetSerializedEditingMode() const { return SerializedEditingMode; }

	UPCGSubsystem* GetSubsystem() const;

	FBox GetGridBounds() const;
	FBox GetLastGeneratedBounds() const { return LastGeneratedBounds; }

	/** Builds the PCG data from a given actor and its PCG component, and places it in a data collection with appropriate tags */
	static FPCGDataCollection CreateActorPCGDataCollection(AActor* Actor, const UPCGComponent* Component, EPCGDataType InDataFilter, bool bParseActor = true, bool* bOutOptionalSanitizedTagAttributeName = nullptr);

	/** Builds the canonical PCG data from a given actor and its PCG component if any. */
	static UPCGData* CreateActorPCGData(AActor* Actor, const UPCGComponent* Component, bool bParseActor = true);

protected:
	void RefreshSchedulingPolicy();

protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Settings, Instanced, meta = (NoResetToDefault, DisplayPriority = 100))
	TObjectPtr<UPCGGraphInstance> GraphInstance;

	UPROPERTY(Transient, VisibleAnywhere, Category = Debug)
	uint32 GenerationGridSize = PCGHiGenGrid::UnboundedGridSize();

	/** Current editing mode that depends on the serialized editing mode and loading. If the component is set to GenerateAtRuntime, this will behave as Preview. */
	UPROPERTY(Transient, EditAnywhere, Category = "Editing Settings", meta = (DisplayName = "Editing Mode", EditCondition = "!bIsComponentLocal && GenerationTrigger != EPCGComponentGenerationTrigger::GenerateAtRuntime", DisplayPriority = 300))
	EPCGEditorDirtyMode CurrentEditingMode = EPCGEditorDirtyMode::Normal;

	UPROPERTY(VisibleAnywhere, Category = "Editing Settings", meta = (NoResetToDefault, DisplayPriority = 300))
	EPCGEditorDirtyMode SerializedEditingMode = EPCGEditorDirtyMode::Normal;

	/** Used to store the CurrentEditingMode when it is forcefully changed by another system, such as runtime generation. */
	EPCGEditorDirtyMode PreviousEditingMode = EPCGEditorDirtyMode::Normal;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Input Node Settings (Deprecated)", meta = (DisplayPriority = 800))
	EPCGComponentInput InputType = EPCGComponentInput::Actor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Input Node Settings (Deprecated)", meta = (DisplayPriority = 900))
	bool bParseActorComponents = true;

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UPCGGraph> Graph_DEPRECATED;

private:
	// Note for upgrade: can be safely replaced by bIsComponentPartitioned. Needed a new variable to change the default value. Kept to allow proper value change, cannot be deprecated. Do not use.
	UPROPERTY()
	bool bIsPartitioned = true;

	/** Track if component should disable 'bIsComponentPartitioned'. Used to deprecate partitioned components in non-WP levels before support for partitioning in non-WP levels. */
	bool bDisableIsComponentPartitionedOnLoad = false;
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
	FPCGTaskId GenerateInternal(bool bForce, EPCGHiGenGrid Grid, EPCGComponentGenerationTrigger RequestedGenerationTrigger, const TArray<FPCGTaskId>& Dependencies);
	FPCGTaskId CleanupInternal(bool bRemoveComponents, const TArray<FPCGTaskId>& Dependencies);

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

	void RefreshAfterGraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType);
	void OnGraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType);

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;

	/** Sets up actor, tracking, landscape and graph callbacks */
	void SetupCallbacksOnCreation();

	/** Returns true if something changed in the tracking. */
	bool UpdateTrackingCache(TArray<FPCGSelectionKey>* OptionalChangedKeys = nullptr);

	/** Apply a function to all settings that track a given key. */
	void ApplyToEachSettings(const FPCGSelectionKey& InKey, const TFunctionRef<void(const FPCGSelectionKey&, const FPCGSettingsAndCulling&)> InCallback) const;

	/** Return all the keys tracked by the component (statically and dynamically). */
	TArray<FPCGSelectionKey> GatherTrackingKeys() const;

	/** Return true if the key is tracked, and if so, bOutIsCulled will contains if the key is culled or not. */
	bool IsKeyTrackedAndCulled(const FPCGSelectionKey& Key, bool& bOutIsCulled) const;

	/** Compare the temp map to the stored map for dynamic tracking and register/unregister accordingly. If it is a local component, it will push the info to the original. */
	void UpdateDynamicTracking();

	bool ShouldTrackLandscape() const;

	void MarkResourcesAsTransientOnLoad();
	bool DeletePreviewResources();
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

	// NOTE: This should not be made visible or editable because it will change the way the BP actors are
	// duplicated/setup and might trigger an ensure in the resources.
	UPROPERTY()
	TArray<TObjectPtr<UPCGManagedResource>> GeneratedResources;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPCGManagedResource>> LoadedPreviewResources;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = Debug)
	bool bGenerationInProgress = false;
#endif

	// When doing a cleanup, locking resource modification. Used as sentinel.
	bool GeneratedResourcesInaccessible = false;

	UPROPERTY(VisibleInstanceOnly, Category = Debug)
	FBox LastGeneratedBounds = FBox(EForceInit::ForceInit);

	UPROPERTY(VisibleInstanceOnly, Category = Debug)
	FPCGDataCollection GeneratedGraphOutput;

	/** If any graph edges cross execution grid sizes, data on the edge is stored / retrieved from this map. */
	UPROPERTY(Transient, VisibleAnywhere, Category = Debug)
	TMap<FString, FPCGDataCollection> PerPinGeneratedOutput;

	mutable FRWLock PerPinGeneratedOutputLock;

	FPCGTaskId CurrentGenerationTask = InvalidPCGTaskId;
	FPCGTaskId CurrentCleanupTask = InvalidPCGTaskId;

#if WITH_EDITOR
	FPCGTaskId CurrentRefreshTask = InvalidPCGTaskId;
#endif // WITH_EDITOR

	UPROPERTY(VisibleAnywhere, Transient, Category = Debug, meta = (EditCondition = false, EditConditionHides))
	bool bIsComponentLocal = false;

#if WITH_EDITOR
	int32 InspectionCounter = 0;
	FBox LastGeneratedBoundsPriorToUndo = FBox(EForceInit::ForceInit);
#endif

#if WITH_EDITORONLY_DATA
	FPCGSelectionKeyToSettingsMap StaticallyTrackedKeysToSettings;

	// Temporary storage for dynamic tracking that will be filled during component execution.
	FPCGSelectionKeyToSettingsMap CurrentExecutionDynamicTracking;
	// Temporary storage for dynamic tracking that will keep all settings that could have dynamic tracking, in order to detect changes.
	TSet<const UPCGSettings*> CurrentExecutionDynamicTrackingSettings;
	mutable FCriticalSection CurrentExecutionDynamicTrackingLock;

	// Need to keep a reference to all tracked settings to still react to changes after a map load (since the component won't have been executed).
	// Serialization will be done in the Serialize function
	FPCGSelectionKeyToSettingsMap DynamicallyTrackedKeysToSettings;

	UPROPERTY(Transient)
	TMap<FPCGStack, FPCGDataCollection> InspectionCache;
#endif

#if WITH_EDITOR
	mutable FRWLock InspectionCacheLock;

	/** Map from nodes to all stacks for which the node produced at least one data item. */
	TMap<TObjectKey<const UPCGNode>, TSet<FPCGStack>> NodeToStacksThatProducedData;
	mutable FRWLock NodeToStacksThatProducedDataLock;

	/** Map from nodes to all stacks for which a task for the node was executed. */
	TMap<TObjectKey<const UPCGNode>, TSet<FPCGStack>> NodeToStacksInWhichNodeExecuted;
	mutable FRWLock NodeToStacksInWhichNodeExecutedLock;

	/** Map from nodes to stacks to mask of output pins that were deactivated during execution. */
	mutable TMap<TObjectKey<const UPCGNode>, TMap<const FPCGStack, uint64>> NodeToStackToInactivePinMask;
	mutable FRWLock NodeToStackToInactivePinMaskLock;

	/** The tracking system will not trigger a generation on this component for these change origins. Populated within the scope
	* of an element. Entries removed when counter is decremented to 0, so empty map means no active ignores.
	*/
	TMap<TObjectKey<UObject>, int32> IgnoredChangeOriginsToCounters;
	FRWLock IgnoredChangeOriginsLock;
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
	TObjectPtr<const UPCGComponent> SourceComponent;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
