// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "PCGNode.h"
#include "PCGSettings.h"

#include "PCGGraph.generated.h"

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPCGGraphChanged, UPCGGraph* /*Graph*/, EPCGChangeType /*ChangeType*/);
#endif // WITH_EDITOR

UCLASS(BlueprintType, ClassGroup = (Procedural), hidecategories=(Object))
class PCG_API UPCGGraph : public UObject
{
#if WITH_EDITOR
	friend class FPCGEditor;
#endif // WITH_EDITOR

	GENERATED_BODY()

public:
	UPCGGraph(const FObjectInitializer& ObjectInitializer);
	/** ~Begin UObject interface */
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	/** ~End UObject interface */

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = AssetInfo, AssetRegistrySearchable)
	bool bExposeToLibrary = false;

	UPROPERTY(EditAnywhere, Category = AssetInfo, AssetRegistrySearchable)
	FText Category;

	UPROPERTY(EditAnywhere, Category = AssetInfo, AssetRegistrySearchable)
	FText Description;
#endif

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bLandscapeUsesMetadata = true;

	/** Creates a default node based on the settings class wanted. Returns the newly created node. */
	UFUNCTION(BlueprintCallable, Category = Graph, meta=(DeterminesOutputType = "InSettingsClass", DynamicOutputParam = "DefaultNodeSettings"))
	UPCGNode* AddNodeOfType(TSubclassOf<class UPCGSettings> InSettingsClass, UPCGSettings*& DefaultNodeSettings);

	/** Creates a node and assigns it in the input settings. Returns the created node. */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGNode* AddNode(UPCGSettings* InSettings);

	/** Adds a directed edge in the graph. Returns the "To" node for easy chaining */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGNode* AddEdge(UPCGNode* From, UPCGNode* To);

	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGNode* AddLabeledEdge(UPCGNode* From, const FName& InboundLabel, UPCGNode* To, const FName& OutboundLabel);

	/** Returns the graph input node */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGNode* GetInputNode() const { return InputNode; }

	/** Returns the graph output node */
	UFUNCTION(BlueprintCallable, Category = Graph)
	UPCGNode* GetOutputNode() const { return OutputNode; }

	/** Duplicate a given node by creating a new node with the same settings and properties, but without any edges and add it to the graph */
	TObjectPtr<UPCGNode> ReconstructNewNode(const UPCGNode* InNode);

	bool Contains(UPCGNode* Node) const;
	const TArray<UPCGNode*>& GetNodes() const { return Nodes; }
	void AddNode(UPCGNode* InNode);
	void RemoveNode(UPCGNode* InNode);
	bool RemoveEdge(UPCGNode* From, const FName& FromLabel, UPCGNode* To, const FName& ToLabel);
	bool RemoveAllInboundEdges(UPCGNode* InNode);
	bool RemoveAllOutboundEdges(UPCGNode* InNode);
	bool RemoveInboundEdges(UPCGNode* InNode, const FName& InboundLabel);
	bool RemoveOutboundEdges(UPCGNode* InNode, const FName& OutboundLabel);

#if WITH_EDITOR
	void DisableNotificationsForEditor();
	void EnableNotificationsForEditor();
	void ToggleUserPausedNotificationsForEditor();
	bool NotificationsForEditorArePausedByUser() const { return bUserPausedNotificationsInGraphEditor; }
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

#if WITH_EDITOR
	FOnPCGGraphChanged OnGraphChangedDelegate;
#endif // WITH_EDITOR

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

#if WITH_EDITOR
private:
	void NotifyGraphChanged(EPCGChangeType ChangeType);
	void OnNodeChanged(UPCGNode* InNode, EPCGChangeType ChangeType);

	int32 GraphChangeNotificationsDisableCounter = 0;
	bool bDelayedChangeNotification = false;
	EPCGChangeType DelayedChangeType = EPCGChangeType::None;
	bool bIsNotifying = false;
	bool bUserPausedNotificationsInGraphEditor = false;
#endif // WITH_EDITOR
};
