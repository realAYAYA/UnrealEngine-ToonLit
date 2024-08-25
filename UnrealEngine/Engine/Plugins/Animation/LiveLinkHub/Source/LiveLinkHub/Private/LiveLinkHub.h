// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkTypes.h"
#include "Templates/SubclassOf.h"
#include "Templates/UniquePtr.h"
#include "Toolkits/FConsoleCommandExecutor.h"

class FLiveLinkHubClient;
class FLiveLinkHubClientsController;
class FLiveLinkHubPlaybackController;
class FLiveLinkHubRecordingController;
class FLiveLinkHubRecordingListController;
class FLiveLinkHubSubjectController;
class FLiveLinkHubWindowController;
struct FLiveLinkSubjectKey;
class FLiveLinkHubProvider;
class ILiveLinkHubSessionManager;
class SWindow;
class ULiveLinkRole;

/**
 * Main interface for the live link hub.
 */
class ILiveLinkHub
{
public:
	virtual ~ILiveLinkHub() = default;

	/** Whether the hub is currently playing a recording. */
	virtual bool IsInPlayback() const = 0;
	/** Whether the hub is currently recording livelink data. */
	virtual bool IsRecording() const = 0;
};

/**
 * Implementation of the live link hub.
 * Contains the apps' different components and is responsible for handling communication between them.
 */
class FLiveLinkHub : public ILiveLinkHub, public TSharedFromThis<FLiveLinkHub>
{
public:
	virtual ~FLiveLinkHub() override;

	//~ Begin ILiveLinkHub interface
	virtual bool IsInPlayback() const override;
	virtual bool IsRecording() const override;
	//~ End ILiveLinkHub interface

public:
	/** Launch the slate application and initialize its components. */
	void Initialize();
	/** Tick the hub. */
	void Tick();
	/** Get the root window that hosts the hub's slate application. */
	TSharedRef<SWindow> GetRootWindow() const;
	/** Get the livelink provider used to rebroadcast livelink data to connected UE clients. */
	TSharedPtr<FLiveLinkHubProvider> GetLiveLinkProvider() const;
	/** Get the controller that manages recording livelink data. */
	TSharedPtr<FLiveLinkHubRecordingController> GetRecordingController() const;
	/** Get the recording list controller, that handles displaying livelink recording assets. */
	TSharedPtr<FLiveLinkHubRecordingListController> GetRecordingListController() const;
	/** Get the controller that manages playing back livelink data. */
	TSharedPtr<FLiveLinkHubPlaybackController> GetPlaybackController() const;
	/** Get the controller that manages clients. */
	TSharedPtr<FLiveLinkHubClientsController> GetClientsController() const;
	/** Get the live link hub command list. */
	TSharedPtr<FUICommandList> GetCommandList() const { return CommandList; }
	/** Get the session manager. */
	TSharedPtr<ILiveLinkHubSessionManager> GetSessionManager() const;
	
private:
	//~ LiveLink Client delegates
	void OnStaticDataReceived_AnyThread(const FLiveLinkSubjectKey& InSubjectKey, TSubclassOf<ULiveLinkRole> InRole, const FLiveLinkStaticDataStruct& InStaticDataStruct) const;
	void OnFrameDataReceived_AnyThread(const FLiveLinkSubjectKey& InSubjectKey, const FLiveLinkFrameDataStruct& InFrameDataStruct) const;
	void OnSubjectMarkedPendingKill_AnyThread(const FLiveLinkSubjectKey& InSubjectKey) const;
	void OnSubjectAdded(FLiveLinkSubjectKey InSubjectKey) const;
	//~ LiveLink Client delegates

	/** Bind all available live link hub commands. */
	void BindCommands();

	/** Clear all client settings. */
	FName GetSubjectNameOverride(const FLiveLinkSubjectKey& InSubjectKey) const;

	/** Create a new config. */
	void NewConfig();

	/** Save an existing config to a new file. */
	void SaveConfigAs();

	/** Whether the Save command can be used. */
	bool CanSaveConfig() const;
	
	/** Save the config to the current file. */
	void SaveConfig();

	/** Open an existing config. */
	void OpenConfig();
	
private:
	/** Implements the logic to manage the clients tabs. */
	TSharedPtr<FLiveLinkHubClientsController> ClientsController;
	/**  Implements the logic for triggering recording. */
	TSharedPtr<FLiveLinkHubRecordingController> RecordingController;
	/** Implements the logic for displaying the list of recordings. */
	TSharedPtr<FLiveLinkHubRecordingListController> RecordingListController;
	/** Implements the logic for triggering the playback of a livelink recording. */
	TSharedPtr<FLiveLinkHubPlaybackController> PlaybackController;
	/** Implements the controller responsible for displaying and managing subject data. */
	TSharedPtr<FLiveLinkHubSubjectController> SubjectController;
	/** Controller responsible for creating and managing the app's slate windows. */
	TSharedPtr<FLiveLinkHubWindowController> WindowController;
	/** Object responsible for managing sessions.  */
	TSharedPtr<ILiveLinkHubSessionManager> SessionManager;
	/** LiveLinkHub's livelink client. */
	TSharedPtr<FLiveLinkHubClient> LiveLinkHubClient;
	/** LiveLinkProvider used to transfer data to connected UE clients. */
	TSharedPtr<FLiveLinkHubProvider> LiveLinkProvider;
	/** Handles execution of commands. */
	TUniquePtr<FConsoleCommandExecutor> CommandExecutor;
	/** Available live link hub commands. */
	TSharedPtr<FUICommandList> CommandList;
	/** The last opened config path. */
	FString LastConfigPath;

	friend class FLiveLinkHubModule;
};
