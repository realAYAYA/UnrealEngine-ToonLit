// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Player/PlayerStreamReader.h"
#include "Player/AdaptiveStreamingPlayerResourceRequest.h"
#include "Player/DASH/PlaylistReaderDASH_Internal.h"
#include "Player/DASH/PlayerEventDASH.h"
#include "Demuxer/ParserISO14496-12.h"
#include "Demuxer/ParserMKV.h"
#include "HTTP/HTTPManager.h"
#include "Player/DRM/DRMManager.h"

namespace Electra
{

class FStreamSegmentRequestDASH : public IStreamSegment
{
public:
	FStreamSegmentRequestDASH();
	virtual ~FStreamSegmentRequestDASH();

	void SetPlaybackSequenceID(uint32 PlaybackSequenceID) override;
	uint32 GetPlaybackSequenceID() const override;

	void SetExecutionDelay(const FTimeValue& UTCNow, const FTimeValue& ExecutionDelay) override;
	FTimeValue GetExecuteAtUTCTime() const override;

	EStreamType GetType() const override;

	void GetDependentStreams(TArray<TSharedPtrTS<IStreamSegment>>& OutDependentStreams) const override;
	void GetRequestedStreams(TArray<TSharedPtrTS<IStreamSegment>>& OutRequestedStreams) override;
	void GetEndedStreams(TArray<TSharedPtrTS<IStreamSegment>>& OutAlreadyEndedStreams) override;

	//! Returns the first PTS value as indicated by the media timeline. This should correspond to the actual absolute PTS of the sample.
	FTimeValue GetFirstPTS() const override;

	int32 GetQualityIndex() const override;
	int32 GetBitrate() const override;

	void GetDownloadStats(Metrics::FSegmentDownloadStats& OutStats) const override;
	bool GetStartupDelay(FTimeValue& OutStartTime, FTimeValue& OutTimeIntoSegment, FTimeValue& OutSegmentDuration) const override;

	EStreamType												StreamType = EStreamType::Unsupported;				//!< Type of stream (video, audio, etc.)
	int32													QualityIndex = 0;
	int32													MaxQualityIndex = 0;
	FStreamCodecInformation									CodecInfo;											//!< Partial codec info as can be collected from the MPD.
	TSharedPtrTS<IPlaybackAssetRepresentation>				Representation;										//!< The representation this request belongs to.
	TSharedPtrTS<IPlaybackAssetAdaptationSet>				AdaptationSet;										//!< The adaptation set the representation belongs to.
	TSharedPtrTS<ITimelineMediaAsset>						Period;												//!< The period the adaptation set belongs to.
	FManifestDASHInternal::FSegmentInformation				Segment;											//!< Segment information (URLs and timing values)
	TArray<TSharedPtrTS<FStreamSegmentRequestDASH>>			DependentStreams;									//!< Streams this segment depends on. Currently only used to hold the set of requests for the initial playback start.
	bool													bIsEOSSegment = false;								//!< true if this is not an actual request but a stream-has-already-ended request.
	bool													bIsInitialStartRequest = false;						//!< true if this is the initial playback start request.
	FTimeValue												PeriodStart;										//!< Value to add to all DTS & PTS to map them into the Period timeline
	FTimeValue												AST = FTimeValue::GetZero();						//!< Value of AST to add to all time to generate wallclock time
	FTimeValue												AdditionalAdjustmentTime = FTimeValue::GetZero();	//!< Sum of any other time corrections
	bool													bInsertFillerData = false;							//!< true to insert empty access units into the buffer instead of reading actual data.
	int64													TimestampSequenceIndex = 0;							//!< Sequence index to set in all timestamp values of the decoded access unit.
	FTimeValue												FrameAccurateStartTime;								//!< If set, the start time as was requested in a Seek() (not in media local time)

	// UTC wallclock times during which this segment can be fetched;
	FTimeValue												ASAST;
	FTimeValue												SAET;
	FTimeValue												DownloadDelayTime;

	// Encryption
	TSharedPtrTS<ElectraCDM::IMediaCDMClient>				DrmClient;
	FString													DrmMimeType;


	// Internal work variables
	TSharedPtrTS<FBufferSourceInfo>							SourceBufferInfo;
	int32													NumOverallRetries = 0;								//!< Number of retries for this _segment_ across all possible quality levels and CDNs.
	uint32													CurrentPlaybackSequenceID = ~0U;					//!< Set by the player before adding the request to the stream reader.

	Metrics::FSegmentDownloadStats							DownloadStats;
	HTTP::FConnectionInfo									ConnectionInfo;

	bool													bWarnedAboutTimescale = false;
};



/**
 *
**/
class FStreamReaderDASH : public IStreamReader
{
public:
	FStreamReaderDASH();
	virtual ~FStreamReaderDASH();

	virtual UEMediaError Create(IPlayerSessionServices* PlayerSessionService, const CreateParam& InCreateParam) override;
	virtual void Close() override;

	//! Adds a request to read from a stream
	virtual EAddResult AddRequest(uint32 CurrentPlaybackSequenceID, TSharedPtrTS<IStreamSegment> Request) override;

	//! Cancels any ongoing requests of the given stream type. Silent cancellation will not notify OnFragmentClose() or OnFragmentReachedEOS().
	virtual void CancelRequest(EStreamType StreamType, bool bSilent) override;

	//! Cancels all pending requests.
	virtual void CancelRequests() override;

private:
	ELECTRA_IMPL_DEFAULT_ERROR_METHODS(DASHStreamReader);

	struct FStreamHandler : public FMediaThread, public IParserISO14496_12::IReader, public IParserISO14496_12::IBoxCallback, public IParserMKV::IReader
	{
		struct FReadBuffer
		{
			FReadBuffer()
			{
				Reset();
			}
			void Reset()
			{
				ReceiveBuffer.Reset();
				ParsePos = 0;
				MaxParsePos = TNumericLimits<int64>::Max();
			}
			TSharedPtrTS<IElectraHttpManager::FReceiveBuffer>	ReceiveBuffer;
			int64												ParsePos;
			int64												MaxParsePos;
		};


		struct FActiveTrackData
		{
			void Reset()
			{
				BufferSourceInfo.Reset();
				DurationSuccessfullyRead.SetToZero();
				DurationSuccessfullyDelivered.SetToZero();
				AverageDuration.SetToZero();
				LargestDTS.SetToInvalid();
				SmallestPTS.SetToInvalid();
				LargestPTS.SetToInvalid();
				NumAddedTotal = 0;
				bIsFirstInSequence = true;
				bReadPastLastPTS = false;
				bTaggedLastSample = false;
				bGotAllSamples = false;
				bReachedEndOfKnownDuration = false;

				AccessUnitFIFO.Empty();
				SortedAccessUnitFIFO.Empty();
				CSD.Reset();
				StreamType = EStreamType::Unsupported;
				Bitrate = 0;
				bNeedToRecalculateDurations = false;
			}

			struct FSample
			{
				FTimeValue PTS;
				FAccessUnit* AU = nullptr;
				uint32 SequentialIndex = 0;
				FSample(FAccessUnit* InAU, uint32 InSequentialIndex) : PTS(InAU->PTS), AU(InAU), SequentialIndex(InSequentialIndex) { InAU->AddRef(); }
				FSample(const FSample& rhs)
				{
					PTS = rhs.PTS;
					SequentialIndex = rhs.SequentialIndex;
					if ((AU = rhs.AU) != nullptr)
					{
						AU->AddRef();
					}
				}
				void Release()
				{
					FAccessUnit::Release(AU);
					AU = nullptr;
				}
				~FSample()
				{
					Release();
				}
			};

			void AddAccessUnit(FAccessUnit* InAU)
			{
				if (InAU)
				{
					AccessUnitFIFO.Emplace(FActiveTrackData::FSample(InAU, NumAddedTotal));
					if (bNeedToRecalculateDurations)
					{
						SortedAccessUnitFIFO.Emplace(FActiveTrackData::FSample(InAU, NumAddedTotal));
						SortedAccessUnitFIFO.Sort([](const FActiveTrackData::FSample& a, const FActiveTrackData::FSample& b){return a.PTS < b.PTS;});
					}
					// If a valid non-zero duration exists on the AU we take it as the average duration.
					if ((!AverageDuration.IsValid() || AverageDuration.IsZero()) && InAU->Duration.IsValid() && InAU->Duration > FTimeValue::GetZero())
					{
						AverageDuration = InAU->Duration;
					}
					if (!LargestDTS.IsValid() || InAU->DTS > LargestDTS)
					{
						LargestDTS = InAU->DTS;
					}
					if (!SmallestPTS.IsValid() || InAU->PTS < SmallestPTS)
					{
						SmallestPTS = InAU->PTS;
					}
					if (!LargestPTS.IsValid() || AccessUnitFIFO.Last().AU->PTS > LargestPTS)
					{
						LargestPTS = AccessUnitFIFO.Last().AU->PTS;
					}
					++NumAddedTotal;
				}
			}

			TArray<FSample> AccessUnitFIFO;
			TArray<FSample> SortedAccessUnitFIFO;
			FTimeValue DurationSuccessfullyRead {(int64)0};
			FTimeValue DurationSuccessfullyDelivered {(int64)0};
			FTimeValue AverageDuration {(int64)0};
			FTimeValue SmallestPTS;
			FTimeValue LargestPTS;
			FTimeValue LargestDTS;
			TSharedPtrTS<FAccessUnit::CodecData> CSD;
			TSharedPtrTS<FBufferSourceInfo> BufferSourceInfo;
			EStreamType StreamType = EStreamType::Unsupported;
			uint32 NumAddedTotal = 0;
			int32 Bitrate = 0;
			bool bIsFirstInSequence = true;
			bool bReadPastLastPTS = false;
			bool bTaggedLastSample = false;
			bool bGotAllSamples = false;
			bool bNeedToRecalculateDurations = false;
			bool bReachedEndOfKnownDuration = false;
		};


		static uint32											UniqueDownloadID;

		IStreamReader::CreateParam								Parameters;
		TSharedPtrTS<FStreamSegmentRequestDASH>					CurrentRequest;
		FMediaSemaphore											WorkSignal;
		FMediaEvent												IsIdleSignal;
		bool													bRunOnThreadPool = false;
		volatile bool											bTerminate = false;
		volatile bool											bWasStarted = false;
		volatile bool											bRequestCanceled = false;
		volatile bool											bSilentCancellation = false;
		volatile bool											bHasErrored = false;
		bool													bAbortedByABR = false;
		bool													bAllowEarlyEmitting = false;
		bool													bFillRemainingDuration = false;

		IPlayerSessionServices*									PlayerSessionService = nullptr;
		FReadBuffer												ReadBuffer;
		FMediaEvent												DownloadCompleteSignal;
		TSharedPtrTS<IParserISO14496_12>						MP4Parser;
		int32													NumMOOFBoxesFound = 0;
		FActiveTrackData										ActiveTrackData;
		TArray<TSharedPtrTS<DASH::FPlayerEvent>>				SegmentEventsFound;

		FMediaCriticalSection									MetricUpdateLock;
		int32													ProgressReportCount = 0;
		TSharedPtrTS<IAdaptiveStreamSelector>					StreamSelector;
		FString													ABRAbortReason;


		FStreamHandler();
		virtual ~FStreamHandler();
		void Cancel(bool bSilent);
		void SignalWork();
		void WorkerThread();
		void RunInThreadPool();
		void HandleRequest();
		void HandleRequestMP4();
		void HandleRequestMKV();

		FErrorDetail LoadInitSegment(TSharedPtrTS<FMPDLoadRequestDASH>& OutLoadRequest, Metrics::FSegmentDownloadStats& OutStats, const TSharedPtrTS<FStreamSegmentRequestDASH>& Request);
		FErrorDetail GetInitSegment(TSharedPtrTS<const IParserISO14496_12>& OutMP4InitSegment, const TSharedPtrTS<FStreamSegmentRequestDASH>& InRequest);
		FErrorDetail GetInitSegment(TSharedPtrTS<const IParserMKV>& OutMKVInitSegment, const TSharedPtrTS<FStreamSegmentRequestDASH>& InRequest);
		FErrorDetail RetrieveSideloadedFile(TSharedPtrTS<const TArray<uint8>>& OutData, const TSharedPtrTS<FStreamSegmentRequestDASH>& InRequest);
		void CheckForInbandDASHEvents();
		void HandleEventMessages();

		void LogMessage(IInfoLog::ELevel Level, const FString& Message);

		int32 HTTPProgressCallback(const IElectraHttpManager::FRequest* Request);
		void HTTPCompletionCallback(const IElectraHttpManager::FRequest* Request);
		void HTTPUpdateStats(const FTimeValue& CurrentTime, const IElectraHttpManager::FRequest* Request);

		bool HasErrored() const;

		enum class EEmitType
		{
			UntilBlocked,
			AllRemaining,
			KnownDurationOnly
		};
		enum class EEmitResult
		{
			SentNothing,
			Sent,
			AllReachedEOS,
		};
		EEmitResult EmitSamples(EEmitType InEmitType, const TSharedPtrTS<FStreamSegmentRequestDASH>& InRequest);
		void UpdateAUDropState(FAccessUnit* InAU, const TSharedPtrTS<FStreamSegmentRequestDASH>& InRequest);


		// Methods from IParserISO14496_12::IReader
		int64 ReadData(void* IntoBuffer, int64 NumBytesToRead) override;
		bool HasReachedEOF() const override;
		bool HasReadBeenAborted() const override;
		int64 GetCurrentOffset() const override;
		// Methods from IParserISO14496_12::IBoxCallback
		IParserISO14496_12::IBoxCallback::EParseContinuation OnFoundBox(IParserISO14496_12::FBoxType Box, int64 BoxSizeInBytes, int64 FileDataOffset, int64 BoxDataOffset) override;
		IParserISO14496_12::IBoxCallback::EParseContinuation OnEndOfBox(IParserISO14496_12::FBoxType Box, int64 BoxSizeInBytes, int64 FileDataOffset, int64 BoxDataOffset) override;

		// Methods from IParserMKV::IReader
		int64 MKVReadData(void* InDestinationBuffer, int64 InNumBytesToRead, int64 InFromOffset) override;
		int64 MKVGetCurrentFileOffset() const override;
		int64 MKVGetTotalSize() override;
		bool MKVHasReadBeenAborted() const override;
	};

	FStreamHandler						StreamHandlers[3];		// 0 = video, 1 = audio, 2 = subtitle
	IPlayerSessionServices*				PlayerSessionService = nullptr;
	bool								bIsStarted = false;
	FErrorDetail						ErrorDetail;
};


} // namespace Electra

