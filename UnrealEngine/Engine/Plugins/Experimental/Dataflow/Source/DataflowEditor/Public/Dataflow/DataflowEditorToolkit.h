// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "Misc/NotifyHook.h"
#include "GraphEditor.h"
#include "TickableEditorObject.h"
#include "Dataflow/DataflowSelectionView.h"
#include "Dataflow/SelectionViewWidget.h"
#include "Dataflow/DataflowCollectionSpreadSheet.h"
#include "Dataflow/CollectionSpreadSheetWidget.h"

class FEditorViewportTabContent;
class IDetailsView;
class FTabManager;
class IStructureDetailsView;
class IToolkitHost;
class UDataflow;
class USkeletalMesh;
class SDataflowGraphEditor;

namespace Dataflow
{
	class DATAFLOWEDITOR_API FAssetContext : public TEngineContext<FContextSingle>
	{
	public:
		DATAFLOW_CONTEXT_INTERNAL(TEngineContext<FContextSingle>, FAssetContext);

		FAssetContext(UObject* InOwner, UDataflow* InGraph, FTimestamp InTimestamp)
			: Super(InOwner, InGraph, InTimestamp)
		{}
	};
}

class DATAFLOWEDITOR_API FDataflowEditorToolkit : public FAssetEditorToolkit, public FTickableEditorObject, public FNotifyHook, public FGCObject
{
public:
	~FDataflowEditorToolkit();

	static bool CanOpenDataflowEditor(UObject* ObjectToEdit);

	virtual void InitializeEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* ObjectToEdit);

	// IToolkit Interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	// FTickableEditorObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override;

	// FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override; 

	// Tab spawners 
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_GraphCanvas(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_AssetDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_NodeDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Skeletal(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SelectionView(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_CollectionSpreadSheet(const FSpawnTabArgs& Args);

	// Callbacks for Tab
	void OnTabClosed(TSharedRef<SDockTab> Tab);

	// Member Access
	UObject* GetAsset() { return Asset; }
	const UObject* GetAsset() const { return Asset; }

	UDataflow* GetDataflow() { return Dataflow; }
	const UDataflow* GetDataflow() const { return Dataflow; }

	TSharedPtr<Dataflow::FEngineContext> GetContext() { return Context; }
	TSharedPtr<const Dataflow::FEngineContext> GetContext() const { return Context; }

	TSharedPtr<IDetailsView> GetAssetDetailsEditor() {return AssetDetailsEditor;}
	const TSharedPtr<IDetailsView> GetAssetDetailsEditor() const { return AssetDetailsEditor; }

	TSharedPtr<IStructureDetailsView> GetNodeDetailsEditor() { return NodeDetailsEditor; }
	const TSharedPtr<IStructureDetailsView> GetNodeDetailsEditor() const { return NodeDetailsEditor; }

	TSharedPtr<SDataflowGraphEditor> GetGraphEditor() { return GraphEditor; }
	const TSharedPtr<SDataflowGraphEditor> GetGraphEditor() const { return GraphEditor; }

protected:

	//~ Begin DataflowEditorActions
	void OnPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent);
	bool OnNodeVerifyTitleCommit(const FText& NewText, UEdGraphNode* GraphNode, FText& OutErrorMessage);
	void OnNodeTitleCommitted(const FText& InNewText, ETextCommit::Type InCommitType, UEdGraphNode* GraphNode);
	void OnNodeSelectionChanged(const TSet<UObject*>& NewSelection);
	void OnNodeDeleted(const TSet<UObject*>& NewSelection);
	void OnAssetPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent);
	//~ End DataflowEditorActions

private:

	TObjectPtr<UObject> Asset = nullptr;
	TObjectPtr<UDataflow> Dataflow = nullptr;
	FString TerminalPath = "";

	static const FName ViewportTabId;
	TSharedPtr<FEditorViewportTabContent> ViewportEditor;

	static const FName GraphCanvasTabId;
	TSharedPtr<SDataflowGraphEditor> GraphEditor;
	TSharedPtr<FUICommandList> GraphEditorCommands;
	TSharedRef<SDataflowGraphEditor> CreateGraphEditorWidget(UDataflow* ObjectToEdit, TSharedPtr<IStructureDetailsView> PropertiesEditor);

	static const FName AssetDetailsTabId;
	TSharedPtr<IDetailsView> AssetDetailsEditor;
	TSharedPtr<IDetailsView> CreateAssetDetailsEditorWidget(UObject* ObjectToEdit);

	static const FName NodeDetailsTabId;
	TSharedPtr<IStructureDetailsView> NodeDetailsEditor;
	TSharedPtr<IStructureDetailsView> CreateNodeDetailsEditorWidget(UObject* ObjectToEdit);

	static const FName SkeletalTabId;
	TObjectPtr<USkeleton> StubSkeleton;
	TObjectPtr<USkeletalMesh> StubSkeletalMesh;
	TSharedPtr<class ISkeletonTree> SkeletalEditor;
	TSharedPtr<ISkeletonTree> CreateSkeletalEditorWidget(UObject* ObjectToEdit);

	TSet<UObject*> PrevNodeSelection;

	static const FName SelectionViewTabId_1;
	TSharedPtr<FDataflowSelectionView> DataflowSelectionView_1;

	static const FName SelectionViewTabId_2;
	TSharedPtr<FDataflowSelectionView> DataflowSelectionView_2;

	static const FName SelectionViewTabId_3;
	TSharedPtr<FDataflowSelectionView> DataflowSelectionView_3;

	static const FName SelectionViewTabId_4;
	TSharedPtr<FDataflowSelectionView> DataflowSelectionView_4;

	static const FName CollectionSpreadSheetTabId_1;
	TSharedPtr<FDataflowCollectionSpreadSheet> DataflowCollectionSpreadSheet_1;

	static const FName CollectionSpreadSheetTabId_2;
	TSharedPtr<FDataflowCollectionSpreadSheet> DataflowCollectionSpreadSheet_2;

	static const FName CollectionSpreadSheetTabId_3;
	TSharedPtr<FDataflowCollectionSpreadSheet> DataflowCollectionSpreadSheet_3;

	static const FName CollectionSpreadSheetTabId_4;
	TSharedPtr<FDataflowCollectionSpreadSheet> DataflowCollectionSpreadSheet_4;

	TArray<IDataflowViewListener*> ViewListeners;

	TSharedPtr<Dataflow::FEngineContext> Context;
	Dataflow::FTimestamp LastNodeTimestamp = Dataflow::FTimestamp::Invalid;

	FDelegateHandle OnSelectionChangedMulticastDelegateHandle;
	FDelegateHandle OnNodeDeletedMulticastDelegateHandle;
	FDelegateHandle OnFinishedChangingPropertiesDelegateHandle;
	FDelegateHandle OnFinishedChangingAssetPropertiesDelegateHandle;
};
