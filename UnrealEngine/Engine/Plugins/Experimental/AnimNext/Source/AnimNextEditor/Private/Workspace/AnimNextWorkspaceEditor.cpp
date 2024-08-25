// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextWorkspaceEditor.h"

#include "AnimNextRigVMAssetEntry.h"
#include "GraphDocumentSummoner.h"
#include "AnimNextWorkspaceEditorMode.h"
#include "AnimNextWorkspace.h"
#include "AnimNextWorkspaceFactory.h"
#include "AssetDocumentSummoner.h"
#include "EdGraphNode_Comment.h"
#include "EditorUtils.h"
#include "ExternalPackageHelper.h"
#include "Framework/Commands/GenericCommands.h"
#include "RigVMModel/RigVMController.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"
#include "SWorkspacePicker.h"
#include "Dialog/SCustomDialog.h"
#include "Graph/AnimNextGraphDocumentSummoner.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Param/ParameterBlockGraphDocumentSummoner.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Framework/Application/SlateApplication.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraph_EdGraphNode.h"
#include "GraphEditAction.h"
#include "IAnimNextRigVMGraphInterface.h"
#include "UncookedOnlyUtils.h"

#define LOCTEXT_NAMESPACE "AnimNextWorkspaceEditor"

namespace UE::AnimNext::Editor
{

namespace WorkspaceModes
{
	const FName WorkspaceEditor("AnimNextWorkspaceEditorMode");
}

namespace WorkspaceTabs
{
	const FName Details("DetailsTab");
	const FName WorkspaceView("WorkspaceView");
	const FName LeftAssetDocument("LeftAssetDocument");
	const FName MiddleAssetDocument("MiddleAssetDocument");
	const FName AnimNextGraphDocument("AnimNextGraphDocument");
	const FName ParameterBlockGraphDocument("ParameterBlockGraphDocument");
}

const FName WorkspaceAppIdentifier("AnimNextWorkspaceEditor");

TMap<FName, FWorkspaceEditor::FAssetDocumentWidgetFactoryFunc> FWorkspaceEditor::AssetDocumentWidgetFactories;

FWorkspaceEditor::FWorkspaceEditor()
{
}

FWorkspaceEditor::~FWorkspaceEditor()
{
}

void FWorkspaceEditor::InitEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InInitToolkitHost, UAnimNextWorkspace* InWorkspace)
{
	Workspace = InWorkspace;

	DocumentManager = MakeShared<FDocumentTracker>(NAME_None);
	DocumentManager->Initialize(SharedThis(this));

	TSharedRef<FParameterBlockGraphDocumentSummoner> ParameterGraphDocumentSummoner = MakeShared<FParameterBlockGraphDocumentSummoner>(WorkspaceTabs::ParameterBlockGraphDocument, SharedThis(this));
	ParameterGraphDocumentSummoner->OnSaveGraphState().BindSP(this, &FWorkspaceEditor::HandleSaveGraphState);
	DocumentManager->RegisterDocumentFactory(ParameterGraphDocumentSummoner);

	TSharedRef<FAnimNextGraphDocumentSummoner> GraphDocumentSummoner = MakeShared<FAnimNextGraphDocumentSummoner>(WorkspaceTabs::AnimNextGraphDocument, SharedThis(this));
	GraphDocumentSummoner->OnSaveGraphState().BindSP(this, &FWorkspaceEditor::HandleSaveGraphState);
	DocumentManager->RegisterDocumentFactory(GraphDocumentSummoner);

	TSharedRef<FAssetDocumentSummoner> LeftAssetDocumentSummoner = MakeShared<FAssetDocumentSummoner>(WorkspaceTabs::LeftAssetDocument, SharedThis(this));
	LeftAssetDocumentSummoner->SetAllowedAssetClassPaths(
	{
		UAnimNextGraph::StaticClass()->GetClassPathName(),
		UAnimNextParameterBlock::StaticClass()->GetClassPathName(),
	});
	LeftAssetDocumentSummoner->OnSaveDocumentState().BindSP(this, &FWorkspaceEditor::HandleSaveDocumentState);
	DocumentManager->RegisterDocumentFactory(LeftAssetDocumentSummoner);

	TSharedRef<FAssetDocumentSummoner> MiddleAssetDocumentSummoner = MakeShared<FAssetDocumentSummoner>(WorkspaceTabs::MiddleAssetDocument, SharedThis(this));
	MiddleAssetDocumentSummoner->SetAllowedAssetClassPaths(
	{
		UAnimNextSchedule::StaticClass()->GetClassPathName(),
	});
	MiddleAssetDocumentSummoner->OnSaveDocumentState().BindSP(this, &FWorkspaceEditor::HandleSaveDocumentState);
	DocumentManager->RegisterDocumentFactory(MiddleAssetDocumentSummoner);

	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;
	InitAssetEditor(InMode, InInitToolkitHost, WorkspaceAppIdentifier, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InWorkspace);

	BindCommands();

	AddApplicationMode(WorkspaceModes::WorkspaceEditor, MakeShared<FWorkspaceEditorMode>(SharedThis(this)));
	SetCurrentMode(WorkspaceModes::WorkspaceEditor);

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

void FWorkspaceEditor::RegisterAssetDocumentWidget(FName InAssetClassName, FAssetDocumentWidgetFactoryFunc&& InFunction)
{
	AssetDocumentWidgetFactories.Add(InAssetClassName, MoveTemp(InFunction));
}

void FWorkspaceEditor::UnregisterAssetDocumentWidget(FName InAssetClassName)
{
	AssetDocumentWidgetFactories.Remove(InAssetClassName);
}

void FWorkspaceEditor::OpenWorkspaceForAsset(UObject* InAsset, EOpenWorkspaceMethod InOpenMethod)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> RelevantWorkspaceAssets;

	if(InOpenMethod != EOpenWorkspaceMethod::AlwaysOpenNewWorkspace)
	{
		// Look for existing workspaces that export this asset
		FARFilter ARFilter;
		ARFilter.ClassPaths.Add(UAnimNextWorkspace::StaticClass()->GetClassPathName());
		ARFilter.bRecursiveClasses = true;

		TArray<FAssetData> AllWorkspaceAssets;
		AssetRegistryModule.Get().GetAssets(ARFilter, AllWorkspaceAssets);

		for(const FAssetData& WorkspaceAsset : AllWorkspaceAssets)
		{
			FAnimNextWorkspaceAssetRegistryExports Exports;
			FUtils::GetExportedAssetsForWorkspace(WorkspaceAsset, Exports);

			FSoftObjectPath ObjectPath(InAsset);
			for(const FAnimNextWorkspaceAssetRegistryExportEntry& ExportEntry : Exports.Assets)
			{
				if(ExportEntry.Asset == ObjectPath)
				{
					RelevantWorkspaceAssets.Add(WorkspaceAsset);
					break;
				}
			}
		}
	}

	FWorkspaceEditor* WorkspaceEditor = nullptr;

	auto HandleNewWorkspace = [InAsset, &WorkspaceEditor]()
	{
		UAnimNextWorkspaceFactory* Factory = NewObject<UAnimNextWorkspaceFactory>();
		UPackage* Package = CreatePackage(nullptr);
		FName PackageName = *FPaths::GetBaseFilename(Package->GetName());
		UAnimNextWorkspace* NewWorkspace = CastChecked<UAnimNextWorkspace>(Factory->FactoryCreateNew(UAnimNextWorkspace::StaticClass(), Package, PackageName, RF_Public | RF_Standalone, NULL, GWarn));
		NewWorkspace->AddAsset(InAsset, false);
		NewWorkspace->MarkPackageDirty();
		TSharedRef<FWorkspaceEditor> Editor = MakeShared<FWorkspaceEditor>();
		Editor->InitEditor(EToolkitMode::Standalone, nullptr, NewWorkspace);

		WorkspaceEditor = &Editor.Get();
	};

	auto HandleExistingWorkspace = [InAsset, &WorkspaceEditor](const FAssetData& InAssetData)
	{
		if(UAnimNextWorkspace* ExistingWorkspace = Cast<UAnimNextWorkspace>(InAssetData.GetAsset()))
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(ExistingWorkspace);

			WorkspaceEditor = static_cast<FWorkspaceEditor*>(GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(ExistingWorkspace, true));
		}
	};

	if(InOpenMethod == EOpenWorkspaceMethod::AlwaysOpenNewWorkspace || RelevantWorkspaceAssets.Num() == 0)
	{
		// No relevant workspaces, so open a new one and add the asset
		HandleNewWorkspace();
	}
	else if(RelevantWorkspaceAssets.Num() == 1)
	{
		// One existing workspace, open it
		HandleExistingWorkspace(RelevantWorkspaceAssets[0]);
	}
	else
	{
		// Multiple existing workspaces, present a window to let the user choose one to open with
		TSharedRef<SWorkspacePicker> WorkspacePicker = SNew(SWorkspacePicker)
			.WorkspaceAssets(RelevantWorkspaceAssets)
			.OnAssetSelected_Lambda(HandleExistingWorkspace)
			.OnNewAsset_Lambda(HandleNewWorkspace);

		WorkspacePicker->ShowModal();
	}
	
	if(WorkspaceEditor)
	{
		WorkspaceEditor->OpenAssets({InAsset});
	}
}

void FWorkspaceEditor::OnGraphSelectionChanged(const TSet<UObject*>& NewSelection)
{
	SetSelectedObjects(NewSelection.Array());
}

void FWorkspaceEditor::RestoreEditedObjectState()
{
	for (const FEditedDocumentInfo& Document : Workspace->LastEditedDocuments)
	{
		if (UObject* Obj = Document.EditedObjectPath.ResolveObject())
		{
			if(TSharedPtr<SDockTab> DockTab = OpenDocument(Obj, FDocumentTracker::RestorePreviousDocument))
			{
				if(Obj->IsA<UEdGraph>())
				{
					TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(DockTab->GetContent());
					GraphEditor->SetViewLocation(Document.SavedViewOffset, Document.SavedZoomAmount);
				}
			}
		}
	}
}

void FWorkspaceEditor::SaveEditedObjectState()
{
	// Clear currently edited documents
	Workspace->LastEditedDocuments.Empty();

	// Ask all open documents to save their state, which will update LastEditedGraphDocuments
	DocumentManager->SaveAllState();
}

TSharedPtr<SDockTab> FWorkspaceEditor::OpenDocument(const UObject* InForObject, FDocumentTracker::EOpenDocumentCause InCause)
{
	if(InCause != FDocumentTracker::RestorePreviousDocument)
	{
		Workspace->LastEditedDocuments.AddUnique(const_cast<UObject*>(InForObject));
	}

	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(InForObject);
	return DocumentManager->OpenDocument(Payload, InCause);
}

void FWorkspaceEditor::OpenAssets(TConstArrayView<FAssetData> InAssets)
{
	for(const FAssetData& Asset : InAssets)
	{
		if(UObject* LoadedAsset = Asset.GetAsset())
		{
			OpenDocument(LoadedAsset, FDocumentTracker::EOpenDocumentCause::OpenNewDocument);

			// If its a RigVM asset, open its first graph as well
			if(UAnimNextRigVMAsset* RigVMAsset = Cast<UAnimNextRigVMAsset>(LoadedAsset))
			{
				UAnimNextRigVMAssetEditorData* EditorData = UncookedOnly::FUtils::GetEditorData(RigVMAsset);
				check(EditorData);
				EditorData->ForEachEntryOfType<IAnimNextRigVMGraphInterface>([this](IAnimNextRigVMGraphInterface* InEntry)
				{
					OpenDocument(InEntry->GetEdGraph(), FDocumentTracker::EOpenDocumentCause::OpenNewDocument);
					return false;
				});
			}
		}
	}
}

void FWorkspaceEditor::BindCommands()
{
}

void FWorkspaceEditor::ExtendMenu()
{
	
}

void FWorkspaceEditor::ExtendToolbar()
{
	
}

void FWorkspaceEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	DocumentManager->SetTabManager(InTabManager);

	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void FWorkspaceEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FWorkflowCentricApplication::UnregisterTabSpawners(InTabManager);
}

FName FWorkspaceEditor::GetToolkitFName() const
{
	return FName("AnimNextWorkspaceEditor");
}

FText FWorkspaceEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "AnimNextWorkspaceEditor");
}

FString FWorkspaceEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "AnimNextWorkspaceEditor ").ToString();
}

FLinearColor FWorkspaceEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

void FWorkspaceEditor::InitToolMenuContext(FToolMenuContext& InMenuContext)
{
}

void FWorkspaceEditor::SaveAsset_Execute()
{
	// If asset is a default 'Untitled' workspace, redirect to the 'save as' flow
	FString AssetPath = Workspace->GetOutermost()->GetPathName();
	if(AssetPath.StartsWith(TEXT("/Temp/Untitled")))
	{
		// Ensure we dont also 'save as' other externally linked assets at this point
		TGuardValue<bool> SaveWorkspaceOnly(bSavingWorkspaceOnly, true);

		SaveAssetAs_Execute();
	}
	else
	{
		FWorkflowCentricApplication::SaveAsset_Execute();
	}
}

void FWorkspaceEditor::OnGraphModified(ERigVMGraphNotifType Type, URigVMGraph* Graph, UObject* Subject)
{
	if (Type == ERigVMGraphNotifType::InteractionBracketClosed)
	{
		if (DetailsView.IsValid())
		{
			DetailsView->ForceRefresh();
		}
	}
}

void FWorkspaceEditor::OnOpenGraph(URigVMGraph* InGraph)
{
	if(IRigVMClientHost* RigVMClientHost = InGraph->GetImplementingOuter<IRigVMClientHost>())
	{
		if(UObject* EditorObject = RigVMClientHost->GetEditorObjectForRigVMGraph(InGraph))
		{
			OpenDocument(EditorObject, FDocumentTracker::EOpenDocumentCause::OpenNewDocument);
		}
	}
}

void FWorkspaceEditor::OnDeleteEntries(const TArray<UAnimNextRigVMAssetEntry*>& InEntries)
{
	if(InEntries.Num() > 0)
	{
		for(UAnimNextRigVMAssetEntry* Entry : InEntries)
		{
			if(IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry))
			{
				if(URigVMEdGraph* EdGraph = GraphInterface->GetEdGraph())
				{
					CloseDocumentTab(EdGraph);
				}
			}
		}
	}
}

void FWorkspaceEditor::SetFocusedGraphEditor(TSharedPtr<SGraphEditor> InGraphEditor)
{
	// Update the graph editor that is currently focused
	if (FocusedGraphEdPtr != InGraphEditor)
	{
		FocusedGraphEdPtr = InGraphEditor;

		SetSelectedObjects({});
	}
}

UEdGraph* FWorkspaceEditor::GetFocusedGraph() const
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

URigVMGraph* FWorkspaceEditor::GetFocusedVMGraph() const
{
	if(UEdGraph* EdGraph = GetFocusedGraph())
	{
		if(IRigVMClientHost* RigVMClientHost = EdGraph->GetImplementingOuter<IRigVMClientHost>())
		{
			return Cast<URigVMGraph>(RigVMClientHost->GetRigVMGraphForEditorObject(EdGraph));
		}
	}
	return nullptr;
}

URigVMController* FWorkspaceEditor::GetFocusedVMController() const
{
	if(UEdGraph* EdGraph = GetFocusedGraph())
	{
		if(IRigVMClientHost* RigVMClientHost = EdGraph->GetImplementingOuter<IRigVMClientHost>())
		{
			return RigVMClientHost->GetRigVMClient()->GetController(EdGraph);
		}
	}
	return nullptr;
}

void FWorkspaceEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (UEdGraphNode_Comment* CommentBeingChanged = Cast<UEdGraphNode_Comment>(NodeBeingChanged))
	{
		GetFocusedVMController()->SetCommentTextByName(CommentBeingChanged->GetFName(), NewText.ToString(), CommentBeingChanged->FontSize, CommentBeingChanged->bCommentBubbleVisible, CommentBeingChanged->bColorCommentBubble, true, true);
	}
}

void FWorkspaceEditor::CloseDocumentTab(const UObject* DocumentID)
{
	Workspace->LastEditedDocuments.Remove(const_cast<UObject*>(DocumentID));

	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(DocumentID);
	DocumentManager->CloseTab(Payload);
}

bool FWorkspaceEditor::InEditingMode() const
{
	// @TODO: disallow editing when debugging when implemented

	return true;
}

bool FWorkspaceEditor::IsEditable(UEdGraph* InGraph) const
{
	return InGraph && InEditingMode() && InGraph->bEditable;
}

FGraphPanelSelectionSet FWorkspaceEditor::GetSelectedNodes() const
{
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = FocusedGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		CurrentSelection = FocusedGraphEd->GetSelectedNodes();
	}
	return CurrentSelection;
}

void FWorkspaceEditor::DeleteSelectedNodes()
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	bool bRelinkPins = false;
	TArray<URigVMNode*> NodesToRemove;

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Node->CanUserDeleteNode())
			{
				if (const URigVMEdGraphNode* RigVMEdGraphNode = Cast<URigVMEdGraphNode>(Node))
				{
					bRelinkPins = bRelinkPins || FSlateApplication::Get().GetModifierKeys().IsShiftDown();

					if(URigVMNode* ModelNode = GetFocusedVMController()->GetGraph()->FindNodeByName(*RigVMEdGraphNode->GetModelNodePath()))
					{
						NodesToRemove.Add(ModelNode);
					}
				}
				else if (const UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node))
				{
					if(URigVMNode* ModelNode = GetFocusedVMController()->GetGraph()->FindNodeByName(CommentNode->GetFName()))
					{
						NodesToRemove.Add(ModelNode);
					}
				}
				else
				{
					Node->GetGraph()->RemoveNode(Node);
				}
			}
		}
	}

	if(NodesToRemove.IsEmpty())
	{
		return;
	}

	GetFocusedVMController()->OpenUndoBracket(TEXT("Delete selected nodes"));
	if(bRelinkPins && NodesToRemove.Num() == 1)
	{
		GetFocusedVMController()->RelinkSourceAndTargetPins(NodesToRemove[0], true);;
	}
	GetFocusedVMController()->RemoveNodes(NodesToRemove, true);
	GetFocusedVMController()->CloseUndoBracket();
}

bool FWorkspaceEditor::CanDeleteSelectedNodes()
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

void FWorkspaceEditor::SetSelectedObjects(const TArray<UObject*>& InObjects)
{
	if(DetailsView.IsValid())
	{
		DetailsView->SetObjects(InObjects);
	}
}

void FWorkspaceEditor::GetSaveableObjects(TArray<UObject*>& OutObjects) const
{
	// Base class will pick up edited object
	FWorkflowCentricApplication::GetSaveableObjects(OutObjects);

	if(!bSavingWorkspaceOnly)
	{
		for(TSoftObjectPtr<UObject>& SoftAsset : Workspace->Assets)
		{
			if(UObject* Asset = SoftAsset.Get())
			{
				// Add object referenced by workspace
				OutObjects.Add(Asset);

				// Get external objects too
				FExternalPackageHelper::GetExternalSaveableObjects(Asset, OutObjects);
			}
		}
	}
}

void FWorkspaceEditor::HandleSaveGraphState(UEdGraph* InGraph, FVector2D InViewOffset, float InZoomAmount)
{
	Workspace->LastEditedDocuments.AddUnique(FEditedDocumentInfo(InGraph, InViewOffset, InZoomAmount));
}

void FWorkspaceEditor::HandleSaveDocumentState(UObject* InObject)
{
	Workspace->LastEditedDocuments.AddUnique(FEditedDocumentInfo(InObject));
}

bool FWorkspaceEditor::OnRequestClose(EAssetEditorCloseReason InCloseReason)
{
	auto RequiresSave = [this]()
	{
		UPackage* Package = Workspace->GetOutermost();
		return Package->GetPathName().StartsWith(TEXT("/Temp/Untitled"));
	};

	// Give the user opportunity to save temp workspaces
	if(RequiresSave())
	{
		// Ensure we dont also 'save as' other externally linked assets at this point
		TGuardValue<bool> SaveWorkspaceOnly(bSavingWorkspaceOnly, true);

		SaveAssetAs_Execute();
	}

	return true;
}

void FWorkspaceEditor::OnClose()
{
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(nullptr);
		DetailsView.Reset();
	}

	FWorkflowCentricApplication::OnClose();
}

}

#undef LOCTEXT_NAMESPACE