// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"
#include "GraphEditor.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

class UAnimNextRigVMAssetEntry;
enum class ERigVMGraphNotifType : uint8;
class UAnimNextWorkspace;
class FDocumentTracker;
class FDocumentTabFactory;
class FTabInfo;
class FTabManager;
class IToolkitHost;
class URigVMGraph;
class URigVMController;
class UDetailsViewWrapperObject;

namespace UE::AnimNext::Editor
{

class FWorkspaceEditorMode;
struct FGraphDocumentSummoner;
struct FWorkspaceTabSummoner;
struct FAssetDocumentSummoner;
class FModule;

namespace WorkspaceModes
{
	extern const FName WorkspaceEditor;
}

namespace WorkspaceTabs
{
	extern const FName Details;
	extern const FName WorkspaceView;
	extern const FName LeftAssetDocument;
	extern const FName MiddleAssetDocument;
	extern const FName AnimNextGraphDocument;
	extern const FName ParameterBlockGraphDocument;
}

class FWorkspaceEditor : public FWorkflowCentricApplication
{
public:
	FWorkspaceEditor();
	virtual ~FWorkspaceEditor() override;

	/** Edits the specified asset */
	void InitEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InInitToolkitHost, UAnimNextWorkspace* InAnimNextWorkspace);

	using FAssetDocumentWidgetFactoryFunc = TUniqueFunction<TSharedRef<SWidget>(TSharedRef<FWorkspaceEditor>, UObject*)>;

	static void RegisterAssetDocumentWidget(FName InAssetClassName, FAssetDocumentWidgetFactoryFunc&&  InFunction);
	static void UnregisterAssetDocumentWidget(FName InAssetClassName);

	enum class EOpenWorkspaceMethod : int32
	{
		// If the asset is already used in a workspace, open that (if not already opened)
		// If the asset is already used in more than one workspace, let the user choose the workspace to open it in
		// If the asset is not yet in a workspace, create a default workspace, add the asset and open the workspace
		Default,

		// Always open a new workspace asset and add the asset to it
		AlwaysOpenNewWorkspace,
	};

	// Open an asset inside the workspace editor.
	static void OpenWorkspaceForAsset(UObject* InAsset, EOpenWorkspaceMethod InOpenMethod);

	void OnGraphSelectionChanged(const TSet<UObject*>& NewSelection);

private:
	friend class FWorkspaceEditorMode;
	friend struct FGraphDocumentSummoner;
	friend struct FWorkspaceTabSummoner;
	friend struct FAssetDocumentSummoner;
	friend class FModule;

	// IToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void InitToolMenuContext(FToolMenuContext& InMenuContext) override;
	virtual void SaveAsset_Execute() override;
	virtual bool OnRequestClose(EAssetEditorCloseReason InCloseReason) override;
	virtual void OnClose() override;

	// FAssetEditorToolkit interface
	virtual void GetSaveableObjects(TArray<UObject*>& OutObjects) const override;

	void BindCommands();
	
	void ExtendMenu();

	void ExtendToolbar();

	void SetFocusedGraphEditor(TSharedPtr<SGraphEditor> InGraphEditor);

	UEdGraph* GetFocusedGraph() const;

	URigVMGraph* GetFocusedVMGraph() const;

	URigVMController* GetFocusedVMController() const;

	// Handle node titles being changed
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	void CloseDocumentTab(const UObject* DocumentID);

	bool InEditingMode() const;

	bool IsEditable(UEdGraph* InGraph) const;
	
	FGraphPanelSelectionSet GetSelectedNodes() const;
	
	void DeleteSelectedNodes();
	
	bool CanDeleteSelectedNodes();

	void SetSelectedObjects(const TArray<UObject*>& InObjects);

	void HandleDetailsViewCreated(TSharedRef<IDetailsView> InDetailsView)
	{
		DetailsView = InDetailsView;
	}

	void RestoreEditedObjectState();

	void SaveEditedObjectState();

	TSharedPtr<SDockTab> OpenDocument(const UObject* InForObject, FDocumentTracker::EOpenDocumentCause InCause);

	// Open the supplied assets for editing within the workspace editor
	void OpenAssets(TConstArrayView<FAssetData> InAssets);

	void HandleSaveGraphState(UEdGraph* InGraph, FVector2D InViewOffset, float InZoomAmount);

	void HandleSaveDocumentState(UObject* InObject);

	void OnGraphModified(ERigVMGraphNotifType Type, URigVMGraph* Graph, UObject* Subject);

	void OnOpenGraph(URigVMGraph* InGraph);

	void OnDeleteEntries(const TArray<UAnimNextRigVMAssetEntry*>& InEntries);

	// The asset we are editing
	UAnimNextWorkspace* Workspace = nullptr;

	// Currently focused graph editor
	TWeakPtr<SGraphEditor> FocusedGraphEdPtr;

	// Document tracker
	TSharedPtr<FDocumentTracker> DocumentManager;

	// Command list for this editor
	TSharedPtr<FUICommandList> CommandList;

	// Our details panel
	TSharedPtr<IDetailsView> DetailsView;

	TArray<TStrongObjectPtr<UDetailsViewWrapperObject>> WrapperObjects;

	static TMap<FName, FAssetDocumentWidgetFactoryFunc> AssetDocumentWidgetFactories;

	bool bSavingWorkspaceOnly = false;
};

}