// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceInsightsModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ISourceCodeAccessModule.h"
#include "ISourceCodeAccessor.h"
#include "Logging/MessageLog.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Trace/StoreClient.h"
#include "Trace/StoreService.h"
#include "TraceServices/ITraceServicesModule.h"

// Insights
#include "Insights/ContextSwitches/ContextSwitchesProfilerManager.h"
#include "Insights/CookProfiler/CookProfilerManager.h"
#include "Insights/ImportTool/TableImportTool.h"
#include "Insights/InsightsManager.h"
#include "Insights/InsightsStyle.h"
#include "Insights/IUnrealInsightsModule.h"
#include "Insights/LoadingProfiler/LoadingProfilerManager.h"
#include "Insights/Log.h"
#include "Insights/MemoryProfiler/MemoryProfilerManager.h"
#include "Insights/NetworkingProfiler/NetworkingProfilerManager.h"
#include "Insights/TaskGraphProfiler/TaskGraphProfilerManager.h"
#include "Insights/Tests/InsightsTestRunner.h"
#include "Insights/TimingProfilerManager.h"
#include "Insights/Widgets/SStartPageWindow.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_LOG_CATEGORY(TraceInsights);

LLM_DEFINE_TAG(Insights);

IMPLEMENT_MODULE(FTraceInsightsModule, TraceInsights);

FString FTraceInsightsModule::UnrealInsightsLayoutIni;

////////////////////////////////////////////////////////////////////////////////////////////////////
// FTraceInsightsModule
////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::StartupModule()
{
	LLM_SCOPE_BYTAG(Insights);

	ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>("TraceServices");
	TraceAnalysisService = TraceServicesModule.GetAnalysisService();
	TraceModuleService = TraceServicesModule.GetModuleService();

	FInsightsStyle::Initialize();
#if !WITH_EDITOR
	FAppStyle::SetAppStyleSet(FInsightsStyle::Get());
#endif

	// Register FInsightsManager first, as the main component (first to init, last to shutdown).
	RegisterComponent(FInsightsManager::CreateInstance(TraceAnalysisService.ToSharedRef(), TraceModuleService.ToSharedRef()));

	// Register other default components.
	RegisterComponent(FTimingProfilerManager::CreateInstance());
	RegisterComponent(FLoadingProfilerManager::CreateInstance());
	RegisterComponent(FNetworkingProfilerManager::CreateInstance());
	RegisterComponent(FMemoryProfilerManager::CreateInstance());
	RegisterComponent(Insights::FTaskGraphProfilerManager::CreateInstance());
	RegisterComponent(Insights::FContextSwitchesProfilerManager::CreateInstance());
	RegisterComponent(Insights::FCookProfilerManager::CreateInstance());
	RegisterComponent(Insights::FTableImportTool::CreateInstance());

#if !WITH_EDITOR
	ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
	SourceCodeAccessModule.OnOpenFileFailed().AddRaw(this, &FTraceInsightsModule::HandleCodeAccessorOpenFileFailed);
#endif

	UnrealInsightsLayoutIni = GConfig->GetConfigFilename(TEXT("UnrealInsightsLayout"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::ShutdownModule()
{
	LLM_SCOPE_BYTAG(Insights);

#if !WITH_EDITOR
	if (FModuleManager::Get().IsModuleLoaded("SourceCodeAccess"))
	{
		ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::GetModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
		SourceCodeAccessModule.OnOpenFileFailed().RemoveAll(this);
	}
#endif

	if (PersistentLayout.IsValid())
	{
		// Save application layout.
		FLayoutSaveRestore::SaveToConfig(UnrealInsightsLayoutIni, PersistentLayout.ToSharedRef());
		GConfig->Flush(false, UnrealInsightsLayoutIni);
	}

	UnregisterTabSpawners();

#define INSIGHTS_CHECK_SHARED_REFERENCES 1
#if INSIGHTS_CHECK_SHARED_REFERENCES
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	auto TimingInsightsWindow = FTimingProfilerManager::Get()->GetProfilerWindow();
	auto AssetLoadingInsightsWindow = FLoadingProfilerManager::Get()->GetProfilerWindow();
	auto NetworkingInsightsWindow0 = FNetworkingProfilerManager::Get()->GetProfilerWindow(0);
	auto MemoryInsightsWindow = FMemoryProfilerManager::Get()->GetProfilerWindow();
#endif

	// Unregister components. Shutdown in the reverse order they were registered.
	for (int32 ComponentIndex = Components.Num() - 1; ComponentIndex >= 0; --ComponentIndex)
	{
		Components[ComponentIndex]->Shutdown();
	}
	Components.Reset();

#if INSIGHTS_CHECK_SHARED_REFERENCES
	ensure(MemoryInsightsWindow.GetSharedReferenceCount() <= 1);
	MemoryInsightsWindow.Reset();

	ensure(NetworkingInsightsWindow0.GetSharedReferenceCount() <= 1);
	NetworkingInsightsWindow0.Reset();

	ensure(AssetLoadingInsightsWindow.GetSharedReferenceCount() <= 1);
	AssetLoadingInsightsWindow.Reset();

	ensure(TimingInsightsWindow.GetSharedReferenceCount() <= 1);
	TimingInsightsWindow.Reset();

	if (ensure(Session.GetSharedReferenceCount() <= 1))
	{
		Session.Reset();
	}
	else // Some component(s) failed to release the references to Session shared ptr!
	{
		UE_LOG(TraceInsights, Warning, TEXT("The analysis Session is still referenced! Force delete!"));
		const TraceServices::IAnalysisSession* SessionPtr = Session.Get();
		Session.Reset();
		delete SessionPtr;
	}
#endif
#undef INSIGHTS_CHECK_SHARED_REFERENCES

	FInsightsStyle::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::RegisterComponent(TSharedPtr<IInsightsComponent> Component)
{
	if (Component.IsValid())
	{
		LLM_SCOPE_BYTAG(Insights);
		Components.Add(Component.ToSharedRef());
		Component->Initialize(*this);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::UnregisterComponent(TSharedPtr<IInsightsComponent> Component)
{
	if (Component.IsValid())
	{
		LLM_SCOPE_BYTAG(Insights);
		Component->Shutdown();
		Components.Remove(Component.ToSharedRef());
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::CreateDefaultStore()
{
#if WITH_TRACE_STORE
	const FString StoreDir = FPaths::ProjectSavedDir() / TEXT("TraceSessions");

	// Create the Store Service.
	UE::Trace::FStoreService::FDesc StoreServiceDesc;
	StoreServiceDesc.StoreDir = *StoreDir;
	StoreServiceDesc.RecorderPort = 1981;
	StoreServiceDesc.ThreadCount = 2;
	StoreService.Reset(UE::Trace::FStoreService::Create(StoreServiceDesc));

	if (StoreService.IsValid())
	{
		ConnectToStore(TEXT("127.0.0.1"), StoreService->GetPort());
	}
#else
	ConnectToStore(TEXT("127.0.0.1"));
#endif // WITH_TRACE_STORE
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FString FTraceInsightsModule::GetDefaultStoreDir()
{
	using UE::Trace::FStoreClient;

	TUniquePtr<FStoreClient> StoreClient(FStoreClient::Connect(TEXT("localhost")));

	if (!StoreClient)
	{
		UE_LOG(TraceInsights, Error, TEXT("Failed to connect to the store client."));
		return FString("");
	}

	const FStoreClient::FStatus* Status = StoreClient->GetStatus();
	if (!StoreClient)
	{
		UE_LOG(TraceInsights, Error, TEXT("Failed to get the status of the store client."));
		return FString("");
	}

	return FString(Status->GetStoreDir());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

UE::Trace::FStoreClient* FTraceInsightsModule::GetStoreClient()
{
	return FInsightsManager::Get()->GetStoreClient();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FTraceInsightsModule::ConnectToStore(const TCHAR* InStoreHost, uint32 InStorePort)
{
	return FInsightsManager::Get()->ConnectToStore(InStoreHost, InStorePort);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::RegisterTabSpawners()
{
	// Allow components to register major tabs.
	for (TSharedRef<IInsightsComponent>& Component : Components)
	{
		Component->RegisterMajorTabs(*this);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::UnregisterTabSpawners()
{
	// Unregister major tabs in the reverse order they were registered.
	for (int32 ComponentIndex = Components.Num() - 1; ComponentIndex >= 0; --ComponentIndex)
	{
		Components[ComponentIndex]->UnregisterMajorTabs();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::CreateSessionBrowser(const FCreateSessionBrowserParams& Params)
{
	RegisterTabSpawners();

	//////////////////////////////////////////////////
	// Create the main window.

	const bool bEmbedTitleAreaContent = false;

	// Get desktop metrics. It also ensures the correct metrics will be used later in SWindow.
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(
		static_cast<float>(DisplayMetrics.PrimaryDisplayWorkAreaRect.Left),
		static_cast<float>(DisplayMetrics.PrimaryDisplayWorkAreaRect.Top));

	const FVector2D ClientSize(960.0f * DPIScaleFactor, 640.0f * DPIScaleFactor);

	TSharedRef<SWindow> RootWindow = SNew(SWindow)
		.Title(NSLOCTEXT("TraceInsightsModule", "UnrealInsightsBrowserAppName", "Unreal Insights Session Browser"))
		.CreateTitleBar(!bEmbedTitleAreaContent)
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.IsInitiallyMaximized(false)
		.IsInitiallyMinimized(false)
		.SizingRule(ESizingRule::UserSized)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.ClientSize(ClientSize)
		.AdjustInitialSizeAndPositionForDPIScale(false);

	//RootWindow->GetTitleBar()->SetAllowMenuBar(true);

	const bool bShowRootWindowImmediately = false;
	FSlateApplication::Get().AddWindow(RootWindow, bShowRootWindowImmediately);

	FGlobalTabmanager::Get()->SetRootWindow(RootWindow);
	FGlobalTabmanager::Get()->SetAllowWindowMenuBar(true);

	FSlateNotificationManager::Get().SetRootWindow(RootWindow);

	//////////////////////////////////////////////////
	// Setup the window's content.

	TSharedRef<FTabManager::FLayout> DefaultLayout = FTabManager::NewLayout("TraceSessionBrowserLayout_v1.1");
	DefaultLayout->AddArea
	(
		FTabManager::NewPrimaryArea()
		->Split
		(
			FTabManager::NewStack()
			->AddTab(FInsightsManagerTabs::TraceStoreTabId, ETabState::OpenedTab)
			->AddTab(FInsightsManagerTabs::ConnectionTabId, ETabState::OpenedTab)
			//->AddTab(FInsightsManagerTabs::LauncherTabId, ETabState::ClosedTab)
			->SetForegroundTab(FInsightsManagerTabs::TraceStoreTabId)
		)
	);

	AddAreaForWidgetReflector(DefaultLayout, Params.bAllowDebugTools);

	// Load layout from ini file.
	PersistentLayout = FLayoutSaveRestore::LoadFromConfig(UnrealInsightsLayoutIni, DefaultLayout);

	// Restore application layout.
	const EOutputCanBeNullptr OutputCanBeNullptr = EOutputCanBeNullptr::Never;
	TSharedPtr<SWidget> Content = FGlobalTabmanager::Get()->RestoreFrom(PersistentLayout.ToSharedRef(), RootWindow, bEmbedTitleAreaContent, OutputCanBeNullptr);
	RootWindow->SetContent(Content.ToSharedRef());
	RootWindow->GetOnWindowClosedEvent().AddRaw(this, &FTraceInsightsModule::OnWindowClosedEvent);

	//////////////////////////////////////////////////
	// Show the window.

	RootWindow->ShowWindow();
	const bool bForceWindowToFront = true;
	RootWindow->BringToFront(bForceWindowToFront);

	//////////////////////////////////////////////////
	// Set up command line parameter forwarding.

	TSharedPtr<class STraceStoreWindow> TraceStoreWnd = FInsightsManager::Get()->GetTraceStoreWindow();
	if (TraceStoreWnd.IsValid())
	{
		TraceStoreWnd->SetEnableAutomaticTesting(Params.bInitializeTesting);
		TraceStoreWnd->SetEnableDebugTools(Params.bAllowDebugTools);
		TraceStoreWnd->SetStartProcessWithStompMalloc(Params.bStartProcessWithStompMalloc);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::CreateSessionViewer(bool bAllowDebugTools)
{
	RegisterTabSpawners();

#if !WITH_EDITOR

	//////////////////////////////////////////////////
	// Create the main window.

#if PLATFORM_MAC
	const bool bEmbedTitleAreaContent = true;
#else
	const bool bEmbedTitleAreaContent = false;
#endif

	// Get desktop metrics. It also ensures the correct metrics will be used later in SWindow.
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(
		static_cast<float>(DisplayMetrics.PrimaryDisplayWorkAreaRect.Left),
		static_cast<float>(DisplayMetrics.PrimaryDisplayWorkAreaRect.Top));

	const FVector2D ClientSize(1280.f * DPIScaleFactor, 720.0f * DPIScaleFactor);

	TSharedRef<SWindow> RootWindow = SNew(SWindow)
		.Title(NSLOCTEXT("TraceInsightsModule", "UnrealInsightsAppName", "Unreal Insights"))
		.CreateTitleBar(!bEmbedTitleAreaContent)
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.IsInitiallyMaximized(false)
		.IsInitiallyMinimized(false)
		.SizingRule(ESizingRule::UserSized)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.ClientSize(ClientSize)
		.AdjustInitialSizeAndPositionForDPIScale(false);

	const bool bShowRootWindowImmediately = false;
	FSlateApplication::Get().AddWindow(RootWindow, bShowRootWindowImmediately);

	FGlobalTabmanager::Get()->SetRootWindow(RootWindow);
	FGlobalTabmanager::Get()->SetAllowWindowMenuBar(true);

	FSlateNotificationManager::Get().SetRootWindow(RootWindow);

	//////////////////////////////////////////////////
	// Setup the window's content.

	TSharedRef<FTabManager::FLayout> DefaultLayout = FTabManager::NewLayout("UnrealInsightsLayout_v1.1");

	AddAreaForSessionViewer(DefaultLayout);

	AddAreaForWidgetReflector(DefaultLayout, false);

	// Load layout from ini file.
	PersistentLayout = FLayoutSaveRestore::LoadFromConfig(UnrealInsightsLayoutIni, DefaultLayout);

	// Restore application layout.
	const EOutputCanBeNullptr OutputCanBeNullptr = EOutputCanBeNullptr::Never;
	TSharedPtr<SWidget> Content = FGlobalTabmanager::Get()->RestoreFrom(PersistentLayout.ToSharedRef(), RootWindow, bEmbedTitleAreaContent, OutputCanBeNullptr);

	RootWindow->SetContent(Content.ToSharedRef());
	RootWindow->GetOnWindowClosedEvent().AddRaw(this, &FTraceInsightsModule::OnWindowClosedEvent);

	//////////////////////////////////////////////////
	// Show the window.

	RootWindow->ShowWindow();
	const bool bForceWindowToFront = true;
	RootWindow->BringToFront(bForceWindowToFront);

#endif // !WITH_EDITOR
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::AddAreaForSessionViewer(TSharedRef<FTabManager::FLayout> Layout)
{
	TSharedRef<FTabManager::FStack> Stack = FTabManager::NewStack();

#if WITH_EDITOR
	// In editor, we default to all tabs closed.
	Stack->AddTab(FInsightsManagerTabs::SessionInfoTabId, ETabState::ClosedTab);
	Stack->AddTab(FInsightsManagerTabs::TimingProfilerTabId, ETabState::ClosedTab);
	Stack->AddTab(FInsightsManagerTabs::LoadingProfilerTabId, ETabState::ClosedTab);
	Stack->AddTab(FInsightsManagerTabs::NetworkingProfilerTabId, ETabState::ClosedTab);
	Stack->AddTab(FInsightsManagerTabs::MemoryProfilerTabId, ETabState::ClosedTab);
	//Stack->SetForegroundTab(FTabId(FInsightsManagerTabs::TimingProfilerTabId));

	// Create area for the main window.
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);
	Layout->AddArea
	(
		FTabManager::NewArea(1280.f * DPIScaleFactor, 720.0f * DPIScaleFactor)
		->Split(Stack)
	);
#else
	Stack->AddTab(FInsightsManagerTabs::SessionInfoTabId, ETabState::OpenedTab);
	Stack->AddTab(FInsightsManagerTabs::TimingProfilerTabId, ETabState::OpenedTab);
	Stack->AddTab(FInsightsManagerTabs::LoadingProfilerTabId, ETabState::ClosedTab);
	Stack->AddTab(FInsightsManagerTabs::NetworkingProfilerTabId, ETabState::ClosedTab);
	Stack->AddTab(FInsightsManagerTabs::MemoryProfilerTabId, ETabState::ClosedTab);
	Stack->AddTab(FInsightsManagerTabs::MessageLogTabId, ETabState::ClosedTab);
	Stack->SetForegroundTab(FTabId(FInsightsManagerTabs::TimingProfilerTabId));

	Layout->AddArea
	(
		FTabManager::NewPrimaryArea()
		->Split(Stack)
	);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::AddAreaForWidgetReflector(TSharedRef<FTabManager::FLayout> Layout, bool bIsOpenedTab)
{
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);

	// Create area and tab for Slate's WidgetReflector.
	Layout->AddArea
	(
		FTabManager::NewArea(800.0f * DPIScaleFactor, 400.0f * DPIScaleFactor)
		->SetWindow(FVector2D(10.0f * DPIScaleFactor, 10.0f * DPIScaleFactor), false)
		->Split
		(
			FTabManager::NewStack()->AddTab(FTabId("WidgetReflector"), bIsOpenedTab ? ETabState::OpenedTab : ETabState::ClosedTab)
		)
	);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::ShutdownUserInterface()
{
	if (PersistentLayout.IsValid())
	{
		// Save application layout.
		FLayoutSaveRestore::SaveToConfig(UnrealInsightsLayoutIni, PersistentLayout.ToSharedRef());
		GConfig->Flush(false, UnrealInsightsLayoutIni);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::RegisterMajorTabConfig(const FName& InMajorTabId, const FInsightsMajorTabConfig& InConfig)
{
	TabConfigs.Add(InMajorTabId, InConfig);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::UnregisterMajorTabConfig(const FName& InMajorTabId)
{
	TabConfigs.Remove(InMajorTabId);
}


////////////////////////////////////////////////////////////////////////////////////////////////////

FOnRegisterMajorTabExtensions& FTraceInsightsModule::OnRegisterMajorTabExtension(const FName& InMajorTabId)
{
	return MajorTabExtensionDelegates.FindOrAdd(InMajorTabId);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FInsightsMajorTabConfig& FTraceInsightsModule::FindMajorTabConfig(const FName& InMajorTabId) const
{
	const FInsightsMajorTabConfig* FoundConfig = TabConfigs.Find(InMajorTabId);
	if (FoundConfig != nullptr)
	{
		return *FoundConfig;
	}

	static FInsightsMajorTabConfig DefaultConfig;
	return DefaultConfig;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FOnRegisterMajorTabExtensions* FTraceInsightsModule::FindMajorTabLayoutExtension(const FName& InMajorTabId)
{
	return MajorTabExtensionDelegates.Find(InMajorTabId);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FString& FTraceInsightsModule::GetUnrealInsightsLayoutIni()
{
	return UnrealInsightsLayoutIni;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::SetUnrealInsightsLayoutIni(const FString& InIniPath)
{
	UnrealInsightsLayoutIni = InIniPath;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<const TraceServices::IAnalysisSession> FTraceInsightsModule::GetAnalysisSession() const
{
	return FInsightsManager::Get()->GetSession();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::StartAnalysisForTrace(uint32 InTraceId, bool InAutoQuit)
{
	if (InTraceId != 0)
	{
		FInsightsManager::Get()->LoadTrace(InTraceId, InAutoQuit);
	}
	UpdateAppTitle();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::StartAnalysisForLastLiveSession(float InRetryTime)
{
	FInsightsManager::Get()->LoadLastLiveSession(InRetryTime);
	UpdateAppTitle();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::StartAnalysisForTraceFile(const TCHAR* InTraceFile, bool InAutoQuit)
{
	if (InTraceFile != nullptr)
	{
		FInsightsManager::Get()->LoadTraceFile(FString(InTraceFile), InAutoQuit);
	}
	UpdateAppTitle();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::OnWindowClosedEvent(const TSharedRef<SWindow>&)
{
	for (TSharedRef<IInsightsComponent>& Component : Components)
	{
		Component->OnWindowClosedEvent();
	}

	FGlobalTabmanager::Get()->SaveAllVisualState();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::InitializeTesting(bool InInitAutomationModules, bool InAutoQuit)
{
#if !UE_BUILD_SHIPPING && !WITH_EDITOR
	auto TestRunner = FInsightsTestRunner::CreateInstance();

	TestRunner->SetInitAutomationModules(InInitAutomationModules);
	TestRunner->SetAutoQuit(InAutoQuit);

	RegisterComponent(TestRunner);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::ScheduleCommand(const FString& InCmd)
{
	FString ActualCmd = InCmd;
	ActualCmd.TrimCharInline(TEXT('\"'), nullptr);
	ActualCmd.TrimCharInline(TEXT('\''), nullptr);

#if !UE_BUILD_SHIPPING && !WITH_EDITOR
	if (ActualCmd.StartsWith(TEXT("Automation RunTests")))
	{
		FInsightsTestRunner::Get()->ScheduleCommand(ActualCmd);
		return;
	}
#endif

	FInsightsManager::Get()->ScheduleCommand(ActualCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::RunAutomationTest(const FString& InCmd)
{
#if !UE_BUILD_SHIPPING && !WITH_EDITOR
	FString ActualCmd = InCmd;
	ActualCmd.TrimCharInline(TEXT('\"'), nullptr);
	ActualCmd.TrimCharInline(TEXT('\''), nullptr);

	if (ActualCmd.StartsWith(TEXT("Automation RunTests")))
	{
		FInsightsTestRunner::Get()->ScheduleCommand(ActualCmd);
		FInsightsTestRunner::Get()->RunTests();
		return;
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FTraceInsightsModule::Exec(const TCHAR* Cmd, FOutputDevice& Ar)
{
	for (TSharedRef<IInsightsComponent>& Component : Components)
	{
		if (Component->Exec(Cmd, Ar))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::UpdateAppTitle()
{
	FInsightsManager::Get()->UpdateAppTitle();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FTraceInsightsModule::HandleCodeAccessorOpenFileFailed(const FString& Filename)
{
	FMessageLog ReportMessageLog(FInsightsManager::Get()->GetLogListingName());
	FText Text = FText::Format(NSLOCTEXT("TraceInsightsModule", "FailedToOpenSourceFile", "Failed to open source file!\n\"{0}\""), FText::FromString(Filename));
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning, Text);
	ReportMessageLog.AddMessage(Message);
	ReportMessageLog.Notify();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
