// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Modules/ModuleInterface.h"
#include "Widgets/SWindow.h"

class SMultiLineEditableTextBox;
class FOutputLogHistory;
class SOutputLog;
class SDockTab;
class FSpawnTabArgs;
struct FOutputLogCreationParams;
struct FOutputLogFilter;

/** Style of the debug console */
namespace EDebugConsoleStyle
{
	enum Type
	{
		/** Shows the debug console input line with tab completion only */
		Compact,

		/** Shows a scrollable log window with the input line on the bottom */
		WithLog,
	};
};

struct FDebugConsoleDelegates
{
	FSimpleDelegate OnFocusLost;
	FSimpleDelegate OnConsoleCommandExecuted;
	FSimpleDelegate OnCloseConsole;
};

class FOutputLogModule : public IModuleInterface
{
public:
	virtual void StartupModule();
	virtual void ShutdownModule();

	static OUTPUTLOG_API FOutputLogModule& Get();

	/** Returns whether debug console widgets should be hidden */
	virtual bool ShouldHideConsole() const;

	/** Generates a console input box widget.  Remember, this widget will become invalid if the
		output log DLL is unloaded on the fly. */
	virtual TSharedRef<SWidget> MakeConsoleInputBox(TSharedPtr<SMultiLineEditableTextBox>& OutExposedEditableTextBox, const FSimpleDelegate& OnCloseConsole, const FSimpleDelegate& OnConsoleCommandExecuted) const;
	
	virtual TSharedRef<SWidget> MakeOutputLogDrawerWidget(const FSimpleDelegate& OnCloseConsole);

	virtual TSharedRef<SWidget> MakeOutputLogWidget(const FOutputLogCreationParams& Params);
	
	/** Opens a debug console in the specified window, if not already open */
	virtual void ToggleDebugConsoleForWindow(const TSharedRef<SWindow>& Window, const EDebugConsoleStyle::Type InStyle, const FDebugConsoleDelegates& DebugConsoleDelegates);

	/** Closes the debug console for the specified window */
	virtual void CloseDebugConsole();

	virtual void ClearOnPIE(const bool bIsSimulating);

	virtual void FocusOutputLogConsoleBox(const TSharedRef<SWidget> OutputLogToFocus);

	virtual const TSharedPtr<SWidget> GetOutputLog() const;

	/** Opens and focuses on the Output Log Drawer if the status bar exists, otherwise opens and focuses on the Output Log Tab. */
	void OUTPUTLOG_API FocusOutputLog();

	const TSharedPtr<SDockTab> GetOutputLogTab() const { return OutputLogTab.Pin(); }

	struct FOutputFilterParams
	{
		TOptional<bool> bShowErrors;
		TOptional<bool> bShowWarnings;
		TOptional<bool> bShowLogs;
		TOptional<TSet<ELogVerbosity::Type>> IgnoreFilterVerbosities;
	};

	/** Change the output log's filter. If CategoriesToShow is empty, all categories will be shown. */
	void OUTPUTLOG_API UpdateOutputLogFilter(const TArray<FName>& CategoriesToShow, TOptional<bool> bShowErrors = TOptional<bool>(), TOptional<bool> bShowWarnings = TOptional<bool>(), TOptional<bool> bShowLogs = TOptional<bool>());
	void OUTPUTLOG_API UpdateOutputLogFilter(const TArray<FName>& CategoriesToShow, const FOutputFilterParams& InParams);

	/** Opens the output log tab, or brings it to front if it's already open */
	void OUTPUTLOG_API OpenOutputLog() const;

	/** Returns the value of bCycleToOutputLogDrawer from the module OutputLogSettings. This function helps StatusBar to access properties in OutputLogSetting 
	through OutputLogModule to avoid dependencies. */
	virtual bool ShouldCycleToOutputLogDrawer() const;

private:
	TSharedRef<SDockTab> SpawnOutputLogTab(const FSpawnTabArgs& Args);
	void OnOutputLogTabClosed(TSharedRef<SDockTab> Tab);

	TSharedRef<SDockTab> SpawnDeviceOutputLogTab(const FSpawnTabArgs& Args);

private:
	/** Our global output log app spawner */
	TSharedPtr<FOutputLogHistory> OutputLogHistory;

	/** Caches the user selected Filters as the OutputLog tab can be closed and remade multiple times */
	TUniquePtr<FOutputLogFilter> OutputLogFilterCache;

	/** Our global active output log that belongs to a tab */
	TWeakPtr<SOutputLog> OutputLog;

	/** Global tab that the output log resides in */
	TWeakPtr<SDockTab> OutputLogTab;

	/** The output log that lives in a status bar drawer */
	TWeakPtr<SOutputLog> OutputLogDrawer;

	/** Weak pointer to a debug console that's currently open, if any */
	TWeakPtr<SWidget> DebugConsole;

	/** Weak pointer to the widget to focus once they console window closes */
	TWeakPtr<SWidget> PreviousKeyboardFocusedWidget;
};
