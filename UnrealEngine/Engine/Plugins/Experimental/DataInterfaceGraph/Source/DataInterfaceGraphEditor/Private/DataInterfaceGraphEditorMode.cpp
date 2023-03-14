// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfaceGraphEditorMode.h"
#include "DataInterfaceGraphEditor.h"
#include "PropertyEditorModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Widgets/Layout/SSpacer.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "GraphEditorMode"

namespace UE::DataInterfaceGraphEditor
{

struct FGraphTabHistory : public FGenericTabHistory
{
public:
	FGraphTabHistory(TSharedPtr<FDocumentTabFactory> InFactory, TSharedPtr<FTabPayload> InPayload)
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

FGraphEditorSummoner::FGraphEditorSummoner(TSharedPtr<FWorkflowCentricApplication> InHostingApp)
	: FDocumentTabFactoryForObjects<UEdGraph>(Tabs::Document, InHostingApp)
{
}

void FGraphEditorSummoner::OnTabActivated(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	OnGraphEditorFocusedDelegate.ExecuteIfBound(GraphEditor);
}

void FGraphEditorSummoner::OnTabBackgrounded(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	OnGraphEditorBackgroundedDelegate.ExecuteIfBound(GraphEditor);
}

void FGraphEditorSummoner::OnTabRefreshed(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	GraphEditor->NotifyGraphChanged();
}

void FGraphEditorSummoner::SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());

	FVector2D ViewLocation;
	float ZoomAmount;
	GraphEditor->GetViewLocation(ViewLocation, ZoomAmount);

	UEdGraph* Graph = Payload->IsValid() ? FTabPayload_UObject::CastChecked<UEdGraph>(Payload) : nullptr;

	OnSaveGraphStateDelegate.ExecuteIfBound(Graph, ViewLocation, ZoomAmount);
}

TAttribute<FText> FGraphEditorSummoner::ConstructTabNameForObject(UEdGraph* DocumentID) const
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

TSharedRef<SWidget> FGraphEditorSummoner::CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
{
	check(Info.TabInfo.IsValid());
	
	if(OnCreateGraphEditorWidgetDelegate.IsBound())
	{
		return OnCreateGraphEditorWidgetDelegate.Execute(Info.TabInfo.ToSharedRef(), DocumentID);
	}
	else
	{
		return SNew(SSpacer);
	}
}

const FSlateBrush* FGraphEditorSummoner::GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
{
	if(OnGetTabIconDelegate.IsBound())
	{
		return OnGetTabIconDelegate.Execute(DocumentID);
	}
	else
	{
		return FAppStyle::Get().GetBrush(TEXT("GraphEditor.EventGraph_16x"));
	}
}

TSharedRef<FGenericTabHistory> FGraphEditorSummoner::CreateTabHistoryNode(TSharedPtr<FTabPayload> Payload)
{
	return MakeShared<FGraphTabHistory>(SharedThis(this), Payload);
}

class FDetailsTabSummoner : public FWorkflowTabFactory
{
public:
	FDetailsTabSummoner(TSharedPtr<FAssetEditorToolkit> InHostingApp, FOnDetailsViewCreated InOnDetailsViewCreated)
		: FWorkflowTabFactory(Tabs::Details, InHostingApp)
	{
		TabLabel = LOCTEXT("DetailsTabLabel", "Details");
		TabIcon = FSlateIcon("EditorStyle", "LevelEditor.Tabs.Details");
		ViewMenuDescription = LOCTEXT("DetailsTabMenuDescription", "Details");
		ViewMenuTooltip = LOCTEXT("DetailsTabToolTip", "Shows the details tab for selected objects.");
		bIsSingleton = true;

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.bHideSelectionTip = true;

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		
		InOnDetailsViewCreated.ExecuteIfBound(DetailsView.ToSharedRef());
	}

	TSharedPtr<IDetailsView> GetDetailsView() const
	{
		return DetailsView;
	}

private:
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override
	{
		return DetailsView.ToSharedRef();
	}
	
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override
	{
		return ViewMenuTooltip;
	}
	
	TSharedPtr<IDetailsView> DetailsView;
};

FGraphEditorMode::FGraphEditorMode(TSharedRef<FWorkflowCentricApplication> InHostingApp)
	: FApplicationMode(Modes::GraphEditor)
	, HostingAppPtr(InHostingApp)
{
	HostingAppPtr = InHostingApp;

	TSharedRef<FGraphEditor> GraphEditor = StaticCastSharedRef<FGraphEditor>(InHostingApp);
	
	TabFactories.RegisterFactory(MakeShared<FDetailsTabSummoner>(GraphEditor, FOnDetailsViewCreated::CreateSP(&GraphEditor.Get(), &FGraphEditor::HandleDetailsViewCreated)));

	TabLayout = FTabManager::NewLayout("Standalone_DataInterfaceGraphEditor_Layout_v1.0")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(	
				FTabManager::NewSplitter()
				->SetSizeCoefficient(1.0f)
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.8f)
					->SetHideTabWell(false)
					->AddTab(Tabs::Document, ETabState::ClosedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->SetHideTabWell(false)
					->AddTab(Tabs::Details, ETabState::OpenedTab)
				)
			)
		);

	GraphEditor->RegisterModeToolbarIfUnregistered(GetModeName());
}

void FGraphEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FWorkflowCentricApplication> HostingApp = HostingAppPtr.Pin();
	HostingApp->PushTabFactories(TabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FGraphEditorMode::AddTabFactory(FCreateWorkflowTabFactory FactoryCreator)
{
	if (FactoryCreator.IsBound())
	{
		TabFactories.RegisterFactory(FactoryCreator.Execute(HostingAppPtr.Pin()));
	}
}

void FGraphEditorMode::RemoveTabFactory(FName TabFactoryID)
{
	TabFactories.UnregisterFactory(TabFactoryID);
}

}

#undef LOCTEXT_NAMESPACE