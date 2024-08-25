// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkHubModule.h"
#include "Templates/SharedPointer.h"

class FLiveLinkHub;
class FLiveLinkHubPlaybackController;
class FLiveLinkHubProvider;
class FLiveLinkHubRecordingController;
class FLiveLinkHubRecordingListController;
class FLiveLinkHubSubjectController;
class FUICommandList;
class ILiveLinkHubSessionManager;

#ifndef WITH_LIVELINK_HUB
#define WITH_LIVELINK_HUB 0
#endif

class FLiveLinkHubModule : public ILiveLinkHubModule
{
public:
	//~ Begin ILiveLinkHubModule interface
	virtual void StartLiveLinkHub() override;
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End ILiveLinkHubModule interface

	/** Get the livelink hub object. */
	TSharedPtr<FLiveLinkHub> GetLiveLinkHub() const;
	/** Get the livelink provider responsible for forwarding livelink data to connected UE clients. */
	TSharedPtr<FLiveLinkHubProvider> GetLiveLinkProvider() const;
	/** Get the recording controller. */
	TSharedPtr<FLiveLinkHubRecordingController> GetRecordingController() const;
	/** Get the recording list controller. */
	TSharedPtr<FLiveLinkHubRecordingListController> GetRecordingListController() const;
	/** Get the playback controller. */
	TSharedPtr<FLiveLinkHubPlaybackController> GetPlaybackController() const;
	/** Get the subject controller. */
	TSharedPtr<FLiveLinkHubSubjectController> GetSubjectController() const;
	/** Get the subject controller. */
    TSharedPtr<ILiveLinkHubSessionManager> GetSessionManager() const;

#if !WITH_LIVELINK_HUB
	/** Launch livelink hub. */
	void OpenLiveLinkHub() const;
#endif
private:
	/** LiveLinkHub object responsible for initializing the different controllers. */
	TSharedPtr<FLiveLinkHub> LiveLinkHub;
};
