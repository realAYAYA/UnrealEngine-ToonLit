// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "NetworkReplayStreaming.h"
#include "LocalFileNetworkReplayStreaming.h"

enum class EGameDelegates_SaveGame : short;

/**
 * Local file streamer that supports playback/recording to files on disk, and transferring replays to and from SaveGame slots.
 *
 * EnumerateStreams may be used to list all available replays that are in SaveGame slots.
 * The Name member in any FNetworkReplayStreamInfo returned will be the SaveGame slot where the replay lives.
 *
 * EnumerateRecentStreams may be used to list all available replays that are not in SaveGame slots.
 * The Name member in any FNetworkReplayStreamInfo returned will be the relative path where the replay lives.
 *
 * StartStreaming can be used to play replays both in and not in SaveGame slots.
 * StartStreaming does not automatically put a replay in a SaveGame slot.
 *
 * KeepReplay can be used to move a non SaveGame slot replay into a SaveGame slot. The original replay is left untouched.
 *
 * DeleteFinishedStream can be used to delete replays both in and not in SaveGame slots.
 *
 * Only one Save Game operation is permitted to occur at a single time (even across Streamers).
 *
 * TODO: Proper handling of UserIndex.
 */
class SAVEGAMENETWORKREPLAYSTREAMING_API FSaveGameNetworkReplayStreamer : public FLocalFileNetworkReplayStreamer
{
public:

	FSaveGameNetworkReplayStreamer();
	FSaveGameNetworkReplayStreamer(const FString& DemoSavePath, const FString& PlaybackReplayName);

	/** INetworkReplayStreamer implementation */
	virtual void StartStreaming(const FStartStreamingParameters& Params, const FStartStreamingCallback& Delegate) override;
	virtual void DeleteFinishedStream(const FString& ReplayName, const FDeleteFinishedStreamCallback& Delegate) override;
	virtual void DeleteFinishedStream(const FString& StreamName, const int32 UserIndex, const FDeleteFinishedStreamCallback& Delegate) override;
	virtual void EnumerateStreams(const FNetworkReplayVersion& InReplayVersion, const int32 UserIndex, const FString& MetaString, const TArray< FString >& ExtraParms, const FEnumerateStreamsCallback& Delegate) override;
	virtual void EnumerateRecentStreams(const FNetworkReplayVersion& ReplayVersion, const int32 UserIndex, const FEnumerateStreamsCallback& Delegate) override;
	virtual void KeepReplay(const FString& ReplayName, const bool bKeep, const FKeepReplayCallback& Delegate) override;
	virtual void KeepReplay(const FString& ReplayName, const bool bKeep, const int32 UserIndex, const FKeepReplayCallback& Delegate) override;
	virtual void RenameReplayFriendlyName(const FString& ReplayName, const FString& NewFriendlyName, const FRenameReplayCallback& Delegate) override;
	virtual void RenameReplayFriendlyName(const FString& ReplayName, const FString& NewFriendlyName, const int32 UserIndex, const FRenameReplayCallback& Delegate) override;
	virtual void RenameReplay(const FString& ReplayName, const FString& NewName, const FRenameReplayCallback& Delegate) override;
	virtual void RenameReplay(const FString& ReplayName, const FString& NewName, const int32 UserIndex, const FRenameReplayCallback& Delegate) override;

	virtual void EnumerateEvents(const FString& ReplayName, const FString& Group, const FEnumerateEventsCallback& Delegate) override;
	virtual void EnumerateEvents( const FString& ReplayName, const FString& Group, const int32 UserIndex, const FEnumerateEventsCallback& Delegate) override;
	virtual void RequestEventData(const FString& EventID, const FRequestEventDataCallback& Delegate) override;
	virtual void RequestEventData(const FString& ReplayName, const FString& EventID, const FRequestEventDataCallback& Delegate) override;
	virtual void RequestEventData(const FString& ReplayName, const FString& EventId, const int32 UserIndex, const FRequestEventDataCallback& Delegate) override;
	virtual void RequestEventGroupData(const FString& Group, const FRequestEventGroupDataCallback& Delegate) override;
	virtual void RequestEventGroupData(const FString& ReplayName, const FString& Group, const FRequestEventGroupDataCallback& Delegate) override;
	virtual void RequestEventGroupData(const FString& ReplayName, const FString& Group, const int32 UserIndex, const FRequestEventGroupDataCallback& Delegate) override;
protected:

	void StartStreamingSaved(const FStartStreamingParameters& Params, const FStartStreamingCallback& Delegate);
	void DeleteFinishedStreamSaved(const FString& ReplayName, const int32 UserIndex, const FDeleteFinishedStreamCallback& Delegate) const;
	void KeepReplaySaved(const FString& ReplayName, const bool bKeep, const int32 UserIndex, const FKeepReplayCallback& Result);
	void RenameReplayFriendlyNameSaved(const FString& ReplayName, const FString& NewFriendlyName, const int32 UserIndex, const FRenameReplayCallback& Delegate) const;
	void RenameReplaySaved(const FString& ReplayName, const FString& NewName, const int32 UserIndex, const FRenameReplayCallback& Delegate);
	void EnumerateEventsSaved(const FString& ReplayName, const FString& Group, const int32 UserIndex, const FEnumerateEventsCallback& Delegate) const;
	void RequestEventDataSaved(const FString& ReplayName, const FString& EventID, const int32 UserIndex, const FRequestEventDataCallback& Delegate);
	void RequestEventGroupDataSaved(const FString& ReplayName, const FString& Group, const int32 UserIndex, const FRequestEventGroupDataCallback& Delegate);

	void StartStreaming_Internal(const FStartStreamingParameters& Params, FStartStreamingResult& Result);
	void DeleteFinishedStream_Internal(const FString& ReplayName, const int32 UserIndex, FDeleteFinishedStreamResult& Result) const;
	void EnumerateStreams_Internal(const FNetworkReplayVersion& ReplayVersion, const int32 UserIndex, const FString& MetaString, const TArray< FString >& ExtraParms, FEnumerateStreamsResult& Result);
	void KeepReplay_Internal(const FString& ReplayName, const bool bKeep, const int32 UserIndex, FKeepReplayResult& Result);
	void RenameReplayFriendlyName_Internal(const FString& ReplayName, const FString& NewFriendlyName, const int32 UserIndex, FRenameReplayResult& Result) const;
	void EnumerateEvents_Internal(const FString& ReplayName, const FString& Group, const int32 UserIndex, FEnumerateEventsResult& Result) const;
	void RequestEventData_Internal(const FString& ReplayName, const FString& EventID, const int32 UserIndex, FRequestEventDataResult& Result);
	void RequestEventGroupData_Internal(const FString& ReplayName, const FString& Group, const int32 UserIndex, FRequestEventGroupDataResult& Result);

	struct FSaveGameReplayVersionedInfo
	{
		// Save game file version.
		uint32 FileVersion;

		// Events that are serialized in the header.
		FReplayEventList Events;

		// Actual event data. Indices correlate to Event index.
		TArray<TArray<uint8>> EventData;
	};

	struct FSaveGameMetaData
	{
		FString ReplayName;

		FLocalFileReplayInfo ReplayInfo;

		FSaveGameReplayVersionedInfo VersionedInfo;
	};

	struct FSaveGameSanitizedNames
	{
		FString ReplayMetaName;
		FString ReplayName;
		int32 ReplayIndex;
	};

	bool StreamNameToSanitizedNames(const FString& StreamName, FSaveGameSanitizedNames& OutSanitizedNames) const;
	void ReplayIndexToSanitizedNames(const int32 ReplayIndex, FSaveGameSanitizedNames& OutSanitizedNames) const;

	bool ReadMetaDataFromLocalStream(FArchive& Archive, FSaveGameMetaData& OutMetaData) const;
	bool ReadMetaDataFromSaveGame(class ISaveGameSystem& SaveGameSystem, const FSaveGameSanitizedNames& SanitizedNames, const int32 UserIndex, FSaveGameMetaData& OutMetaData, FStreamingResultBase& OutResult) const;
	void PopulateStreamInfoFromMetaData(const FSaveGameMetaData& MetaData, FNetworkReplayStreamInfo& OutStreamInfo) const;

	bool SerializeMetaData(FArchive& Archive, FSaveGameMetaData& MetaData) const;
	bool SerializeVersionedMetaData(FArchive& Archive, FSaveGameMetaData& MetaData) const;

	// Returns whether the input name corresponds to a save game.
	bool IsSaveGameFileName(const FString& ReplayName) const;

	int32 GetReplayIndexFromName(const FString& ReplayName) const;

	FString GetFullPlaybackName() const;
	FString GetLocalPlaybackName() const;

	virtual TArrayView<const FString> GetAdditionalRelativeDemoPaths() const override;

	struct FSaveGameOptionInfo
	{
		EGameDelegates_SaveGame Option;
		bool bIsForRename = false;
		bool bIsSavingMetaData = false;
		int32 SaveDataSize = INDEX_NONE;

		FString ReplayFriendlyName;
		FString ReplaySaveName;
	};

	/**
	 * Called during KeepReplay to get options when saving a replay.
	 * Note, this may be called off the GameThread and may not be called on every platform.
	 * @see FGameDelegates::GetExtendedSaveGameInfoDelegate
	 *
	 * @param OptionInfo	Info struct that describes the requested option and current streamer status.
	 * @param OptionValue	(Out) The desired value for the option.
	 *
	 * @return True if this event was handled. False if it should be passed to the original delegate (
	 */
	virtual bool GetSaveGameOption(const FSaveGameOptionInfo& OptionInfo, FString& OptionValue) const { return false; }


	// Special replay name that will be used when copying over SaveGame replays for playback.
	const FString PlaybackReplayName;

	static const FString& GetDefaultDemoSavePath();
	static const FString& GetTempDemoRelativeSavePath();
	static const FString& GetDefaultPlaybackName();

private:

	TFunction<bool(const TCHAR*, EGameDelegates_SaveGame, FString&)> WrapGetSaveGameOption() const;

	// Although this isn't used on the GameThread, it should only be created / destroyed
	// from the same thread. Therefore, no need to make it thread safe (for now).
	mutable TWeakPtr<FSaveGameOptionInfo> WeakOptionInfo;

	friend class FSaveGameNetworkReplayStreamingFactory;
};

class SAVEGAMENETWORKREPLAYSTREAMING_API FSaveGameNetworkReplayStreamingFactory : public FLocalFileNetworkReplayStreamingFactory
{
public:
	virtual void StartupModule() override;

	virtual TSharedPtr<INetworkReplayStreamer> CreateReplayStreamer() override;
};
