// Copyright Epic Games, Inc. All Rights Reserved.


#include "Frame/MainFrameActions.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "AboutScreen.h"
#include "CreditsScreen.h"
#include "DesktopPlatformModule.h"
#include "ISourceControlModule.h"
#include "ISourceControlWindowsModule.h"
#include "GameProjectGenerationModule.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "SourceCodeNavigation.h"
#include "ISettingsModule.h"
#include "Interfaces/IProjectManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "PlatformInfo.h"
#include "Styling/AppStyle.h"
#include "Settings/EditorExperimentalSettings.h"
#include "CookerSettings.h"
#include "UnrealEdMisc.h"
#include "Editor/UnrealEdEngine.h"
#include "Editor/Transactor.h"
#include "Preferences/UnrealEdOptions.h"
#include "UnrealEdGlobals.h"
#include "FileHelpers.h"
#include "EditorAnalytics.h"
#include "LevelEditor.h"
#include "Interfaces/IProjectTargetPlatformEditorModule.h"
#include "Misc/ConfigCacheIni.h"
#include "MainFrameModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Commands/GenericCommands.h"
#include "Dialogs/SOutputLogDialog.h"
#include "Dialogs/Dialogs.h"
#include "IUATHelperModule.h"
#include "Menus/LayoutsMenu.h"
#include "TargetReceipt.h"
#include "IDocumentation.h"

#include "Settings/EditorSettings.h"
#include "AnalyticsEventAttribute.h"
#include "Kismet2/DebuggerCommands.h"
#include "GameMapsSettings.h"
#include "SourceControlWindows.h"

#define LOCTEXT_NAMESPACE "MainFrameActions"

DEFINE_LOG_CATEGORY_STATIC(MainFrameActions, Log, All);


TSharedRef< FUICommandList > FMainFrameCommands::ActionList( new FUICommandList() );

TWeakPtr<SNotificationItem> FMainFrameActionCallbacks::ChoosePackagesToCheckInNotification;

namespace
{
	const FName SwitchProjectBundle = "SwitchProject";
}

FMainFrameCommands::FMainFrameCommands()
	: TCommands<FMainFrameCommands>(
		TEXT("MainFrame"), // Context name for fast lookup
		LOCTEXT( "MainFrame", "Main Frame" ), // Localized context name for displaying
		NAME_None,	 // No parent context
		FAppStyle::GetAppStyleSetName() ), // Icon Style Set
	  ToggleFullscreenConsoleCommand(
		TEXT( "MainFrame.ToggleFullscreen" ),
		TEXT( "Toggles the editor between \"full screen\" mode and \"normal\" mode.  In full screen mode, the task bar and window title area are hidden." ),
		FConsoleCommandDelegate::CreateStatic( &FMainFrameActionCallbacks::ToggleFullscreen_Execute ) )
{
	AddBundle(SwitchProjectBundle, LOCTEXT("SwitchProjectBundle", "Switch Project"));
}


void FMainFrameCommands::RegisterCommands()
{
	// Some commands cannot be processed in a commandlet or if the editor is started without a project
	if ( !IsRunningCommandlet() && FApp::HasProjectName() && !IsRunningDedicatedServer())
	{
		// The global action list was created at static initialization time. Create a handler for otherwise unhandled keyboard input to route key commands through this list.
		FSlateApplication::Get().SetUnhandledKeyDownEventHandler( FOnKeyEvent::CreateStatic( &FMainFrameActionCallbacks::OnUnhandledKeyDownEvent ) );
	}

	// Make a default can execute action that disables input when in debug mode
	FCanExecuteAction DefaultExecuteAction = FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::DefaultCanExecuteAction );

	UI_COMMAND( SaveAll, "Save All", "Saves all unsaved levels and assets to disk", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control | EModifierKey::Shift, EKeys::S ) );
	ActionList->MapAction( SaveAll, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::SaveAll ), FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::CanSaveWorld ) );

	UI_COMMAND( ChooseFilesToSave, "Choose Files to Save...", "Opens a dialog with save options for content and levels", EUserInterfaceActionType::Button, FInputChord( EModifierKey::Control | EModifierKey::Alt | EModifierKey::Shift, EKeys::S ) );
	ActionList->MapAction( ChooseFilesToSave, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ChoosePackagesToSave ), FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::CanSaveWorld ) );

	UI_COMMAND( ViewChangelists, "View Changes", "Opens a dialog displaying current changes.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction(ViewChangelists, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ViewChangelists ), FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::CanViewChangelists ) );

	UI_COMMAND( SubmitContent, "Submit Content", "Opens a dialog with check in options for content and levels.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( SubmitContent, FExecuteAction::CreateLambda([]() { FSourceControlWindows::ChoosePackagesToCheckIn(); }), FCanExecuteAction::CreateStatic(&FSourceControlWindows::CanChoosePackagesToCheckIn ), FGetActionCheckState(), FIsActionButtonVisible::CreateStatic(FSourceControlWindows::ShouldChoosePackagesToCheckBeVisible) );

	UI_COMMAND( SyncContent, "Sync Content", "Saves all unsaved levels and assets to disk and then downloads the latest versions from revision control.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction(SyncContent, FExecuteAction::CreateLambda([]() { FSourceControlWindows::SyncLatest(); }), FCanExecuteAction::CreateStatic( &FSourceControlWindows::CanSyncLatest ) );

	UI_COMMAND( ConnectToSourceControl, "Connect to Revision Control...", "Connect to a revision control system for tracking changes to your content and levels.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( ConnectToSourceControl, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ConnectToSourceControl ), DefaultExecuteAction );

	UI_COMMAND( ChangeSourceControlSettings, "Change Revision Control Settings...", "Opens a dialog to change revision control settings.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( ChangeSourceControlSettings, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::ConnectToSourceControl), DefaultExecuteAction);

	UI_COMMAND( NewProject, "New Project...", "Opens a dialog to create a new game project", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( NewProject, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::NewProject, false, true), DefaultExecuteAction );

	UI_COMMAND( OpenProject, "Open Project...", "Opens a dialog to choose a game project to open", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( OpenProject, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::NewProject, true, false), DefaultExecuteAction );

	UI_COMMAND(AddCodeToProject, "New C++ Class...", "Adds C++ code to the project. The code can only be compiled if you have an appropriate C++ compiler installed.", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(AddCodeToProject, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::AddCodeToProject), FCanExecuteAction::CreateStatic(&FMainFrameActionCallbacks::CanAddCodeToProject), FGetActionCheckState(), FIsActionButtonVisible::CreateStatic(&FMainFrameActionCallbacks::IsAddCodeToProjectVisible));

	UI_COMMAND(RefreshCodeProject, "Refresh code project", "Refreshes your C++ code project.", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(RefreshCodeProject, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::RefreshCodeProject), FCanExecuteAction::CreateStatic(&FMainFrameActionCallbacks::CanRefreshCodeProject), FGetActionCheckState(), FIsActionButtonVisible::CreateStatic(&FMainFrameActionCallbacks::IsRefreshCodeProjectVisible));

	UI_COMMAND(OpenIDE, "Open IDE", "Opens your C++ code in an integrated development environment.", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(OpenIDE, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::OpenIDE), FCanExecuteAction::CreateStatic(&FMainFrameActionCallbacks::CanOpenIDE), FGetActionCheckState(), FIsActionButtonVisible::CreateStatic(&FMainFrameActionCallbacks::IsOpenIDEVisible));

	UI_COMMAND( ZipUpProject, "Zip Project", "Zips the project into a zip file.", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction(ZipUpProject, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ZipUpProject ), DefaultExecuteAction);

	//UI_COMMAND( LocalizeProject, "Localize Project...", "Opens the dashboard for managing project localization data.", EUserInterfaceActionType::Button, FInputChord() );
	//ActionList->MapAction( LocalizeProject, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::LocalizeProject ), DefaultExecuteAction );

	const int32 MaxProjects = 20;
	for( int32 CurProjectIndex = 0; CurProjectIndex < MaxProjects; ++CurProjectIndex )
	{
		// NOTE: The actual label and tool-tip will be overridden at runtime when the command is bound to a menu item, however
		// we still need to set one here so that the key bindings UI can function properly
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("CurrentProjectIndex"), CurProjectIndex);
		const FText Message = FText::Format( LOCTEXT( "SwitchProject", "Switch Project {CurrentProjectIndex}" ), Arguments ); 
		TSharedRef< FUICommandInfo > SwitchProject =
			FUICommandInfoDecl(
				this->AsShared(),
				FName( *FString::Printf( TEXT( "SwitchProject%i" ), CurProjectIndex ) ),
				Message,
				LOCTEXT( "SwitchProjectToolTip", "Restarts the editor and switches to selected project" ),
				SwitchProjectBundle)
			.UserInterfaceType( EUserInterfaceActionType::Button )
			.DefaultChord( FInputChord() );
		SwitchProjectCommands.Add( SwitchProject );

		ActionList->MapAction( SwitchProjectCommands[ CurProjectIndex ], FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::SwitchProjectByIndex, CurProjectIndex ),
			FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::CanSwitchToProject, CurProjectIndex ),
			FIsActionChecked::CreateStatic( &FMainFrameActionCallbacks::IsSwitchProjectChecked, CurProjectIndex ) );
	}

	UI_COMMAND( Exit, "Exit", "Exits the application", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( Exit, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::Exit ), DefaultExecuteAction );

	ActionList->MapAction( FGenericCommands::Get().Undo, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ExecuteExecCommand, FString( TEXT("TRANSACTION UNDO") ) ), FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::Undo_CanExecute ) );

	ActionList->MapAction( FGenericCommands::Get().Redo, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ExecuteExecCommand, FString( TEXT("TRANSACTION REDO") ) ), FCanExecuteAction::CreateStatic( &FMainFrameActionCallbacks::Redo_CanExecute ) );

	UI_COMMAND( OpenDeviceManagerApp, "Device Manager", "Opens up the device manager app", EUserInterfaceActionType::Check, FInputChord() );
	ActionList->MapAction( OpenDeviceManagerApp, 
												FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::OpenSlateApp, FName( TEXT( "DeviceManager" ) ) ),
												FCanExecuteAction(),
												FIsActionChecked::CreateStatic( &FMainFrameActionCallbacks::OpenSlateApp_IsChecked, FName( TEXT( "DeviceManager" ) ) ) );

	UI_COMMAND( OpenSessionManagerApp, "Session Manager", "Opens up the session manager app", EUserInterfaceActionType::Check, FInputChord() );
	ActionList->MapAction( OpenSessionManagerApp, 
												FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::OpenSlateApp, FName( "SessionFrontend" ) ),
												FCanExecuteAction(),
												FIsActionChecked::CreateStatic( &FMainFrameActionCallbacks::OpenSlateApp_IsChecked, FName("SessionFrontend" ) ) );

	UI_COMMAND(OpenMarketplace, "Open Marketplace", "Opens the Marketplace", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(OpenMarketplace, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::OpenMarketplace));

	UI_COMMAND(DocumentationHome, "Documentation Home", "Authoritative, in-depth technical resources for using Unreal Engine", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(DocumentationHome, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::DocumentationHome));

	UI_COMMAND(BrowseAPIReference, "C++ API Reference", "Classes, functions, and other elements that make up the C++ API", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(BrowseAPIReference, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::BrowseAPIReference));

	UI_COMMAND(BrowseCVars, "Console Variables", "Reference companion for console variables and commands", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(BrowseCVars, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::BrowseCVars));

	UI_COMMAND(VisitCommunityHome, "Dev Community", "Join the worldwide community of Unreal developers and explore all the resources it has to offer", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(VisitCommunityHome, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::VisitCommunityHome));

	UI_COMMAND(VisitOnlineLearning, "Learning Library", "Learn Unreal Engine for free with easy-to-follow video courses and guided learning paths", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(VisitOnlineLearning, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::VisitOnlineLearning));

	UI_COMMAND(VisitForums, "Forums", "View announcements and engage in discussions with other developers", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(VisitForums, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::VisitForums));

	UI_COMMAND(VisitSearchForAnswersPage, "Q&A", "Search for answers, ask questions, and share your knowledge with other developers", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(VisitSearchForAnswersPage, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::VisitSearchForAnswersPage));

	UI_COMMAND(VisitCommunitySnippets, "Snippets", "Access and share ready-to-use code blocks and scripts", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(VisitCommunitySnippets, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::VisitCommunitySnippets));

	UI_COMMAND(VisitSupportWebSite, "Support", "Options for personalized technical support", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(VisitSupportWebSite, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::VisitSupportWebSite));

	UI_COMMAND(ReportABug, "Report a Bug", "Found a bug? Let us know about it", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(ReportABug, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::ReportABug));

	UI_COMMAND(OpenIssueTracker, "Issue Tracker", "Check the current status of public bugs and other issues", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(OpenIssueTracker, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::OpenIssueTracker));

	UI_COMMAND( AboutUnrealEd, "About Unreal Editor", "Version and copyright information", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( AboutUnrealEd, FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::AboutUnrealEd_Execute ) );

	UI_COMMAND( CreditsUnrealEd, "Credits", "Contributors to this version of Unreal Engine", EUserInterfaceActionType::Button, FInputChord() );
	ActionList->MapAction( CreditsUnrealEd, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::CreditsUnrealEd_Execute) );

	UI_COMMAND(VisitEpicGamesDotCom, "Visit UnrealEngine.com", "Learn more about Unreal technology", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(VisitEpicGamesDotCom, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::VisitEpicGamesDotCom));

	// Layout commands
	UI_COMMAND(ImportLayout, "Import Layout...", "Import a custom layout (or set of layouts) from a different directory and load it into your current instance of the Unreal Editor UI", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(ImportLayout, FExecuteAction::CreateStatic(&FLayoutsMenuLoad::ImportLayout));

	UI_COMMAND(SaveLayoutAs, "Save Layout As...", "Save the current layout customization on disk so it can be loaded later", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(SaveLayoutAs, FExecuteAction::CreateStatic(&FLayoutsMenuSave::SaveLayoutAs));

	UI_COMMAND(ExportLayout, "Export Layout...", "Export the custom layout customization to a different directory", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(ExportLayout, FExecuteAction::CreateStatic(&FLayoutsMenuSave::ExportLayout));

	UI_COMMAND(RemoveUserLayouts, "Remove All User Layouts...", "Remove all the layout customizations created by the user", EUserInterfaceActionType::Button, FInputChord());
	ActionList->MapAction(
		RemoveUserLayouts,
		FExecuteAction::CreateStatic(&FLayoutsMenuRemove::RemoveUserLayouts),
		FCanExecuteAction::CreateStatic(&FLayoutsMenu::IsThereUserLayouts)
	);


#if !PLATFORM_MAC && !PLATFORM_LINUX // Fullscreen mode in the editor is currently unsupported on Mac and Linux
	UI_COMMAND( ToggleFullscreen, "Enable Fullscreen", "Enables fullscreen mode for the application, expanding across the entire monitor", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::F11) );
	ActionList->MapAction( ToggleFullscreen,
		FExecuteAction::CreateStatic( &FMainFrameActionCallbacks::ToggleFullscreen_Execute ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FMainFrameActionCallbacks::FullScreen_IsChecked )
	);
#endif

	UI_COMMAND(OpenWidgetReflector, "Open Widget Reflector", "Opens the Widget Reflector", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift | EModifierKey::Control , EKeys::W));
	ActionList->MapAction(OpenWidgetReflector, FExecuteAction::CreateStatic(&FMainFrameActionCallbacks::OpenWidgetReflector_Execute));

	FGlobalEditorCommonCommands::MapActions(ActionList);
}

FReply FMainFrameActionCallbacks::OnUnhandledKeyDownEvent(const FKeyEvent& InKeyEvent)
{
	if(!GIsSlowTask)
	{
		if (FMainFrameCommands::ActionList->ProcessCommandBindings(InKeyEvent))
		{
			return FReply::Handled();
		}
		else if (FPlayWorldCommands::GlobalPlayWorldActions.IsValid() && FPlayWorldCommands::GlobalPlayWorldActions->ProcessCommandBindings(InKeyEvent))
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

bool FMainFrameActionCallbacks::DefaultCanExecuteAction()
{
	return FSlateApplication::Get().IsNormalExecution();
}

void FMainFrameActionCallbacks::ChoosePackagesToSave()
{
	const bool bPromptUserToSave = true;
	const bool bSaveMapPackages = true;
	const bool bSaveContentPackages = true;
	const bool bFastSave = false;
	const bool bClosingEditor = false;
	const bool bNotifyNoPackagesSaved = true;
	const bool bCanBeDeclined = false;

	// Skip because user will be prompted to select the packages to save, so we don't want the parent package save actually saving an unselected external package
	const bool bSkipExternalObjectSave = true;
		
	FEditorFileUtils::SaveDirtyPackages( bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined, nullptr, FEditorFileUtils::FShouldIgnorePackage::Default, bSkipExternalObjectSave);
}

void FMainFrameActionCallbacks::ViewChangelists()
{
	ISourceControlWindowsModule::Get().ShowChangelistsTab();
}

bool FMainFrameActionCallbacks::CanViewChangelists()
{
	return ISourceControlWindowsModule::Get().CanShowChangelistsTab();
}

void FMainFrameActionCallbacks::ConnectToSourceControl()
{
	ELoginWindowMode::Type Mode = !FSlateApplication::Get().GetActiveModalWindow().IsValid() ? ELoginWindowMode::Modeless : ELoginWindowMode::Modal;
	ISourceControlModule::Get().ShowLoginDialog(FSourceControlLoginClosed(), Mode, EOnLoginWindowStartup::PreserveProvider);
}

bool FMainFrameActionCallbacks::CanSaveWorld()
{
	return FSlateApplication::Get().IsNormalExecution() && (!GUnrealEd || !GUnrealEd->GetPackageAutoSaver().IsAutoSaving());
}

void FMainFrameActionCallbacks::SaveAll()
{
	const bool bPromptUserToSave = false;
	const bool bSaveMapPackages = true;
	const bool bSaveContentPackages = true;
	const bool bFastSave = false;
	const bool bNotifyNoPackagesSaved = false;
	const bool bCanBeDeclined = false;
	FEditorFileUtils::SaveDirtyPackages( bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined );
}

TArray<FRecentProjectFile> FMainFrameActionCallbacks::RecentProjects;

void FMainFrameActionCallbacks::CacheProjectNames()
{
	// The switch project menu is filled with recently opened project files
	RecentProjects = GetDefault<UEditorSettings>()->RecentlyOpenedProjectFiles;
}

void FMainFrameActionCallbacks::OpenMarketplace()
{
	FUnrealEdMisc::Get().OpenMarketplace();
}

void FMainFrameActionCallbacks::NewProject( bool bAllowProjectOpening, bool bAllowProjectCreate )
{
	if (GUnrealEd->WarnIfLightingBuildIsCurrentlyRunning())
	{
		return;
	}

	FText Title;
	if (bAllowProjectOpening && bAllowProjectCreate)
	{
		Title = LOCTEXT( "SelectProjectWindowHeader", "Select Project");
	}
	else if (bAllowProjectOpening)
	{
		Title = LOCTEXT( "OpenProjectWindowHeader", "Open Project");
	}
	else
	{
		Title = LOCTEXT( "NewProjectWindowHeader", "New Project");
	}

	TSharedRef<SWindow> NewProjectWindow =
		SNew(SWindow)
		.Title(Title)
		.ClientSize( FMainFrameModule::GetProjectBrowserWindowSize() )
		.SizingRule( ESizingRule::UserSized )
		.SupportsMinimize(false) .SupportsMaximize(false);

	NewProjectWindow->SetContent( FGameProjectGenerationModule::Get().CreateGameProjectDialog(bAllowProjectOpening, bAllowProjectCreate) );

	IMainFrameModule& MainFrameModule = FModuleManager::GetModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	if (MainFrameModule.GetParentWindow().IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(NewProjectWindow, MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(NewProjectWindow);
	}
}

void FMainFrameActionCallbacks::AddCodeToProject()
{
	FGameProjectGenerationModule::Get().OpenAddCodeToProjectDialog();
}

bool FMainFrameActionCallbacks::CanAddCodeToProject()
{
	return IsCPPAllowed();
}

bool FMainFrameActionCallbacks::IsAddCodeToProjectVisible()
{
	return IsCPPAllowed();
}

void FMainFrameActionCallbacks::RefreshCodeProject()
{
	if ( !FSourceCodeNavigation::IsCompilerAvailable() )
	{
		// Attempt to trigger the tutorial if the user doesn't have a compiler installed for the project.
		FSourceCodeNavigation::AccessOnCompilerNotFound().Broadcast();
	}

	FText FailReason, FailLog;
	if(!FGameProjectGenerationModule::Get().UpdateCodeProject(FailReason, FailLog))
	{
		SOutputLogDialog::Open(LOCTEXT("RefreshProject", "Refresh Project"), FailReason, FailLog, FText::GetEmpty());
	}
}

bool FMainFrameActionCallbacks::CanRefreshCodeProject()
{
	return IsCPPAllowed() && IsCodeProject();
}

bool FMainFrameActionCallbacks::IsRefreshCodeProjectVisible()
{
	return IsCPPAllowed();
}

bool FMainFrameActionCallbacks::IsCodeProject()
{
	// Not particularly rigorous, but assume it's a code project if it can find a Source directory
	const bool bIsCodeProject = IFileManager::Get().DirectoryExists(*FPaths::GameSourceDir());
	return bIsCodeProject;
}

void FMainFrameActionCallbacks::OpenIDE()
{
	if ( !FSourceCodeNavigation::IsCompilerAvailable() )
	{
		// Attempt to trigger the tutorial if the user doesn't have a compiler installed for the project.
		FSourceCodeNavigation::AccessOnCompilerNotFound().Broadcast();
	}
	else
	{
		if ( !FSourceCodeNavigation::OpenModuleSolution() )
		{
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("OpenIDEFailed_UnableToOpenSolution", "Unable to open solution"));
		}
	}
}

bool FMainFrameActionCallbacks::CanOpenIDE()
{
	return IsCPPAllowed() && IsCodeProject();
}

bool FMainFrameActionCallbacks::IsOpenIDEVisible()
{
	return IsCPPAllowed() && FSourceCodeNavigation::DoesModuleSolutionExist();
}

bool FMainFrameActionCallbacks::IsCPPAllowed()
{
	return ensure(GUnrealEd) && GUnrealEd->GetUnrealEdOptions()->IsCPPAllowed();
}

void FMainFrameActionCallbacks::ZipUpProject()
{
	bool bOpened = false;
	TArray<FString> SaveFilenames;
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform != NULL)
	{
		bOpened = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			NSLOCTEXT("UnrealEd", "ZipUpProject", "Zip file location").ToString(),
			FPaths::ProjectDir(),
			FApp::GetProjectName(),
			TEXT("Zip file|*.zip"),
			EFileDialogFlags::None,
			SaveFilenames);
	}

	if (bOpened)
	{
		for (FString FileName : SaveFilenames)
		{
			// Ensure path is full rather than relative (for macs)
			FString FinalFileName = FPaths::ConvertRelativePathToFull(FileName);
			FString ProjectPath = FPaths::IsProjectFilePathSet() ? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) : FPaths::RootDir() / FApp::GetProjectName();

			FString CommandLine = FString::Printf(TEXT("ZipProjectUp -nocompileeditor -project=\"%s\" -install=\"%s\""), *ProjectPath, *FinalFileName);

			IUATHelperModule::Get().CreateUatTask( CommandLine, GetTargetPlatformManager()->GetRunningTargetPlatform()->DisplayName(), LOCTEXT("ZipTaskName", "Zipping Up Project"),
				LOCTEXT("ZipTaskShortName", "Zip Project Task"), FAppStyle::GetBrush(TEXT("MainFrame.CookContent")), nullptr, IUATHelperModule::UatTaskResultCallack(), FPaths::GetPath(FinalFileName));
		}
	}
}

void FMainFrameActionCallbacks::SwitchProjectByIndex(int32 ProjectIndex)
{
	FUnrealEdMisc::Get().SwitchProject(RecentProjects[ProjectIndex].ProjectName);
}

void FMainFrameActionCallbacks::SwitchProject(const FString& GameOrProjectFileName)
{
	FUnrealEdMisc::Get().SwitchProject(GameOrProjectFileName);
}

void FMainFrameActionCallbacks::OpenBackupDirectory(FString BackupFile)
{
	FPlatformProcess::LaunchFileInDefaultExternalApplication(*FPaths::GetPath(FPaths::ConvertRelativePathToFull(BackupFile)));
}

void FMainFrameActionCallbacks::ToggleFullscreen_Execute()
{
#if !PLATFORM_MAC && !PLATFORM_LINUX // Fullscreen mode in the editor is currently unsupported on Mac or Linux
	if ( GIsEditor && FApp::HasProjectName() )
	{
		TSharedPtr<SDockTab> LevelEditorTabPtr = FGlobalTabmanager::Get()->TryInvokeTab(FTabId("LevelEditor"));
		const TSharedPtr<SWindow> LevelEditorWindow = FSlateApplication::Get().FindWidgetWindow( LevelEditorTabPtr.ToSharedRef() );

		if (LevelEditorWindow->GetWindowMode() == EWindowMode::Windowed)
		{
			LevelEditorWindow->SetWindowMode(EWindowMode::WindowedFullscreen);
		}
		else
		{
			LevelEditorWindow->SetWindowMode(EWindowMode::Windowed);
		}
	}
#endif
}

bool FMainFrameActionCallbacks::FullScreen_IsChecked()
{
	const TSharedPtr<SDockTab> LevelEditorTabPtr = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>( "LevelEditor" ).GetLevelEditorTab();

	const TSharedPtr<SWindow> LevelEditorWindow = LevelEditorTabPtr.IsValid()
		? LevelEditorTabPtr->GetParentWindow()
		: TSharedPtr<SWindow>();

	return (LevelEditorWindow.IsValid())
		? (LevelEditorWindow->GetWindowMode() != EWindowMode::Windowed)
		: false;
}


bool FMainFrameActionCallbacks::CanSwitchToProject( int32 InProjectIndex )
{
	if (FApp::HasProjectName() && RecentProjects[InProjectIndex].ProjectName.StartsWith(FApp::GetProjectName()))
	{
		return false;
	}

	if (FPaths::IsProjectFilePathSet() && RecentProjects[InProjectIndex].ProjectName == FPaths::GetProjectFilePath())
	{
		return false;
	}

	return true;
}

bool FMainFrameActionCallbacks::IsSwitchProjectChecked( int32 InProjectIndex )
{
	return CanSwitchToProject( InProjectIndex ) == false;
}


void FMainFrameActionCallbacks::Exit()
{
	FSlateApplication::Get().LeaveDebuggingMode();
	// Shut down the editor
	// NOTE: We can't close the editor from within this stack frame as it will cause various DLLs
	//       (such as MainFrame) to become unloaded out from underneath the code pointer.  We'll shut down
	//       as soon as it's safe to do so.
	GEngine->DeferredCommands.Add( TEXT("CLOSE_SLATE_MAINFRAME"));
}


bool FMainFrameActionCallbacks::Undo_CanExecute()
{
	return GUnrealEd->Trans->CanUndo() && FSlateApplication::Get().IsNormalExecution();
}

bool FMainFrameActionCallbacks::Redo_CanExecute()
{
	return GUnrealEd->Trans->CanRedo() && FSlateApplication::Get().IsNormalExecution();
}


void FMainFrameActionCallbacks::ExecuteExecCommand( FString Command )
{
	GUnrealEd->Exec( GEditor->GetEditorWorldContext(false).World(), *Command );
}


void FMainFrameActionCallbacks::OpenSlateApp_ViaModule( FName AppName, FName ModuleName )
{
	FModuleManager::Get().LoadModule( ModuleName );
	OpenSlateApp( AppName );
}

void FMainFrameActionCallbacks::OpenSlateApp( FName AppName )
{
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(AppName));
}

bool FMainFrameActionCallbacks::OpenSlateApp_IsChecked( FName AppName )
{
	return false;
}

void FMainFrameActionCallbacks::ReportABug()
{
	FString ReportABugURL;
	if (FUnrealEdMisc::Get().GetURL(TEXT("ReportABugURL"), ReportABugURL, false))
	{
		FPlatformProcess::LaunchURL(*ReportABugURL, NULL, NULL);
	}
}

void FMainFrameActionCallbacks::OpenIssueTracker()
{
	FString IssueTrackerURL;
	if (FUnrealEdMisc::Get().GetURL(TEXT("IssueTrackerURL"), IssueTrackerURL, false))
	{
		FPlatformProcess::LaunchURL(*IssueTrackerURL, NULL, NULL);
	}
}

void FMainFrameActionCallbacks::VisitSearchForAnswersPage()
{
	FString SearchForAnswersURL;
	if(FUnrealEdMisc::Get().GetURL( TEXT("SearchForAnswersURL"), SearchForAnswersURL, true ))
	{
		FPlatformProcess::LaunchURL( *SearchForAnswersURL, NULL, NULL );
	}
}

void FMainFrameActionCallbacks::VisitSupportWebSite()
{
	FString SupportWebsiteURL;
	if(FUnrealEdMisc::Get().GetURL( TEXT("SupportWebsiteURL"), SupportWebsiteURL, true ))
	{
		FPlatformProcess::LaunchURL( *SupportWebsiteURL, NULL, NULL );
	}
}

void FMainFrameActionCallbacks::VisitEpicGamesDotCom()
{
	FString EpicGamesURL;
	if(FUnrealEdMisc::Get().GetURL( TEXT("EpicGamesURL"), EpicGamesURL ))
	{
		FPlatformProcess::LaunchURL( *EpicGamesURL, NULL, NULL );
	}
}

void FMainFrameActionCallbacks::DocumentationHome()
{
	IDocumentation::Get()->OpenHome(FDocumentationSourceInfo(TEXT("help_menu")));
}

void FMainFrameActionCallbacks::VisitOnlineLearning()
{
	FString URL;
	if (FUnrealEdMisc::Get().GetURL(TEXT("OnlineLearningURL"), URL))
	{
		FPlatformProcess::LaunchURL(*URL, NULL, NULL);
	}
}

void FMainFrameActionCallbacks::BrowseAPIReference()
{
	IDocumentation::Get()->OpenAPIHome(FDocumentationSourceInfo(TEXT("help_menu")));
}

void FMainFrameActionCallbacks::BrowseCVars()
{
	GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("help"));
}

void FMainFrameActionCallbacks::VisitCommunityHome()
{
	FString URL;
	if (FUnrealEdMisc::Get().GetURL(TEXT("CommunityHomeURL"), URL))
	{
		FPlatformProcess::LaunchURL(*URL, NULL, NULL);
	}
}

void FMainFrameActionCallbacks::VisitForums()
{
	FString URL;
	if (FUnrealEdMisc::Get().GetURL(TEXT("ForumsURL"), URL))
	{
		FPlatformProcess::LaunchURL(*URL, NULL, NULL);
	}
}

void FMainFrameActionCallbacks::VisitCommunitySnippets()
{
	FString URL;
	if (FUnrealEdMisc::Get().GetURL(TEXT("SnippetsURL"), URL))
	{
		FPlatformProcess::LaunchURL(*URL, NULL, NULL);
	}
}

void FMainFrameActionCallbacks::AboutUnrealEd_Execute()
{
	const FText AboutWindowTitle = LOCTEXT( "AboutUnrealEditor", "About Unreal Editor" );

	TSharedPtr<SWindow> AboutWindow = 
		SNew(SWindow)
		.Title( AboutWindowTitle )
		.ClientSize(FVector2D(720.f, 538.f))
		.SupportsMaximize(false) .SupportsMinimize(false)
		.SizingRule( ESizingRule::FixedSize )
		[
			SNew(SAboutScreen)
		];

	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
	TSharedPtr<SWindow> ParentWindow = MainFrame.GetParentWindow();

	if ( ParentWindow.IsValid() )
	{
		FSlateApplication::Get().AddModalWindow(AboutWindow.ToSharedRef(), ParentWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(AboutWindow.ToSharedRef());
	}
}

void FMainFrameActionCallbacks::CreditsUnrealEd_Execute()
{
	const FText CreditsWindowTitle = LOCTEXT("CreditsUnrealEditor", "Credits");

	TSharedPtr<SWindow> CreditsWindow =
		SNew(SWindow)
		.Title(CreditsWindowTitle)
		.ClientSize(FVector2D(600.f, 700.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize)
		[
			SNew(SCreditsScreen)
		];

	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	TSharedPtr<SWindow> ParentWindow = MainFrame.GetParentWindow();

	if ( ParentWindow.IsValid() )
	{
		FSlateApplication::Get().AddModalWindow(CreditsWindow.ToSharedRef(), ParentWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(CreditsWindow.ToSharedRef());
	}
}

void FMainFrameActionCallbacks::OpenWidgetReflector_Execute()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId("WidgetReflector"));
}


/* FMainFrameActionCallbacks implementation
 *****************************************************************************/

void FMainFrameActionCallbacks::AddMessageLog( const FText& Text, const FText& Detail, const FString& TutorialLink, const FString& DocumentationLink )
{
	TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
	Message->AddToken(FTextToken::Create(Text));
	Message->AddToken(FTextToken::Create(Detail));
	if (!TutorialLink.IsEmpty())
	{
		Message->AddToken(FTutorialToken::Create(TutorialLink));
	}
	if (!DocumentationLink.IsEmpty())
	{
		Message->AddToken(FDocumentationToken::Create(DocumentationLink));
	}
	FMessageLog MessageLog("PackagingResults");
	MessageLog.AddMessage(Message);
	MessageLog.Open();
}

#undef LOCTEXT_NAMESPACE
