// Copyright Epic Games, Inc. All Rights Reserved.

#include "GraphDocumentSummoner.h"

#include "AnimNextWorkspaceEditor.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSpacer.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraph/RigVMEdGraph.h"
#include "Framework/Commands/GenericCommands.h"

#define LOCTEXT_NAMESPACE "AnimNextGraphDocumentSummoner"

namespace UE::AnimNext::Editor
{

FGraphDocumentSummoner::FGraphDocumentSummoner(FName InIdentifier, TSharedPtr<FWorkspaceEditor> InHostingApp)
	: FDocumentTabFactoryForObjects<UEdGraph>(InIdentifier, InHostingApp)
	, HostingAppPtr(InHostingApp)
{
	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction(FGenericCommands::Get().Delete,
			FExecuteAction::CreateSP(HostingAppPtr.Pin().Get(), &FWorkspaceEditor::DeleteSelectedNodes),
			FCanExecuteAction::CreateSP(HostingAppPtr.Pin().Get(), &FWorkspaceEditor::CanDeleteSelectedNodes));
}

void FGraphDocumentSummoner::OnTabActivated(TSharedPtr<SDockTab> Tab) const
{
	if(TSharedPtr<FWorkspaceEditor> WorkspaceEditor = HostingAppPtr.Pin())
	{
		WorkspaceEditor->SetFocusedGraphEditor(StaticCastSharedRef<SGraphEditor>(Tab->GetContent()));
	}
}

void FGraphDocumentSummoner::OnTabBackgrounded(TSharedPtr<SDockTab> Tab) const
{
	if(TSharedPtr<FWorkspaceEditor> WorkspaceEditor = HostingAppPtr.Pin())
	{
		WorkspaceEditor->SetFocusedGraphEditor(nullptr);
	}
}

void FGraphDocumentSummoner::OnTabRefreshed(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	GraphEditor->NotifyGraphChanged();
}

void FGraphDocumentSummoner::SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());

	FVector2D ViewLocation;
	float ZoomAmount;
	GraphEditor->GetViewLocation(ViewLocation, ZoomAmount);

	UEdGraph* Graph = Payload->IsValid() ? FTabPayload_UObject::CastChecked<UEdGraph>(Payload) : nullptr;

	OnSaveGraphStateDelegate.ExecuteIfBound(Graph, ViewLocation, ZoomAmount);
}

TAttribute<FText> FGraphDocumentSummoner::ConstructTabNameForObject(UEdGraph* DocumentID) const
{
	if (DocumentID)
	{
		if (const UEdGraphSchema* Schema = DocumentID->GetSchema())
		{
			FGraphDisplayInfo Info;
			Schema->GetGraphDisplayInformation(*DocumentID, /*out*/ Info);
			return Info.DisplayName;
		}
		else
		{
			// if we don't have a schema, we're dealing with a malformed (or incomplete graph)...
			// possibly in the midst of some transaction - here we return the object's outer path 
			// so we can at least get some context as to which graph we're referring
			return FText::FromString(DocumentID->GetPathName());
		}
	}

	return LOCTEXT("UnknownGraphName", "Unknown");
}

TSharedRef<SWidget> FGraphDocumentSummoner::CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
{
	SGraphEditor::FGraphEditorEvents Events;
	Events.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &FGraphDocumentSummoner::OnCreateGraphActionMenu);
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FGraphDocumentSummoner::OnGraphSelectionChanged, TWeakObjectPtr<UEdGraph>(DocumentID));
	
	return SNew(SGraphEditor)
		.AdditionalCommands(CommandList)
		.IsEditable(HostingAppPtr.Pin().Get(), &FWorkspaceEditor::IsEditable, DocumentID)
		.GraphToEdit(DocumentID)
		.GraphEvents(Events)
		.AssetEditorToolkit(HostingAppPtr);
}

void FGraphDocumentSummoner::OnGraphSelectionChanged(const TSet<UObject*>& NewSelection, TWeakObjectPtr<UEdGraph> InGraph) const
{
	URigVMEdGraph* RigVMEdGraph = Cast<URigVMEdGraph>(InGraph.Get());
	if (RigVMEdGraph == nullptr)
	{
		return;
	}

	if (RigVMEdGraph->bIsSelecting || GIsTransacting)
	{
		return;
	}

	TGuardValue<bool> SelectGuard(RigVMEdGraph->bIsSelecting, true);

	TArray<FName> NodeNamesToSelect;
	for (UObject* Object : NewSelection)
	{
		if (URigVMEdGraphNode* RigVMEdGraphNode = Cast<URigVMEdGraphNode>(Object))
		{
			NodeNamesToSelect.Add(RigVMEdGraphNode->GetModelNodeName());
		}
		else if(UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
		{
			NodeNamesToSelect.Add(Node->GetFName());
		}
	}
	RigVMEdGraph->GetController()->SetNodeSelection(NodeNamesToSelect, true, true);

	HostingAppPtr.Pin()->OnGraphSelectionChanged(NewSelection);
}

bool FGraphDocumentSummoner::IsPayloadSupported(TSharedRef<FTabPayload> Payload) const
{
	UObject* Object = Payload->IsValid() ? FTabPayload_UObject::CastChecked<UObject>(Payload) : nullptr;
	return Object && Object->IsA<UEdGraph>();
}

const FSlateBrush* FGraphDocumentSummoner::GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
{
	return FAppStyle::Get().GetBrush(TEXT("GraphEditor.EventGraph_16x"));
}

TSharedRef<FGenericTabHistory> FGraphDocumentSummoner::CreateTabHistoryNode(TSharedPtr<FTabPayload> Payload)
{
	struct FLocalGraphTabHistory : public FGenericTabHistory
	{
	public:
		FLocalGraphTabHistory(TSharedPtr<FDocumentTabFactory> InFactory, TSharedPtr<FTabPayload> InPayload)
			: FGenericTabHistory(InFactory, InPayload)
			, SavedLocation(FVector2D::ZeroVector)
			, SavedZoomAmount(INDEX_NONE)
		{
		}

	private:
		// FGenericTabHistory interface
		virtual void EvokeHistory(TSharedPtr<::FTabInfo> InTabInfo, bool bPrevTabMatches) override
		{
			FWorkflowTabSpawnInfo SpawnInfo;
			SpawnInfo.Payload = Payload;
			SpawnInfo.TabInfo = InTabInfo;

			if(bPrevTabMatches)
			{
				TSharedPtr<SDockTab> DockTab = InTabInfo->GetTab().Pin();
				GraphEditor = StaticCastSharedRef<SGraphEditor>(DockTab->GetContent());
			}
			else
			{
				TSharedRef<SGraphEditor> GraphEditorRef = StaticCastSharedRef<SGraphEditor>(FactoryPtr.Pin()->CreateTabBody(SpawnInfo));
				GraphEditor = GraphEditorRef;
				FactoryPtr.Pin()->UpdateTab(InTabInfo->GetTab().Pin(), SpawnInfo, GraphEditorRef);
			}
		}
	
		virtual void SaveHistory() override
		{
			if (IsHistoryValid())
			{
				check(GraphEditor.IsValid());
				GraphEditor.Pin()->GetViewLocation(SavedLocation, SavedZoomAmount);
				GraphEditor.Pin()->GetViewBookmark(SavedBookmarkId);
			}
		}
	
		virtual void RestoreHistory() override
		{
			if (IsHistoryValid())
			{
				check(GraphEditor.IsValid());
				GraphEditor.Pin()->SetViewLocation(SavedLocation, SavedZoomAmount, SavedBookmarkId);
			}
		}
	
		/** The graph editor represented by this history node. While this node is inactive, the graph editor is invalid */
		TWeakPtr<SGraphEditor> GraphEditor;
	
		/** Saved location the graph editor was at when this history node was last visited */
		FVector2D SavedLocation;
	
		/** Saved zoom the graph editor was at when this history node was last visited */
		float SavedZoomAmount;
	
		/** Saved bookmark ID the graph editor was at when this history node was last visited */
		FGuid SavedBookmarkId;
	};

	return MakeShared<FLocalGraphTabHistory>(SharedThis(this), Payload);
}

}

#undef LOCTEXT_NAMESPACE