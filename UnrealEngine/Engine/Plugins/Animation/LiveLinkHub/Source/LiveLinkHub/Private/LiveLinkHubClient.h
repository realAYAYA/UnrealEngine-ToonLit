// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkClient.h"

#include "Delegates/DelegateCombinations.h"
#include "LiveLinkTypes.h"
#include "Templates/SubclassOf.h"

class FLiveLinkHubPlaybackController;
class FLiveLinkHubRecordingController;
class ILiveLinkHub;
struct ILiveLinkProvider;

DECLARE_TS_MULTICAST_DELEGATE_TwoParams(FOnFrameDataReceived_AnyThread, const FLiveLinkSubjectKey&, const FLiveLinkFrameDataStruct&);
DECLARE_TS_MULTICAST_DELEGATE_ThreeParams(FOnStaticDataReceived_AnyThread, const FLiveLinkSubjectKey&, TSubclassOf<ULiveLinkRole>, const FLiveLinkStaticDataStruct&);
DECLARE_TS_MULTICAST_DELEGATE_OneParam(FOnSubjectMarkedPendingKill_AnyThread, const FLiveLinkSubjectKey&);


class FLiveLinkHubClient : public FLiveLinkClient
{
public:
	FLiveLinkHubClient(TSharedPtr<ILiveLinkHub> InLiveLinkHub);
	virtual ~FLiveLinkHubClient();

	/** Utility method to grab a subject's static data. Used by the RecordingController when static data is missing from the recording. */
	const FLiveLinkStaticDataStruct* GetSubjectStaticData(const FLiveLinkSubjectKey& InSubjectKey);
	
	/** Get the delegate called when frame data is received. */
	FOnFrameDataReceived_AnyThread& OnFrameDataReceived_AnyThread()
	{
		return OnFrameDataReceivedDelegate_AnyThread;
	}

	/** Get the delegate called when static data is received. */
	FOnStaticDataReceived_AnyThread& OnStaticDataReceived_AnyThread()
	{
		return OnStaticDataReceivedDelegate_AnyThread;
	}

	/** 
	 * Get the delegate called when a subject is marked for deletion. 
	 * This delegate will fire as soon as the subject is marked for deletion, while the OnSubjectRemovedDelegate may trigger at a later time.
	 */
	FOnSubjectMarkedPendingKill_AnyThread& OnSubjectMarkedPendingKill_AnyThread()
	{
		return OnSubjectMarkedPendingKillDelegate_AnyThread;
	}

public:
	//~ Begin ILiveLinkClient interface
	virtual bool CreateSource(const FLiveLinkSourcePreset& InSourcePreset) override;
	virtual bool CreateSubject(const FLiveLinkSubjectPreset& InSubjectPreset) override;
	virtual void PushSubjectStaticData_AnyThread(const FLiveLinkSubjectKey& SubjectKey, TSubclassOf<ULiveLinkRole> Role, FLiveLinkStaticDataStruct&& InStaticData) override;
	virtual void PushSubjectFrameData_AnyThread(const FLiveLinkSubjectKey& SubjectKey, FLiveLinkFrameDataStruct&& FrameData) override;
	virtual FText GetSourceStatus(FGuid InEntryGuid) const override;
	virtual bool IsSubjectValid(const FLiveLinkSubjectKey& InSubjectKey) const override;
	virtual void RemoveSubject_AnyThread(const FLiveLinkSubjectKey& InSubjectKey) override;
	//~ End ILiveLinkClient interface

private:
	/** Create a LiveLinkPlaybackSource which acts as a dummy source when doing playback. */
	bool CreatePlaybackSource(const FLiveLinkSourcePreset& InSourcePreset);
	/** Create a LiveLinkPlaybackSubject which acts as a dummy subject when doing playback. */
	bool CreatePlaybackSubject(const FLiveLinkSubjectPreset& InSubjectPreset);
	/** Lock to stop multiple threads accessing the Subjects from the collection at the same time */
	mutable FCriticalSection CollectionAccessCriticalSection;

private:
	/** Weak pointer to the live link hub. */
	TWeakPtr<ILiveLinkHub> LiveLinkHub;
    /** Delegate called when frame data is received. */
    FOnFrameDataReceived_AnyThread OnFrameDataReceivedDelegate_AnyThread;
    /** Delegate called when static data is received. */
    FOnStaticDataReceived_AnyThread OnStaticDataReceivedDelegate_AnyThread;
	/** Delegate called when a subject is marked for deletion. */
	FOnSubjectMarkedPendingKill_AnyThread OnSubjectMarkedPendingKillDelegate_AnyThread;
};
