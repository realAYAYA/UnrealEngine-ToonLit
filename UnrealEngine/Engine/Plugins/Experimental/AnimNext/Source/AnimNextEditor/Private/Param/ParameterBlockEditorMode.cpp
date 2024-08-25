// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParameterBlockEditorMode.h"
#include "AnimNextParameterBlockEditor.h"
#include "AnimNextRigVMAssetEntry.h"
#include "IAnimNextRigVMGraphInterface.h"
#include "PropertyEditorModule.h"
#include "Common/SRigVMAssetView.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Modules/ModuleManager.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "RigVMModel/RigVMGraph.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "ParameterBlockEditorMode"

namespace UE::AnimNext::Editor
{

struct FParameterBlockTabHistory : public FGenericTabHistory
{
public:
	FParameterBlockTabHistory(TSharedPtr<FDocumentTabFactory> InFactory, TSharedPtr<FTabPayload> InPayload)
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

FParameterBlockEditorDocumentSummoner::FParameterBlockEditorDocumentSummoner(TSharedPtr<FWorkflowCentricApplication> InHostingApp)
	: FDocumentTabFactoryForObjects<UEdGraph>(ParameterBlockTabs::Document, InHostingApp)
{
}

void FParameterBlockEditorDocumentSummoner::OnTabActivated(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	OnGraphEditorFocusedDelegate.ExecuteIfBound(GraphEditor);
}

void FParameterBlockEditorDocumentSummoner::OnTabBackgrounded(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	OnGraphEditorBackgroundedDelegate.ExecuteIfBound(GraphEditor);
}

void FParameterBlockEditorDocumentSummoner::OnTabRefreshed(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	GraphEditor->NotifyGraphChanged();
}

void FParameterBlockEditorDocumentSummoner::SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());

	FVector2D ViewLocation;
	float ZoomAmount;
	GraphEditor->GetViewLocation(ViewLocation, ZoomAmount);

	UEdGraph* Graph = Payload->IsValid() ? FTabPayload_UObject::CastChecked<UEdGraph>(Payload) : nullptr;

	OnSaveGraphStateDelegate.ExecuteIfBound(Graph, ViewLocation, ZoomAmount);
}

TAttribute<FText> FParameterBlockEditorDocumentSummoner::ConstructTabNameForObject(UEdGraph* DocumentID) const
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

TSharedRef<SWidget> FParameterBlockEditorDocumentSummoner::CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
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

const FSlateBrush* FParameterBlockEditorDocumentSummoner::GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
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

TSharedRef<FGenericTabHistory> FParameterBlockEditorDocumentSummoner::CreateTabHistoryNode(TSharedPtr<FTabPayload> Payload)
{
	return MakeShared<FParameterBlockTabHistory>(SharedThis(this), Payload);
}

class FParameterBlockDetailsTabSummoner : public FWorkflowTabFactory
{
public:
	FParameterBlockDetailsTabSummoner(TSharedPtr<FParameterBlockEditor> InHostingApp, FOnDetailsViewCreated InOnDetailsViewCreated)
		: FWorkflowTabFactory(ParameterBlockTabs::Details, InHostingApp)
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

class FParameterBlockTabSummoner : public FWorkflowTabFactory
{
public:
	FParameterBlockTabSummoner(TSharedPtr<FParameterBlockEditor> InHostingApp)
		: FWorkflowTabFactory(ParameterBlockTabs::Parameters, InHostingApp)
	{
		TabLabel = LOCTEXT("ParameterBlockTabLabel", "Parameter Block");
		TabIcon = FSlateIcon("EditorStyle", "LevelEditor.Tabs.Details");
		ViewMenuDescription = LOCTEXT("ParameterBlockTabMenuDescription", "Parameter Block");
		ViewMenuTooltip = LOCTEXT("ParameterBlockTabToolTip", "Shows all parameters in this parameter block.");
		bIsSingleton = true;
	}

private:
	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override
	{
		return SNew(SRigVMAssetView, StaticCastSharedPtr<FParameterBlockEditor>(HostingApp.Pin())->EditorData)
			.OnSelectionChanged(this, &FParameterBlockTabSummoner::HandleSelectionChanged)
			.OnOpenGraph(this, &FParameterBlockTabSummoner::HandleOpenGraph)
			.OnDeleteEntries(this, &FParameterBlockTabSummoner::HandleDeleteEntries);
	}
	
	virtual FText GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const override
	{
		return ViewMenuTooltip;
	}

	void HandleSelectionChanged(const TArray<UObject*>& InObjects) const
	{
		TSharedPtr<FParameterBlockEditor> ParametersEditor = StaticCastSharedPtr<FParameterBlockEditor>(HostingApp.Pin());
		ParametersEditor->SetSelectedObjects(InObjects);
	}

	void HandleOpenGraph(URigVMGraph* InGraph) const
	{
		TSharedPtr<FParameterBlockEditor> ParameterBlockEditor = StaticCastSharedPtr<FParameterBlockEditor>(HostingApp.Pin());
		UAnimNextParameterBlock_EditorData* EditorData = StaticCastSharedPtr<FParameterBlockEditor>(HostingApp.Pin())->EditorData;
		if(UObject* EditorObject = EditorData->GetEditorObjectForRigVMGraph(InGraph))
		{
			ParameterBlockEditor->OpenDocument(EditorObject, FDocumentTracker::EOpenDocumentCause::OpenNewDocument);
		}
	}

	void HandleDeleteEntries(const TArray<UAnimNextRigVMAssetEntry*>& InEntries) const
	{
		TSharedPtr<FParameterBlockEditor> ParameterBlockEditor = StaticCastSharedPtr<FParameterBlockEditor>(HostingApp.Pin());
		UAnimNextParameterBlock_EditorData* EditorData = StaticCastSharedPtr<FParameterBlockEditor>(HostingApp.Pin())->EditorData;

		for(UAnimNextRigVMAssetEntry* Entry : InEntries)
		{
			if(IAnimNextRigVMGraphInterface* GraphInterface = Cast<IAnimNextRigVMGraphInterface>(Entry))
			{
				if(URigVMEdGraph* EdGraph = GraphInterface->GetEdGraph())
				{
					ParameterBlockEditor->CloseDocumentTab(EdGraph);
				}
			}
		}
	}
};

FParameterBlockEditorMode::FParameterBlockEditorMode(TSharedRef<FParameterBlockEditor> InHostingApp)
	: FApplicationMode(ParameterBlockModes::ParameterBlockEditor)
	, ParameterBlockEditorPtr(InHostingApp)
{
	TSharedRef<FParameterBlockEditor> ParametersEditor = StaticCastSharedRef<FParameterBlockEditor>(InHostingApp);
	
	TabFactories.RegisterFactory(MakeShared<FParameterBlockDetailsTabSummoner>(ParametersEditor, FOnDetailsViewCreated::CreateSP(&ParametersEditor.Get(), &FParameterBlockEditor::HandleDetailsViewCreated)));
	TabFactories.RegisterFactory(MakeShared<FParameterBlockTabSummoner>(ParametersEditor));

	TabLayout = FTabManager::NewLayout("Standalone_AnimNextParameterBlockEditor_Layout_v1.0")
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
					->SetSizeCoefficient(0.25f)
					->SetHideTabWell(false)
					->AddTab(ParameterBlockTabs::Parameters, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->SetHideTabWell(false)
					->AddTab(ParameterBlockTabs::Document, ETabState::ClosedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.25f)
					->SetHideTabWell(false)
					->AddTab(ParameterBlockTabs::Details, ETabState::OpenedTab)
				)
			)
		);

	ParametersEditor->RegisterModeToolbarIfUnregistered(GetModeName());
}

void FParameterBlockEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FParameterBlockEditor> HostingApp = ParameterBlockEditorPtr.Pin();
	HostingApp->PushTabFactories(TabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FParameterBlockEditorMode::AddTabFactory(FCreateWorkflowTabFactory FactoryCreator)
{
	if (FactoryCreator.IsBound())
	{
		TabFactories.RegisterFactory(FactoryCreator.Execute(ParameterBlockEditorPtr.Pin()));
	}
}

void FParameterBlockEditorMode::RemoveTabFactory(FName TabFactoryID)
{
	TabFactories.UnregisterFactory(TabFactoryID);
}

void FParameterBlockEditorMode::PreDeactivateMode()
{
	FApplicationMode::PreDeactivateMode();

	TSharedPtr<FParameterBlockEditor> ParameterBlockEditor = ParameterBlockEditorPtr.Pin();

	ParameterBlockEditor->SaveEditedObjectState();
}

void FParameterBlockEditorMode::PostActivateMode()
{
	// Reopen any documents that were open when the blueprint was last saved
	TSharedPtr<FParameterBlockEditor> ParameterBlockEditor = ParameterBlockEditorPtr.Pin();
	ParameterBlockEditor->RestoreEditedObjectState();

	FApplicationMode::PostActivateMode();
}


}

#undef LOCTEXT_NAMESPACE