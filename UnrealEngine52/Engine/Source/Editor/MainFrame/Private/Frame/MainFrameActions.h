// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "HAL/IConsoleManager.h"
#include "Input/Reply.h"
#include "Framework/Commands/Commands.h"

class FUICommandList;
struct FRecentProjectFile;

/**
 * Unreal editor main frame actions
 */
class FMainFrameCommands : public TCommands<FMainFrameCommands>
{
public:
	/** FMainFrameCommands constructor */
	FMainFrameCommands();

	/** List of all of the main frame commands */
	static TSharedRef< FUICommandList > ActionList;

	TSharedPtr< FUICommandInfo > SaveAll;
	TSharedPtr< FUICommandInfo > Exit;
	TSharedPtr< FUICommandInfo > ChooseFilesToSave;
	TSharedPtr< FUICommandInfo > ViewChangelists;
	TSharedPtr< FUICommandInfo > SubmitContent;
	TSharedPtr< FUICommandInfo > SyncContent;
	TSharedPtr< FUICommandInfo > ConnectToSourceControl;
	TSharedPtr< FUICommandInfo > ChangeSourceControlSettings;
	TSharedPtr< FUICommandInfo > NewProject;
	TSharedPtr< FUICommandInfo > OpenProject;
	TSharedPtr< FUICommandInfo > AddCodeToProject;
	TSharedPtr< FUICommandInfo > OpenIDE;
	TSharedPtr< FUICommandInfo > RefreshCodeProject;
	TSharedPtr< FUICommandInfo > ZipUpProject;
	TSharedPtr< FUICommandInfo > LocalizeProject;
	TArray< TSharedPtr< FUICommandInfo > > SwitchProjectCommands;

	TSharedPtr< FUICommandInfo > OpenContentBrowser;
	TSharedPtr< FUICommandInfo > OpenLevelEditor;
	TSharedPtr< FUICommandInfo > OpenOutputLog;
	TSharedPtr< FUICommandInfo > OpenMessageLog;
	TSharedPtr< FUICommandInfo > OpenKeybindings;
	TSharedPtr< FUICommandInfo > OpenSessionManagerApp;
	TSharedPtr< FUICommandInfo > OpenDeviceManagerApp;
	TSharedPtr< FUICommandInfo > OpenToolbox;
	TSharedPtr< FUICommandInfo > OpenDebugView;
	TSharedPtr< FUICommandInfo > OpenClassViewer;
	TSharedPtr< FUICommandInfo > OpenWidgetReflector;
	TSharedPtr< FUICommandInfo > OpenMarketplace;

	TSharedPtr< FUICommandInfo > DocumentationHome;
	TSharedPtr< FUICommandInfo > BrowseAPIReference;
	TSharedPtr< FUICommandInfo > BrowseCVars;
	TSharedPtr< FUICommandInfo > VisitCommunityHome;
	TSharedPtr< FUICommandInfo > VisitOnlineLearning;
	TSharedPtr< FUICommandInfo > VisitForums;
	TSharedPtr< FUICommandInfo > VisitSearchForAnswersPage;
	TSharedPtr< FUICommandInfo > VisitCommunitySnippets;
	TSharedPtr< FUICommandInfo > ReportABug;
	TSharedPtr< FUICommandInfo > OpenIssueTracker;
	TSharedPtr< FUICommandInfo > VisitSupportWebSite;
	TSharedPtr< FUICommandInfo > VisitEpicGamesDotCom;
	TSharedPtr< FUICommandInfo > AboutUnrealEd;
	TSharedPtr< FUICommandInfo > CreditsUnrealEd;

	// Layout
	TSharedPtr< FUICommandInfo > ImportLayout;
	TSharedPtr< FUICommandInfo > SaveLayoutAs;
	TSharedPtr< FUICommandInfo > ExportLayout;
	TSharedPtr< FUICommandInfo > RemoveUserLayouts;

	TSharedPtr< FUICommandInfo > ToggleFullscreen;

	virtual void RegisterCommands() override;


private:

	/** Console command for toggling full screen.  We need this to expose the full screen toggle action to
	    the game UI system for play-in-editor view ports */
	FAutoConsoleCommand ToggleFullscreenConsoleCommand;

	void RegisterLayoutCommands();
};


/**
 * Implementation of various main frame action callback functions
 */
class FMainFrameActionCallbacks
{

public:
	/** Global handler for unhandled key-down events in the editor. */
	static FReply OnUnhandledKeyDownEvent(const FKeyEvent& InKeyEvent);

	/**
	 * The default can execute action for all commands unless they override it
	 * By default commands cannot be executed if the application is in K2 debug mode.
	 */
	static bool DefaultCanExecuteAction();

	/** Determine whether we are allowed to save the world at this moment */
	static bool CanSaveWorld();

	/** Saves all levels and asset packages */
	static void SaveAll();
	
	/** Opens a dialog to choose packages to save */
	static void ChoosePackagesToSave();

	/** Opens a dialog to view the pending changelists */
	static void ViewChangelists();

	/** Determines whether we can show the changelist view */
	static bool CanViewChangelists();

	/** Enable source control features */
	static void ConnectToSourceControl();

	/** Quits the application */
	static void Exit();

	/** Edit menu commands */
	static bool Undo_CanExecute();
	static bool Redo_CanExecute();

	/**
	 * Called when many of the menu items in the main frame context menu are clicked
	 *
	 * @param Command	The command to execute
	 */
	static void ExecuteExecCommand( FString Command );

	/** Opens up the specified slate window by name after loading the module */
	static void OpenSlateApp_ViaModule( FName AppName, FName ModuleName );

	/** Opens up the specified slate window by name */
	static void OpenSlateApp( FName AppName );

	/** Checks if a slate window is already open */
	static bool OpenSlateApp_IsChecked( FName AppName );

	/** Visits the report a bug page */
	static void ReportABug();

	/** Opens the issue tracker page */
	static void OpenIssueTracker();

	/** Visits the UDN support web site */
	static void VisitSupportWebSite();

	/** Visits EpicGames.com */
	static void VisitEpicGamesDotCom();

	/** Opens the documentation home page*/
	static void DocumentationHome();

	/** Opens the API documentation site */
	static void BrowseAPIReference();

	/** Creates an HTML file to browse the console variables and commands */
	static void BrowseCVars();

	/** Visits the home page of the Epic Games Dev Community web site */
	static void VisitCommunityHome();

	/** Visits the Learning Library on the Epic Games Dev Community web site */
	static void VisitOnlineLearning();

	/** Visits the Unreal Engine community forums */
	static void VisitForums();

	/** Visits the Q&A section of the Unreal Engine community forums */
	static void VisitSearchForAnswersPage();

	/** Visits the Snippets Repository on the Epic Games Dev Community web site */
	static void VisitCommunitySnippets();

	static void AboutUnrealEd_Execute();

	static void CreditsUnrealEd_Execute();

	static void OpenWidgetReflector_Execute();

	/** Opens the new project dialog */
	static void NewProject( bool bAllowProjectOpening, bool bAllowProjectCreate );

	/** Adds code to the current project if it does not already have any */
	static void AddCodeToProject();

	/** Whether we can add code to the current project */
	static bool CanAddCodeToProject();

	/** Whether menu to add code to the current project is visible */
	static bool IsAddCodeToProjectVisible();

	/** Checks whether a menu action for packaging the project can execute. */
	static bool PackageProjectCanExecute( const FName PlatformInfoName );

	/** Refresh the project in the current IDE */
	static void RefreshCodeProject();

	/** Whether we can refresh the project in the current IDE */
	static bool CanRefreshCodeProject();

	/** Whether menu to refresh the project in the current IDE is visible */
	static bool IsRefreshCodeProjectVisible();

	/** Determines whether the project is a code project */
	static bool IsCodeProject();

	/** Opens an IDE to edit c++ code */
	static void OpenIDE();

	/** Whether we can open the IDE to edit c++ code */
	static bool CanOpenIDE();

	/** Whether menu to open the IDE to edit c++ code is visible */
	static bool IsOpenIDEVisible();

	/** Whether C++ edition/creation is allowed from the editor */
	static bool IsCPPAllowed();

	/** Zips up the project */
	static void ZipUpProject();

	/** Opens the Project Localization Dashboard */
	static void LocalizeProject();
	
	/** Restarts the editor and switches projects */
	static void SwitchProjectByIndex( int32 ProjectIndex );

	/** Opens the specified project file or game. Restarts the editor */
	static void SwitchProject(const FString& GameOrProjectFileName);

	/** Opens the directory where the backup for preferences is stored. */
	static void OpenBackupDirectory( FString BackupFile );

	/** Toggle the level editor's fullscreen mode */
	static void ToggleFullscreen_Execute();

	/** Is the level editor fullscreen? */
	static bool FullScreen_IsChecked();

	/** 
	 * Checks if the selected project can be switched to
	 *
	 * @param	InProjectIndex  Index from the available projects
	 *
	 * @return true if the selected project can be switched to (i.e. it's not the current project)
	 */
	static bool CanSwitchToProject( int32 InProjectIndex );

	/** 
	 * Checks which Switch Project sub menu entry should be checked
	 *
	 * @param	InProjectIndex  Index from the available projects
	 *
	 * @return true if the menu entry should be checked
	 */
	static bool IsSwitchProjectChecked( int32 InProjectIndex );

	/** Gathers all available projects the user can switch to from main menu */
	static void CacheProjectNames();

	/** Opens the marketplace */
	static void OpenMarketplace();
public:

	// List of projects that the user can switch to.
	static TArray<FRecentProjectFile> RecentProjects;

protected:

	/**
	 * Adds a message to the message log.
	 *
	 * @param Text The main message text.
	 * @param Detail The detailed description.
	 * @param TutorialLink A link to an associated tutorial.
	 * @param DocumentationLink A link to documentation.
	 */
	static void AddMessageLog( const FText& Text, const FText& Detail, const FString& TutorialLink, const FString& DocumentationLink );

private:

	/** The notification in place while we choose packages to check in */
	static TWeakPtr<SNotificationItem> ChoosePackagesToCheckInNotification;
};
