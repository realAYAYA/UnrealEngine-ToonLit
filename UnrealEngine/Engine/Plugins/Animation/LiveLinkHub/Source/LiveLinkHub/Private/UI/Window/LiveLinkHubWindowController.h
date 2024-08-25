// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Docking/TabManager.h"

class FLiveLinkHubMainTabController;
class FLiveLinkHubSessionTabBase;
class FOutputLogController;
class ILiveLinkClient;
class ILiveLinkHubComponent;
class FModalWindowManager;
class SWindow;

struct FLiveLinkHubWindowInitParams
{
	/** Config path for hub layout ini */
	FString LiveLinkHubLayoutIni;

	FLiveLinkHubWindowInitParams() = default;

	FLiveLinkHubWindowInitParams(FString LiveLinkHubLayoutIni)
		: LiveLinkHubLayoutIni(MoveTemp(LiveLinkHubLayoutIni))
	{}
};

/** Responsible for creating the Slate window for the hub. */
class FLiveLinkHubWindowController : public TSharedFromThis<FLiveLinkHubWindowController>
{
public:
	
	FLiveLinkHubWindowController(const FLiveLinkHubWindowInitParams& Params);
	~FLiveLinkHubWindowController();

	/** Get the root window. */
	TSharedPtr<SWindow> GetRootWindow() const { return RootWindow; }
	/** Restore the window's layout from a config. */
	void RestoreLayout();
private:
	/** Create the main window. */
	TSharedRef<SWindow> CreateWindow();
	/** Create the slate application that hosts the livelink hub. */
	TSharedPtr<FModalWindowManager> InitializeSlateApplication();
	/** Initialize components. */
	void InitComponents(const TSharedRef<FTabManager::FStack>& MainArea);
	/** Window closed handler. */
	void OnWindowClosed(const TSharedRef<SWindow>& Window);
	/** Save the layout to a json file. */
	void SaveLayout() const;
private:
	/** Pointer to the livelink client. */
	ILiveLinkClient* LiveLinkClient = nullptr;
	/** The ini file to use for saving the layout */
	FString LiveLinkHubLayoutIni;
	/** Holds the current layout for saving later. */
	TSharedPtr<FTabManager::FLayout> PersistentLayout;
	/** The main window being managed */
	TSharedPtr<SWindow> RootWindow;
	/** Manages the main tab. */
	TSharedRef<FLiveLinkHubMainTabController> MainTabController;
	/** LiveLinkHub components */
	TArray<TSharedRef<ILiveLinkHubComponent>> LiveLinkHubComponents;
	/** Manages modal windows for the application. */
	TSharedPtr<FModalWindowManager> ModalWindowManager;
	/** Menu bar widget for the hub. */
	TSharedPtr<class SWindowTitleBar> WindowTitleBar;

};
