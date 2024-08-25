// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Framework/Docking/LayoutExtender.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxExtender.h"

class FExtender;
class FWorkspaceItem;

namespace UE
{
namespace Trace
{
	class FStoreClient;
}
}

namespace TraceServices
{
	class IAnalysisSession;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Major tab IDs for Insights tools */
struct TRACEINSIGHTS_API FInsightsManagerTabs
{
	static const FName StartPageTabId; // DEPRECATED
	static const FName TraceStoreTabId;
	static const FName ConnectionTabId;
	static const FName LauncherTabId;
	static const FName SessionInfoTabId;
	static const FName TimingProfilerTabId;
	static const FName LoadingProfilerTabId;
	static const FName NetworkingProfilerTabId;
	static const FName MemoryProfilerTabId;
	static const FName AutomationWindowTabId;
	static const FName MessageLogTabId;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Tab IDs for the timing profiler */
struct TRACEINSIGHTS_API FTimingProfilerTabs
{
	// Tab identifiers
	static const FName ToolbarID; // DEPRECATED
	static const FName FramesTrackID;
	static const FName TimingViewID;
	static const FName TimersID;
	static const FName CallersID;
	static const FName CalleesID;
	static const FName StatsCountersID;
	static const FName LogViewID;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Configuration for an Insights major tab */
struct TRACEINSIGHTS_API FInsightsMajorTabConfig
{
	FInsightsMajorTabConfig()
		: Layout(nullptr)
		, WorkspaceGroup(nullptr)
		, bIsAvailable(true)
	{}

	/** Helper function for creating unavailable tab configs */
	static FInsightsMajorTabConfig Unavailable()
	{
		FInsightsMajorTabConfig Config;
		Config.bIsAvailable = false;
		return Config;
	}

	/** Identifier for this config */
	FName ConfigId;

	/** Display name for this config */
	FText ConfigDisplayName;

	/** Label for the tab. If this is not set the default will be used */
	TOptional<FText> TabLabel;

	/** Tooltip for the tab. If this is not set the default will be used */
	TOptional<FText> TabTooltip;

	/** Icon for the tab. If this is not set the default will be used */
	TOptional<FSlateIcon> TabIcon;

	/** The tab layout to use. If not specified, the default will be used. */
	TSharedPtr<FTabManager::FLayout> Layout;

	/** The menu workspace group to use. If not specified, the default will be used. */
	TSharedPtr<FWorkspaceItem> WorkspaceGroup;

	/** Whether the tab is available for selection (i.e. registered with the tab manager) */
	bool bIsAvailable;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Combination of extenders applied to the individual major tabs within Insights */
struct TRACEINSIGHTS_API FInsightsMajorTabExtender
{
	FInsightsMajorTabExtender(TSharedPtr<FTabManager>& InTabManager, TSharedRef<FWorkspaceItem> InWorkspaceGroup)
		: MenuExtender(MakeShared<FExtender>())
		, TabManager(InTabManager)
		, WorkspaceGroup(InWorkspaceGroup)
	{}

	TSharedPtr<FExtender>& GetMenuExtender() { return MenuExtender; }
	FLayoutExtender& GetLayoutExtender() { return LayoutExtender; }
	FMinorTabConfig& AddMinorTabConfig() { return MinorTabs.AddDefaulted_GetRef(); }
	TSharedPtr<FTabManager> GetTabManager() const { return TabManager; }
	TSharedRef<FWorkspaceItem> GetWorkspaceGroup() { return WorkspaceGroup; }
	const TArray<FMinorTabConfig>& GetMinorTabs() const { return MinorTabs; }

protected:
	/** Extender used to add to the menu for this tab */
	TSharedPtr<FExtender> MenuExtender;

	/** Any additional minor tabs to add */
	TArray<FMinorTabConfig> MinorTabs;

	/** Extender used when creating the layout for this tab */
	FLayoutExtender LayoutExtender;

	/** Tab manager for this major tab*/
	TSharedPtr<FTabManager> TabManager;

	TSharedRef<FWorkspaceItem> WorkspaceGroup;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Contains parameters that are passed to the CreateSessionBrowser function to control specific behaviors. */
struct TRACEINSIGHTS_API FCreateSessionBrowserParams
{
	bool bAllowDebugTools = false;
	bool bInitializeTesting = false;
	bool bStartProcessWithStompMalloc = false;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Called back to register common layout extensions */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRegisterMajorTabExtensions, FInsightsMajorTabExtender& /*MajorTabExtender*/);

/** Delegate invoked when a major tab is created */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInsightsMajorTabCreated, FName /*MajorTabId*/, TSharedRef<FTabManager> /*TabManager*/)

class TRACEINSIGHTS_API IInsightsComponent;

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Interface for an Unreal Insights module. */
class TRACEINSIGHTS_API IUnrealInsightsModule : public IModuleInterface
{
public:
	/**
	 * Registers an IInsightsComponent. The component will Initialize().
	 */
	virtual void RegisterComponent(TSharedPtr<IInsightsComponent> Component) = 0;

	/**
	 * Unregisters an IInsightsComponent. The component will Shutdown().
	 */
	virtual void UnregisterComponent(TSharedPtr<IInsightsComponent> Component) = 0;

	//////////////////////////////////////////////////

	/**
	 * Creates the default trace store (for "Browser" mode).
	 */
	virtual void CreateDefaultStore() = 0;

	/**
	 * Gets the default trace store (for "Browser" mode).
	*/
	virtual FString GetDefaultStoreDir() = 0;

	/**
	 * Gets the store client.
	 */
	virtual UE::Trace::FStoreClient* GetStoreClient() = 0;

	/**
	 * Connects to a specified store.
	 *
	 * @param InStoreHost The host of the store to connect to.
	 * @param InStorePort The port of the store to connect to.
	 * @return If connected succesfully or not.
	 */
	virtual bool ConnectToStore(const TCHAR* InStoreHost, uint32 InStorePort=0) = 0;

	//////////////////////////////////////////////////

	/**
	 * Gets the current analysis session.
	 */
	virtual TSharedPtr<const TraceServices::IAnalysisSession> GetAnalysisSession() const = 0;

	/**
	 * Starts analysis of the specified trace. Called when the application starts in "Viewer" mode.
	 *
	 * @param InTraceId The id of the trace to analyze.
	 * @param InAutoQuit - The Application will close when session analysis is complete or fails to start
	 */
	virtual void StartAnalysisForTrace(uint32 InTraceId, bool InAutoQuit = false) = 0;

	/**
	 * Starts analysis of the last live session. Called when the application starts in "Viewer" mode.
	 * On failure, if InRetryTime is > 0, retry connecting every frame for RetryTime seconds 
	 * 
	 * @param InRetryTime How many seconds to retry connecting asyncronously
	 */
	virtual void StartAnalysisForLastLiveSession(float InRetryTime = 1.0f) = 0;

	/**
	 * Starts analysis of the specified *.utrace file. Called when the application starts in "Viewer" mode.
	 *
	 * @param InTraceFile The filename (*.utrace) of the trace to analyze.
	 * @param InAutoQuit - The Application will close when session analysis is complete or fails to start
	 */
	virtual void StartAnalysisForTraceFile(const TCHAR* InTraceFile, bool InAutoQuit = false) = 0;

	//////////////////////////////////////////////////

	/**
	 * Registers a major tab layout. This defines how the major tab will appear when spawned.
	 * If this is not called prior to tabs being spawned then the built-in default layout will be used.
	 * @param InMajorTabId The major tab ID we are supplying a layout to
	 * @param InConfig The config to use when spawning the major tab
	 */
	virtual void RegisterMajorTabConfig(const FName& InMajorTabId, const FInsightsMajorTabConfig& InConfig) = 0;

	/**
	 * Unregisters a major tab layout. This will revert the major tab to spawning with its default layout
	 * @param InMajorTabId The major tab ID we are supplying a layout to
	 */
	virtual void UnregisterMajorTabConfig(const FName& InMajorTabId) = 0;

	/**
	 * Allows for registering a delegate callback for populating a FInsightsMajorTabExtender structure.
	 * @param InMajorTabId The major tab ID to register the delegate for
	 */
	virtual FOnRegisterMajorTabExtensions& OnRegisterMajorTabExtension(const FName& InMajorTabId) = 0;

	/** Callback invoked when a major tab is created */
	virtual FOnInsightsMajorTabCreated& OnMajorTabCreated() = 0;

	/** Finds a major tab config for the specified id. */
	virtual const FInsightsMajorTabConfig& FindMajorTabConfig(const FName& InMajorTabId) const = 0;

	virtual FOnRegisterMajorTabExtensions* FindMajorTabLayoutExtension(const FName& InMajorTabId) = 0;

	/** Sets the ini path for saving persistent layout data. */
	virtual void SetUnrealInsightsLayoutIni(const FString& InIniPath) = 0;

	/**
	 * Called when the application starts in "Browser" mode.
	 */
	virtual void CreateSessionBrowser(const FCreateSessionBrowserParams& Params) = 0;

	/**
	 * Called when the application starts in "Viewer" mode.
	 */
	virtual void CreateSessionViewer(bool bAllowDebugTools = false) = 0;

	/**
	 * Called when the application shutsdown.
	 */
	virtual void ShutdownUserInterface() = 0;

	/**
	* Called to schedule a command to run after session analysis is complete. Intended for running Automation RunTests commands.
	*/
	virtual void ScheduleCommand(const FString& InCmd) = 0;

	/**
	* Called to run automation test in Insights.
	*/
	virtual void RunAutomationTest(const FString& InCmd) = 0;

	/**
	* Called to initialize testing in stand alone Insights.
	 * @param InInitAutomationModules If true Insights will initialize the modules required for running automation tests.
	 * @param InAutoQuit If true Insights will close after completing session analysis and running any tests started using the ScheduleCommand function.
	*/
	virtual void InitializeTesting(bool InInitAutomationModules, bool InAutoQuit) = 0;

	/** Execute command. */
	virtual bool Exec(const TCHAR* Cmd, FOutputDevice& Ar) = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class TRACEINSIGHTS_API IInsightsComponent
{
public:
	/** Initializes this component. Called by TraceInsights module when this component is registered. */
	virtual void Initialize(IUnrealInsightsModule& Module) = 0;

	/** Shutsdown this component. Called by TraceInsights module when this component is unregistered. */
	virtual void Shutdown() = 0;

	/* Allows this component to register major tabs. */
	virtual void RegisterMajorTabs(IUnrealInsightsModule& InsightsModule) = 0;

	/* Requests this component to unregister its major tabs. */
	virtual void UnregisterMajorTabs() = 0;

	/* Called by the TraceInsights module when it receives the OnWindowClosedEvent. Can be used to close any panels that should not persist in the layout. */
	virtual void OnWindowClosedEvent() {}

	/** Execute command. */
	virtual bool Exec(const TCHAR* Cmd, FOutputDevice& Ar) { return false; }
};

////////////////////////////////////////////////////////////////////////////////////////////////////
