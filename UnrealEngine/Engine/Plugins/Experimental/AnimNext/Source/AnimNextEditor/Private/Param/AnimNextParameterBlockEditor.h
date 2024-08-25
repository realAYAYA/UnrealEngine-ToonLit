// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"
#include "GraphEditor.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

enum class ERigVMGraphNotifType : uint8;
class UAnimNextParameterBlock;
class UAnimNextParameterBlock_EditorData;
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

class FParametersTabSummoner;
class FParametersEditorMode;

namespace ParameterBlockModes
{
	extern const FName ParameterBlockEditor;
}

namespace ParameterBlockTabs
{
	extern const FName Details;
	extern const FName Parameters;
	extern const FName Document;
}

class FParameterBlockEditor : public FWorkflowCentricApplication
{
public:
	FParameterBlockEditor();
	virtual ~FParameterBlockEditor() override;

	/** Edits the specified asset */
	void InitEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InInitToolkitHost, UAnimNextParameterBlock* InAnimNextParameterBlock);

private:
	friend class FParameterBlockEditorMode;
	friend class FParameterBlockTabSummoner;

	// IToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void InitToolMenuContext(FToolMenuContext& InMenuContext) override;

	// FAssetEditorToolkit interface
	virtual void GetSaveableObjects(TArray<UObject*>& OutObjects) const override;

	void BindCommands();
	
	void ExtendMenu();

	void ExtendToolbar();

	void OnGraphEditorFocused(TSharedRef<SGraphEditor> InGraphEditor);

	void OnGraphEditorBackgrounded(TSharedRef<SGraphEditor> InGraphEditor);
	
	UEdGraph* GetFocusedGraph() const;

	URigVMGraph* GetFocusedVMGraph() const;

	URigVMController* GetFocusedVMController() const;

	// Create new graph editor widget for the supplied document container
	TSharedRef<SGraphEditor> CreateGraphEditorWidget(TSharedRef<FTabInfo> InTabInfo, UEdGraph* InGraph);

	// Create the action menu for the specified graph
	FActionMenuContent OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed);

	// Handle node titles being changed
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	void CloseDocumentTab(const UObject* DocumentID);

	bool InEditingMode() const;

	bool IsEditable(UEdGraph* InGraph) const;
	
	FGraphPanelSelectionSet GetSelectedNodes() const;
	
	void DeleteSelectedNodes();
	
	bool CanDeleteSelectedNodes();

	void SetSelectedObjects(TArray<UObject*> InObjects);

	void HandleDetailsViewCreated(TSharedRef<IDetailsView> InDetailsView)
	{
		DetailsView = InDetailsView;
	}

	void RestoreEditedObjectState();

	void SaveEditedObjectState();

	TSharedPtr<SDockTab> OpenDocument(const UObject* InForObject, FDocumentTracker::EOpenDocumentCause InCause);

	void HandleSaveGraphState(UEdGraph* InGraph, FVector2D InViewOffset, float InZoomAmount);

	// The asset we are editing
	UAnimNextParameterBlock* ParameterBlock = nullptr;

	// Cached editor data ptr
	UAnimNextParameterBlock_EditorData* EditorData = nullptr;

	// Currently focused graph editor
	TWeakPtr<SGraphEditor> FocusedGraphEdPtr;

	// Document tracker
	TSharedPtr<FDocumentTracker> DocumentManager;

	// Factory for document tabs
	TWeakPtr<FDocumentTabFactory> GraphEditorTabFactoryPtr;

	// Command list for the graph editor
	TSharedPtr<FUICommandList> GraphEditorCommands;

	// Our details panel
	TSharedPtr<IDetailsView> DetailsView;

	TArray<TStrongObjectPtr<UDetailsViewWrapperObject>> WrapperObjects;
};

}