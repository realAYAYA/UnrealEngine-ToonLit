// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphAssetToolkit.h"

#include "Customizations/Graph/GraphMemberCustomization.h"
#include "Customizations/Graph/GraphNodeCustomization.h"
#include "Framework/Commands/GenericCommands.h"
#include "Graph/MovieGraphConfig.h"
#include "MovieEdGraphNode.h"
#include "MovieGraphSchema.h"
#include "PropertyEditorModule.h"
#include "SMovieGraphActiveRenderSettingsTabContent.h"
#include "SMovieGraphMembersTabContent.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Graph/SMovieGraphConfigPanel.h"

#define LOCTEXT_NAMESPACE "MovieGraphAssetToolkit"

const FName FMovieGraphAssetToolkit::AppIdentifier(TEXT("MovieGraphAssetEditorApp"));
const FName FMovieGraphAssetToolkit::GraphTabId(TEXT("MovieGraphAssetToolkit"));
const FName FMovieGraphAssetToolkit::DetailsTabId(TEXT("MovieGraphAssetToolkitDetails"));
const FName FMovieGraphAssetToolkit::MembersTabId(TEXT("MovieGraphAssetToolkitMembers"));
const FName FMovieGraphAssetToolkit::ActiveRenderSettingsTabId(TEXT("MovieGraphAssetToolkitActiveRenderSettings"));

// Temporary cvar to enable/disable upgrading to a graph-based configuration
static TAutoConsoleVariable<bool> CVarMoviePipelineEnableRenderGraph(
	TEXT("MoviePipeline.EnableRenderGraph"),
	false,
	TEXT("Determines if the Render Graph feature is enabled in the UI. This is a highly experimental feature and is not ready for use.")
);

FMovieGraphAssetToolkit::FMovieGraphAssetToolkit()
	: bIsInternalSelectionChange(false)
{
}

void FMovieGraphAssetToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceMenu_MovieGraphAssetToolkit", "Render Graph Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(GraphTabId, FOnSpawnTab::CreateSP(this, &FMovieGraphAssetToolkit::SpawnTab_RenderGraphEditor))
		.SetDisplayName(LOCTEXT("RenderGraphTab", "Render Graph"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FMovieGraphAssetToolkit::SpawnTab_RenderGraphDetails))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(MembersTabId, FOnSpawnTab::CreateSP(this, &FMovieGraphAssetToolkit::SpawnTab_RenderGraphMembers))
		.SetDisplayName(LOCTEXT("MembersTab", "Members"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(ActiveRenderSettingsTabId, FOnSpawnTab::CreateSP(this, &FMovieGraphAssetToolkit::SpawnTab_RenderGraphActiveRenderSettings))
		.SetDisplayName(LOCTEXT("ActiveRenderSettingsTab", "Active Render Settings"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Debug"));
}

void FMovieGraphAssetToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	
	InTabManager->UnregisterTabSpawner(GraphTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(MembersTabId);
	InTabManager->UnregisterTabSpawner(ActiveRenderSettingsTabId);
}

void FMovieGraphAssetToolkit::InitMovieGraphAssetToolkit(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UMovieGraphConfig* InitGraph)
{
	InitialGraph = InitGraph;
	
	// Note: Changes to the layout should include a increment to the layout's ID, i.e.
	// MoviePipelineRenderGraphEditor[X] -> MoviePipelineRenderGraphEditor[X+1]. Otherwise, layouts may be messed up
	// without a full reset to layout defaults inside the editor.
	const FName LayoutString = FName("MoviePipelineRenderGraphEditor2");

	// Override the Default Layout provided by FBaseAssetToolkit to hide the viewport and details panel tabs.
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(FName(LayoutString))
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(1.f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->AddTab(MembersTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->SetHideTabWell(true)
					->AddTab(GraphTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.2f)
					->Split
					(
					FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->AddTab(DetailsTabId, ETabState::OpenedTab)
					)
					->Split
					(
					FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->AddTab(ActiveRenderSettingsTabId, ETabState::OpenedTab)
					)
				)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, AppIdentifier, Layout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InitGraph);

	BindGraphCommands();
}

TSharedRef<SDockTab> FMovieGraphAssetToolkit::SpawnTab_RenderGraphEditor(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> NewDockTab = SNew(SDockTab)
		.TabColorScale(GetTabColorScale())
		.Label(LOCTEXT("GraphTab_Title", "Graph"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SAssignNew(MovieGraphWidget, SMoviePipelineGraphPanel)
				.Graph(InitialGraph)
				.OnGraphSelectionChanged_Lambda([this](const TArray<UObject*>& NewSelection) -> void
				{
					if (bIsInternalSelectionChange)
					{
						return;
					}
					
					// Reset selection in the Members panel
					if (MembersTabContent.IsValid())
					{
						TGuardValue<bool> GuardSelectionChange(bIsInternalSelectionChange, true);
						MembersTabContent->ClearSelection();
					}

					if (SelectedGraphObjectsDetailsWidget.IsValid())
					{
						SelectedGraphObjectsDetailsWidget->SetObjects(NewSelection);
					}
				})
			]
		];

	return NewDockTab;
}

TSharedRef<SDockTab> FMovieGraphAssetToolkit::SpawnTab_RenderGraphMembers(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> NewDockTab = SNew(SDockTab)
		.TabColorScale(GetTabColorScale())
		.Label(LOCTEXT("MembersTab_Title", "Members"))
		[
			SAssignNew(MembersTabContent, SMovieGraphMembersTabContent)
			.Editor(SharedThis(this))
			.Graph(InitialGraph)
			.OnActionSelected_Lambda([this](const TArray<TSharedPtr<FEdGraphSchemaAction>>& Selection, ESelectInfo::Type Type)
			{
				if (bIsInternalSelectionChange)
				{
					return;
				}
				
				// Reset selection in the graph
				if (MovieGraphWidget.IsValid())
				{
					TGuardValue<bool> GuardSelectionChange(bIsInternalSelectionChange, true);
					MovieGraphWidget->ClearGraphSelection();
				}
				
				TArray<UObject*> SelectedObjects;
				for (const TSharedPtr<FEdGraphSchemaAction>& SelectedAction : Selection)
				{
					const FMovieGraphSchemaAction* GraphAction = static_cast<FMovieGraphSchemaAction*>(SelectedAction.Get());
					if (GraphAction->ActionTarget)
					{
						SelectedObjects.Add(GraphAction->ActionTarget);
					}
				}

				if (SelectedGraphObjectsDetailsWidget.IsValid())
				{
					SelectedGraphObjectsDetailsWidget->SetObjects(SelectedObjects);
				}
			})
		];

	return NewDockTab;
}

TSharedRef<SDockTab> FMovieGraphAssetToolkit::SpawnTab_RenderGraphDetails(const FSpawnTabArgs& Args)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.ViewIdentifier = "MovieGraphSettings";

	SelectedGraphObjectsDetailsWidget = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	SelectedGraphObjectsDetailsWidget->RegisterInstancedCustomPropertyLayout(
		UMovieGraphMember::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FGraphMemberCustomization::MakeInstance));

	SelectedGraphObjectsDetailsWidget->RegisterInstancedCustomPropertyLayout(
		UMovieGraphNode::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FGraphNodeCustomization::MakeInstance));
	
	return SNew(SDockTab)
		.TabColorScale(GetTabColorScale())
		.Label(LOCTEXT("DetailsTab_Title", "Details"))
		[
			SelectedGraphObjectsDetailsWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FMovieGraphAssetToolkit::SpawnTab_RenderGraphActiveRenderSettings(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabColorScale(GetTabColorScale())
		.Label(LOCTEXT("ActiveRenderSettings_Title", "Active Render Settings"))
		[
			SAssignNew(ActiveRenderSettingsTabContent, SMovieGraphActiveRenderSettingsTabContent)
			.Graph(InitialGraph)
		];
}

void FMovieGraphAssetToolkit::BindGraphCommands()
{
	ToolkitCommands->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &FMovieGraphAssetToolkit::DeleteSelectedMembers),
		FCanExecuteAction::CreateSP(this, &FMovieGraphAssetToolkit::CanDeleteSelectedMembers));
}

void FMovieGraphAssetToolkit::DeleteSelectedMembers()
{
	if (MembersTabContent.IsValid())
	{
		MembersTabContent->DeleteSelectedMembers();
	}
}

bool FMovieGraphAssetToolkit::CanDeleteSelectedMembers()
{
	if (MembersTabContent.IsValid())
	{
		return MembersTabContent->CanDeleteSelectedMembers();
	}

	return false;
}

void FMovieGraphAssetToolkit::PersistEditorOnlyNodes() const
{
	if (InitialGraph && InitialGraph->PipelineEdGraph)
	{
		TArray<TObjectPtr<const UObject>> EditorOnlyNodes;
		for (const TObjectPtr<UEdGraphNode>& GraphEdNode : InitialGraph->PipelineEdGraph->Nodes)
		{
			// Treat any non-MRQ nodes as editor-only nodes
			check(GraphEdNode);
			if (!GraphEdNode->IsA<UMoviePipelineEdGraphNodeBase>())
			{
				EditorOnlyNodes.Add(GraphEdNode);
			}
		}

		InitialGraph->SetEditorOnlyNodes(EditorOnlyNodes);
	}
}

FName FMovieGraphAssetToolkit::GetToolkitFName() const
{
	return FName("MovieGraphEditor");
}

FText FMovieGraphAssetToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("MovieGraphAppLabel", "Movie Graph Editor");
}

FString FMovieGraphAssetToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("MovieGraphEditor");
}

FLinearColor FMovieGraphAssetToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

void FMovieGraphAssetToolkit::SaveAsset_Execute()
{
	// Editor-only nodes are copied to the underlying runtime graph on save/close
	PersistEditorOnlyNodes();
	
	// Perform the default save process
	// TODO: This will fail silently on a transient graph and won't trigger a Save As
	FAssetEditorToolkit::SaveAsset_Execute();

	// TODO: Any custom save logic here
}

void FMovieGraphAssetToolkit::OnClose()
{
	// Editor-only nodes are copied to the underlying runtime graph on save/close
	PersistEditorOnlyNodes();
	
	FAssetEditorToolkit::OnClose();
}

#undef LOCTEXT_NAMESPACE
