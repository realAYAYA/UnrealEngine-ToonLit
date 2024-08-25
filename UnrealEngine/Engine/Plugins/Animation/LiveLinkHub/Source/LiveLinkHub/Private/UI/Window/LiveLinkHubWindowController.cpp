// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubWindowController.h"

#include "CoreGlobals.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "StandaloneRenderer.h"
#include "UI/Widgets/LiveLinkHubMainTabController.h"
#include "UI/Window/ModalWindowManager.h"

#define LOCTEXT_NAMESPACE "LiveLinkHubWindowController"

FLiveLinkHubWindowController::FLiveLinkHubWindowController(const FLiveLinkHubWindowInitParams& Params)
	: LiveLinkHubLayoutIni(Params.LiveLinkHubLayoutIni)
	, MainTabController(MakeShared<FLiveLinkHubMainTabController>())
{
	LiveLinkHubComponents.Add(MainTabController);
	
	ModalWindowManager = InitializeSlateApplication();
}

FLiveLinkHubWindowController::~FLiveLinkHubWindowController()
{
	FGlobalTabmanager::Get()->SaveAllVisualState();
	FSlateApplication::Shutdown();
}

TSharedRef<SWindow> FLiveLinkHubWindowController::CreateWindow()
{
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(DisplayMetrics.PrimaryDisplayWorkAreaRect.Left, DisplayMetrics.PrimaryDisplayWorkAreaRect.Top);

	constexpr bool bEmbedTitleAreaContent = true;
	const FVector2D ClientSize(1200.0f * DPIScaleFactor, 800.0f * DPIScaleFactor);
	TSharedRef<SWindow> RootWindowRef = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "LiveLink Hub"))
		.CreateTitleBar(!bEmbedTitleAreaContent)
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.IsInitiallyMaximized(false)
		.IsInitiallyMinimized(false)
		.SizingRule(ESizingRule::UserSized)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.ClientSize(ClientSize)
		.AdjustInitialSizeAndPositionForDPIScale(false);

	RootWindow = RootWindowRef;
		
	constexpr bool bShowRootWindowImmediately = false;
	FSlateApplication::Get().AddWindow(RootWindowRef, bShowRootWindowImmediately);
	FGlobalTabmanager::Get()->SetRootWindow(RootWindowRef);
	FGlobalTabmanager::Get()->SetAllowWindowMenuBar(true);
	FSlateNotificationManager::Get().SetRootWindow(RootWindowRef);	

	RootWindow->SetOnWindowClosed(FOnWindowClosed::CreateRaw(this, &FLiveLinkHubWindowController::OnWindowClosed));

	return RootWindowRef;
}
void FLiveLinkHubWindowController::RestoreLayout()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FLiveLinkHubWindowController::RestoreLayout);

	const TSharedRef<FTabManager::FLayout> DefaultLayout = FTabManager::NewLayout("LiveLinkHub_v1.0");
	const TSharedRef<FTabManager::FArea> MainWindowArea = FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal);
	const TSharedRef<FTabManager::FStack> MainStack = FTabManager::NewStack();
	InitComponents(MainStack);
	MainStack->SetHideTabWell(true);

	MainWindowArea->Split(MainStack);
	DefaultLayout->AddArea(MainWindowArea);
	PersistentLayout = DefaultLayout;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FLiveLinkHubWindowController::LoadFromConfig);
		PersistentLayout = FLayoutSaveRestore::LoadFromConfig(LiveLinkHubLayoutIni, DefaultLayout);
	}


	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FLiveLinkHubWindowController::RestoreFrom);
		constexpr bool bEmbedTitleAreaContent = true;
		const TSharedPtr<SWidget> Content = FGlobalTabmanager::Get()->RestoreFrom(PersistentLayout.ToSharedRef(), RootWindow, bEmbedTitleAreaContent, EOutputCanBeNullptr::Never);
		RootWindow->SetContent(Content.ToSharedRef());
	}

	RootWindow->ShowWindow();
	constexpr bool bForceWindowToFront = true;
	RootWindow->BringToFront(bForceWindowToFront);

	MainTabController->OpenTab();
}

TSharedPtr<FModalWindowManager> FLiveLinkHubWindowController::InitializeSlateApplication()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FLiveLinkHubWindowController::InitializeAsStandaloneApplication);
	FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());

	// @hack This call will silently fail since we're running a commandlet, so pretend like we aren't one for it.
	// In the future the slate code should be changed to allow visual commandlets to enable high dpi mode.
	const bool bIsRunningCommandlet = PRIVATE_GIsRunningCommandlet;
	PRIVATE_GIsRunningCommandlet = false;
	FSlateApplication::InitHighDPI(true);
	PRIVATE_GIsRunningCommandlet = bIsRunningCommandlet;

	const FText ApplicationTitle = LOCTEXT("AppTitle", "LiveLink Hub");
	FGlobalTabmanager::Get()->SetApplicationTitle(ApplicationTitle);

	return MakeShared<FModalWindowManager>(CreateWindow());
}

void FLiveLinkHubWindowController::InitComponents(const TSharedRef<FTabManager::FStack>& MainArea)
{
	const FLiveLinkHubComponentInitParams Params { SharedThis(this), MainArea };
	for (const TSharedRef<ILiveLinkHubComponent>& LiveLinkHubComponent : LiveLinkHubComponents)
	{
		LiveLinkHubComponent->Init(Params);
	}
}

void FLiveLinkHubWindowController::OnWindowClosed(const TSharedRef<SWindow>& Window)
{
	SaveLayout();
	RootWindow.Reset();
}

void FLiveLinkHubWindowController::SaveLayout() const
{
	if (PersistentLayout)
	{
		FLayoutSaveRestore::SaveToConfig(LiveLinkHubLayoutIni, PersistentLayout.ToSharedRef());
	    GConfig->Flush(false, LiveLinkHubLayoutIni);
	}
}

#undef LOCTEXT_NAMESPACE 
