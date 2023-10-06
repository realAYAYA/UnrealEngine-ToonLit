// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnrealEd.cpp: UnrealEd package file
=============================================================================*/

#include "UnrealEdGlobals.h"
#include "Stats/Stats.h"
#include "Async/TaskGraphInterfaces.h"
#include "EditorModeTools.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "HAL/PlatformSplash.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "EngineGlobals.h"
#include "Modules/ModuleInterface.h"
#include "EditorModeManager.h"
#include "EditorDirectories.h"
#include "Misc/OutputDeviceConsole.h"
#include "UnrealEngine.h"
#include "UnrealEdMisc.h"
#include "EditorModes.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"

#include "DebugToolExec.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "EngineAnalytics.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "GameFramework/InputSettings.h"

#include "GameProjectGenerationModule.h"

#include "EditorActorFolders.h"

#include "IVREditorModule.h"

UUnrealEdEngine* GUnrealEd = nullptr;

DEFINE_LOG_CATEGORY_STATIC(LogUnrealEd, Log, All);

/**
 * Provides access to the FEditorModeTools for the level editor
 */
namespace Internal
{
	static TSharedPtr<FEditorModeTools>& GetGlobalModeManager()
	{
		checkf(!IsRunningCommandlet(), TEXT("The global mode manager should not be created or accessed in a commandlet environment. Check that your mode or module is not accessing the global mode tools or that scriptable features of modes have been moved to subsystems."));

		static TSharedPtr<FEditorModeTools> EditorModeToolsSingleton = MakeShared<FEditorModeTools>();
		return EditorModeToolsSingleton;
	}
};

bool GLevelEditorModeToolsIsValid()
{
	return Internal::GetGlobalModeManager().IsValid();
}

FEditorModeTools& GLevelEditorModeTools()
{
	check(GLevelEditorModeToolsIsValid());
	return *Internal::GetGlobalModeManager().Get();
}

FLevelEditorViewportClient* GCurrentLevelEditingViewportClient = NULL;
/** Tracks the last level editing viewport client that received a key press. */
FLevelEditorViewportClient* GLastKeyLevelEditingViewportClient = NULL;

/**
 * Returns the path to the engine's editor resources directory (e.g. "/../../Engine/Content/Editor/")
 */
const FString GetEditorResourcesDir()
{
	return FPaths::Combine( FPlatformProcess::BaseDir(), *FPaths::EngineContentDir(), TEXT("Editor/") );
}

void CheckAndMaybeGoToVRModeInternal(const bool bIsImmersive)
{
	// Go straight to VR mode if we were asked to
	{
		if (!bIsImmersive && FParse::Param(FCommandLine::Get(), TEXT("VREditor")))
		{
			IVREditorModule& VREditorModule = IVREditorModule::Get();
			VREditorModule.EnableVREditor(true);
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("ForceVREditor")))
		{
			GEngine->DeferredCommands.Add(TEXT("VREd.ForceVRMode"));
		}
	}
}

int32 EditorInit( IEngineLoop& EngineLoop )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EditorInit);

	// Create debug exec.	
	GDebugToolExec = new FDebugToolExec;

	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Editor Initialized"), STAT_EditorStartup, STATGROUP_LoadTime);
	
	FScopedSlowTask SlowTask(100, NSLOCTEXT("EngineLoop", "EngineLoop_Loading", "Loading..."));
	
	SlowTask.EnterProgressFrame(50);

	int32 ErrorLevel = EngineLoop.Init();
	if( ErrorLevel != 0 )
	{
		FPlatformSplash::Hide();
		return 0;
	}

	// Let the analytics know that the editor has started
	if ( FEngineAnalytics::IsAvailable() )
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("GameName"), FApp::GetProjectName()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("CommandLine"), FCommandLine::Get()));

		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.ProgramStarted"), EventAttributes);
	}

	SlowTask.EnterProgressFrame(40);

	// Set up the actor folders singleton
	FActorFolders::Get();

	// Initialize the misc editor
	FUnrealEdMisc::Get().OnInit();
	FCoreDelegates::OnExit.AddLambda([]()
	{
		// Shutdown the global static mode manager
		if (Internal::GetGlobalModeManager().IsValid())
		{
			GLevelEditorModeTools().SetDefaultMode(FBuiltinEditorModes::EM_Default);
			Internal::GetGlobalModeManager().Reset();
		}
	});
	
	SlowTask.EnterProgressFrame(10);

	// Prime our array of default directories for loading and saving content files to
	FEditorDirectories::Get().LoadLastDirectories();

	// Cache the available targets for the current project, so we can display the appropriate options in the package project menu
	FDesktopPlatformModule::Get()->GetTargetsForCurrentProject();

	// =================== CORE EDITOR INIT FINISHED ===================

	// Hide the splash screen now that everything is ready to go
	FPlatformSplash::Hide();

	// Are we in immersive mode?
	const bool bIsImmersive = FPaths::IsProjectFilePathSet() && FParse::Param( FCommandLine::Get(), TEXT( "immersive" ) );
	const bool bIsPlayInEditorRequested = FPaths::IsProjectFilePathSet() && FParse::Param(FCommandLine::Get(), TEXT("pie"));

	// Do final set up on the editor frame and show it
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(EditorInit::MainFrame);

		// Startup Slate main frame and other editor windows if possible
		{
			const bool bStartImmersive = bIsImmersive;
			const bool bStartPIE = bIsImmersive || bIsPlayInEditorRequested;

			IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
			if (!MainFrameModule.IsWindowInitialized())
			{
				if (FSlateApplication::IsInitialized())
				{
					MainFrameModule.CreateDefaultMainFrame(bStartImmersive, bStartPIE);
				}
				else
				{
					RequestEngineExit(TEXT("Slate Application terminated or not initialized for MainFrame"));
					return 1;
				}
			}
		}
	}

	// Go straight to VR mode if we were asked to
	CheckAndMaybeGoToVRModeInternal(bIsImmersive);

	// Check for automated build/submit option
	const bool bDoAutomatedMapBuild = FParse::Param( FCommandLine::Get(), TEXT("AutomatedMapBuild") );

	// Prompt to update the game project file to the current version, if necessary
	if ( FPaths::IsProjectFilePathSet() )
	{
		FGameProjectGenerationModule::Get().CheckForOutOfDateGameProjectFile();
		FGameProjectGenerationModule::Get().CheckAndWarnProjectFilenameValid();
	}

	// =================== EDITOR STARTUP FINISHED ===================
	
	// Stat tracking
	{
		const float StartupTime = (float)( FPlatformTime::Seconds() - GStartTime );

		if( FEngineAnalytics::IsAvailable() )
		{
			FEngineAnalytics::GetProvider().RecordEvent(
				TEXT( "Editor.Performance.Startup" ),
				TEXT( "Duration" ), FString::Printf( TEXT( "%.3f" ), StartupTime ) );
		}
	}

	FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("HierarchicalLODOutliner"));

	//we have to remove invalid keys * after * all the plugins and modules have been loaded.  Doing this in the editor should be caught during a config save
	if (UInputSettings* InputSettings = UInputSettings::GetInputSettings())
	{
		InputSettings->RemoveInvalidKeys();
	}

	// This will be ultimately returned from main(), so no error should be 0.
	return 0;
}

int32 EditorReinit()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EditorReinit);

	// Are we in immersive mode?
	const bool bIsImmersive = FPaths::IsProjectFilePathSet() && FParse::Param(FCommandLine::Get(), TEXT("immersive"));
	// Do final set up on the editor frame and show it
	{
		const bool bStartImmersive = bIsImmersive;
		const bool bStartPIE = bIsImmersive;

		// Startup Slate main frame and other editor windows
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		MainFrameModule.RecreateDefaultMainFrame(bStartImmersive, bStartPIE);
	}
	// Go straight to VR mode if we were asked to
	CheckAndMaybeGoToVRModeInternal(bIsImmersive);
	// No error should be 0
	return 0;
}

void EditorExit()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EditorExit);
	LLM_SCOPE(ELLMTag::EngineMisc);

	// Save out any config settings for the editor so they don't get lost
	GEditor->SaveConfig();
	GLevelEditorModeTools().SaveConfig();


	// Save out default file directories
	FEditorDirectories::Get().SaveLastDirectories();

	// Allow the game thread to finish processing any latent tasks.
	// Some editor functions may queue tasks that need to be run before the editor is finished.
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

	// Cleanup the misc editor
	FUnrealEdMisc::Get().OnExit();

	if( GLogConsole )
	{
		GLogConsole->Show( false );
	}

	delete GDebugToolExec;
	GDebugToolExec = NULL;
}

IMPLEMENT_MODULE( FDefaultModuleImpl, UnrealEd );
