// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfaceGraphEditor.h"
#include "DataInterfaceGraphEditorMode.h"
#include "DataInterfaceGraph.h"
#include "DataInterfaceGraph_EditorData.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphUtilities.h"
#include "SDataInterfaceGraphEditorActionMenu.h"
#include "DataInterfaceUncookedOnlyUtils.h"
#include "Framework/Commands/GenericCommands.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "DataInterfaceGraphEditor"

namespace UE::DataInterfaceGraphEditor
{

namespace Modes
{
	const FName GraphEditor("DataInterfaceEditorMode");
}

namespace Tabs
{
	const FName Details("DetailsTab");
	const FName Document("Document");
}

const FName AppIdentifier("DataInterfaceGraphEditor");

void FGraphEditor::InitEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InInitToolkitHost, UDataInterfaceGraph* InDataInterfaceGraph)
{
	DataInterfaceGraph = InDataInterfaceGraph;

	DataInterfaceGraph_EditorData = UE::DataInterfaceGraphUncookedOnly::FUtils::GetEditorData(DataInterfaceGraph);
	DataInterfaceGraph_EditorData->Initialize(false);
	
	DocumentManager = MakeShared<FDocumentTracker>();
	DocumentManager->Initialize(SharedThis(this));

	TSharedRef<FGraphEditorSummoner> GraphEditorSummoner = MakeShared<FGraphEditorSummoner>(SharedThis(this));
	GraphEditorSummoner->OnCreateGraphEditorWidget().BindSP(this, &FGraphEditor::CreateGraphEditorWidget);
	GraphEditorSummoner->OnGraphEditorFocused().BindSP(this, &FGraphEditor::OnGraphEditorFocused);
	GraphEditorSummoner->OnGraphEditorBackgrounded().BindSP(this, &FGraphEditor::OnGraphEditorBackgrounded);
	GraphEditorTabFactoryPtr = GraphEditorSummoner;
	DocumentManager->RegisterDocumentFactory(GraphEditorSummoner);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(InMode, InInitToolkitHost, AppIdentifier, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InDataInterfaceGraph);

	BindCommands();

	AddApplicationMode(Modes::GraphEditor, MakeShared<FGraphEditorMode>(SharedThis(this)));
	SetCurrentMode(Modes::GraphEditor);
	
	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// Open initial document
	DocumentManager->OpenDocument(FTabPayload_UObject::Make(DataInterfaceGraph_EditorData->EntryPointGraph), FDocumentTracker::EOpenDocumentCause::OpenNewDocument);
	DocumentManager->OpenDocument(FTabPayload_UObject::Make(DataInterfaceGraph_EditorData->RootGraph), FDocumentTracker::EOpenDocumentCause::OpenNewDocument);
	DocumentManager->OpenDocument(FTabPayload_UObject::Make(DataInterfaceGraph_EditorData->FunctionLibraryEdGraph), FDocumentTracker::EOpenDocumentCause::OpenNewDocument);
}

void FGraphEditor::BindCommands()
{
}

void FGraphEditor::ExtendMenu()
{
	
}

void FGraphEditor::ExtendToolbar()
{
	
}

void FGraphEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	DocumentManager->SetTabManager(InTabManager);

	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void FGraphEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	
}

FName FGraphEditor::GetToolkitFName() const
{
	return FName("DataInterfaceGraphEditor");
}

FText FGraphEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "DataInterfaceGraphEditor");
}

FString FGraphEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "DataInterfaceGraphEditor ").ToString();
}

FLinearColor FGraphEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

void FGraphEditor::InitToolMenuContext(FToolMenuContext& InMenuContext)
{
}

void FGraphEditor::OnGraphEditorFocused(TSharedRef<SGraphEditor> InGraphEditor)
{
	// Update the graph editor that is currently focused
	FocusedGraphEdPtr = InGraphEditor;
}

void FGraphEditor::OnGraphEditorBackgrounded(TSharedRef<SGraphEditor> InGraphEditor)
{
	// Update the graph editor that is currently focused
	FocusedGraphEdPtr = nullptr;
}

UEdGraph* FGraphEditor::GetFocusedGraph() const
{
	if (FocusedGraphEdPtr.IsValid())
	{
		if (UEdGraph* Graph = FocusedGraphEdPtr.Pin()->GetCurrentGraph())
		{
			return Graph;
		}
	}
	return nullptr;
}

URigVMGraph* FGraphEditor::GetFocusedVMGraph() const
{
	UDataInterfaceGraph_EdGraph* EdGraph = Cast<UDataInterfaceGraph_EdGraph>(GetFocusedGraph());
	return DataInterfaceGraph_EditorData->GetVMGraphForEdGraph(EdGraph);
}

URigVMController* FGraphEditor::GetFocusedVMController() const
{
	return DataInterfaceGraph_EditorData->GetRigVMClient()->GetController(GetFocusedVMGraph());
}

TSharedRef<SGraphEditor> FGraphEditor::CreateGraphEditorWidget(TSharedRef<FTabInfo> InTabInfo, UEdGraph* InGraph)
{
	if(!GraphEditorCommands.IsValid())
	{
		GraphEditorCommands = MakeShared<FUICommandList>();
	
		GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
			FExecuteAction::CreateSP(this, &FGraphEditor::DeleteSelectedNodes),
			FCanExecuteAction::CreateSP(this, &FGraphEditor::CanDeleteSelectedNodes));
	}
	
	SGraphEditor::FGraphEditorEvents Events;
	Events.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &FGraphEditor::OnCreateGraphActionMenu);
	
	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(this, &FGraphEditor::IsEditable, InGraph)
		.GraphToEdit(InGraph)
		.GraphEvents(Events)
		.AssetEditorToolkit(AsShared());
}

FActionMenuContent FGraphEditor::OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
{
	TSharedRef<SActionMenu> ActionMenu = SNew(SActionMenu)
		.AutoExpandActionMenu(bAutoExpand)
		.Graph(InGraph)
		.NewNodePosition(InNodePosition)
		.DraggedFromPins(InDraggedPins)
		.OnClosedCallback(InOnMenuClosed);

	TSharedPtr<SWidget> FilterTextBox = StaticCastSharedRef<SWidget>(ActionMenu->GetFilterTextBox());
	return FActionMenuContent(StaticCastSharedRef<SWidget>(ActionMenu), FilterTextBox);
}

void FGraphEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (UEdGraphNode_Comment* CommentBeingChanged = Cast<UEdGraphNode_Comment>(NodeBeingChanged))
	{
		GetFocusedVMController()->SetCommentTextByName(CommentBeingChanged->GetFName(), NewText.ToString(), CommentBeingChanged->FontSize, CommentBeingChanged->bCommentBubbleVisible, CommentBeingChanged->bColorCommentBubble, true, true);
	}
}

void FGraphEditor::CloseDocumentTab(const UObject* DocumentID)
{
	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(DocumentID);
	DocumentManager->CloseTab(Payload);
}

bool FGraphEditor::InEditingMode() const
{
	// @TODO: disallow editing when debugging when implemented

	return true;
}

bool FGraphEditor::IsEditable(UEdGraph* InGraph) const
{
	return InGraph && InEditingMode() && InGraph->bEditable;
}

FGraphPanelSelectionSet FGraphEditor::GetSelectedNodes() const
{
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		CurrentSelection = FocusedGraphEd->GetSelectedNodes();
	}
	return CurrentSelection;
}

void FGraphEditor::DeleteSelectedNodes()
{
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (!FocusedGraphEd.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());
	FocusedGraphEd->GetCurrentGraph()->Modify();
	
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

//	SetUISelectionState(NAME_None);

	if(FocusedGraphEd)
	{
		FocusedGraphEd->ClearSelectionSet();
	}

	// Some nodes have sub-objects that are represented as other tabs.
	// Close them here as a pre-pass before we remove their nodes. If the documents are left open they
	// may reference dangling data and function incorrectly in cases such as FindBlueprintforNodeChecked
	for (FGraphPanelSelectionSet::TConstIterator NodeIt( SelectedNodes ); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Node->CanUserDeleteNode())
			{
				auto CloseAllDocumentsTab = [this](const UEdGraphNode* InNode)
				{
					TArray<UObject*> NodesToClose;
					GetObjectsWithOuter(InNode, NodesToClose);
					for (UObject* Node : NodesToClose)
					{
						UEdGraph* NodeGraph = Cast<UEdGraph>(Node);
						if (NodeGraph)
						{
							CloseDocumentTab(NodeGraph);
						}
					}
				};
				
				if (Node->GetSubGraphs().Num() > 0)
				{
					CloseAllDocumentsTab(Node);
				}
			}
		}
	}

	// Now remove the selected nodes
	for (FGraphPanelSelectionSet::TConstIterator NodeIt( SelectedNodes ); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Node->CanUserDeleteNode())
			{
				if (Node->GetSubGraphs().Num() > 0)
				{
					DocumentManager->CleanInvalidTabs();
				}

				FBlueprintEditorUtils::RemoveNode(nullptr, Node);
			}
		}
	}
}

bool FGraphEditor::CanDeleteSelectedNodes()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	bool bCanUserDeleteNode = false;

	if(IsEditable(GetFocusedGraph()) && SelectedNodes.Num() > 0)
	{
		for(UObject* NodeObject : SelectedNodes)
		{
			// If any nodes allow deleting, then do not disable the delete option
			UEdGraphNode* Node = Cast<UEdGraphNode>(NodeObject);
			if(Node->CanUserDeleteNode())
			{
				bCanUserDeleteNode = true;
				break;
			}
		}
	}

	return bCanUserDeleteNode;
}

}

#undef LOCTEXT_NAMESPACE