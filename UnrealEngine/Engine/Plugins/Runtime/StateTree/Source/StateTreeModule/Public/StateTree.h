// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "StateTreeSchema.h"
#include "StateTreePropertyBindings.h"
#include "StateTreeInstanceData.h"
#include "StateTree.generated.h"

class UUserDefinedStruct;

/** Custom serialization version for StateTree Asset */
struct STATETREEMODULE_API FStateTreeCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,
		// Separated conditions to shared instance data.
		SharedInstanceData,
		// Moved evaluators to be global.
		GlobalEvaluators,
		// Moved instance data to arrays.
		InstanceDataArrays,
		// Added index types.
		IndexTypes,
		// Added events.
		AddedEvents,
		// Testing mishap
		AddedFoo,
		// Changed transition delay
		TransitionDelay,
		// Added external transitions
		AddedExternalTransitions,
		// Changed how bindings are represented
		ChangedBindingsRepresentation,
		// Added guid to transitions
		AddedTransitionIds,
		// Added data handles
		AddedDataHandlesIds,
		// Added linked asset state
		AddedLinkedAssetState,
		// Change how external data is accessed
		ChangedExternalDataAccess,
		// Added override option for parameters
		OverridableParameters,
		// Added override option for state parameters
		OverridableStateParameters,
		// Added storing global parameters in instance storage
		StoringGlobalParametersInInstanceStorage,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	/** The GUID for this custom version number */
	const static FGuid GUID;

private:
	FStateTreeCustomVersion() {}
};


#if WITH_EDITOR
/** Struct containing information about the StateTree runtime memory usage. */
struct FStateTreeMemoryUsage
{
	FStateTreeMemoryUsage() = default;
	FStateTreeMemoryUsage(const FString InName, const FStateTreeStateHandle InHandle = FStateTreeStateHandle::Invalid)
		: Name(InName)
		, Handle(InHandle)
	{
	}
	
	void AddUsage(FConstStructView View);
	void AddUsage(const UObject* Object);

	FString Name;
	FStateTreeStateHandle Handle;
	int32 NodeCount = 0;
	int32 EstimatedMemoryUsage = 0;
	int32 ChildNodeCount = 0;
	int32 EstimatedChildMemoryUsage = 0;
};
#endif


/**
 * StateTree asset. Contains the StateTree definition in both editor and runtime (baked) formats.
 */
UCLASS(BlueprintType)
class STATETREEMODULE_API UStateTree : public UDataAsset
{
	GENERATED_BODY()

public:
	/** @return Default instance data. */
	const FStateTreeInstanceData& GetDefaultInstanceData() const { return DefaultInstanceData; }

	/** @return Shared instance data. */
	TSharedPtr<FStateTreeInstanceData> GetSharedInstanceData() const;

	/** @return Number of context data views required for StateTree execution (Tree params, context data, External data). */
	int32 GetNumContextDataViews() const { return NumContextData; }

	/** @return List of external data required by the state tree */
	TConstArrayView<FStateTreeExternalDataDesc> GetExternalDataDescs() const { return ExternalDataDescs; }

	/** @return List of context data enforced by the schema that must be provided through the execution context. */
	TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const { return ContextDataDescs; }

	/** @return true if the other StateTree has compatible context data. */
	bool HasCompatibleContextData(const UStateTree& Other) const;
	
	/** @return List of default parameters of the state tree. Default parameter values can be overridden at runtime by the execution context. */
	const FInstancedPropertyBag& GetDefaultParameters() const { return Parameters; }

	/** @return true if the tree asset can be used at runtime. */
	bool IsReadyToRun() const;

	/** @return schema that was used to compile the StateTree. */
	const UStateTreeSchema* GetSchema() const { return Schema; }

	/** @return Pointer to a state or null if state not found */ 
	const FCompactStateTreeState* GetStateFromHandle(const FStateTreeStateHandle StateHandle) const;

	/** @return State handle matching a given Id; invalid handle if state not found. */
	FStateTreeStateHandle GetStateHandleFromId(const FGuid Id) const;

	/** @return Id of the state matching a given state handle; invalid Id if state not found. */
	FGuid GetStateIdFromHandle(const FStateTreeStateHandle Handle) const;

	/** @return Struct view of the node matching a given node index; invalid view if state not found. */
	FConstStructView GetNode(const int32 NodeIndex) const;

	/** @return Struct views of all nodes */
	const FInstancedStructContainer& GetNodes() const { return Nodes; }

	/** @return Node index matching a given Id; invalid index if node not found. */
	FStateTreeIndex16 GetNodeIndexFromId(const FGuid Id) const;

	/** @return Id of the node matching a given node index; invalid Id if node not found. */
	FGuid GetNodeIdFromIndex(const FStateTreeIndex16 NodeIndex) const;

	/** @return View of all states. */
	TConstArrayView<FCompactStateTreeState> GetStates() const { return States; }

	/** @return Pointer to the transition at a given index; null if not found. */ 
	const FCompactStateTransition* GetTransitionFromIndex(const FStateTreeIndex16 TransitionIndex) const;
	
	/** @return Runtime transition index matching a given Id; invalid index if node not found. */
	FStateTreeIndex16 GetTransitionIndexFromId(const FGuid Id) const;

	/** @return Id of the transition matching a given runtime transition index; invalid Id if transition not found. */
	FGuid GetTransitionIdFromIndex(const FStateTreeIndex16 Index) const;	

	/** @return Property bindings */
	const FStateTreePropertyBindings& GetPropertyBindings() const { return PropertyBindings; }

	UE_DEPRECATED(5.4, "Replaced with GetNumContextDataViews() which contains context data and external data only.")
	int32 GetNumDataViews() const { return 0; }

#if WITH_EDITOR
	/** Resets the compiled data to empty. */
	void ResetCompiled();

	/** Calculates runtime memory usage for different sections of the tree. */
	TArray<FStateTreeMemoryUsage> CalculateEstimatedMemoryUsage() const;
#endif

#if WITH_EDITORONLY_DATA
	/** Edit time data for the StateTree, instance of UStateTreeEditorData */
	UPROPERTY()
	TObjectPtr<UObject> EditorData;

	FDelegateHandle OnObjectsReinstancedHandle;
	FDelegateHandle OnUserDefinedStructReinstancedHandle;
#endif

	/** Hash of the editor data from last compile. Also used to detect mismatching events from recorded traces. */
	UPROPERTY()
	uint32 LastCompiledEditorDataHash = 0;

protected:
	
	/**
	 * Resolves references between data in the StateTree.
	 * @return true if all references to internal and external data are resolved properly, false otherwise.
	 */
	[[nodiscard]] bool Link();

	virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif
	virtual void Serialize(FStructuredArchiveRecord Record) override;
	
#if WITH_EDITOR
	using FReplacementObjectMap = TMap<UObject*, UObject*>;
	void OnObjectsReinstanced(const FReplacementObjectMap& ObjectMap);
	void OnUserDefinedStructReinstanced(const UUserDefinedStruct& UserDefinedStruct);
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void PostLoadAssetRegistryTags(const FAssetData& InAssetData, TArray<FAssetRegistryTag>& OutTagsAndValuesToUpdate) const override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	
private:

	/**
     * Reset the data generated by Link(), this in turn will cause IsReadyToRun() to return false.
     * Used during linking, or to invalidate the linked data when data version is old (requires recompile). 
	 */
	void ResetLinked();

	bool PatchBindings();

	// Data created during compilation, source data in EditorData.
	
	/** Schema used to compile the StateTree. */
	UPROPERTY(Instanced)
	TObjectPtr<UStateTreeSchema> Schema = nullptr;

	/** Runtime states, root state at index 0 */
	UPROPERTY()
	TArray<FCompactStateTreeState> States;

	/** Runtime transitions. */
	UPROPERTY()
	TArray<FCompactStateTransition> Transitions;

	/** Evaluators, Tasks, and Condition nodes. */
	UPROPERTY()
	FInstancedStructContainer Nodes;

	/** Default node instance data (e.g. evaluators, tasks). */
	UPROPERTY()
	FStateTreeInstanceData DefaultInstanceData;

	/** Shared node instance data (e.g. conditions). */
	UPROPERTY()
	FStateTreeInstanceData SharedInstanceData;

	mutable FRWLock PerThreadSharedInstanceDataLock;
	mutable TArray<TSharedPtr<FStateTreeInstanceData>> PerThreadSharedInstanceData;
	
	/** List of names external data enforced by the schema, created at compilation. */
	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;

	UPROPERTY()
	FStateTreePropertyBindings PropertyBindings;

	/** Mapping of state guid for the Editor and state handles, created at compilation. */
	UPROPERTY()
	TArray<FStateTreeStateIdToHandle> IDToStateMappings;

	/** Mapping of node guid for the Editor and node index, created at compilation. */
	UPROPERTY()
	TArray<FStateTreeNodeIdToIndex> IDToNodeMappings;
	
	/** Mapping of state transition identifiers and runtime compact transition index, created at compilation. */
	UPROPERTY()
	TArray<FStateTreeTransitionIdToIndex> IDToTransitionMappings;

	/**
	 * Parameters that could be used for bindings within the Tree.
	 * Default values are stored within the asset but StateTreeReference can be used to parameterized the tree.
	 * @see FStateTreeReference
	 */
	UPROPERTY()
	FInstancedPropertyBag Parameters;


	/** Number of context data, include parameters and all context data. */
	UPROPERTY()
	uint16 NumContextData = 0;

	/** Number of global instance data. */
	UPROPERTY()
	uint16 NumGlobalInstanceData = 0;
	
	/** Index of first evaluator in Nodes. */
	UPROPERTY()
	uint16 EvaluatorsBegin = 0;

	/** Number of evaluators. */
	UPROPERTY()
	uint16 EvaluatorsNum = 0;

	/** Index of first global task in Nodes. */
	UPROPERTY()
	uint16 GlobalTasksBegin = 0;

	/** Number of global tasks. */
	UPROPERTY()
	uint16 GlobalTasksNum = 0;

	/** True if any global task is a transition task. */
	UPROPERTY()
	bool bHasGlobalTransitionTasks = false;
	
	// Data created during linking.
	
	/** List of external data required by the state tree, created during linking. */
	UPROPERTY(Transient)
	TArray<FStateTreeExternalDataDesc> ExternalDataDescs;

	/** True if the StateTree was linked successfully. */
	bool bIsLinked = false;

	friend struct FStateTreeInstance;
	friend struct FStateTreeExecutionContext;
#if WITH_EDITORONLY_DATA
	friend struct FStateTreeCompiler;
#endif
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Misc/ScopeRWLock.h"
#endif
