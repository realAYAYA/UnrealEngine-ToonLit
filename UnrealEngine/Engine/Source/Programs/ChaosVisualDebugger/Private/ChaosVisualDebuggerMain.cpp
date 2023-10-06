// Copyright Epic Games, Inc. All Rights Reserved.


#include "ChaosVisualDebuggerMain.h"
#include "ChaosVisualDebuggerSlateStyle.h"
#include "RequiredProgramMainCPPInclude.h"
#include "StandaloneRenderer.h"
#include "Widgets/Testing/STestSuite.h"
#include "Modules/ModuleManager.h"
#include "ISlateReflectorModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogChaosVisualDebugger, Log, All);

IMPLEMENT_APPLICATION(ChaosVisualDebugger, "Chaos Visual Debugger");


void InitializedSlateApplication()
{
	// crank up a normal Slate application using the platform's standalone renderer
	FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());

	// Set the application name.
	const FText ApplicationTitle = NSLOCTEXT("ChaosVisualDebugger", "AppTitle", "ChaosVisualDebugger");
	FGlobalTabmanager::Get()->SetApplicationTitle(ApplicationTitle);
}

TSharedRef<SDockTab> SpawnViewport(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> ViewportTab =
	SNew(SDockTab)
	.TabRole(ETabRole::MajorTab)
	.Label(NSLOCTEXT("ChaosVisualDebugger","ViewportTabLabel", "Viewport"))
	.ToolTipText(NSLOCTEXT("ChaosVisualDebugger","ViewportTabToolTip", "The Chaos Visual debugger Viewport is under development"));


	FVisualDebuggerStyle::ResetToDefault();	
	
	ViewportTab->SetContent
	(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(30)
		[
			SNew(SImage)
			.Image(FVisualDebuggerStyle::Get().GetBrush("UE4Icon"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ChaosVisualDebugger", "TestBlock", "Visual Debugger, under development!"))
		]
	);

	return ViewportTab;
}


void BuildChaosVDBUserInterface()
{
	// Need to load this module so we have the widget reflector tab available
	FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector");

	FGlobalTabmanager::Get()->RegisterTabSpawner("Viewport", FOnSpawnTab::CreateStatic(&SpawnViewport));

	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("SlateVisualDebugger_Layout")
		->AddArea
		(
			FTabManager::NewArea(1600, 1200)
			->SetWindow(FVector2D(420, 10), false)
			->Split
			(
				FTabManager::NewStack()
				->AddTab("Viewport", ETabState::OpenedTab)
			)
		)
		// This is for debugging the GUI
		/*->AddArea
		(
			// This area will get a 400x600 window at 10,10
			FTabManager::NewArea(400, 600)
			->SetWindow(FVector2D(10, 10), false)
			->Split
			(
				// The area contains a single tab with the widget reflector, for debugging purposes
				FTabManager::NewStack()->AddTab("WidgetReflector", ETabState::OpenedTab)
			)
		)*/
		;

	FGlobalTabmanager::Get()->RestoreFrom(Layout, TSharedPtr<SWindow>());
}


int32 ChaosVisualDebuggerMain(const TCHAR* CommandLine)
{
	UE_LOG(LogChaosVisualDebugger, Display, TEXT("Chaos Visual Debugger - Early Prototype Development"));

	// Override the stack size for the thread pool.
	FQueuedThreadPool::OverrideStackSize = 256 * 1024;

	FCommandLine::Set(CommandLine);

	// start up the main loop
	GEngineLoop.PreInit(CommandLine);

	// Make sure all UObject classes are registered and default properties have been initialized
	ProcessNewlyLoadedUObjects();

	// Tell the module manager it may now process newly-loaded UObjects when new C++ modules are loaded
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	InitializedSlateApplication();

	{
		BuildChaosVDBUserInterface();
		// Bring up the test suite (for testing)
		//RestoreSlateTestSuite();
	}

	while (!IsEngineExitRequested())
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FStats::AdvanceFrame(false);
		FTSTicker::GetCoreTicker().Tick(FApp::GetDeltaTime());
		FSlateApplication::Get().PumpMessages();
		FSlateApplication::Get().Tick();
		FPlatformProcess::Sleep(0);
	}

	FCoreDelegates::OnExit.Broadcast();
	FSlateApplication::Shutdown();
	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FEngineLoop::AppExit();

	return 0;
} 