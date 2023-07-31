// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"
#include "GraphEditor.h"

enum class ERigVMGraphNotifType : uint8;
class UDataInterfaceGraph;
class UDataInterfaceGraph_EditorData;
class FDocumentTracker;
class FDocumentTabFactory;
class FTabInfo;
class FTabManager;
class IToolkitHost;
class URigVMGraph;
class URigVMController;
class UDetailsViewWrapperObject;

namespace UE::DataInterfaceGraphEditor
{

namespace Modes
{
	extern const FName GraphEditor;
}

namespace Tabs
{
	extern const FName Details;
	extern const FName Document;
}

class FGraphEditor : public FWorkflowCentricApplication
{
public:
	/** Edits the specified asset */
	void InitEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InInitToolkitHost, UDataInterfaceGraph* InDataInterfaceGraph);

private:
	friend class FGraphEditorMode;
	
	// IToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void InitToolMenuContext(FToolMenuContext& InMenuContext) override;

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

	TSharedPtr<SGraphEditor> GetGraphEditor(UEdGraph* InEdGraph) const;
	
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	void HandleDetailsViewCreated(TSharedRef<IDetailsView> InDetailsView)
	{
		DetailsView = InDetailsView;
	}
	
	// The asset we are editing
	UDataInterfaceGraph* DataInterfaceGraph = nullptr;

	// Cached editor data ptr
	UDataInterfaceGraph_EditorData* DataInterfaceGraph_EditorData = nullptr;
	
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