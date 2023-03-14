// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertTakeRecorderMessages.h"
#include "ConcertMessages.h"
#include "Delegates/IDelegateInstance.h"
#include "HAL/Platform.h"
#include "Misc/FrameNumber.h"
#include "UObject/StrongObjectPtr.h"
#include "TakePreset.h"
#include "ConcertSyncClient/Public/IConcertClientTransactionBridge.h"

#include "ConcertTakeRecorderClientSessionCustomization.h"

DECLARE_LOG_CATEGORY_EXTERN(LogConcertTakeRecorder, Log, All);

class IConcertSyncClient;
class IConcertClientSession;
struct FConcertSessionContext;
class UTakeRecorder;
enum class ECheckBoxState : uint8;
struct EVisibility;

/**
 * Take Recorder manager that is held by the client sync module that keeps track of when a take is started, stopped or cancelled.
 * Events are registered to client sessions that will then operate on the Take Recorder UIs
 */
class FConcertTakeRecorderManager : public FGCObject
{
public:
	/**
	 * Constructor - registers TakeRecorderInitialized handler with the take recorder module
	 */
	FConcertTakeRecorderManager();

	/**
	 * Destructor - unregisters TakeRecorderInitialized handler from the take recorder module
	 */
	~FConcertTakeRecorderManager();

	/**
	 * Register all custom take recorder events for the specified client session
	 *
	 * @param InSession	The client session to register custom events with
	 */
	void Register(TSharedRef<IConcertClientSession> InSession);

	/**
	 * Unregister previously registered custom take recorder events from the specified client session
	 *
	 * @param InSession The client session to unregister custom events from
	 */
	void Unregister(TSharedRef<IConcertClientSession> InSession);

private:
	//~ Take recorder delegate handlers
	void OnTakeRecorderInitialized(UTakeRecorder* TakeRecorder);
	void OnRecordingFinished(UTakeRecorder* TakeRecorder);
	void OnRecordingCancelled(UTakeRecorder* TakeRecorder);
	void OnFrameAdjustment(UTakeRecorder* TakeRecorder, const FFrameNumber& InPlaybackStartFrame);

	//~ Concert event handlers
	void OnTakeInitializedEvent(const FConcertSessionContext&, const FConcertTakeInitializedEvent& InEvent);
	void OnRecordingFinishedEvent(const FConcertSessionContext&, const FConcertRecordingFinishedEvent&);
	void OnRecordingCancelledEvent(const FConcertSessionContext&, const FConcertRecordingCancelledEvent&);

	void OnRecordSettingsChangeEvent(const FConcertSessionContext&, const FConcertRecordSettingsChangeEvent&);
	void OnMultiUserSyncChangeEvent(const FConcertSessionContext&, const FConcertMultiUserSyncChangeEvent&);

	void OnNamedLevelSequenceEvent(const FConcertSessionContext&, const FConcertRecordingNamedLevelSequenceEvent&);

	void OnSessionClientChanged(IConcertClientSession&, EConcertClientStatus ClientStatus, const FConcertSessionClientInfo& InClientInfo);
	void OnSessionConnectionChanged(IConcertClientSession&, EConcertConnectionStatus ConnectionStatus);

	//~ Widget extension handlers
	void RegisterExtensions();
	void UnregisterExtensions();

	void CreateExtensionWidget(TArray<TSharedRef<class SWidget>>& OutExtensions);
	void CreateRecordButtonOverlay(TArray<TSharedRef<SWidget>>& OutExtensions);

	SIZE_T RemoteRecorders() const;
	bool ShouldIconBeVisible() const;
	EVisibility GetMultiUserIconVisibility() const;

	bool IsTakeSyncEnabled() const;
	ECheckBoxState IsTakeSyncChecked() const;
	void HandleTakeSyncCheckBox(ECheckBoxState State) const;
	EVisibility HandleTakeSyncButtonVisibility() const;

	void UpdateSessionClientList();
	void DisconnectFromSession();
	void ConnectToSession(IConcertClientSession&);

	ETransactionFilterResult ShouldObjectBeTransacted(UObject* InObject, UPackage* InPackage);
private:
	FTakeRecorderParameters SetupTakeParametersForMultiuser(const FTakeRecorderParameters& Input);

	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		Collector.AddReferencedObject(Preset);
		Collector.AddReferencedObject(LastLevelSequence);
	}

	virtual FString GetReferencerName() const override
	{
		return TEXT("FConcertTakeRecorderManager");
	}

	void ReportRecordingError(FText &);
	bool CanRecord() const;
	bool CanAnyRecord() const;

	void SendInitialState(IConcertClientSession&);
	void OnTakeSyncPropertyChange(bool Value);

	void RecordSettingChange(const FConcertClientRecordSetting& RecordSetting);
	void AddRemoteClient(const FConcertSessionClientInfo& ClientInfo);

	void SetLastLevelSequence(ULevelSequence* InLastSequence);
	bool CanReviewLastRecordedSequence() const;

	/** Weak pointer to the client session with which to send events. May be null or stale. */
	TWeakPtr<IConcertClientSession> WeakSession;

	/**
	 * Used to prevent sending out events that were just received by this client.
	 */
	struct FTakeRecorderState
	{
		FString LastStartedTake;
		FString LastStoppedTake;
	} TakeRecorderState;

	UTakePreset* Preset = nullptr;
	ULevelSequence* LastLevelSequence = nullptr;

	TSharedPtr<FConcertTakeRecorderClientSessionCustomization> Customization;

	/** Delegate for any changes in client state. */
	FDelegateHandle				 ClientChangeDelegate;

	/** Delegate for any changes take sync status. */
	FDelegateHandle				 TakeSyncDelegate;

	/**
	 * Denotes if we are currently in recording mode.
	 */
	bool bIsRecording = false;

	/** Indicates if we have warned the user about multiple recorders. */
	bool bHaveWarned = false;
};
