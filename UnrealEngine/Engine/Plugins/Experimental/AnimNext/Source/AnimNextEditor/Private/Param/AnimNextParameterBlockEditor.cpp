// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextParameterBlockEditor.h"

#include "ParameterBlockEditorMode.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "EdGraphNode_Comment.h"
#include "ExternalPackageHelper.h"
#include "Common/SActionMenu.h"
#include "UncookedOnlyUtils.h"
#include "Framework/Commands/GenericCommands.h"
#include "RigVMModel/RigVMController.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Param/AnimNextParameterSettings.h"
#include "Param/AnimNextParameterExecuteContext.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "AnimNextParameterBlockEditor"

namespace UE::AnimNext::Editor
{

namespace ParameterBlockModes
{
	const FName ParameterBlockEditor("AnimNextParameterBlockEditorMode");
}

namespace ParameterBlockTabs
{
	const FName Details("DetailsTab");
	const FName Parameters("ParametersTab");
	const FName Document("Document");
}

const FName ParameterBlockAppIdentifier("AnimNextParameterBlockEditor");

FParameterBlockEditor::FParameterBlockEditor()
{
	UAnimNextParameterSettings* Settings = GetMutableDefault<UAnimNextParameterSettings>();
	Settings->LoadConfig();
}

FParameterBlockEditor::~FParameterBlockEditor()
{
	UAnimNextParameterSettings* Settings = GetMutableDefault<UAnimNextParameterSettings>();
	Settings->SaveConfig();
}

void FParameterBlockEditor::InitEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InInitToolkitHost, UAnimNextParameterBlock* InAnimNextParameterBlock)
{
	ParameterBlock = InAnimNextParameterBlock;

	EditorData = UncookedOnly::FUtils::GetEditorData(ParameterBlock);
	EditorData->Initialize(false);

	DocumentManager = MakeShared<FDocumentTracker>();
	DocumentManager->Initialize(SharedThis(this));

	TSharedRef<FParameterBlockEditorDocumentSummoner> DocumentSummoner = MakeShared<FParameterBlockEditorDocumentSummoner>(SharedThis(this));
	DocumentSummoner->OnCreateGraphEditorWidget().BindSP(this, &FParameterBlockEditor::CreateGraphEditorWidget);
	DocumentSummoner->OnGraphEditorFocused().BindSP(this, &FParameterBlockEditor::OnGraphEditorFocused);
	DocumentSummoner->OnGraphEditorBackgrounded().BindSP(this, &FParameterBlockEditor::OnGraphEditorBackgrounded);
	DocumentSummoner->OnSaveGraphState().BindSP(this, &FParameterBlockEditor::HandleSaveGraphState);
	GraphEditorTabFactoryPtr = DocumentSummoner;
	DocumentManager->RegisterDocumentFactory(DocumentSummoner);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(InMode, InInitToolkitHost, ParameterBlockAppIdentifier, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InAnimNextParameterBlock);

	BindCommands();

	AddApplicationMode(ParameterBlockModes::ParameterBlockEditor, MakeShared<FParameterBlockEditorMode>(SharedThis(this)));
	SetCurrentMode(ParameterBlockModes::ParameterBlockEditor);
	
	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

void FParameterBlockEditor::RestoreEditedObjectState()
{
	for (const FEditedDocumentInfo& Document : EditorData->LastEditedDocuments)
	{
		if (UObject* Obj = Document.EditedObjectPath.ResolveObject())
		{
			if(TSharedPtr<SDockTab> DockTab = OpenDocument(Obj, FDocumentTracker::RestorePreviousDocument))
			{
				TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(DockTab->GetContent());
				GraphEditor->SetViewLocation(Document.SavedViewOffset, Document.SavedZoomAmount);
			}
		}
	}
}

void FParameterBlockEditor::SaveEditedObjectState()
{
	// Clear currently edited documents
	EditorData->LastEditedDocuments.Empty();

	// Ask all open documents to save their state, which will update LastEditedDocuments
	DocumentManager->SaveAllState();
}

TSharedPtr<SDockTab> FParameterBlockEditor::OpenDocument(const UObject* InForObject, FDocumentTracker::EOpenDocumentCause InCause)
{
	if(InCause != FDocumentTracker::RestorePreviousDocument)
	{
		EditorData->LastEditedDocuments.AddUnique(const_cast<UObject*>(InForObject));
	}

	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(InForObject);
	return DocumentManager->OpenDocument(Payload, InCause);
}

void FParameterBlockEditor::BindCommands()
{
}

void FParameterBlockEditor::ExtendMenu()
{
	
}

void FParameterBlockEditor::ExtendToolbar()
{
	
}

void FParameterBlockEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	DocumentManager->SetTabManager(InTabManager);

	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void FParameterBlockEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	
}

FName FParameterBlockEditor::GetToolkitFName() const
{
	return FName("AnimNextParameterBlockEditor");
}

FText FParameterBlockEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "AnimNextParameterBlockEditor");
}

FString FParameterBlockEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "AnimNextParameterBlockEditor ").ToString();
}

FLinearColor FParameterBlockEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

void FParameterBlockEditor::InitToolMenuContext(FToolMenuContext& InMenuContext)
{
}

void FParameterBlockEditor::OnGraphEditorFocused(TSharedRef<SGraphEditor> InGraphEditor)
{
	// Update the graph editor that is currently focused
	FocusedGraphEdPtr = InGraphEditor;
}

void FParameterBlockEditor::OnGraphEditorBackgrounded(TSharedRef<SGraphEditor> InGraphEditor)
{
	// Update the graph editor that is currently focused
	FocusedGraphEdPtr = nullptr;
}

UEdGraph* FParameterBlockEditor::GetFocusedGraph() const
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

URigVMGraph* FParameterBlockEditor::GetFocusedVMGraph() const
{
	UAnimNextParameterBlock_EdGraph* EdGraph = Cast<UAnimNextParameterBlock_EdGraph>(GetFocusedGraph());
	return EditorData->GetRigVMGraphForEditorObject(EdGraph);
}

URigVMController* FParameterBlockEditor::GetFocusedVMController() const
{
	return EditorData->GetRigVMClient()->GetController(GetFocusedVMGraph());
}

TSharedRef<SGraphEditor> FParameterBlockEditor::CreateGraphEditorWidget(TSharedRef<FTabInfo> InTabInfo, UEdGraph* InGraph)
{
	if(!GraphEditorCommands.IsValid())
	{
		GraphEditorCommands = MakeShared<FUICommandList>();
	
		GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
			FExecuteAction::CreateSP(this, &FParameterBlockEditor::DeleteSelectedNodes),
			FCanExecuteAction::CreateSP(this, &FParameterBlockEditor::CanDeleteSelectedNodes));
	}
	
	SGraphEditor::FGraphEditorEvents Events;
	Events.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &FParameterBlockEditor::OnCreateGraphActionMenu);
	
	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(this, &FParameterBlockEditor::IsEditable, InGraph)
		.GraphToEdit(InGraph)
		.GraphEvents(Events)
		.AssetEditorToolkit(AsShared());
}

FActionMenuContent FParameterBlockEditor::OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
{
	TSharedRef<SActionMenu> ActionMenu = SNew(SActionMenu)
		.AutoExpandActionMenu(bAutoExpand)
		.Graph(InGraph)
		.NewNodePosition(InNodePosition)
		.DraggedFromPins(InDraggedPins)
		.OnClosedCallback(InOnMenuClosed)
		.AllowedExecuteContexts( { FRigVMExecuteContext::StaticStruct(), FAnimNextParameterExecuteContext::StaticStruct() });

	TSharedPtr<SWidget> FilterTextBox = StaticCastSharedRef<SWidget>(ActionMenu->GetFilterTextBox());
	return FActionMenuContent(StaticCastSharedRef<SWidget>(ActionMenu), FilterTextBox);
}

void FParameterBlockEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (UEdGraphNode_Comment* CommentBeingChanged = Cast<UEdGraphNode_Comment>(NodeBeingChanged))
	{
		GetFocusedVMController()->SetCommentTextByName(CommentBeingChanged->GetFName(), NewText.ToString(), CommentBeingChanged->FontSize, CommentBeingChanged->bCommentBubbleVisible, CommentBeingChanged->bColorCommentBubble, true, true);
	}
}

void FParameterBlockEditor::CloseDocumentTab(const UObject* DocumentID)
{
	EditorData->LastEditedDocuments.Remove(const_cast<UObject*>(DocumentID));

	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(DocumentID);
	DocumentManager->CloseTab(Payload);
}

bool FParameterBlockEditor::InEditingMode() const
{
	// @TODO: disallow editing when debugging when implemented

	return true;
}

bool FParameterBlockEditor::IsEditable(UEdGraph* InGraph) const
{
	return InGraph && InEditingMode() && InGraph->bEditable;
}

FGraphPanelSelectionSet FParameterBlockEditor::GetSelectedNodes() const
{
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		CurrentSelection = FocusedGraphEd->GetSelectedNodes();
	}
	return CurrentSelection;
}

void FParameterBlockEditor::DeleteSelectedNodes()
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

bool FParameterBlockEditor::CanDeleteSelectedNodes()
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

void FParameterBlockEditor::SetSelectedObjects(TArray<UObject*> InObjects)
{
	if(DetailsView.IsValid())
	{
		DetailsView->SetObjects(InObjects);
	}
}

void FParameterBlockEditor::GetSaveableObjects(TArray<UObject*>& OutObjects) const
{
	// Base class will pick up edited object
	FWorkflowCentricApplication::GetSaveableObjects(OutObjects);

	// Get external objects too
	FExternalPackageHelper::GetExternalSaveableObjects(EditorData, OutObjects);
}

void FParameterBlockEditor::HandleSaveGraphState(UEdGraph* InGraph, FVector2D InViewOffset, float InZoomAmount)
{
	EditorData->LastEditedDocuments.AddUnique(FEditedDocumentInfo(InGraph, InViewOffset, InZoomAmount));
}

}

#undef LOCTEXT_NAMESPACE