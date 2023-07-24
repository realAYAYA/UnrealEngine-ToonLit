// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Player/PlayerStreamReader.h"
#include "Player/AdaptiveStreamingPlayerResourceRequest.h"
#include "Demuxer/ParserISO14496-12.h"
#include "InitSegmentCacheHLS.h"
#include "LicenseKeyCacheHLS.h"
#include "HTTP/HTTPManager.h"
#include "Crypto/StreamCryptoAES128.h"

namespace Electra
{

class FStreamSegmentRequestHLSfmp4 : public IStreamSegment
{
public:
	FStreamSegmentRequestHLSfmp4();
	virtual ~FStreamSegmentRequestHLSfmp4();

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
	bool GetStartupDelay(FTimeValue& OutStartTime, FTimeValue& OutTimeIntoSegment, FTimeValue& OutSegmentDuration) const override
	{ return false; }

	FString																		URL;
	ElectraHTTPStream::FHttpRange												Range;

	EStreamType																	StreamType;							//!< Type of stream (video, audio, etc.)
	uint32																		StreamUniqueID;						//!< The unique stream ID identifying the stream for which this is a request.
	int32																		Bitrate;
	int32																		QualityLevel;

	TSharedPtrTS<IPlaybackAssetRepresentation>									Representation;
	TSharedPtrTS<IPlaybackAssetAdaptationSet>									AdaptationSet;
	TSharedPtrTS<ITimelineMediaAsset>											MediaAsset;
	FString																		CDN;

	//FTimeValue																	PlaylistRelativeStartTime;			//!< The start time of this segment within the media playlist.
	FTimeValue																	AbsoluteDateTime;					//!< The absolute start time of this segment as declared through EXT-X-PROGRAM-DATE-TIME mapping
	FTimeValue																	SegmentDuration;					//!< Duration of the segment as specified in the media playlist.
	int64																		MediaSequence;						//!< The media sequence number of this segment.
	int64																		DiscontinuitySequence;				//!< The discontinuity index after which this segment is located in the media playlist.
	int32																		LocalIndex;							//!< Local index of the segment in the media playlist at the time the request was generated.

	int64																		TimestampSequenceIndex;
	FTimeValue																	EarliestPTS;
	FTimeValue																	LastPTS;
	bool																		bFrameAccuracyRequired;

	bool																		bIsPrefetch;
	bool																		bIsEOSSegment;

	int32																		NumOverallRetries;					//!< Number of retries for this _segment_ across all possible quality levels and CDNs.
	bool																		bInsertFillerData;

	bool																		bHasEncryptedSegments;

	TSharedPtrTS<FBufferSourceInfo>												SourceBufferInfo;

	TSharedPtrTS<IInitSegmentCacheHLS> 											InitSegmentCache;
	TSharedPtrTS<const FManifestHLSInternal::FMediaStream::FInitSegmentInfo>	InitSegmentInfo;

	TSharedPtrTS<ILicenseKeyCacheHLS> 											LicenseKeyCache;
	TSharedPtrTS<const FManifestHLSInternal::FMediaStream::FDRMKeyInfo>			LicenseKeyInfo;

	// List of dependent streams. Usually set for initial playback start requests.
	TArray<TSharedPtrTS<FStreamSegmentRequestHLSfmp4>>							DependentStreams;
	bool																		bIsInitialStartRequest;

	uint32																		CurrentPlaybackSequenceID;			//!< Set by the player before adding the request to the stream reader.

	Metrics::FSegmentDownloadStats												DownloadStats;
	HTTP::FConnectionInfo														ConnectionInfo;
};



/**
 *
**/
class FStreamReaderHLSfmp4 : public IStreamReader
{
public:
	FStreamReaderHLSfmp4();
	virtual ~FStreamReaderHLSfmp4();

	UEMediaError Create(IPlayerSessionServices* PlayerSessionService, const CreateParam& InCreateParam) override;
	void Close() override;

	//! Adds a request to read from a stream
	EAddResult AddRequest(uint32 CurrentPlaybackSequenceID, TSharedPtrTS<IStreamSegment> Request) override;

	//! Cancels any ongoing requests of the given stream type. Silent cancellation will not notify OnFragmentClose() or OnFragmentReachedEOS().
	void CancelRequest(EStreamType StreamType, bool bSilent) override;

	//! Cancels all pending requests.
	void CancelRequests() override;

private:

	struct FStreamHandler : public FMediaThread, public IParserISO14496_12::IReader, public IParserISO14496_12::IBoxCallback
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
				DecryptedPos = 0;
			}
			TSharedPtrTS<IElectraHttpManager::FReceiveBuffer>	ReceiveBuffer;
			int64												ParsePos;
			int64												MaxParsePos;
			int32												DecryptedPos;
		};


		class FStaticResourceRequest : public IAdaptiveStreamingPlayerResourceRequest
		{
		public:
			FStaticResourceRequest(FString InURL, EPlaybackResourceType InType)
				: URL(InURL)
				, Type(InType)
			{ }

			virtual ~FStaticResourceRequest()
			{ }

			EPlaybackResourceType GetResourceType() const override
			{ return Type; }

			FString GetResourceURL() const override
			{ return URL; }

			void SetPlaybackData(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>	PlaybackData) override
			{ Data = PlaybackData; }

			void SignalDataReady() override
			{ DoneSignal.Signal(); }

			bool IsDone() const
			{ return DoneSignal.IsSignaled(); }

			bool WaitDone(int32 WaitMicros)
			{ return DoneSignal.WaitTimeout(WaitMicros); }

			TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> GetData()
			{ return Data; }

		private:
			FString												URL;
			TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>		Data;
			FMediaEvent											DoneSignal;
			EPlaybackResourceType								Type;
		};

		enum class EInitSegmentResult
		{
			Ok,
			AlreadyCached,
			DownloadError,
			ParseError,
			InvalidFormat,
			LicenseKeyError
		};

		enum class ELicenseKeyResult
		{
			Ok,
			AlreadyCached,
			DownloadError,
			FormatError
		};

		static uint32											UniqueDownloadID;

		IStreamReader::CreateParam								Parameters;
		TSharedPtrTS<FStreamSegmentRequestHLSfmp4>				CurrentRequest;
		FMediaSemaphore											WorkSignal;
		volatile bool											bWasStarted = false;
		volatile bool											bTerminate = false;
		volatile bool											bRequestCanceled = false;
		volatile bool											bSilentCancellation = false;
		volatile bool											bHasErrored = false;
		bool													bAbortedByABR = false;
		bool													bAllowEarlyEmitting = false;
		bool													bFillRemainingDuration = false;

		IPlayerSessionServices*									PlayerSessionService = nullptr;
		FReadBuffer												ReadBuffer;
		TSharedPtr<ElectraCDM::IStreamDecrypterAES128, ESPMode::ThreadSafe>	Decrypter;
		FMediaEvent												DownloadCompleteSignal;
		TSharedPtrTS<IParserISO14496_12>						MP4Parser;
		int32													NumMOOFBoxesFound = 0;
		bool													bParsingInitSegment = false;
		bool													bInvalidMP4 = false;

		TMediaQueueDynamicNoLock<FAccessUnit *>					AccessUnitFIFO;
		FTimeValue 												DurationSuccessfullyRead;
		FTimeValue 												DurationSuccessfullyDelivered;

		FMediaCriticalSection									MetricUpdateLock;
		int32													ProgressReportCount = 0;
		TSharedPtrTS<IAdaptiveStreamSelector>					StreamSelector;
		FString													ABRAbortReason;

		FStreamHandler();
		virtual ~FStreamHandler();
		void Cancel(bool bSilent);
		void SignalWork();
		void WorkerThread();
		void HandleRequest();
		EInitSegmentResult GetInitSegment(FErrorDetail& OutErrorDetail, TSharedPtrTS<const IParserISO14496_12>& OutMP4InitSegment, const TSharedPtrTS<FStreamSegmentRequestHLSfmp4>& InRequest);
		ELicenseKeyResult GetLicenseKey(FErrorDetail& OutErrorDetail, TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& OutLicenseKeyData, const TSharedPtrTS<FStreamSegmentRequestHLSfmp4>& InRequest, const TSharedPtr<const FManifestHLSInternal::FMediaStream::FDRMKeyInfo, ESPMode::ThreadSafe>& LicenseKeyInfo);

		void LogMessage(IInfoLog::ELevel Level, const FString& Message);

		int32 HTTPProgressCallback(const IElectraHttpManager::FRequest* Request);
		void HTTPCompletionCallback(const IElectraHttpManager::FRequest* Request);
		void HTTPUpdateStats(const FTimeValue& CurrentTime, const IElectraHttpManager::FRequest* Request);

		bool HasErrored() const;

		// Methods from IParserISO14496_12::IReader
		int64 ReadData(void* IntoBuffer, int64 NumBytesToRead) override;
		bool HasReachedEOF() const override;
		bool HasReadBeenAborted() const override;
		int64 GetCurrentOffset() const override;
		// Methods from IParserISO14496_12::IBoxCallback
		IParserISO14496_12::IBoxCallback::EParseContinuation OnFoundBox(IParserISO14496_12::FBoxType Box, int64 BoxSizeInBytes, int64 FileDataOffset, int64 BoxDataOffset) override;
		IParserISO14496_12::IBoxCallback::EParseContinuation OnEndOfBox(IParserISO14496_12::FBoxType Box, int64 BoxSizeInBytes, int64 FileDataOffset, int64 BoxDataOffset) override;
	};

	// Currently set to use 2 handlers, one for video and one for audio. This could become a pool of n if we need to stream
	// multiple dependent segments, keeping a pool of available and active handlers to cycle between.
	FStreamHandler						StreamHandlers[2];		// 0 = video (MEDIAstreamType_Video), 1 = audio (MEDIAstreamType_Audio)
	IPlayerSessionServices*				PlayerSessionService;
	bool								bIsStarted;
	FErrorDetail						ErrorDetail;
};




} // namespace Electra

