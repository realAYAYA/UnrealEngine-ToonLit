// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGNode.h"
#include "PCGSettings.h"

#include "PropertyBag.h"
#include "UObject/ObjectPtr.h"

#include "PCGGraph.generated.h"

enum class EPCGGraphParameterEvent
{
	GraphChanged,
	GraphPostLoad,
	Added,
	Removed,
	PropertyModified,
	ValueModifiedLocally,
	ValueModifiedByParent
};

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPCGGraphChanged, UPCGGraphInterface* /*Graph*/, EPCGChangeType /*ChangeType*/);
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPCGGraphParametersChanged, UPCGGraphInterface* /*Graph*/, EPCGGraphParameterEvent /*ChangeType*/, FName /*ChangedPropertyName*/);
#endif // WITH_EDITOR

/**
* Extended version of FInstancedPropertyBag, to support overrides and have a custom UI for it
* Must only be used with PCGGraphInstances.
* TODO: Should be made generic and moved to ScriptUtils.
*/
USTRUCT()
struct PCG_API FPCGOverrideInstancedPropertyBag
{
	GENERATED_BODY()

public:
	/** Add/Remove given property from overrides, and reset its value if it is removed. Returns true if the value was changed. */
	bool UpdatePropertyOverride(const FProperty* InProperty, bool bMarkAsOverridden, const FInstancedPropertyBag* ParentUserParameters);

	/** Reset overridden property to its parent value. */
	void ResetPropertyToDefault(const FProperty* InProperty, const FInstancedPropertyBag* ParentUserParameters);

	/** Return if the property is currently marked overridden. */
	bool IsPropertyOverridden(const FProperty* InProperty) const;

	/** Return if the property is currently marked overridden and has a different value than its default value. */
	bool IsPropertyOverriddenAndNotDefault(const FProperty* InProperty, const FInstancedPropertyBag* ParentUserParameters) const;

	/** Reset the struct. */
	void Reset();

	/** Return if the parameters are valid */
	bool IsValid() { return Parameters.IsValid(); }

	/** Will migrate to a new property bag instanced, and will remove porperties that doesn't exists anymore or have changed types. */
	void MigrateToNewBagInstance(const FInstancedPropertyBag& NewBagInstance);

	/** Handle the sync between the parent parameters and the overrides. Will return true if something changed. */
	bool RefreshParameters(const FInstancedPropertyBag* ParentUserParameters, EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName);

	UPROPERTY(EditAnywhere, Category = "")
	FInstancedPropertyBag Parameters;

	UPROPERTY(VisibleAnywhere, Category = "", meta = (Hidden))
	TSet<FGuid> PropertiesIDsOverridden;
};

UCLASS(Abstract)
class PCG_API UPCGGraphInterface : public UObject
{
	GENERATED_BODY()

public:
	/** Return the underlying PCG Graph for this interface. */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGGraph* GetMutablePCGGraph() { return GetGraph(); }

	/** Return the underlying PCG Graph for this interface. */
	UFUNCTION(BlueprintCallable, Category = Graph)
	const UPCGGraph* GetConstPCGGraph() const { return GetGraph(); }

	virtual UPCGGraph* GetGraph() PURE_VIRTUAL(UPCGGraphInterface::GetGraph, return nullptr;)
	virtual const UPCGGraph* GetGraph() const PURE_VIRTUAL(UPCGGraphInterface::GetGraph, return nullptr;)

	virtual const FInstancedPropertyBag* GetUserParametersStruct() const PURE_VIRTUAL(UPCGGraphInterface::GetUserParametersStruct, return nullptr;)

	bool IsInstance() const;

	/** A graph interface is equivalent to another graph interface if they are the same (same ptr), or if they have the same graph. Will be overriden when graph instance supports overrides. */
	virtual bool IsEquivalent(const UPCGGraphInterface* Other) const;

#if WITH_EDITOR
	FOnPCGGraphChanged OnGraphChangedDelegate;
	FOnPCGGraphParametersChanged OnGraphParametersChangedDelegate;
#endif // WITH_EDITOR
};

UCLASS(BlueprintType, ClassGroup = (Procedural), hidecategories=(Object))
class PCG_API UPCGGraph : public UPCGGraphInterface
{
#if WITH_EDITOR
	friend class FPCGEditor;
#endif // WITH_EDITOR

	GENERATED_BODY()

public:
	UPCGGraph(const FObjectInitializer& ObjectInitializer);
	/** ~Begin UObject interface */
	virtual void PostLoad() override;
#if WITH_EDITOR
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif

	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* InProperty) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	/** ~End UObject interface */

	/** ~Begin UPCGGraphInterface interface */
	virtual UPCGGraph* GetGraph() override { return this; }
	virtual const UPCGGraph* GetGraph() const override { return this; }
	/** ~End UPCGGraphInterface interface */

#if WITH_EDITORONLY_DATA
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AssetInfo, AssetRegistrySearchable)
	bool bExposeToLibrary = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AssetInfo, AssetRegistrySearchable)
	FText Category;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = AssetInfo, AssetRegistrySearchable)
	FText Description;
#endif

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bLandscapeUsesMetadata = true;

	/** Creates a node using the given settings interface. Does not manage ownership - done outside of this method. */
	UPCGNode* AddNode(UPCGSettingsInterface* InSettings);

	/** Creates a default node based on the settings class wanted. Returns the newly created node. */
	UFUNCTION(BlueprintCallable, Category = Graph, meta=(DeterminesOutputType = "InSettingsClass", DynamicOutputParam = "DefaultNodeSettings"))
	UPCGNode* AddNodeOfType(TSubclassOf<class UPCGSettings> InSettingsClass, UPCGSettings*& DefaultNodeSettings);

	/** Creates a node containing an instance to the given settings. Returns the created node. */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGNode* AddNodeInstance(UPCGSettings* InSettings);

	/** Creates a node and copies the input settings. Returns the created node. */
	UFUNCTION(BlueprintCallable, Category = Graph, meta = (DeterminesOutputType = "InSettings", DynamicOutputParam = "OutCopiedSettings"))
	UPCGNode* AddNodeCopy(UPCGSettings* InSettings, UPCGSettings*& DefaultNodeSettings);

	/** Removes a node from the graph. */
	UFUNCTION(BlueprintCallable, Category = Graph)
	void RemoveNode(UPCGNode* InNode);

	/** Adds a directed edge in the graph. Returns the "To" node for easy chaining */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGNode* AddEdge(UPCGNode* From, const FName& FromPinLabel, UPCGNode* To, const FName& ToPinLabel);

	/** Removes an edge in the graph. Returns true if an edge was removed. */
	UFUNCTION(BlueprintCallable, Category = Graph)
	bool RemoveEdge(UPCGNode* From, const FName& FromLabel, UPCGNode* To, const FName& ToLabel);

	/** Returns the graph input node */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGNode* GetInputNode() const { return InputNode; }

	/** Returns the graph output node */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGNode* GetOutputNode() const { return OutputNode; }

	/** Duplicate a given node by creating a new node with the same settings and properties, but without any edges and add it to the graph */
	TObjectPtr<UPCGNode> ReconstructNewNode(const UPCGNode* InNode);

	/** Creates an edge between two nodes/pins based on the labels. Returns true if the To node has removed other edges (happens with single pins) */
	bool AddLabeledEdge(UPCGNode* From, const FName& InboundLabel, UPCGNode* To, const FName& OutboundLabel);

	bool Contains(UPCGNode* Node) const;
	const TArray<UPCGNode*>& GetNodes() const { return Nodes; }
	void AddNode(UPCGNode* InNode);

	/** Calls the lambda on every node in graph. */
	void ForEachNode(const TFunction<void(UPCGNode*)>& Action);

	bool RemoveAllInboundEdges(UPCGNode* InNode);
	bool RemoveAllOutboundEdges(UPCGNode* InNode);
	bool RemoveInboundEdges(UPCGNode* InNode, const FName& InboundLabel);
	bool RemoveOutboundEdges(UPCGNode* InNode, const FName& OutboundLabel);

#if WITH_EDITOR
	void DisableNotificationsForEditor();
	void EnableNotificationsForEditor();
	void ToggleUserPausedNotificationsForEditor();
	bool NotificationsForEditorArePausedByUser() const { return bUserPausedNotificationsInGraphEditor; }

	UFUNCTION(BlueprintCallable, Category = "Graph|Advanded")
	void ForceNotificationForEditor();

	void PreNodeUndo(UPCGNode* InPCGNode);
	void PostNodeUndo(UPCGNode* InPCGNode);

	const TArray<TObjectPtr<UObject>>& GetExtraEditorNodes() const { return ExtraEditorNodes; }
	void SetExtraEditorNodes(const TArray<TObjectPtr<const UObject>>& InNodes);
#endif

#if WITH_EDITOR
	FPCGTagToSettingsMap GetTrackedTagsToSettings() const;
	void GetTrackedTagsToSettings(FPCGTagToSettingsMap& OutTagsToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const;
#endif

protected:
	void OnNodeAdded(UPCGNode* InNode);
	void OnNodeRemoved(UPCGNode* InNode);

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Graph)
	TArray<TObjectPtr<UPCGNode>> Nodes;

	// Add input/output nodes
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Graph)
	TObjectPtr<UPCGNode> InputNode;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Graph)
	TObjectPtr<UPCGNode> OutputNode;

#if WITH_EDITORONLY_DATA
	// Extra data to hold information that is useful only in editor (like comments)
	UPROPERTY()
	TArray<TObjectPtr<UObject>> ExtraEditorNodes;
#endif // WITH_EDITORONLY_DATA

	// Parameters
	UPROPERTY(EditAnywhere, Category = Instance, meta = (DisplayName = "Parameters", NoResetToDefault))
	FInstancedPropertyBag UserParameters;

public:
	virtual const FInstancedPropertyBag* GetUserParametersStruct() const override { return &UserParameters; }

#if WITH_EDITOR
private:
	void NotifyGraphChanged(EPCGChangeType ChangeType);
	void OnNodeChanged(UPCGNode* InNode, EPCGChangeType ChangeType);

	void NotifyGraphParametersChanged(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName);
	void OnGraphParametersChanged(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName);

	int32 GraphChangeNotificationsDisableCounter = 0;
	bool bDelayedChangeNotification = false;
	EPCGChangeType DelayedChangeType = EPCGChangeType::None;
	bool bIsNotifying = false;
	bool bUserPausedNotificationsInGraphEditor = false;
	int32 NumberOfUserParametersPreEdit = 0;
	FName UserParameterModifiedName = NAME_None;
#endif // WITH_EDITOR
};

UCLASS(BlueprintType, ClassGroup = (Procedural), hidecategories = (Object))
class PCG_API UPCGGraphInstance : public UPCGGraphInterface
{
	GENERATED_BODY()
public:
	/** ~Begin UPCGGraphInterface interface */
	virtual UPCGGraph* GetGraph() override { return Graph ? Graph->GetGraph() : nullptr; }
	virtual const UPCGGraph* GetGraph() const override { return Graph ? Graph->GetGraph() : nullptr; }
	/** ~End UPCGGraphInterface interface */

	// ~Begin UObject interface
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* InProperty) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	// ~End UObject interface

	// Reconstruction script specific, to fix callbacks on reconstructed component
	// and teardown callback on trashed component.
	void FixCallbacks();
	void TeardownCallbacks();
#endif

protected:
#if WITH_EDITOR
	void OnGraphChanged(UPCGGraphInterface* InGraph, EPCGChangeType ChangeType);
	void NotifyGraphParametersChanged(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName);
	void OnGraphParametersChanged(UPCGGraphInterface* InGraph, EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName);
#endif
	void RefreshParameters(EPCGGraphParameterEvent InChangeType, FName InChangedPropertyName = NAME_None);

public:
	static TObjectPtr<UPCGGraphInterface> CreateInstance(UObject* InOwner, UPCGGraphInterface* InGraph);

	void SetGraph(UPCGGraphInterface* InGraph);
	void CopyParameterOverrides(UPCGGraphInterface* InGraph);
	void UpdatePropertyOverride(const FProperty* InProperty, bool bMarkAsOverridden);
	void ResetPropertyToDefault(const FProperty* InProperty);
	bool IsPropertyOverridden(const FProperty* InProperty) const { return ParametersOverrides.IsPropertyOverridden(InProperty); }
	bool IsPropertyOverriddenAndNotDefault(const FProperty* InProperty) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Instance)
	TObjectPtr<UPCGGraphInterface> Graph;

	UPROPERTY(EditAnywhere, Category = Instance, meta = (NoResetToDefault))
	FPCGOverrideInstancedPropertyBag ParametersOverrides;

	virtual const FInstancedPropertyBag* GetUserParametersStruct() const override { return &ParametersOverrides.Parameters; }

private:
#if WITH_EDITORONLY_DATA
	// Transient, to keep track the undo/redo changed the graph.
	UPCGGraphInterface* UndoRedoGraphCache = nullptr;

	FName UserParameterModifiedName = NAME_None;
#endif // WITH_EDITORONLY_DATA
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
