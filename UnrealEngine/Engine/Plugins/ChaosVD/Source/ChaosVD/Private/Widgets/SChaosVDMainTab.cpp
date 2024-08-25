// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDMainTab.h"

#include "ChaosVDEditorModeTools.h"
#include "ChaosVDEditorVisualizationSettingsTab.h"
#include "ChaosVDEngine.h"
#include "ChaosVDModule.h"
#include "ChaosVDObjectDetailsTab.h"
#include "ChaosVDOutputLogTab.h"
#include "ChaosVDPlaybackController.h"
#include "ChaosVDPlaybackViewportTab.h"
#include "ChaosVDScene.h"
#include "ChaosVDSolversTracksTab.h"
#include "ChaosVDCollisionDataDetailsTab.h"
#include "ChaosVDConstraintDataInspectorTab.h"
#include "ChaosVDSceneQueryDataInspectorTab.h"
#include "ChaosVDStyle.h"
#include "ChaosVDTabsIDs.h"
#include "ChaosVDWorldOutlinerTab.h"
#include "Components/ChaosVDSceneQueryDataComponent.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SWindowTitleBar.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDesktopPlatform.h"
#include "Misc/MessageDialog.h"
#include "StatusBarSubsystem.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Components/ChaosVDParticleDataComponent.h"
#include "Components/ChaosVDSolverCollisionDataComponent.h"
#include "Components/ChaosVDSolverJointConstraintDataComponent.h"
#include "Styling/StyleColors.h"
#include "Styling/ToolBarStyle.h"
#include "Trace/ChaosVDTraceManager.h"
#include "Visualizers/ChaosVDJointConstraintsDataComponentVisualizer.h"
#include "Visualizers/ChaosVDParticleDataComponentVisualizer.h"
#include "Visualizers/ChaosVDSceneQueryDataComponentVisualizer.h"
#include "Visualizers/ChaosVDSolverCollisionDataComponentVisualizer.h"
#include "Widgets/SChaosBrowseTraceFileSourceModal.h"
#include "Widgets/SChaosVDBrowseSessionsModal.h"
#include "Widgets/SChaosVDRecordingControls.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SToolTip.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

void SChaosVDMainTab::Construct(const FArguments& InArgs, TSharedPtr<FChaosVDEngine> InChaosVDEngine)
{
	EditorModeTools = MakeShared<FChaosVDEditorModeTools>(InChaosVDEngine->GetCurrentScene());

	EditorModeTools->SetToolkitHost(StaticCastSharedRef<SChaosVDMainTab>(AsShared()));
	ChaosVDEngine = InChaosVDEngine;
	OwnerTab = InArgs._OwnerTab;

	RegisterComponentVisualizer(UChaosVDSolverCollisionDataComponent::StaticClass()->GetFName(), MakeShared<FChaosVDSolverCollisionDataComponentVisualizer>());
	RegisterComponentVisualizer(UChaosVDSceneQueryDataComponent::StaticClass()->GetFName(), MakeShared<FChaosVDSceneQueryDataComponentVisualizer>());
	RegisterComponentVisualizer(UChaosVDParticleDataComponent::StaticClass()->GetFName(), MakeShared<FChaosVDParticleDataComponentVisualizer>());
	RegisterComponentVisualizer(UChaosVDSolverJointConstraintDataComponent::StaticClass()->GetFName(), MakeShared<FChaosVDJointConstraintsDataComponentVisualizer>());

	TabManager = FGlobalTabmanager::Get()->NewTabManager(InArgs._OwnerTab.ToSharedRef()).ToSharedPtr();

	RegisterTabSpawner<FChaosVDWorldOutlinerTab>(FChaosVDTabID::WorldOutliner);
	RegisterTabSpawner<FChaosVDObjectDetailsTab>(FChaosVDTabID::DetailsPanel);
	RegisterTabSpawner<FChaosVDOutputLogTab>(FChaosVDTabID::OutputLog);
	RegisterTabSpawner<FChaosVDPlaybackViewportTab>(FChaosVDTabID::PlaybackViewport);
	RegisterTabSpawner<FChaosVDSolversTracksTab>(FChaosVDTabID::SolversTrack);
	RegisterTabSpawner<FChaosVDEditorVisualizationSettingsTab>(FChaosVDTabID::CVDEditorSettings);
	RegisterTabSpawner<FChaosVDCollisionDataDetailsTab>(FChaosVDTabID::CollisionDataDetails);
	RegisterTabSpawner<FChaosVDSceneQueryDataInspectorTab>(FChaosVDTabID::SceneQueryDataDetails);
	RegisterTabSpawner<FChaosVDConstraintDataInspectorTab>(FChaosVDTabID::JointsDataDetails);

	StatusBarID = FName(FChaosVDTabID::StatusBar.ToString() + InChaosVDEngine->GetInstanceGuid().ToString());
	
	TSharedPtr<SWidget> StatusBarWidget;
	
	if (UStatusBarSubsystem* StatusBarSubsystem = GEditor ? GEditor->GetEditorSubsystem<UStatusBarSubsystem>() : nullptr)
	{
		 StatusBarWidget = StatusBarSubsystem->MakeStatusBarWidget(StatusBarID, TabManager->GetOwnerTab().ToSharedRef());

		// Status bars come with the output log and content browser drawers by default, therefore we need to remove them otherwise they will be on the tool's window
		StatusBarSubsystem->UnregisterDrawer(StatusBarID, "ContentBrowser");
		StatusBarSubsystem->UnregisterDrawer(StatusBarID, "OutputLog");
	}
	else
	{
		// TODO: Add a way to try to create the status bar later in case the status bar subsystem was not ready yet.
	
		StatusBarWidget = SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MainTabStatusBarError", " There was an issue trying to get the status bar ready. The status bar will not be available"))
		];

		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to obtain the status bar subsystem - The status bar will not be available"), ANSI_TO_TCHAR(__FUNCTION__));		
	}

	GenerateMainWindowMenu();

	ChildSlot
	[
		// Row between the tab and main content 
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			// Create the Main Toolbar
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SImage)
				.Image(&FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("SlimToolBar").BackgroundBrush)
			]
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						GenerateMainToolbarWidget()
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SChaosVDRecordingControls, StaticCastSharedRef<SChaosVDMainTab>(AsShared()))
					]
				]
			]
		]
		// Main Visual Debugger Interface content
		+SVerticalBox::Slot()
		.Padding(FMargin(0.0f,5.0f,0.0f,0.0f))
		[
			TabManager->RestoreFrom(GenerateMainLayout(), TabManager->GetOwnerTab()->GetParentWindow()).ToSharedRef()
		]
		+SVerticalBox::Slot()
		.Padding(0.0f, 2.0f, 0.0f, 0.0f)
		.AutoHeight()
		[
			StatusBarWidget.ToSharedRef()
		]
	];

	// Make sure these tabs are always focused at the start
	TabManager->TryInvokeTab(FChaosVDTabID::SolversTrack);
	TabManager->TryInvokeTab(FChaosVDTabID::DetailsPanel);
}

void SChaosVDMainTab::BringToFront()
{
	if (TabManager.IsValid())
	{
		if (const TSharedPtr<SDockTab> TabPtr = OwnerTab.Pin())
		{
			TabManager->DrawAttention(TabPtr.ToSharedRef());
		}
	}
}

void SChaosVDMainTab::OnToolkitHostingStarted(const TSharedRef<IToolkit>& Toolkit)
{
}

void SChaosVDMainTab::OnToolkitHostingFinished(const TSharedRef<IToolkit>& Toolkit)
{
}

UWorld* SChaosVDMainTab::GetWorld() const
{
	return GetChaosVDEngineInstance()->GetCurrentScene()->GetUnderlyingWorld();
}

FEditorModeTools& SChaosVDMainTab::GetEditorModeManager() const
{
	check(EditorModeTools.IsValid())
	return *EditorModeTools;
}

TSharedPtr<FComponentVisualizer> SChaosVDMainTab::FindComponentVisualizer(UClass* ClassPtr)
{
	TSharedPtr<FComponentVisualizer> Visualizer;
	while (!Visualizer.IsValid() && (ClassPtr != nullptr) && (ClassPtr != UActorComponent::StaticClass()))
	{
		Visualizer = FindComponentVisualizer(ClassPtr->GetFName());
		ClassPtr = ClassPtr->GetSuperClass();
	}

	return Visualizer;
}

TSharedPtr<FComponentVisualizer> SChaosVDMainTab::FindComponentVisualizer(FName ClassName)
{
	TSharedPtr<FComponentVisualizer>* FoundVisualizer = ComponentVisualizersMap.Find(ClassName);

	return FoundVisualizer ? *FoundVisualizer : nullptr;
}

void SChaosVDMainTab::RegisterComponentVisualizer(FName ClassName, const TSharedPtr<FComponentVisualizer>& Visualizer)
{
	if (!ComponentVisualizersMap.Contains(ClassName))
	{
		ComponentVisualizersMap.Add(ClassName, Visualizer);
		ComponentVisualizers.Add(Visualizer);
	}
}

void SChaosVDMainTab::HandleTabSpawned(TSharedRef<SDockTab> Tab, FName TabID)
{
	if (!ActiveTabsByID.Contains(TabID))
	{
		ActiveTabsByID.Add(TabID, Tab);
	}
}

void SChaosVDMainTab::HandleTabDestroyed(TSharedRef<SDockTab> Tab, FName TabID)
{
	ActiveTabsByID.Remove(TabID);
}

TSharedRef<FTabManager::FLayout> SChaosVDMainTab::GenerateMainLayout()
{
	return FTabManager::NewLayout("ChaosVisualDebugger_Layout")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->SetExtensionId("TopLevelArea")
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.8f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)
					->AddTab(FChaosVDTabID::PlaybackViewport, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(FChaosVDTabID::SolversTrack, ETabState::OpenedTab)
					->AddTab(FChaosVDTabID::OutputLog, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.15f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)
					->AddTab(FChaosVDTabID::WorldOutliner, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(FChaosVDTabID::DetailsPanel, ETabState::OpenedTab)
					->AddTab(FChaosVDTabID::CollisionDataDetails, ETabState::OpenedTab)
					->AddTab(FChaosVDTabID::SceneQueryDataDetails, ETabState::OpenedTab)
					->AddTab(FChaosVDTabID::JointsDataDetails, ETabState::ClosedTab)
					->AddTab(FChaosVDTabID::CVDEditorSettings, ETabState::ClosedTab)
				)
			)
		);
}

void SChaosVDMainTab::GenerateMainWindowMenu()
{
	FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(TSharedPtr<FUICommandList >());
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("FileMenuLabel", "File"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(LOCTEXT("OpenFileMenuLabel", "OpenFile"), FText(), FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]()
				{
					BrowseAndOpenChaosVDRecording();
				})));
		}),
		"File"
	);
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("WindowMenuLabel", "Window"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
		}),
		"Window"
	);

	TabManager->SetAllowWindowMenuBar(true);
	TabManager->SetMenuMultiBox(MenuBarBuilder.GetMultiBox(), MenuBarBuilder.MakeWidget());
}

FReply SChaosVDMainTab::BrowseAndOpenChaosVDRecording()
{
	const TSharedRef<SChaosBrowseTraceFileSourceModal> SessionBrowserModal = SNew(SChaosBrowseTraceFileSourceModal);

	EChaosVDBrowseFileModalResponse Response = SessionBrowserModal->ShowModal();
	switch(Response)
	{
		case EChaosVDBrowseFileModalResponse::OpenLastFolder:
			{
				BrowseChaosVDRecordingFromFolder();
				break;
			}
		case EChaosVDBrowseFileModalResponse::OpenProfilingFolder:
			{
				BrowseChaosVDRecordingFromFolder(*FPaths::ProfilingDir());
				break;
			}
		case EChaosVDBrowseFileModalResponse::OpenTraceStore:
			{
				//TODO: Support remote Trace Stores
				const FString TraceStorePath = FChaosVDModule::Get().GetTraceManager()->GetLocalTraceStoreDirPath();
				if (TraceStorePath.IsEmpty())
				{
					UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to access Trace Store..."), ANSI_TO_TCHAR(__FUNCTION__));
					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("OpenTraceStoreFailedMessage", "Failed to access the Trace Store, The default profiling folder will be open. \n Please see the logs for mor details... "));
				}

				BrowseChaosVDRecordingFromFolder(TraceStorePath);
				break;
			}
		case EChaosVDBrowseFileModalResponse::Cancel:
			break;
		default:
				ensureMsgf(false, TEXT("Invalid responce received"));
			break;
	}

	return FReply::Handled();
}

TSharedRef<SButton> SChaosVDMainTab::CreateSimpleButton(TFunction<FText()>&& GetTextDelegate, TFunction<FText()>&& ToolTipTextDelegate, const FSlateBrush* ButtonIcon, const UChaosVDMainToolbarMenuContext* MenuContext, const FOnClicked& InButtonClickedCallback)
{
	const TSharedRef<SChaosVDMainTab> MainTab = MenuContext->MainTab.Pin().ToSharedRef();
	
	TSharedRef<SButton> Button = SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), "SimpleButton")
								.ToolTipText_Lambda(MoveTemp(ToolTipTextDelegate))
								.ContentPadding(FMargin(6.0f, 0.0f))
								.OnClicked(InButtonClickedCallback)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									[
										SNew(SImage)
										.Image(ButtonIcon)
										.ColorAndOpacity(FSlateColor::UseForeground())
									]
									+SHorizontalBox::Slot()
									.Padding(FMargin(4, 0, 0, 0))
									.VAlign(VAlign_Center)
									.AutoWidth()
									[
										SNew(STextBlock)
										.TextStyle(FAppStyle::Get(), "NormalText")
										.Text_Lambda(MoveTemp(GetTextDelegate))
									]
								];

	return Button;
}

TSharedRef<SWidget> SChaosVDMainTab::GenerateMainToolbarWidget()
{
	RegisterMainTabMenu();

	FToolMenuContext MenuContext;

	UChaosVDMainToolbarMenuContext* CommonContextObject = NewObject<UChaosVDMainToolbarMenuContext>();
	CommonContextObject->MainTab = SharedThis(this);

	MenuContext.AddObject(CommonContextObject);

	return UToolMenus::Get()->GenerateWidget(MainToolBarName, MenuContext);
}

void SChaosVDMainTab::BrowseChaosVDRecordingFromFolder(FStringView FolderPath)
{
	TArray<FString> OutOpenFilenames;
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		FString ExtensionStr;
		ExtensionStr += TEXT("Unreal Trace|*.utrace|");
		//TODO: Re-enable this when we add "Clips" support as these will use our own format
		//ExtensionStr += TEXT("Chaos Visual Debugger|*.cvd");
	
		DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
			LOCTEXT("OpenDialogTitle", "Open Chaos Visual Debug File").ToString(),
			FolderPath.GetData(),
			TEXT(""),
			*ExtensionStr,
			EFileDialogFlags::None,
			OutOpenFilenames
		);
	}

	if (OutOpenFilenames.Num() > 0)
	{
		if (OutOpenFilenames[0].EndsWith(TEXT("utrace")))
		{
			GetChaosVDEngineInstance()->LoadRecording(OutOpenFilenames[0]);
		}
	}
}

bool SChaosVDMainTab::ConnectToLiveSession(int32 SessionID, const FString SessionAddress) const
{
	FChaosVDTraceSessionDescriptor NewSessionFromFileDescriptor;
	NewSessionFromFileDescriptor.SessionName = FChaosVDModule::Get().GetTraceManager()->ConnectToLiveSession(SessionAddress, SessionID);
	NewSessionFromFileDescriptor.bIsLiveSession = true;

	bool bSuccess = false;

	if (NewSessionFromFileDescriptor.SessionName.IsEmpty())
	{
		// If it failed we want to clean the current session name, so it is ok calling it either way
		GetChaosVDEngineInstance()->SetCurrentSession(FChaosVDTraceSessionDescriptor());
	}
	else
	{
		GetChaosVDEngineInstance()->SetCurrentSession(NewSessionFromFileDescriptor);
		bSuccess = true;
	}

	return bSuccess;
}

void SChaosVDMainTab::RegisterMainTabMenu()
{
	const UToolMenus* ToolMenus = UToolMenus::Get();
	if (ToolMenus->IsMenuRegistered(MainToolBarName))
	{
		return;
	}

	UToolMenu* ToolBar = UToolMenus::Get()->RegisterMenu(MainToolBarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);

	FToolMenuSection& Section = ToolBar->AddSection("LoadRecording");
	Section.AddDynamicEntry("OpenFile", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		const UChaosVDMainToolbarMenuContext* Context = InSection.FindContext<UChaosVDMainToolbarMenuContext>();
		TSharedPtr<SChaosVDMainTab> MainTabPtr = Context->MainTab.Pin();
		if (!MainTabPtr)
		{
			return;
		}

		TSharedRef<SButton> OpenFileButton = MainTabPtr->CreateSimpleButton(
			[](){ return LOCTEXT("OpenFile", "Open File"); },
			[](){ return LOCTEXT("OpenFileDesc", "Click here to open a Chaos Visual Debugger file."); },
			FChaosVDStyle::Get().GetBrush("OpenFileIcon"),
			Context, FOnClicked::CreateLambda([WeakTab = MainTabPtr.ToWeakPtr()]()
			{
				if (TSharedPtr<SChaosVDMainTab> TabPtr = WeakTab.Pin())
				{
					return TabPtr->BrowseAndOpenChaosVDRecording();
				}

				return FReply::Handled();
			}));

		InSection.AddEntry(
			FToolMenuEntry::InitWidget(
				"OpenFileButton",
				OpenFileButton,
				FText::GetEmpty(),
				true,
				false
			));
	}));

	Section.AddDynamicEntry("ConnectToSession", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		const UChaosVDMainToolbarMenuContext* Context = InSection.FindContext<UChaosVDMainToolbarMenuContext>();
		TSharedPtr<SChaosVDMainTab> MainTabPtr = Context->MainTab.Pin();
		if (!MainTabPtr)
		{
			return;
		}

		TFunction<FText()> GetTextDelegate = [WeakTab = MainTabPtr.ToWeakPtr()]()
		{
			if (TSharedPtr<SChaosVDMainTab> TabPtr = WeakTab.Pin())
			{
				return TabPtr->GetConnectButtonText();
			}

			return FText();
		};

		TFunction<FText()> GetTooltipTextDelegate = [WeakTab = MainTabPtr.ToWeakPtr()]()
		{
			if (TSharedPtr<SChaosVDMainTab> TabPtr = WeakTab.Pin())
			{
				return TabPtr->GetConnectButtonTooltipText();
			}

			return FText();
		};
		
		FOnClicked OnClickedDelegate = FOnClicked::CreateLambda([WeakTab = StaticCastWeakPtr<SChaosVDMainTab>(MainTabPtr->AsWeak())]()
		{
			if (TSharedPtr<SChaosVDMainTab> TabPtr = WeakTab.Pin())
			{
				return TabPtr->HandleSessionConnectionClicked();
			}

			return FReply::Handled();
		});

		TSharedRef<SButton> ConnectToSessionButton = MainTabPtr->CreateSimpleButton(MoveTemp(GetTextDelegate), MoveTemp(GetTooltipTextDelegate), FChaosVDStyle::Get().GetBrush("OpenSessionIcon"), Context, MoveTemp(OnClickedDelegate));

		InSection.AddEntry(
			FToolMenuEntry::InitWidget(
				"ConnectToSession",
				ConnectToSessionButton,
				FText::GetEmpty(),
				true,
				false
			));
	}));
	
	Section.AddSeparator(NAME_None);
}

void SChaosVDMainTab::BrowseLiveSessionsFromTraceStore() const
{
	const TSharedRef<SChaosVDBrowseSessionsModal> SessionBrowserModal = SNew(SChaosVDBrowseSessionsModal);

	if (SessionBrowserModal->ShowModal() != EAppReturnType::Cancel)
	{
		bool bSuccess = false;
		const FChaosVDTraceSessionInfo SessionInfo = SessionBrowserModal->GetSelectedTraceInfo();
		if (SessionInfo.bIsValid)
		{
			const FString SessionAddress = SessionBrowserModal->GetSelectedTraceStoreAddress();
			bSuccess = ConnectToLiveSession(SessionInfo.TraceID, SessionAddress);
		}

		if (!bSuccess)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToConnectToSessionMessage", "Failed to connect to session"));	
		}
	}
}

FReply SChaosVDMainTab::HandleSessionConnectionClicked()
{
	if (TSharedPtr<FChaosVDPlaybackController> PlaybackControllerPtr =  GetChaosVDEngineInstance()->GetPlaybackController())
	{
		const bool bIsAlreadyInLiveSession = PlaybackControllerPtr->IsPlayingLiveSession();
	
		if (bIsAlreadyInLiveSession)
		{
			FChaosVDModule::Get().GetTraceManager()->CloseSession(GetChaosVDEngineInstance()->GetCurrentSessionDescriptor().SessionName);
			PlaybackControllerPtr->HandleDisconnectedFromSession();
		}
		else
		{
			BrowseLiveSessionsFromTraceStore();
		}
	}

	return FReply::Handled();
}

FText SChaosVDMainTab::GetConnectButtonText() const
{
	TSharedPtr<FChaosVDPlaybackController> PlaybackControllerPtr = GetChaosVDEngineInstance()->GetPlaybackController();
	if (PlaybackControllerPtr.IsValid())
	{
		const bool bIsAlreadyInLiveSession = PlaybackControllerPtr->IsPlayingLiveSession();
		return bIsAlreadyInLiveSession ? LOCTEXT("DisconnectFromSession", "Disconnect from Session") : LOCTEXT("ConnectToSession", "Connect to Session");
	}
	return FText();
}

FText SChaosVDMainTab::GetConnectButtonTooltipText() const
{
	TSharedPtr<FChaosVDPlaybackController> PlaybackControllerPtr = GetChaosVDEngineInstance()->GetPlaybackController();
	if (PlaybackControllerPtr.IsValid())
	{
		const bool bIsAlreadyInLiveSession = PlaybackControllerPtr->IsPlayingLiveSession();
		return bIsAlreadyInLiveSession ? LOCTEXT("DisconnectFromSessionTooltip", "Disconnects from the current live session but it does not stop it") : LOCTEXT("ConnectToSessionTooltip", "Opens a panel where you can browse active live sessions and connect to one");
	}
	return FText();
}

#undef LOCTEXT_NAMESPACE
