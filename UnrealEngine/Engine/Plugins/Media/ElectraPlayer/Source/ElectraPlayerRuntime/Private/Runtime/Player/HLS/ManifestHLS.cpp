// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCore.h"
#include "ElectraPlayerPrivate.h"
#include "ManifestHLS.h"
#include "ManifestBuilderHLS.h"
#include "PlaylistReaderHLS.h"
#include "InitSegmentCacheHLS.h"
#include "StreamReaderHLSfmp4.h"
#include "Player/PlayerSessionServices.h"
#include "Player/AdaptivePlayerOptionKeynames.h"
#include "SynchronizedClock.h"
#include "Utilities/StringHelpers.h"
#include "Utilities/URLParser.h"


DECLARE_CYCLE_STAT(TEXT("FPlayPeriodHLS::FindSegment"), STAT_ElectraPlayer_HLS_FindSegment, STATGROUP_ElectraPlayer);
DECLARE_CYCLE_STAT(TEXT("FPlayPeriodHLS::GetSegmentInformation"), STAT_ElectraPlayer_HLS_GetSegmentInformation, STATGROUP_ElectraPlayer);


namespace Electra
{

/**
 * Interface to a playback period.
 */
class FPlayPeriodHLS : public IManifest::IPlayPeriod
{
public:
	FPlayPeriodHLS(IPlayerSessionServices* SessionServices, IPlaylistReaderHLS* PlaylistReader, TSharedPtrTS<FManifestHLSInternal> Manifest);
	virtual ~FPlayPeriodHLS();

	void SetStreamPreferences(EStreamType ForStreamType, const FStreamSelectionAttributes& StreamAttributes) override;

	EReadyState GetReadyState() override;
	void Load() override;
	void PrepareForPlay() override;
	int64 GetDefaultStartingBitrate() const override;

	TSharedPtrTS<FBufferSourceInfo> GetSelectedStreamBufferSourceInfo(EStreamType StreamType) override;
	FString GetSelectedAdaptationSetID(EStreamType StreamType) override;
	ETrackChangeResult ChangeTrackStreamPreference(EStreamType ForStreamType, const FStreamSelectionAttributes& StreamAttributes) override;

	// TODO: need to provide metadata (duration, streams, languages, etc.)

	IManifest::FResult GetStartingSegment(TSharedPtrTS<IStreamSegment>& OutSegment, const FPlayerSequenceState& InSequenceState, const FPlayStartPosition& StartPosition, IManifest::ESearchType SearchType) override;
	IManifest::FResult GetContinuationSegment(TSharedPtrTS<IStreamSegment>& OutSegment, EStreamType StreamType, const FPlayerSequenceState& SequenceState, const FPlayStartPosition& StartPosition, IManifest::ESearchType SearchType) override;
	IManifest::FResult GetNextSegment(TSharedPtrTS<IStreamSegment>& OutSegment, TSharedPtrTS<const IStreamSegment> CurrentSegment, const FPlayStartOptions& Options) override;
	IManifest::FResult GetRetrySegment(TSharedPtrTS<IStreamSegment>& OutSegment, TSharedPtrTS<const IStreamSegment> CurrentSegment, const FPlayStartOptions& Options, bool bReplaceWithFillerData) override;
	IManifest::FResult GetLoopingSegment(TSharedPtrTS<IStreamSegment>& OutSegment, const FPlayerSequenceState& SequenceState, const FPlayStartPosition& StartPosition, IManifest::ESearchType SearchType) override;
	void IncreaseSegmentFetchDelay(const FTimeValue& IncreaseAmount) override;

	// Obtains information on the stream segmentation of a particular stream starting at a given current reference segment (optional, if not given returns suitable default values).
	void GetSegmentInformation(TArray<FSegmentInformation>& OutSegmentInformation, FTimeValue& OutAverageSegmentDuration, TSharedPtrTS<const IStreamSegment> CurrentSegment, const FTimeValue& LookAheadTime, const FString& AdaptationSetID, const FString& RepresentationID) override;

	TSharedPtrTS<ITimelineMediaAsset> GetMediaAsset() const override;

	void SelectStream(const FString& AdaptationSetID, const FString& RepresentationID) override;
	void TriggerInitSegmentPreload(const TArray<FInitSegmentPreload>& InitSegmentsToPreload) override;

private:
	struct FSegSearchParam
	{
		FSegSearchParam()
			: MediaSequence(-1)
			, DiscontinuitySequence(-1)
			, LocalIndex(-1)
			, StreamUniqueID(0)
			, bFrameAccurateSearch(false)
			, TimestampSequenceIndex(0)
		{
		}
		FTimeValue	Time;						//!< Time to search for.
		FTimeValue	Duration;					//!< If set we search for a start time of Time + Duration (aka the next segment)
		int64		MediaSequence;				//!< If >= 0 we are searching for a specific segment based on media sequence number
		int64		DiscontinuitySequence;		//!< If >= 0 we are searching for a segment after the this discontinuity sequence number
		int32		LocalIndex;
		uint32		StreamUniqueID;				//!< If != 0 the search is for the same stream as was the previous segment. We can use the media sequence index.
		FTimeValue	LastPTS;
		bool		bFrameAccurateSearch;
		int64		TimestampSequenceIndex;
	};

	void LogMessage(IInfoLog::ELevel Level, const FString& Message);

	IManifest::FResult GetMediaStreamForID(TSharedPtrTS<FManifestHLSInternal::FPlaylistBase>& OutPlaylist, TSharedPtrTS<FManifestHLSInternal::FMediaStream>& OutMediaStream, uint32 UniqueID) const;

	IManifest::FResult GetNextOrRetrySegment(TSharedPtrTS<IStreamSegment>& OutSegment, TSharedPtrTS<const IStreamSegment> CurrentSegment, const FPlayStartOptions& Options, bool bRetry);

	IManifest::FResult FindSegment(TSharedPtrTS<FStreamSegmentRequestHLSfmp4>& OutRequest, TSharedPtrTS<FManifestHLSInternal::FPlaylistBase> InPlaylist, TSharedPtrTS<FManifestHLSInternal::FMediaStream> InStream, uint32 StreamUniqueID, EStreamType StreamType, const FSegSearchParam& SearchParam, IManifest::ESearchType SearchType);

	void RefreshDenylistState();

	TSharedPtrTS<FManifestHLSInternal>			InternalManifest;
	IPlayerSessionServices* 					SessionServices;
	IPlaylistReaderHLS*							PlaylistReader;
	EReadyState									CurrentReadyState;

	uint32										ActiveVideoUniqueID;
	uint32										ActiveAudioUniqueID;

	TSharedPtrTS<FBufferSourceInfo>				CurrentSourceBufferInfoVideo;
	TSharedPtrTS<FBufferSourceInfo>				CurrentSourceBufferInfoAudio;
};



TSharedPtrTS<FManifestHLS> FManifestHLS::Create(IPlayerSessionServices* SessionServices, IPlaylistReaderHLS* PlaylistReader, TSharedPtrTS<FManifestHLSInternal> Manifest)
{
	return TSharedPtrTS<FManifestHLS>(new FManifestHLS(SessionServices, PlaylistReader, Manifest));
}

FManifestHLS::FManifestHLS(IPlayerSessionServices* InSessionServices, IPlaylistReaderHLS* InPlaylistReader, TSharedPtrTS<FManifestHLSInternal> Manifest)
	: InternalManifest(Manifest)
	, SessionServices(InSessionServices)
	, PlaylistReader(InPlaylistReader)
{
}

FManifestHLS::~FManifestHLS()
{
}

IManifest::EType FManifestHLS::GetPresentationType() const
{
	FManifestHLSInternal::ScopedLockPlaylists lock(InternalManifest);
	return InternalManifest->MasterPlaylistVars.PresentationType;
}

TSharedPtrTS<const FLowLatencyDescriptor> FManifestHLS::GetLowLatencyDescriptor() const
{
	return nullptr; 
}

FTimeValue FManifestHLS::GetAnchorTime() const
{
	return FTimeValue::GetZero();
}
FTimeRange FManifestHLS::GetTotalTimeRange() const
{
	if (InternalManifest.IsValid() && InternalManifest->CurrentMediaAsset.IsValid())
	{
		FScopeLock lock(&InternalManifest->CurrentMediaAsset->UpdateLock);
		return InternalManifest->CurrentMediaAsset->TimeRange;
	}
	return FTimeRange();
}
FTimeRange FManifestHLS::GetSeekableTimeRange() const
{
	if (InternalManifest.IsValid() && InternalManifest->CurrentMediaAsset.IsValid())
	{
		FScopeLock lock(&InternalManifest->CurrentMediaAsset->UpdateLock);
		return InternalManifest->CurrentMediaAsset->SeekableTimeRange;
	}
	return FTimeRange();
}
FTimeRange FManifestHLS::GetPlaybackRange() const
{
	return FTimeRange();
}
void FManifestHLS::GetSeekablePositions(TArray<FTimespan>& OutPositions) const
{
	if (InternalManifest.IsValid() && InternalManifest->CurrentMediaAsset.IsValid())
	{
		FScopeLock lock(&InternalManifest->CurrentMediaAsset->UpdateLock);
		OutPositions = InternalManifest->CurrentMediaAsset->SeekablePositions;
	}
}
FTimeValue FManifestHLS::GetDuration() const
{
	if (InternalManifest.IsValid() && InternalManifest->CurrentMediaAsset.IsValid())
	{
		FScopeLock lock(&InternalManifest->CurrentMediaAsset->UpdateLock);
		return InternalManifest->CurrentMediaAsset->Duration;
	}
	return FTimeValue();
}
FTimeValue FManifestHLS::GetDefaultStartTime() const
{
	return FTimeValue::GetInvalid();
}
void FManifestHLS::ClearDefaultStartTime()
{
}



/**
 * Returns track metadata. For period based presentations the streams can be different per period in which case the metadata of the first period is returned.
 *
 * @param OutMetadata
 * @param StreamType
 */
void FManifestHLS::GetTrackMetadata(TArray<FTrackMetadata>& OutMetadata, EStreamType StreamType) const
{
	FManifestHLSInternal::ScopedLockPlaylists lock(InternalManifest);
	switch(StreamType)
	{
		case EStreamType::Video:
			OutMetadata = InternalManifest->TrackMetadataVideo;
			break;
		case EStreamType::Audio:
			OutMetadata = InternalManifest->TrackMetadataAudio;
			break;
		case EStreamType::Subtitle:
			OutMetadata.Empty();
			break;
	}
}


void FManifestHLS::UpdateDynamicRefetchCounter()
{
	// No-op.
}


/**
 * Returns the duration that should be present in the buffers at all times
 * (except for the end of the presentation).
 *
 * @return
 */
FTimeValue FManifestHLS::GetMinBufferTime() const
{
	// HLS does not offer a minimum duration to be in the buffers at all times. For expedited startup we use 2 seconds here.
	return FTimeValue().SetFromSeconds(2.0);
}

FTimeValue FManifestHLS::GetDesiredLiveLatency() const
{
	FTimeValue ll;
	if (InternalManifest.IsValid() && InternalManifest->CurrentMediaAsset.IsValid() && InternalManifest->MasterPlaylistVars.PresentationType == IManifest::EType::Live)
	{
		FTimeRange Full, Seekable;
		FScopeLock lock(&InternalManifest->CurrentMediaAsset->UpdateLock);
		Full = InternalManifest->CurrentMediaAsset->TimeRange;
		Seekable = InternalManifest->CurrentMediaAsset->SeekableTimeRange;
		ll = Full.End - Seekable.End;
	}
	return ll;
}


TSharedPtrTS<IProducerReferenceTimeInfo> FManifestHLS::GetProducerReferenceTimeInfo(int64 ID) const
{
	return nullptr;
}





/**
 * Returns a play period for the specified start time.
 * Since we are not currently splitting the media timeline into individual periods
 * we simply return a new period here regardless of the starting time.
 *
 * @param OutPlayPeriod
 * @param StartPosition
 * @param SearchType
 *
 * @return
 */
IManifest::FResult FManifestHLS::FindPlayPeriod(TSharedPtrTS<IPlayPeriod>& OutPlayPeriod, const FPlayStartPosition& StartPosition, ESearchType SearchType)
{
	TSharedPtrTS<FPlayPeriodHLS> Period(new FPlayPeriodHLS(SessionServices, PlaylistReader, InternalManifest));
	OutPlayPeriod = Period;
	return IManifest::FResult(IManifest::FResult::EType::Found);
}

IManifest::FResult FManifestHLS::FindNextPlayPeriod(TSharedPtrTS<IPlayPeriod>& OutPlayPeriod, TSharedPtrTS<const IStreamSegment> CurrentSegment)
{
	// There is no following period.
	return IManifest::FResult(IManifest::FResult::EType::PastEOS);
}

void FManifestHLS::TriggerClockSync(IManifest::EClockSyncType InClockSyncType)
{
	// No-op.
}

void FManifestHLS::TriggerPlaylistRefresh()
{
	// No-op.
}

/**
 * Creates a stream reader for the media segments.
 *
 * @return
 */
IStreamReader* FManifestHLS::CreateStreamReaderHandler()
{
	return new FStreamReaderHLSfmp4;
}










FPlayPeriodHLS::FPlayPeriodHLS(IPlayerSessionServices* InSessionServices, IPlaylistReaderHLS* InPlaylistReader, TSharedPtrTS<FManifestHLSInternal> Manifest)
	: InternalManifest(Manifest)
	, SessionServices(InSessionServices)
	, PlaylistReader(InPlaylistReader)
{
	check(PlaylistReader);

	CurrentReadyState = IManifest::IPlayPeriod::EReadyState::NotLoaded;

	// Set the active video and audio stream IDs to 0, which means none are selected.
	ActiveVideoUniqueID = 0;
	ActiveAudioUniqueID = 0;
}

FPlayPeriodHLS::~FPlayPeriodHLS()
{
}


void FPlayPeriodHLS::LogMessage(IInfoLog::ELevel Level, const FString& Message)
{
	if (SessionServices)
	{
		SessionServices->PostLog(Facility::EFacility::HLSManifest, Level, Message);
	}
}

/**
 * Sets stream preferences.
 *
 * @param ForStreamType
 * @param StreamAttributes
 */
void FPlayPeriodHLS::SetStreamPreferences(EStreamType ForStreamType, const FStreamSelectionAttributes& StreamAttributes)
{
}


/**
 * Returns the bitrate of the default stream (usually the first one specified).
 *
 * @return
 */
int64 FPlayPeriodHLS::GetDefaultStartingBitrate() const
{
	FManifestHLSInternal::ScopedLockPlaylists lock(InternalManifest);
	if (InternalManifest->VariantStreams.Num())
	{
		return InternalManifest->VariantStreams[0]->Bandwidth;
	}
	return 0;
}

TSharedPtrTS<FBufferSourceInfo> FPlayPeriodHLS::GetSelectedStreamBufferSourceInfo(EStreamType StreamType)
{
	if (StreamType == EStreamType::Video)
	{
		return CurrentSourceBufferInfoVideo;
	}
	else if (StreamType == EStreamType::Audio)
	{
		return CurrentSourceBufferInfoAudio;
	}
	return nullptr;
}

FString FPlayPeriodHLS::GetSelectedAdaptationSetID(EStreamType StreamType)
{
	int32 nA = GetMediaAsset()->GetNumberOfAdaptationSets(StreamType);
	if (nA)
	{
		return GetMediaAsset()->GetAdaptationSetByTypeAndIndex(StreamType, 0)->GetUniqueIdentifier();
	}
	return FString();
}


IManifest::IPlayPeriod::ETrackChangeResult FPlayPeriodHLS::ChangeTrackStreamPreference(EStreamType ForStreamType, const FStreamSelectionAttributes& StreamAttributes)
{
	// Currently track switching is not implemented.
	return IManifest::IPlayPeriod::ETrackChangeResult::NotChanged;
}


/**
 * Returns the current ready state of the period.
 *
 * @return
 */
IManifest::IPlayPeriod::EReadyState FPlayPeriodHLS::GetReadyState()
{
	return CurrentReadyState;
}



void FPlayPeriodHLS::Load()
{
	CurrentReadyState = IManifest::IPlayPeriod::EReadyState::Loaded;
}

/**
 * Prepares the period for playback.
 */
void FPlayPeriodHLS::PrepareForPlay()
{
	// For now we just go with the streams for which we loaded the playlists initially.
	// FIXME: in the future, based on preferences and options, select the streams we want.

	FManifestHLSInternal::ScopedLockPlaylists lock(InternalManifest);

	uint32 OldVideoUniqueID = ActiveVideoUniqueID;
	uint32 OldAudioUniqueID = ActiveAudioUniqueID;

	for(int32 i=0; i<InternalManifest->VariantStreams.Num(); ++i)
	{
		if (InternalManifest->VariantStreams[i]->Internal.LoadState == FManifestHLSInternal::FPlaylistBase::FInternal::ELoadState::Loaded)
		{
			ActiveVideoUniqueID = InternalManifest->VariantStreams[i]->Internal.UniqueID;
			break;
		}
	}

	for (TMultiMap<FString, TSharedPtrTS<FManifestHLSInternal::FRendition>>::TConstIterator It = InternalManifest->AudioRenditions.CreateConstIterator(); It; ++It)
	{
		if (It.Value()->Internal.LoadState == FManifestHLSInternal::FPlaylistBase::FInternal::ELoadState::Loaded)
		{
			ActiveAudioUniqueID = It.Value()->Internal.UniqueID;
			break;
		}
	}
	// In case there is an audio rendition without a dedicated playlist we look at audio-only variant streams
	if (ActiveAudioUniqueID == 0)
	{
		for(int32 i=0; i<InternalManifest->AudioOnlyStreams.Num(); ++i)
		{
			if (InternalManifest->AudioOnlyStreams[i]->Internal.LoadState == FManifestHLSInternal::FPlaylistBase::FInternal::ELoadState::Loaded)
			{
				ActiveAudioUniqueID = InternalManifest->AudioOnlyStreams[i]->Internal.UniqueID;
				break;
			}
		}
	}

	// Tell the manifest which stream IDs are now actively used.
	InternalManifest->SelectActiveStreamID(ActiveVideoUniqueID, OldVideoUniqueID);
	InternalManifest->SelectActiveStreamID(ActiveAudioUniqueID, OldAudioUniqueID);

	// Set up source buffer information for video and audio.
	// These are currently dummies since we do not support track switching yet.
	if (ActiveVideoUniqueID)
	{
		CurrentSourceBufferInfoVideo = MakeSharedTS<FBufferSourceInfo>();
		CurrentSourceBufferInfoVideo->PeriodID = InternalManifest->CurrentMediaAsset->GetAssetIdentifier();
		CurrentSourceBufferInfoVideo->PeriodAdaptationSetID = TEXT("video.0");
		CurrentSourceBufferInfoVideo->HardIndex = 0;
	}
	if (ActiveAudioUniqueID)
	{
		CurrentSourceBufferInfoAudio = MakeSharedTS<FBufferSourceInfo>();
		CurrentSourceBufferInfoAudio->PeriodID = InternalManifest->CurrentMediaAsset->GetAssetIdentifier();
		CurrentSourceBufferInfoAudio->PeriodAdaptationSetID = TEXT("audio.0");
		CurrentSourceBufferInfoAudio->HardIndex = 0;
	}

	CurrentReadyState = IManifest::IPlayPeriod::EReadyState::IsReady;
}



/**
 * Returns the media stream for the specified ID.
 *
 * @param OutPlaylist
 * @param OutMediaStream
 * @param UniqueID
 *
 * @return
 */
IManifest::FResult FPlayPeriodHLS::GetMediaStreamForID(TSharedPtrTS<FManifestHLSInternal::FPlaylistBase>& OutPlaylist, TSharedPtrTS<FManifestHLSInternal::FMediaStream>& OutMediaStream, uint32 UniqueID) const
{
	FManifestHLSInternal::ScopedLockPlaylists lock(InternalManifest);
	check(UniqueID);
	if (UniqueID)
	{
		TWeakPtrTS<FManifestHLSInternal::FPlaylistBase>* PlaylistID = InternalManifest->PlaylistIDMap.Find(UniqueID);
		if (PlaylistID != nullptr)
		{
			TSharedPtrTS<FManifestHLSInternal::FPlaylistBase> Playlist = PlaylistID->Pin();

			if (Playlist.IsValid())
			{
				// Sanity check the ID
				if (Playlist->Internal.UniqueID == UniqueID)
				{
					OutPlaylist = Playlist;

					// Playlist currently denylisted?
					if (Playlist->Internal.Denylisted.IsValid())
					{
						// Return and assume a allowlist stream will be selected.
						return IManifest::FResult().RetryAfterMilliseconds(50);
					}
					// Check the load state
					switch(Playlist->Internal.LoadState)
					{
						case FManifestHLSInternal::FPlaylistBase::FInternal::ELoadState::Loaded:
						{
							TSharedPtrTS<FManifestHLSInternal::FMediaStream>	MediaStream = Playlist->Internal.MediaStream;
							// The stream really better be there!
							if (MediaStream.IsValid())
							{
								OutMediaStream = MediaStream;
								return IManifest::FResult(IManifest::FResult::EType::Found);
							}
							else
							{
								return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage(FString::Printf(TEXT("Media stream for unique ID %u is not present!"), UniqueID)));
							}
						}
						case FManifestHLSInternal::FPlaylistBase::FInternal::ELoadState::NotLoaded:
						{
							return IManifest::FResult(IManifest::FResult::EType::NotLoaded);
						}
						case FManifestHLSInternal::FPlaylistBase::FInternal::ELoadState::Pending:
						{
							return IManifest::FResult().RetryAfterMilliseconds(50);
						}
					}
					// Should never get here, but if we do let's bail gracefully.
					return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage("Internal error. Unhandled switch case"));
				}
				else
				{
					return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage(FString::Printf(TEXT("Playlist unique ID %u does not match requested ID of %u"), Playlist->Internal.UniqueID, UniqueID)));
				}
			}
			else
			{
				return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage(FString::Printf(TEXT("Playlist for unique ID %u has been destroyed"), UniqueID)));
			}
		}
		else
		{
			return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage(FString::Printf(TEXT("No media stream found for unique ID %u"), UniqueID)));
		}
	}
	else
	{
		return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage(FString::Printf(TEXT("Invalid unique media stream ID %u"), UniqueID)));
	}
}





/**
 * Locate a segment in the stream's playlist.
 *
 * @param OutRequest
 * @param InPlaylist
 * @param InStream
 * @param StreamUniqueID
 * @param StreamType
 * @param SearchParam
 * @param SearchType
 *
 * @return
 */
IManifest::FResult FPlayPeriodHLS::FindSegment(TSharedPtrTS<FStreamSegmentRequestHLSfmp4>& OutRequest, TSharedPtrTS<FManifestHLSInternal::FPlaylistBase> InPlaylist, TSharedPtrTS<FManifestHLSInternal::FMediaStream> InStream, uint32 StreamUniqueID, EStreamType StreamType, const FPlayPeriodHLS::FSegSearchParam& SearchParam, IManifest::ESearchType SearchType)
{
	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_HLS_FindSegment);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, HLS_FindSegment);


	// VOD or EVENT playlist?
///////	if (InStream->PlaylistType == FManifestHLSInternal::FMediaStream::EPlaylistType::VOD || InStream->PlaylistType == FManifestHLSInternal::FMediaStream::EPlaylistType::Event)


	FURL_RFC3986 UrlBuilder;
	UrlBuilder.Parse(InPlaylist->Internal.PlaylistLoadRequest.URL);
	TSharedPtrTS<FStreamSegmentRequestHLSfmp4>	Req(new FStreamSegmentRequestHLSfmp4);
	Req->StreamType 	= StreamType;
	Req->StreamUniqueID = StreamUniqueID;
	Req->MediaAsset = InternalManifest->CurrentMediaAsset;
	if (!Req->MediaAsset.IsValid())
	{
		return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage("Internal error, media asset not found on asset timeline!"));
	}
	if (Req->MediaAsset->GetNumberOfAdaptationSets(StreamType) > 1)
	{
		return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage(FString::Printf(TEXT("Internal error, more than one %s rendition group found on asset timeline!"), GetStreamTypeName(StreamType))));
	}
	Req->AdaptationSet = Req->MediaAsset->GetAdaptationSetByTypeAndIndex(StreamType, 0);
	if (!Req->AdaptationSet.IsValid())
	{
		return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage(FString::Printf(TEXT("Internal error, no %s rendition group found on asset timeline!"), GetStreamTypeName(StreamType))));
	}
	Req->Representation = Req->AdaptationSet->GetRepresentationByUniqueIdentifier(LexToString(StreamUniqueID));
	if (!Req->Representation.IsValid())
	{
		return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage(FString::Printf(TEXT("Internal error, %s rendition not found in group on asset timeline!"), GetStreamTypeName(StreamType))));
	}
	Req->InitSegmentCache   	   = InternalManifest->InitSegmentCache;
	Req->LicenseKeyCache		   = InternalManifest->LicenseKeyCache;
	Req->bHasEncryptedSegments     = InStream->bHasEncryptedSegments;
	if (StreamType == EStreamType::Video)
	{
		Req->SourceBufferInfo = CurrentSourceBufferInfoVideo;
		Req->Bitrate				   = InPlaylist->GetBitrate();
		check(InternalManifest->BandwidthToQualityIndex.Find(Req->Bitrate) != nullptr);
		Req->QualityLevel   		   = InternalManifest->BandwidthToQualityIndex[Req->Bitrate];
	}
	else if (StreamType == EStreamType::Audio)
	{
		Req->SourceBufferInfo = CurrentSourceBufferInfoAudio;
		Req->Bitrate = InPlaylist->GetBitrate();
	}

	const TArray<FManifestHLSInternal::FMediaStream::FMediaSegment>& SegmentList = InStream->SegmentList;
	if (SegmentList.Num())
	{
		FTimeValue searchTime = SearchParam.Time;
		int32 SelectedSegmentIndex = -1;

		// Searching for the next segment within the same stream?
		if (SearchParam.StreamUniqueID != 0)
		{
			check(SearchType == IManifest::ESearchType::StrictlyAfter || SearchType == IManifest::ESearchType::Same);
			if (SearchType != IManifest::ESearchType::StrictlyAfter && SearchType != IManifest::ESearchType::Same)
			{
				return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage("Can only find next or retry segment in same stream right now"));
			}

			int64 NextSequenceNumber;
			if (SearchType == IManifest::ESearchType::Same)
			{
				NextSequenceNumber = SearchParam.MediaSequence;
			}
			else
			{
				NextSequenceNumber = SearchParam.MediaSequence + 1;
			}
			// We can use the media sequence number.
			for(int32 i=0,iMax=SegmentList.Num(); i<iMax; ++i)
			{
				if (SegmentList[i].SequenceNumber >= NextSequenceNumber)
				{
					SelectedSegmentIndex = i;
					break;
				}
			}
		}
		else
		{
			for(int32 i=0,iMax=SegmentList.Num(); i<iMax; ++i)
			{
				// Find the segment whose start time is >= the time we're looking for.
				if (SegmentList[i].AbsoluteDateTime >= searchTime)
				{
					// Do we want the segment with start time >= the search time?
					if (SearchType == IManifest::ESearchType::After)
					{
						// Yes, we're done.
						SelectedSegmentIndex = i;
						break;
					}
					// Do we want the segment with start time > the time we're looking for?
					else if (SearchType == IManifest::ESearchType::StrictlyAfter)
					{
						// Only go forward if we did hit the search time exactly ( == ) and we're not on the last first segment already!
						if (SegmentList[i].AbsoluteDateTime == searchTime)
						{
							// Continue the loop. The next segment, if it exists, will have a greater search time and we'll catch it then.
							continue;
						}
						SelectedSegmentIndex = i;
						break;
					}
					// Do we want the segment with start time <= the search time?
					else if (SearchType == IManifest::ESearchType::Before)
					{
						SelectedSegmentIndex = i;
						// Only go back if we did not hit the search time exactly ( == ) and we're not on the very first segment already!
						if (SegmentList[i].AbsoluteDateTime > searchTime && i > 0)
						{
							--SelectedSegmentIndex;
						}
						break;
					}
					// Do we want the segment with start time < the search time?
					else if (SearchType == IManifest::ESearchType::StrictlyBefore)
					{
						// If we cannot go back one segment we can return.
						if (i == 0)
						{
							return IManifest::FResult(IManifest::FResult::EType::BeforeStart);
						}
						SelectedSegmentIndex = i-1;
						break;
					}
					// Do we want the segment whose start time is closest to the search time?
					else if (SearchType == IManifest::ESearchType::Closest)
					{
						SelectedSegmentIndex = i;
						// If there is an earlier segment we can check which one is closer.
						if (i > 0)
						{
							FTimeValue diffHere   = SegmentList[i].AbsoluteDateTime - searchTime;
							FTimeValue diffBefore = searchTime - SegmentList[i - 1].AbsoluteDateTime;
							// In the exceptionally rare case the difference to either segment is the same we pick the earlier one.
							if (diffBefore <= diffHere)
							{
								--SelectedSegmentIndex;
							}
						}
						break;
					}
					// Do we want the segment for the exact same start time as the search time?
					else if (SearchType == IManifest::ESearchType::Same)
					{
						// This is used for retrying a failed segment. Usually on another quality level or CDN.
						// To allow for slight variations in the time we do a 'closest' search if the exact time can't be found.
						SelectedSegmentIndex = i;
						// If we hit the time dead on we are done.
						if (SegmentList[i].AbsoluteDateTime == searchTime)
						{
							break;
						}
						// Otherwise we do the same as for the 'closest' search.
						if (i > 0)
						{
							FTimeValue diffHere   = SegmentList[i].AbsoluteDateTime - searchTime;
							FTimeValue diffBefore = searchTime - SegmentList[i - 1].AbsoluteDateTime;
							// In the exceptionally rare case the difference to either segment is the same we pick the earlier one.
							if (diffBefore <= diffHere)
							{
								--SelectedSegmentIndex;
							}
						}
						break;
					}

					else
					{
						checkNoEntry();
						return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage("Internal error, unsupported segment search mode!"));
					}
				}
			}
			// If not found but there are segments in the list they all have a start time smaller than we are looking for.
			// Let's look at the last segment to see if the search time falls into the duration of it.
			if (SelectedSegmentIndex < 0 && SegmentList.Num())
			{
				// Whether we can use the last segment also depends on the search mode.
				if (SearchType == IManifest::ESearchType::Closest ||
					SearchType == IManifest::ESearchType::Before ||
					SearchType == IManifest::ESearchType::StrictlyBefore)
				{
					int32 LastSegmentIndex = SegmentList.Num() - 1;
					if (searchTime < SegmentList[LastSegmentIndex].AbsoluteDateTime + SegmentList[LastSegmentIndex].Duration)
					{
						SelectedSegmentIndex = LastSegmentIndex;
					}
				}
			}
		}

		// Did we find the segment?
		if (SelectedSegmentIndex >= 0 && SelectedSegmentIndex < SegmentList.Num())
		{
			// If there is a playback range and we have reached it we are at EOS, regardless of whether this is a Live stream or not.
			if (SearchParam.LastPTS.IsValid() && SegmentList[SelectedSegmentIndex].AbsoluteDateTime >= SearchParam.LastPTS)
			{
				Req->AbsoluteDateTime = SegmentList[SelectedSegmentIndex].AbsoluteDateTime;
				Req->bIsEOSSegment = true;
				OutRequest = Req;
				return IManifest::FResult(IManifest::FResult::EType::PastEOS);
			}

			//Req->PlaylistRelativeStartTime = SegmentList[SelectedSegmentIndex].RelativeStartTime;
			Req->AbsoluteDateTime   	   = SegmentList[SelectedSegmentIndex].AbsoluteDateTime;
			Req->SegmentDuration		   = SegmentList[SelectedSegmentIndex].Duration;
			Req->MediaSequence  		   = SegmentList[SelectedSegmentIndex].SequenceNumber;
			Req->DiscontinuitySequence     = SegmentList[SelectedSegmentIndex].DiscontinuityCount;
			Req->LocalIndex 			   = SelectedSegmentIndex;
			Req->bIsPrefetch			   = SegmentList[SelectedSegmentIndex].bIsPrefetch;
			Req->bIsEOSSegment  		   = false;
			Req->URL					   = FURL_RFC3986(UrlBuilder).ResolveWith(SegmentList[SelectedSegmentIndex].URI).Get();

			Req->EarliestPTS = searchTime;
			Req->LastPTS = SearchParam.LastPTS;
			Req->bFrameAccuracyRequired = SearchParam.bFrameAccurateSearch;
			Req->TimestampSequenceIndex = SearchParam.TimestampSequenceIndex;

			Req->InitSegmentInfo		   = SegmentList[SelectedSegmentIndex].InitSegmentInfo;
			Req->LicenseKeyInfo 		   = SegmentList[SelectedSegmentIndex].DRMKeyInfo;

			if (SegmentList[SelectedSegmentIndex].ByteRange.IsSet())
			{
				Req->Range.Start		= SegmentList[SelectedSegmentIndex].ByteRange.GetStart();
				Req->Range.EndIncluding = SegmentList[SelectedSegmentIndex].ByteRange.GetEnd();
			}
			OutRequest = Req;
			return IManifest::FResult(IManifest::FResult::EType::Found);
		}
		else
		{
			// Not having found a segment means we're beyond this presentation.
			// Unless this is a VOD list or it has as ENDLIST tag we have to try this later, assuming that an updated playlist has added additional segments.
			if (InStream->PlaylistType == FManifestHLSInternal::FMediaStream::EPlaylistType::VOD || InStream->bHasListEnd)
			{
				Req->AbsoluteDateTime = SearchParam.Time;
				Req->bIsEOSSegment = true;
				OutRequest = Req;
				return IManifest::FResult(IManifest::FResult::EType::PastEOS);
			}
			else
			{
				// Try again after half a target duration.
				return IManifest::FResult(IManifest::FResult::EType::TryAgainLater).RetryAfterMilliseconds(InStream->TargetDuration.GetAsMilliseconds() / 2);
			}
		}
	}
	else
	{
		// No segments is not really expected. If this occurs we assume the presentation has ended.
		Req->AbsoluteDateTime = SearchParam.Time;
		Req->bIsEOSSegment = true;
		OutRequest = Req;
		return IManifest::FResult(IManifest::FResult::EType::PastEOS);
	}
	return IManifest::FResult(IManifest::FResult::EType::NotFound);
}


IManifest::FResult FPlayPeriodHLS::GetStartingSegment(TSharedPtrTS<IStreamSegment>& OutSegment, const FPlayerSequenceState& InSequenceState, const FPlayStartPosition& StartPosition, IManifest::ESearchType SearchType)
{
	FManifestHLSInternal::ScopedLockPlaylists lock(InternalManifest);

	RefreshDenylistState();

	TSharedPtrTS<FManifestHLSInternal::FPlaylistBase>	VideoPlaylist;
	TSharedPtrTS<FManifestHLSInternal::FPlaylistBase>	AudioPlaylist;
	TSharedPtrTS<FManifestHLSInternal::FMediaStream>	VideoStream;
	TSharedPtrTS<FManifestHLSInternal::FMediaStream>	AudioStream;

	// Get the streams that are selected, if there are selected ones.
	IManifest::FResult vidResult = ActiveVideoUniqueID ? GetMediaStreamForID(VideoPlaylist, VideoStream, ActiveVideoUniqueID) : IManifest::FResult(IManifest::FResult::EType::Found);
	IManifest::FResult audResult = ActiveAudioUniqueID ? GetMediaStreamForID(AudioPlaylist, AudioStream, ActiveAudioUniqueID) : IManifest::FResult(IManifest::FResult::EType::Found);
	if (vidResult.IsSuccess() && audResult.IsSuccess())
	{
		TSharedPtrTS<FStreamSegmentRequestHLSfmp4> 	VideoSegmentRequest;
		TSharedPtrTS<FStreamSegmentRequestHLSfmp4> 	AudioSegmentRequest;
		FSegSearchParam								SearchParam;

		// Frame accurate seek required?
		bool bFrameAccurateSearch = StartPosition.Options.bFrameAccuracy;
		if (bFrameAccurateSearch)
		{
			// Get the segment that starts on or before the search time.
			SearchType = IManifest::ESearchType::Before;
		}

		FTimeValue PlayRangeEnd = StartPosition.Options.PlaybackRange.End;
		check(PlayRangeEnd.IsValid());

		SearchParam.Time = StartPosition.Time;
		SearchParam.LastPTS = PlayRangeEnd;
		SearchParam.bFrameAccurateSearch = bFrameAccurateSearch;
		SearchParam.TimestampSequenceIndex = InSequenceState.SequenceIndex;


		// Do we have both video and audio?
		if (ActiveVideoUniqueID && ActiveAudioUniqueID)
		{
			vidResult = FindSegment(VideoSegmentRequest, VideoPlaylist, VideoStream, ActiveVideoUniqueID, EStreamType::Video, SearchParam, SearchType);
			// Found and PastEOS are valid results here. Everything else is not.
			if (vidResult.GetType() != IManifest::FResult::EType::Found && vidResult.GetType() != IManifest::FResult::EType::PastEOS)
			{
				return vidResult;
			}
			// If the search for video was successful we adjust the search parameters for the audio stream.
			if (vidResult.IsSuccess())
			{
				VideoSegmentRequest->bIsInitialStartRequest = true;

				if (!bFrameAccurateSearch)
				{
					VideoSegmentRequest->EarliestPTS.SetToZero();
					// With the video segment found let's find the corresponding audio segment.
					SearchParam.Time = VideoSegmentRequest->AbsoluteDateTime;
					SearchParam.DiscontinuitySequence = VideoSegmentRequest->DiscontinuitySequence;
					// For audio we start with the segment before the video segment if there is no precise match.
					// The stream reader will skip over all audio access units before the intended start time.
					SearchType = IManifest::ESearchType::Before;
				}
			}
			// Search for audio.
			audResult = FindSegment(AudioSegmentRequest, AudioPlaylist, AudioStream, ActiveAudioUniqueID, EStreamType::Audio, SearchParam, SearchType);
			// Equally here, if successful or PastEOS is acceptable and everything else is not.
			if (audResult.GetType() != IManifest::FResult::EType::Found && audResult.GetType() != IManifest::FResult::EType::PastEOS)
			{
				return audResult;
			}

			// Both segments found?
			if (vidResult.IsSuccess() && audResult.IsSuccess())
			{
				AudioSegmentRequest->bIsInitialStartRequest = true;
				VideoSegmentRequest->DependentStreams.Push(AudioSegmentRequest);
				OutSegment = VideoSegmentRequest;
				return IManifest::FResult(IManifest::FResult::EType::Found);
			}
			// Only audio found?
			else if (audResult.IsSuccess())
			{
				AudioSegmentRequest->bIsInitialStartRequest = true;
				AudioSegmentRequest->DependentStreams.Push(VideoSegmentRequest);
				OutSegment = AudioSegmentRequest;
				return IManifest::FResult(IManifest::FResult::EType::Found);
			}
			// Only video found? Or neither?
			else //if (vidResult.IsSuccess())
			{
				VideoSegmentRequest->bIsInitialStartRequest = true;
				VideoSegmentRequest->DependentStreams.Push(AudioSegmentRequest);
				OutSegment = VideoSegmentRequest;
				return IManifest::FResult(IManifest::FResult::EType::Found);
			}
		}
		// Video only?
		else if (ActiveVideoUniqueID)
		{
			vidResult = FindSegment(VideoSegmentRequest, VideoPlaylist, VideoStream, ActiveVideoUniqueID, EStreamType::Video, SearchParam, SearchType);
			if (vidResult.IsSuccess())
			{
				VideoSegmentRequest->bIsInitialStartRequest = true;
				OutSegment = VideoSegmentRequest;
				return IManifest::FResult(IManifest::FResult::EType::Found);
			}
			else
			{
				return vidResult;
			}
		}
		// Audio only
		else
		{
			audResult = FindSegment(AudioSegmentRequest, AudioPlaylist, AudioStream, ActiveAudioUniqueID, EStreamType::Audio, SearchParam, SearchType);
			if (audResult.IsSuccess())
			{
				AudioSegmentRequest->bIsInitialStartRequest = true;
				OutSegment = AudioSegmentRequest;
				return IManifest::FResult(IManifest::FResult::EType::Found);
			}
			else
			{
				return audResult;
			}
		}
	}
	else
	{
		// Either playlist not yet loaded?
		if (vidResult.GetType() == IManifest::FResult::EType::NotLoaded)
		{
			check(VideoPlaylist.IsValid());

			VideoPlaylist->Internal.LoadState = FManifestHLSInternal::FPlaylistBase::FInternal::ELoadState::Pending;

			FURL_RFC3986 UrlBuilder;
			UrlBuilder.Parse(InternalManifest->MasterPlaylistVars.PlaylistLoadRequest.URL);
			FPlaylistLoadRequestHLS Req;
			Req.LoadType			   = FPlaylistLoadRequestHLS::ELoadType::First;
			Req.InternalUniqueID	   = ActiveVideoUniqueID;
			Req.RequestedAtTime 	   = SessionServices->GetSynchronizedUTCTime()->GetTime();
			Req.URL 				   = FURL_RFC3986(UrlBuilder).ResolveWith(VideoPlaylist->GetURL()).Get();
			Req.AdaptationSetUniqueID  = VideoPlaylist->Internal.AdaptationSetUniqueID;
			Req.RepresentationUniqueID = VideoPlaylist->Internal.RepresentationUniqueID;
			Req.CDN 				   = VideoPlaylist->Internal.CDN;
			PlaylistReader->RequestPlaylistLoad(Req);
			vidResult.RetryAfterMilliseconds(50);
		}
		if (audResult.GetType() == IManifest::FResult::EType::NotLoaded)
		{
			check(AudioPlaylist.IsValid());

			AudioPlaylist->Internal.LoadState = FManifestHLSInternal::FPlaylistBase::FInternal::ELoadState::Pending;

			FURL_RFC3986 UrlBuilder;
			UrlBuilder.Parse(InternalManifest->MasterPlaylistVars.PlaylistLoadRequest.URL);
			FPlaylistLoadRequestHLS Req;
			Req.LoadType			   = FPlaylistLoadRequestHLS::ELoadType::First;
			Req.InternalUniqueID	   = ActiveAudioUniqueID;
			Req.RequestedAtTime 	   = SessionServices->GetSynchronizedUTCTime()->GetTime();
			Req.URL 				   = FURL_RFC3986(UrlBuilder).ResolveWith(AudioPlaylist->GetURL()).Get();
			Req.AdaptationSetUniqueID  = AudioPlaylist->Internal.AdaptationSetUniqueID;
			Req.RepresentationUniqueID = AudioPlaylist->Internal.RepresentationUniqueID;
			Req.CDN 				   = AudioPlaylist->Internal.CDN;
			PlaylistReader->RequestPlaylistLoad(Req);
			audResult.RetryAfterMilliseconds(50);
		}

		if (vidResult.GetType() == IManifest::FResult::EType::TryAgainLater)
		{
			return vidResult;
		}
		if (audResult.GetType() == IManifest::FResult::EType::TryAgainLater)
		{
			return audResult;
		}
		// If both are a go, go!
		if (vidResult.IsSuccess() && audResult.IsSuccess())
		{
			return vidResult;
		}
		// Return that which is at fault.
		return !vidResult.IsSuccess() ? vidResult : audResult;
	}
}

//-----------------------------------------------------------------------------
/**
 * Same as GetStartingSegment() except this is for a specific stream (video, audio, ...) only.
 * To be used when a track (language) change is made and a new segment is needed at the current playback position.
 */
IManifest::FResult FPlayPeriodHLS::GetContinuationSegment(TSharedPtrTS<IStreamSegment>& OutSegment, EStreamType StreamType, const FPlayerSequenceState& SequenceState, const FPlayStartPosition& StartPosition, IManifest::ESearchType SearchType)
{
	// Not currently supported.
	// We could call GetStartingSegment() and return the segment request of the stream type that was found there
	// but that has a start time matched to the video which may not be desirable.
	return IManifest::FResult(IManifest::FResult::EType::NotFound);
}



void FPlayPeriodHLS::IncreaseSegmentFetchDelay(const FTimeValue& IncreaseAmount)
{
	// No-op.
}


IManifest::FResult FPlayPeriodHLS::GetLoopingSegment(TSharedPtrTS<IStreamSegment>& OutSegment, const FPlayerSequenceState& SequenceState, const FPlayStartPosition& StartPosition, IManifest::ESearchType SearchType)
{
	return GetStartingSegment(OutSegment, SequenceState, StartPosition, SearchType);
}



IManifest::FResult FPlayPeriodHLS::GetNextOrRetrySegment(TSharedPtrTS<IStreamSegment>& OutSegment, TSharedPtrTS<const IStreamSegment> InCurrentSegment, const FPlayStartOptions& Options, bool bRetry)
{
	// Need to have a current segment to find the next one.
	if (!InCurrentSegment.IsValid())
	{
		return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage("Cannot get next segment without a current segment!"));
	}
	const FStreamSegmentRequestHLSfmp4* CurrentRequest = static_cast<const FStreamSegmentRequestHLSfmp4*>(InCurrentSegment.Get());
	check(CurrentRequest->DependentStreams.Num() == 0);
	if (CurrentRequest->DependentStreams.Num())
	{
		return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage("Cannot get next segment for a segment with dependent segments!"));
	}
	if (CurrentRequest->StreamUniqueID == 0)
	{
		return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage("Cannot get next segment for a segment having no unique stream ID!"));
	}

	FManifestHLSInternal::ScopedLockPlaylists lock(InternalManifest);
	uint32 ForStreamID = 0;
	switch(CurrentRequest->GetType())
	{
		case EStreamType::Video:
		{
			ForStreamID = ActiveVideoUniqueID;
			break;
		}
		case EStreamType::Audio:
		{
			ForStreamID = ActiveAudioUniqueID;
			break;
		}
		default:
		{
			return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage(FString::Printf(TEXT("Cannot get next segment for unsupported stream type \"%s\"!"), GetStreamTypeName(CurrentRequest->GetType()))));
		}
	}
	if (ForStreamID == 0)
	{
		return IManifest::FResult(IManifest::FResult::EType::NotFound).SetErrorDetail(FErrorDetail().SetMessage(FString::Printf(TEXT("Cannot get next segment stream type \"%s\" since no stream is actively selected!"), GetStreamTypeName(CurrentRequest->GetType()))));
	}

	RefreshDenylistState();

	TSharedPtrTS<FManifestHLSInternal::FMediaStream>	Stream;
	TSharedPtrTS<FManifestHLSInternal::FPlaylistBase> Playlist;
	IManifest::FResult Result = GetMediaStreamForID(Playlist, Stream, ForStreamID);
	if (Result.IsSuccess())
	{
		TSharedPtrTS<FStreamSegmentRequestHLSfmp4> 	NextSegmentRequest;
		FSegSearchParam								SearchParam;

		// Frame accurate seek required?
		bool bFrameAccurateSearch = Options.bFrameAccuracy;
		FTimeValue PlayRangeEnd = Options.PlaybackRange.End;
		check(PlayRangeEnd.IsValid());
		SearchParam.LastPTS = PlayRangeEnd;
		SearchParam.bFrameAccurateSearch = bFrameAccurateSearch;

		SearchParam.Time				  = CurrentRequest->AbsoluteDateTime;
		SearchParam.Duration			  = CurrentRequest->SegmentDuration;
		SearchParam.MediaSequence   	  = CurrentRequest->MediaSequence;
		SearchParam.DiscontinuitySequence = CurrentRequest->DiscontinuitySequence;
		SearchParam.LocalIndex  		  = CurrentRequest->LocalIndex;
		SearchParam.StreamUniqueID  	  = CurrentRequest->StreamUniqueID == ForStreamID ? ForStreamID : 0;
		SearchParam.TimestampSequenceIndex = CurrentRequest->TimestampSequenceIndex;
		Result = FindSegment(NextSegmentRequest, Playlist, Stream, ForStreamID, CurrentRequest->GetType(), SearchParam, bRetry ? IManifest::ESearchType::Same : IManifest::ESearchType::StrictlyAfter);
		if (Result.IsSuccess())
		{
			// Continuing with the next segment implicitly means there is no AU time offset.
			if (!bRetry)
			{
				NextSegmentRequest->EarliestPTS.SetToZero();
			}
			else
			{
				NextSegmentRequest->NumOverallRetries = CurrentRequest->NumOverallRetries + 1;
			}
			OutSegment = NextSegmentRequest;
			return IManifest::FResult(IManifest::FResult::EType::Found);
		}
		if (Result.GetType() == IManifest::FResult::EType::TryAgainLater)
		{
			return Result;
		}
		else
		{
			checkf(Result.GetType() == IManifest::FResult::EType::PastEOS, TEXT("What error is this?"));
			return Result;
		}
	}
	else
	{
		// Playlist not loaded?
		if (Result.GetType() == IManifest::FResult::EType::NotLoaded)
		{
			check(Playlist.IsValid());

			Playlist->Internal.LoadState = FManifestHLSInternal::FPlaylistBase::FInternal::ELoadState::Pending;

			FURL_RFC3986 UrlBuilder;
			UrlBuilder.Parse(InternalManifest->MasterPlaylistVars.PlaylistLoadRequest.URL);
			FPlaylistLoadRequestHLS Req;
			Req.LoadType			   = FPlaylistLoadRequestHLS::ELoadType::First;
			Req.InternalUniqueID	   = ForStreamID;
			Req.RequestedAtTime 	   = SessionServices->GetSynchronizedUTCTime()->GetTime();
			Req.URL 				   = UrlBuilder.ResolveWith(Playlist->GetURL()).Get();
			Req.AdaptationSetUniqueID  = Playlist->Internal.AdaptationSetUniqueID;
			Req.RepresentationUniqueID = Playlist->Internal.RepresentationUniqueID;
			Req.CDN 				   = Playlist->Internal.CDN;
			PlaylistReader->RequestPlaylistLoad(Req);
			Result.RetryAfterMilliseconds(50);
		}
		return Result;
	}

	return IManifest::FResult(IManifest::FResult::EType::PastEOS);
}

/**
 * Selects the next segment to download.
 * This might be a segment from a different variant stream after a quality switch.
 *
 * @param OutSegment
 * @param InCurrentSegment
 * @param Options
 *
 * @return
 */
IManifest::FResult FPlayPeriodHLS::GetNextSegment(TSharedPtrTS<IStreamSegment>& OutSegment, TSharedPtrTS<const IStreamSegment> InCurrentSegment, const FPlayStartOptions& Options)
{
	return GetNextOrRetrySegment(OutSegment, InCurrentSegment, Options, false);
}


IManifest::FResult FPlayPeriodHLS::GetRetrySegment(TSharedPtrTS<IStreamSegment>& OutSegment, TSharedPtrTS<const IStreamSegment> InCurrentSegment, const FPlayStartOptions& Options, bool bReplaceWithFillerData)
{
	// To insert filler data we can use the current request over again.
	if (bReplaceWithFillerData)
	{
		const FStreamSegmentRequestHLSfmp4* CurrentRequest = static_cast<const FStreamSegmentRequestHLSfmp4*>(InCurrentSegment.Get());
		TSharedPtrTS<FStreamSegmentRequestHLSfmp4> NewRequest(new FStreamSegmentRequestHLSfmp4);
		*NewRequest = *CurrentRequest;
		NewRequest->bInsertFillerData = true;
		// We treat replacing the segment with filler data as a retry.
		++NewRequest->NumOverallRetries;
		OutSegment = NewRequest;
		return IManifest::FResult(IManifest::FResult::EType::Found);
	}
	return GetNextOrRetrySegment(OutSegment, InCurrentSegment, Options, true);
}


/**
 * Checks if any potentially denylisted stream can be used again.
 */
void FPlayPeriodHLS::RefreshDenylistState()
{
	FTimeValue Now = SessionServices->GetSynchronizedUTCTime()->GetTime();

	// Note: the manifest must be locked already.
	for (TMap<uint32, TWeakPtrTS<FManifestHLSInternal::FPlaylistBase>>::TIterator It = InternalManifest->PlaylistIDMap.CreateIterator(); It; ++It)
	{
		TSharedPtrTS<FManifestHLSInternal::FRendition::FPlaylistBase> Stream = It.Value().Pin();
		if (Stream.IsValid())
		{
			if (Stream->Internal.Denylisted.IsValid())
			{
				if (Now >= Stream->Internal.Denylisted->BecomesAvailableAgainAtUTC)
				{
					Stream->Internal.LoadState  	  = FManifestHLSInternal::FPlaylistBase::FInternal::ELoadState::NotLoaded;
					Stream->Internal.bReloadTriggered = false;
					Stream->Internal.bNewlySelected   = false;
					Stream->Internal.ExpiresAtTime.SetToPositiveInfinity();

					// Tell the stream selector that this stream is available again.
					TSharedPtrTS<IAdaptiveStreamSelector> StreamSelector(SessionServices->GetStreamSelector());
					StreamSelector->MarkStreamAsAvailable(Stream->Internal.Denylisted->AssetIDs);

					Stream->Internal.Denylisted.Reset();
					LogMessage(IInfoLog::ELevel::Info, FString::Printf(TEXT("Lifting denylist of playlist \"%s\""), *Stream->Internal.PlaylistLoadRequest.URL));
				}
			}
		}
	}
}



/**
 * Returns segment information (duration and estimated byte size) of the
 * next n segments for the indicated stream.
 *
 * @param OutSegmentInformation
 * @param OutAverageSegmentDuration
 * @param InCurrentSegment
 * @param LookAheadTime
 * @param AdaptationSetID
 * @param RepresentationID
 */
void FPlayPeriodHLS::GetSegmentInformation(TArray<FSegmentInformation>& OutSegmentInformation, FTimeValue& OutAverageSegmentDuration, TSharedPtrTS<const IStreamSegment> InCurrentSegment, const FTimeValue& LookAheadTime, const FString& AdaptationSetID, const FString& RepresentationID)
{
	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_HLS_GetSegmentInformation);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, HLS_GetSegmentInformation);

	OutSegmentInformation.Empty();
	OutAverageSegmentDuration.SetToInvalid();

	FTimeValue StartingTime(FTimeValue::GetZero());

	const FStreamSegmentRequestHLSfmp4* CurrentSegment = static_cast<const FStreamSegmentRequestHLSfmp4*>(InCurrentSegment.Get());

	if (CurrentSegment)
	{
		// The time of the next segment needs to be larger than that of the current. We add half the duration to the time to do that.
		// The reason being that adding the whole duration might get us slightly further than the next segment actually is, particularly if it is in
		// another variant playlist.
		StartingTime = CurrentSegment->AbsoluteDateTime + (CurrentSegment->SegmentDuration / 2);
	}

	FManifestHLSInternal::ScopedLockPlaylists lock(InternalManifest);
	const FManifestHLSInternal* pInt = InternalManifest.Get();
	check(pInt);
	if (!pInt)
	{
		return;
	}

	// The representation ID is the unique ID of the stream as a string. Convert it back
	uint32 UniqueID;
	LexFromString(UniqueID, *RepresentationID);
	TSharedPtrTS<FManifestHLSInternal::FPlaylistBase> Playlist = pInt->GetPlaylistForUniqueID(UniqueID);
	if (!Playlist.IsValid() || !Playlist->IsVariantStream())
	{
		return;
	}

	// Get the bitrate of the intended variant stream
	const int32 Bitrate = Playlist->GetBitrate();
	// Is the playlist of this stream loaded?
	TSharedPtrTS<FManifestHLSInternal::FMediaStream>	MediaStream = Playlist->Internal.MediaStream;
	bool bIsIntendedStream = false;
	if (MediaStream.IsValid())
	{
		bIsIntendedStream = true;
	}
	else
	{
		// Not loaded. At this point we have to _assume_ that all video variant streams are segmented the same so we search for any
		// loaded variant and use its segmentation to return information for.
		for(int32 VideoStreamIndex=0; VideoStreamIndex<pInt->VariantStreams.Num(); ++VideoStreamIndex)
		{
			MediaStream = pInt->VariantStreams[VideoStreamIndex]->Internal.MediaStream;
			if (MediaStream.IsValid())
			{
				break;
			}
		}
	}
	// This should have yielded a playlist.
	check(MediaStream.IsValid());
	if (MediaStream.IsValid())
	{
		const TArray<FManifestHLSInternal::FMediaStream::FMediaSegment>& SegmentList = MediaStream->SegmentList;

		FTimeValue TimeToGo(LookAheadTime);
		FTimeValue AccumulatedDuration(FTimeValue::GetZero());

		// Find the segment we need to start with.
		int32 FirstIndex = 0;
		for(; FirstIndex < SegmentList.Num(); ++FirstIndex)
		{
			if (SegmentList[FirstIndex].AbsoluteDateTime >= StartingTime)
			{
				break;
			}
		}

		while(TimeToGo > FTimeValue::GetZero() && FirstIndex < SegmentList.Num())
		{
			FSegmentInformation& si = OutSegmentInformation.AddDefaulted_GetRef();
			si.Duration = SegmentList[FirstIndex].Duration;
			if (si.Duration <= FTimeValue::GetZero())
			{
				break;
			}
			// Set the actual byte size only if a byte range is defined and if we are operating on the intended stream. Otherwise use default size for duration and bitrate.
			si.ByteSize = bIsIntendedStream && SegmentList[FirstIndex].ByteRange.IsSet() ? SegmentList[FirstIndex].ByteRange.GetNumBytes() : static_cast<int64>(Bitrate * si.Duration.GetAsSeconds() / 8);
			AccumulatedDuration += si.Duration;
			TimeToGo -= si.Duration;
			++FirstIndex;
		}
		// Fill the remaining duration with the average segment duration or, if that is somehow not valid, the target duration.
		FTimeValue fillDuration = MediaStream->TotalAccumulatedSegmentDuration;
		if (!fillDuration.IsValid() || fillDuration <= FTimeValue::GetZero() || SegmentList.Num() == 0)
		{
			fillDuration = MediaStream->TargetDuration;
		}
		else
		{
			fillDuration /= SegmentList.Num();
		}
		if (fillDuration > FTimeValue::GetZero())
		{
			while(TimeToGo > FTimeValue::GetZero())
			{
				FSegmentInformation& si = OutSegmentInformation.AddDefaulted_GetRef();
				si.Duration = fillDuration;
				si.ByteSize = static_cast<int64>(Bitrate * si.Duration.GetAsSeconds() / 8);
				AccumulatedDuration += si.Duration;
				TimeToGo -= si.Duration;
			}
		}
		// Set up average duration.
		if (OutSegmentInformation.Num())
		{
			OutAverageSegmentDuration = AccumulatedDuration / OutSegmentInformation.Num();
		}
	}
	else
	{
		// Not a single playlist active?
		// Should we synthesize some information based on stream bitrate (which we have) and a fake fixed duration?
		check(!"Now what?");
	}
}




TSharedPtrTS<ITimelineMediaAsset> FPlayPeriodHLS::GetMediaAsset() const
{
	FManifestHLSInternal::ScopedLockPlaylists lock(InternalManifest);
	const FManifestHLSInternal* pInt = InternalManifest.Get();
	if (pInt)
	{
		// HLS only has a single playback "asset". (aka Period)
		return pInt->CurrentMediaAsset;
	}
	return TSharedPtrTS<ITimelineMediaAsset>();
}



void FPlayPeriodHLS::SelectStream(const FString& AdaptationSetID, const FString& RepresentationID)
{
	RefreshDenylistState();

	// The representation ID is the unique ID of the stream as a string. Convert it back
	uint32 UniqueID;
	LexFromString(UniqueID, *RepresentationID);

	// Which stream type is this?
	if (AdaptationSetID.Equals(TEXT("$video$")))
	{
		// Different from what we have actively selected?
		if (UniqueID != ActiveVideoUniqueID)
		{
			// FIXME: We could emit a QoS event here. Although selecting the stream does not mean it will actually get used.
			//        There is still a chance that (if the playlist is not loaded yet) another stream will be selected right away.

			// Tell the manifest that we are now using a different stream.
			InternalManifest->SelectActiveStreamID(UniqueID, ActiveVideoUniqueID);

			ActiveVideoUniqueID = UniqueID;
		}
	}
	else if (AdaptationSetID.Equals(TEXT("$audio$")))
	{
		// Different from what we have actively selected?
		if (UniqueID != ActiveAudioUniqueID)
		{
			// FIXME: We could emit a QoS event here. Although selecting the stream does not mean it will actually get used.
			//        There is still a chance that (if the playlist is not loaded yet) another stream will be selected right away.

			// Tell the manifest that we are now using a different stream.
			InternalManifest->SelectActiveStreamID(UniqueID, ActiveAudioUniqueID);

			ActiveAudioUniqueID = UniqueID;
		}
	}
}

void FPlayPeriodHLS::TriggerInitSegmentPreload(const TArray<FInitSegmentPreload>& InitSegmentsToPreload)
{
	// No-op
}


} // namespace Electra
