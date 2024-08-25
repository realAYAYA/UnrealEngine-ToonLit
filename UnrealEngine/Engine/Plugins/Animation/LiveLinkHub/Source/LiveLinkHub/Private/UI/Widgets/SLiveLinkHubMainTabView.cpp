// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLiveLinkHubMainTabView.h"

#include "Clients/LiveLinkHubClientsController.h"
#include "Features/IModularFeatures.h"
#include "IDetailsView.h"
#include "ILiveLinkClient.h"
#include "LiveLinkClientPanelViews.h"
#include "LiveLinkClientPanelToolbar.h"
#include "LiveLinkHub.h"
#include "LiveLinkHubModule.h"
#include "LiveLinkPanelController.h"
#include "LiveLinkTypes.h"
#include "Modules/ModuleManager.h"
#include "Recording/LiveLinkHubRecordingListController.h"
#include "Subjects/LiveLinkHubSubjectController.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "LiveLinkHub.SLiveLinkHubMainTabView"

const FName SLiveLinkHubMainTabView::SourcesTabId("SourcesTabId");
const FName SLiveLinkHubMainTabView::SourceDetailsTabId("SourceDetailsTabId");
const FName SLiveLinkHubMainTabView::SubjectsTabId("SubjectsTabId");
const FName SLiveLinkHubMainTabView::PlaybackTabId("PlaybackTabId");
const FName SLiveLinkHubMainTabView::ClientsTabId("ClientsTabId");
const FName SLiveLinkHubMainTabView::ClientDetailsTabId("ClientDetailsTabId");

const FText SLiveLinkHubMainTabView::SourcesTabName = LOCTEXT("SourcesTabLabel", "Sources");
const FText SLiveLinkHubMainTabView::SourceDetailsTabName = LOCTEXT("SourceDetailsTabLabel", "Source Details");
const FText SLiveLinkHubMainTabView::SubjectsTabName = LOCTEXT("SubjectsTabLabel", "Subjects");
const FText SLiveLinkHubMainTabView::PlaybackTabName = LOCTEXT("PlaybackTabLabel", "Playback");
const FText SLiveLinkHubMainTabView::ClientsTabName = LOCTEXT("ClientsTabLabel", "Clients");
const FText SLiveLinkHubMainTabView::ClientDetailsTabName = LOCTEXT("ClientDetailsTabLabel", "Client Details");

void SLiveLinkHubMainTabView::Construct(const FArguments& InArgs)
{
	PanelController = MakeShared<FLiveLinkPanelController>();
	PanelController->OnSubjectSelectionChanged().AddSP(this, &SLiveLinkHubMainTabView::OnSubjectSelectionChanged);

	SLiveLinkHubTabViewWithManagerBase::Construct(
		SLiveLinkHubTabViewWithManagerBase::FArguments()
		.ConstructUnderWindow(InArgs._ConstructUnderWindow)
		.ConstructUnderMajorTab(InArgs._ConstructUnderMajorTab)
		.CreateTabs(FCreateTabs::CreateLambda([this, &InArgs](const TSharedRef<FTabManager>& InTabManager, const TSharedRef<FTabManager::FLayout>& InLayout)
		{
			CreateTabs(InTabManager, InLayout, InArgs);
		}))
		.LayoutName("LiveLinkHubSourcesTabView_v1.2")
	);
}

SLiveLinkHubMainTabView::~SLiveLinkHubMainTabView()
{
	if (PanelController)
	{
		PanelController->OnSubjectSelectionChanged().RemoveAll(this);
	}
}

void SLiveLinkHubMainTabView::CreateTabs(const TSharedRef<FTabManager>& InTabManager, const TSharedRef<FTabManager::FLayout>& InLayout, const FArguments& InArgs)
{
	static const FName LiveLinkStyleName = "LiveLinkStyle";

	InTabManager->RegisterTabSpawner(SourcesTabId, FOnSpawnTab::CreateSP(this, &SLiveLinkHubMainTabView::SpawnSourcesTab))
		.SetDisplayName(SourcesTabName);

	/* Disabled until client details are used in the hub.
	InTabManager->RegisterTabSpawner(SourceDetailsTabId, FOnSpawnTab::CreateSP(this, &SLiveLinkHubMainTabView::SpawnSourcesDetailsTab))
		.SetDisplayName(SourceDetailsTabName);
	*/

	InTabManager->RegisterTabSpawner(SubjectsTabId, FOnSpawnTab::CreateSP(this, &SLiveLinkHubMainTabView::SpawnSubjectsTab))
		.SetIcon(FSlateIcon(LiveLinkStyleName, TEXT("LiveLinkHub.Subjects.Icon")))
		.SetDisplayName(SubjectsTabName);
	InTabManager->RegisterTabSpawner(PlaybackTabId, FOnSpawnTab::CreateSP(this, &SLiveLinkHubMainTabView::SpawnPlaybackTab))
		.SetIcon(FSlateIcon(LiveLinkStyleName, TEXT("LiveLinkHub.Playback.Icon")))
		.SetDisplayName(PlaybackTabName);
	InTabManager->RegisterTabSpawner(ClientsTabId, FOnSpawnTab::CreateSP(this, &SLiveLinkHubMainTabView::SpawnClientsTab))
		.SetDisplayName(ClientsTabName);
	InTabManager->RegisterTabSpawner(ClientDetailsTabId, FOnSpawnTab::CreateSP(this, &SLiveLinkHubMainTabView::SpawnClientDetailsTab))
		.SetDisplayName(ClientDetailsTabName);

	InLayout->AddArea
		(
			FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewSplitter()
					->SetSizeCoefficient(1.f)
					->SetOrientation(Orient_Horizontal)
					->Split
					(
						FTabManager::NewSplitter()
						->SetSizeCoefficient(0.25f)
						->SetOrientation(Orient_Vertical)
						->Split
						(
							FTabManager::NewStack()
							->SetSizeCoefficient(0.5f)
							->SetHideTabWell(true)
							->AddTab(SourcesTabId, ETabState::OpenedTab)
						)
					)
					->Split
					(
						FTabManager::NewSplitter()
						->SetSizeCoefficient(0.25f)
						->SetOrientation(Orient_Vertical)
						->Split
						(
							FTabManager::NewStack()
							->SetSizeCoefficient(0.5f)
							->AddTab(SubjectsTabId, ETabState::OpenedTab)
							->AddTab(PlaybackTabId, ETabState::OpenedTab)
							->SetForegroundTab(SubjectsTabId)
						)
					)
					->Split
					(
						FTabManager::NewSplitter()
						->SetSizeCoefficient(0.25f)
						->SetOrientation(Orient_Vertical)
						->Split
						(
							FTabManager::NewStack()
							->SetSizeCoefficient(0.5f)
							->SetHideTabWell(true)
							->AddTab(ClientsTabId, ETabState::OpenedTab)
						)
						->Split
						(
							FTabManager::NewStack()
							->SetSizeCoefficient(0.5f)
							->SetHideTabWell(true)
							->AddTab(ClientDetailsTabId, ETabState::OpenedTab)
						)
					)
				)
		);
}

TSharedRef<SDockTab> SLiveLinkHubMainTabView::SpawnSourcesTab(const FSpawnTabArgs& InTabArgs)
{
	FLiveLinkClient* Client = (FLiveLinkClient*)&IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");

	TSharedRef<SWidget> CustomToolbarHeader = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 5.f, 0.f, 5.f)
		[
			SNew(SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(FSlateIcon("LiveLinkStyle", "LiveLinkHub.Sources.Icon").GetIcon())
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.Padding(FMargin(4.0, 2.0))
		[
			SNew(STextBlock)
			.Font( DEFAULT_FONT( "Regular", 14 ) )
			.Text(LOCTEXT("SourcesHeaderText", "Sources"))
		];
		
	
	return SNew(SDockTab)
		.TabRole(PanelTab)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(FMargin(4.0f, 0.f,0.f,0.f))
			.AutoHeight()
			[
				SNew(SLiveLinkClientPanelToolbar, Client)
				.SourceButtonAlignment(HAlign_Right)
				.ParentWindow(LiveLinkHubModule.GetLiveLinkHub()->GetRootWindow())
				.ShowPresetPicker(false)
				.ShowSettings(false)
				.CustomHeader(CustomToolbarHeader)
				.IsEnabled_Lambda([this]()
				{
					const FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");
					return !LiveLinkHubModule.GetPlaybackController()->IsInPlayback();
				})
			]
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				PanelController->SourcesView->SourcesListView.ToSharedRef()
			]
		];
}


TSharedRef<SDockTab> SLiveLinkHubMainTabView::SpawnSourcesDetailsTab(const FSpawnTabArgs& InTabArgs)
{
	return SNew(SDockTab)
		.Label(SourceDetailsTabName)
		.TabRole(PanelTab)
		[
			PanelController->SourcesDetailsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> SLiveLinkHubMainTabView::SpawnSubjectsTab(const FSpawnTabArgs& InTabArgs)
{
	FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");

	return SNew(SDockTab)
		.Label(SubjectsTabName)
		.TabRole(PanelTab)
		[
			SNew(SSplitter)
			.Orientation(EOrientation::Orient_Vertical)
			+ SSplitter::Slot()
			.Value(0.5f)
			[
				PanelController->SubjectsView->SubjectsTreeView.ToSharedRef()
			]
			+
			SSplitter::Slot()
			.Value(0.5f)
			[
				LiveLinkHubModule.GetSubjectController()->MakeSubjectView()
			]
		];
}

TSharedRef<SDockTab> SLiveLinkHubMainTabView::SpawnPlaybackTab(const FSpawnTabArgs& InTabArgs)
{
	FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");

	return SNew(SDockTab)
		.Label(PlaybackTabName)
		.TabRole(PanelTab)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				LiveLinkHubModule.GetRecordingListController()->MakeRecordingList()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				LiveLinkHubModule.GetPlaybackController()->MakePlaybackWidget()
			]
		];
}

TSharedRef<SDockTab> SLiveLinkHubMainTabView::SpawnClientsTab(const FSpawnTabArgs& InTabArgs)
{
	FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");
	TSharedPtr<FLiveLinkHubClientsController> ClientsController = LiveLinkHubModule.GetLiveLinkHub()->GetClientsController();

	return SNew(SDockTab)
		.Label(ClientsTabName)
		.TabRole(PanelTab)
		[
			ClientsController->MakeClientsView()
		];
}

TSharedRef<SDockTab> SLiveLinkHubMainTabView::SpawnClientDetailsTab(const FSpawnTabArgs& InTabArgs)
{
	FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");
	TSharedPtr<FLiveLinkHubClientsController> ClientsController = LiveLinkHubModule.GetLiveLinkHub()->GetClientsController();

	return SNew(SDockTab)
		.Label(ClientDetailsTabName)
		.TabRole(PanelTab)
		[
			ClientsController->MakeClientDetailsView()
		];
}

void SLiveLinkHubMainTabView::OnSubjectSelectionChanged(const FLiveLinkSubjectKey& SubjectKey)
{
	FLiveLinkHubModule& LiveLinkHubModule = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub");
	LiveLinkHubModule.GetSubjectController()->SetSubject(SubjectKey);
}

#undef LOCTEXT_NAMESPACE
