// Copyright Epic Games, Inc. All Rights Reserved.

#include "HTTP/HTTPManager.h"
#include "StreamReaderHLSfmp4.h"
#include "Demuxer/ParserISO14496-12.h"
#include "StreamAccessUnitBuffer.h"
#include "Player/PlayerLicenseKey.h"
#include "HAL/LowLevelMemTracker.h"
#include "ElectraPlayerPrivate.h"

#define INTERNAL_ERROR_INIT_SEGMENT_DOWNLOAD_ERROR					1
#define INTERNAL_ERROR_INIT_SEGMENT_PARSE_ERROR						2
#define INTERNAL_ERROR_INIT_SEGMENT_UNSUPPORTED_FORMAT				3
#define INTERNAL_ERROR_INIT_SEGMENT_LICENSEKEY_ERROR				10


DECLARE_CYCLE_STAT(TEXT("FStreamReaderHLSfmp4_HandleRequest"), STAT_ElectraPlayer_HLS_StreamReader, STATGROUP_ElectraPlayer);


namespace Electra
{

FStreamSegmentRequestHLSfmp4::FStreamSegmentRequestHLSfmp4()
{
	StreamType  			  = EStreamType::Video;
	StreamUniqueID  		  = 0;
	Bitrate 				  = 0;
	QualityLevel			  = 0;
	MediaSequence   		  = -1;
	DiscontinuitySequence     = -1;
	LocalIndex  			  = -1;
	TimestampSequenceIndex	  = 0;
	bIsPrefetch 			  = false;
	bIsEOSSegment   		  = false;
	bHasEncryptedSegments     = false;
	NumOverallRetries   	  = 0;
	bInsertFillerData   	  = false;
	bIsInitialStartRequest    = false;
	CurrentPlaybackSequenceID = ~0U;
	bFrameAccuracyRequired	  = false;
}

FStreamSegmentRequestHLSfmp4::~FStreamSegmentRequestHLSfmp4()
{
}


void FStreamSegmentRequestHLSfmp4::SetPlaybackSequenceID(uint32 PlaybackSequenceID)
{
	CurrentPlaybackSequenceID = PlaybackSequenceID;
}

uint32 FStreamSegmentRequestHLSfmp4::GetPlaybackSequenceID() const
{
	return CurrentPlaybackSequenceID;
}

void FStreamSegmentRequestHLSfmp4::SetExecutionDelay(const FTimeValue& UTCNow, const FTimeValue& ExecutionDelay)
{
	// No-op for HLS.
}

FTimeValue FStreamSegmentRequestHLSfmp4::GetExecuteAtUTCTime() const
{
	// Right now.
	return FTimeValue::GetInvalid();
}


EStreamType FStreamSegmentRequestHLSfmp4::GetType() const
{
	return StreamType;
}

void FStreamSegmentRequestHLSfmp4::GetDependentStreams(TArray<TSharedPtrTS<IStreamSegment>>& OutDependentStreams) const
{
	OutDependentStreams.Empty();
	for(auto& Stream : DependentStreams)
	{
		OutDependentStreams.Emplace(Stream);
	}
}

void FStreamSegmentRequestHLSfmp4::GetRequestedStreams(TArray<TSharedPtrTS<IStreamSegment>>& OutRequestedStreams)
{
	OutRequestedStreams.Empty();
	OutRequestedStreams.Push(SharedThis(this));
	for(auto& Stream : DependentStreams)
	{
		OutRequestedStreams.Emplace(Stream);
	}
}


void FStreamSegmentRequestHLSfmp4::GetEndedStreams(TArray<TSharedPtrTS<IStreamSegment>>& OutAlreadyEndedStreams)
{
	OutAlreadyEndedStreams.Empty();
	if (bIsEOSSegment)
	{
		OutAlreadyEndedStreams.Push(SharedThis(this));
	}
	for(int32 i=0; i<DependentStreams.Num(); ++i)
	{
		if (DependentStreams[i]->bIsEOSSegment)
		{
			OutAlreadyEndedStreams.Push(DependentStreams[i]);
		}
	}
}

FTimeValue FStreamSegmentRequestHLSfmp4::GetFirstPTS() const
{
	return EarliestPTS.IsValid() && !EarliestPTS.IsPositiveInfinity() && EarliestPTS > AbsoluteDateTime ? EarliestPTS : AbsoluteDateTime;
}

int32 FStreamSegmentRequestHLSfmp4::GetQualityIndex() const
{
	return QualityLevel;
}

int32 FStreamSegmentRequestHLSfmp4::GetBitrate() const
{
	return Bitrate;
}

void FStreamSegmentRequestHLSfmp4::GetDownloadStats(Metrics::FSegmentDownloadStats& OutStats) const
{
	OutStats = DownloadStats;
}

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

FStreamReaderHLSfmp4::FStreamReaderHLSfmp4()
	: PlayerSessionService(nullptr)
	, bIsStarted(false)
{
}


FStreamReaderHLSfmp4::~FStreamReaderHLSfmp4()
{
	Close();
}

UEMediaError FStreamReaderHLSfmp4::Create(IPlayerSessionServices* InPlayerSessionService, const IStreamReader::CreateParam& InCreateParam)
{
	PlayerSessionService = InPlayerSessionService;

	if (!InCreateParam.MemoryProvider || !InCreateParam.EventListener)
	{
		return UEMEDIA_ERROR_BAD_ARGUMENTS;
	}

	bIsStarted = true;
	for(int32 i=0; i<FMEDIA_STATIC_ARRAY_COUNT(StreamHandlers); ++i)
	{
		StreamHandlers[i].PlayerSessionService = PlayerSessionService;
		StreamHandlers[i].Parameters		   = InCreateParam;
		StreamHandlers[i].bWasStarted		   = false;
		StreamHandlers[i].bTerminate		   = false;
		StreamHandlers[i].bRequestCanceled     = false;
		StreamHandlers[i].bSilentCancellation  = false;
		StreamHandlers[i].bHasErrored   	   = false;

		StreamHandlers[i].ThreadSetName(i==0?"ElectraPlayer::fmp4 Video":"ElectraPlayer::fmp4 Audio");
	}
	return UEMEDIA_ERROR_OK;
}

void FStreamReaderHLSfmp4::Close()
{
	if (bIsStarted)
	{
		bIsStarted = false;
		// Signal the worker threads to end.
		for(int32 i=0; i<FMEDIA_STATIC_ARRAY_COUNT(StreamHandlers); ++i)
		{
			StreamHandlers[i].bTerminate = true;
			StreamHandlers[i].Cancel(true);
			StreamHandlers[i].SignalWork();
		}
		// Wait until they finished.
		for(int32 i=0; i<FMEDIA_STATIC_ARRAY_COUNT(StreamHandlers); ++i)
		{
			if (StreamHandlers[i].bWasStarted)
			{
				StreamHandlers[i].ThreadWaitDone();
				StreamHandlers[i].ThreadReset();
			}
		}
	}
}

IStreamReader::EAddResult FStreamReaderHLSfmp4::AddRequest(uint32 CurrentPlaybackSequenceID, TSharedPtrTS<IStreamSegment> InRequest)
{
	TSharedPtrTS<FStreamSegmentRequestHLSfmp4> Request = StaticCastSharedPtr<FStreamSegmentRequestHLSfmp4>(InRequest);

	// Dependent streams will be added individually. Anything that is still set here must be ignored from this point forth.
	Request->DependentStreams.Empty();

	// Video and audio only for now.
	if (Request->GetType() != EStreamType::Video && Request->GetType() != EStreamType::Audio)
	{
		check(!"no good");
		ErrorDetail.SetMessage(FString::Printf(TEXT("Request is not video or audio")));
		return IStreamReader::EAddResult::Error;
	}

	// Get the handler for the request.
	FStreamHandler* Handler = nullptr;
	switch(Request->GetType())
	{
		case  EStreamType::Video:
			Handler = &StreamHandlers[0];
			break;
		case  EStreamType::Audio:
			Handler = &StreamHandlers[1];
			break;
		default:
			check(!"Whoops");
			break;
	}
	if (!Handler)
	{
		check(!"no good");
		ErrorDetail.SetMessage(FString::Printf(TEXT("No handler for stream type")));
		return IStreamReader::EAddResult::Error;
	}
	// Is the handler busy?
	if (Handler->CurrentRequest.IsValid())
	{
		check(!"why is the handler busy??");
		return IStreamReader::EAddResult::TryAgainLater;
	}

	Request->SetPlaybackSequenceID(CurrentPlaybackSequenceID);
	// Only add the request if this is not an EOD segment.
	if (!Request->bIsEOSSegment)
	{
		if (!Handler->bWasStarted)
		{
			Handler->ThreadStart(FMediaRunnable::FStartDelegate::CreateRaw(Handler, &FStreamHandler::WorkerThread));
			Handler->bWasStarted = true;
		}
		Handler->bRequestCanceled = false;
		Handler->bSilentCancellation = false;
		Handler->CurrentRequest = Request;
		Handler->SignalWork();
	}
	return IStreamReader::EAddResult::Added;
}

void FStreamReaderHLSfmp4::CancelRequest(EStreamType StreamType, bool bSilent)
{
	if (StreamType == EStreamType::Video)
	{
		StreamHandlers[0].Cancel(bSilent);
	}
	else if (StreamType == EStreamType::Audio)
	{
		StreamHandlers[1].Cancel(bSilent);
	}
}


void FStreamReaderHLSfmp4::CancelRequests()
{
	for(int32 i=0; i<FMEDIA_STATIC_ARRAY_COUNT(StreamHandlers); ++i)
	{
		StreamHandlers[i].Cancel(false);
	}
}





uint32 FStreamReaderHLSfmp4::FStreamHandler::UniqueDownloadID = 1;

FStreamReaderHLSfmp4::FStreamHandler::FStreamHandler()
{
}

FStreamReaderHLSfmp4::FStreamHandler::~FStreamHandler()
{
	// NOTE: The thread will have been terminated by the enclosing FStreamReaderHLSfmp4's Close() method!
}

void FStreamReaderHLSfmp4::FStreamHandler::Cancel(bool bSilent)
{
	bSilentCancellation = bSilent;
	bRequestCanceled = true;
}

void FStreamReaderHLSfmp4::FStreamHandler::SignalWork()
{
	WorkSignal.Release();
}

void FStreamReaderHLSfmp4::FStreamHandler::WorkerThread()
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);

	check(PlayerSessionService);
	StreamSelector = PlayerSessionService->GetStreamSelector();
	check(StreamSelector.IsValid());
	while(!bTerminate)
	{
		WorkSignal.Obtain();
		if (!bTerminate)
		{
			if (CurrentRequest.IsValid())
			{
				if (!bRequestCanceled)
				{
					HandleRequest();
				}
				else
				{
					CurrentRequest.Reset();
				}
			}
			bRequestCanceled = false;
			bSilentCancellation = false;
		}
	}
	StreamSelector.Reset();
}


void FStreamReaderHLSfmp4::FStreamHandler::LogMessage(IInfoLog::ELevel Level, const FString& Message)
{
	if (PlayerSessionService)
	{
		PlayerSessionService->PostLog(Facility::EFacility::HLSFMP4Reader, Level, Message);
	}
}


int32 FStreamReaderHLSfmp4::FStreamHandler::HTTPProgressCallback(const IElectraHttpManager::FRequest* Request)
{
	HTTPUpdateStats(MEDIAutcTime::Current(), Request);
	++ProgressReportCount;

	// Aborted?
	return HasReadBeenAborted() ? 1 : 0;
}

void FStreamReaderHLSfmp4::FStreamHandler::HTTPCompletionCallback(const IElectraHttpManager::FRequest* Request)
{
	HTTPUpdateStats(FTimeValue::GetInvalid(), Request);

	bHasErrored = Request->ConnectionInfo.StatusInfo.ErrorDetail.IsError();
	DownloadCompleteSignal.Signal();
}

void FStreamReaderHLSfmp4::FStreamHandler::HTTPUpdateStats(const FTimeValue& CurrentTime, const IElectraHttpManager::FRequest* Request)
{
	TSharedPtrTS<FStreamSegmentRequestHLSfmp4> SegmentRequest = CurrentRequest;
	if (SegmentRequest.IsValid())
	{
		FMediaCriticalSection::ScopedLock lock(MetricUpdateLock);
		SegmentRequest->ConnectionInfo = Request->ConnectionInfo;
		// Update the current download stats which we report periodically to the ABR.
		Metrics::FSegmentDownloadStats& ds = SegmentRequest->DownloadStats;
		if (Request->ConnectionInfo.EffectiveURL.Len())
		{
			ds.URL 			  = Request->ConnectionInfo.EffectiveURL;
		}
		ds.HTTPStatusCode     = Request->ConnectionInfo.StatusInfo.HTTPStatus;
		ds.TimeToFirstByte    = Request->ConnectionInfo.TimeUntilFirstByte;
		ds.TimeToDownload     = ((CurrentTime.IsValid() ? CurrentTime : Request->ConnectionInfo.RequestEndTime) - Request->ConnectionInfo.RequestStartTime).GetAsSeconds();
		ds.ByteSize 		  = Request->ConnectionInfo.ContentLength;
		ds.NumBytesDownloaded = Request->ConnectionInfo.BytesReadSoFar;
	}
}




FStreamReaderHLSfmp4::FStreamHandler::ELicenseKeyResult FStreamReaderHLSfmp4::FStreamHandler::GetLicenseKey(FErrorDetail& OutErrorDetail, TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& OutLicenseKeyData, const TSharedPtrTS<FStreamSegmentRequestHLSfmp4>& InRequest, const TSharedPtr<const FManifestHLSInternal::FMediaStream::FDRMKeyInfo, ESPMode::ThreadSafe>& LicenseKeyInfo)
{
	if (LicenseKeyInfo.IsValid() && LicenseKeyInfo->URI.Len())
	{
		TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> LicenseKeyData;
		if (InRequest->LicenseKeyCache.IsValid())
		{
			LicenseKeyData = InRequest->LicenseKeyCache->GetLicenseKeyFor(LicenseKeyInfo);
		}
		if (LicenseKeyData.IsValid())
		{
			OutLicenseKeyData = LicenseKeyData;
			return FStreamReaderHLSfmp4::FStreamHandler::ELicenseKeyResult::AlreadyCached;
		}
		else
		{
			TSharedPtrTS<IElectraHttpManager::FRequest> HTTP(new IElectraHttpManager::FRequest);
			ReadBuffer.Reset();
			ReadBuffer.ReceiveBuffer = MakeSharedTS<IElectraHttpManager::FReceiveBuffer>();

			Metrics::FSegmentDownloadStats& ds = InRequest->DownloadStats;
			ds.URL = LicenseKeyInfo->URI;

			HTTP->Parameters.URL   = LicenseKeyInfo->URI;
			HTTP->ReceiveBuffer    = ReadBuffer.ReceiveBuffer;

			ProgressReportCount = 0;
			DownloadCompleteSignal.Reset();

			// Is there a static resource provider that we can try?
			bool bHaveStaticResponse = false;
			TSharedPtr<IAdaptiveStreamingPlayerResourceProvider, ESPMode::ThreadSafe> StaticResourceProvider = PlayerSessionService->GetStaticResourceProvider();
			if (StaticResourceProvider)
			{
				TSharedPtr<FStaticResourceRequest, ESPMode::ThreadSafe>	StaticRequest = MakeShared<FStaticResourceRequest, ESPMode::ThreadSafe>(/*SharedThis(this), DoneSignal,*/ LicenseKeyInfo->URI, IAdaptiveStreamingPlayerResourceRequest::EPlaybackResourceType::LicenseKey);
				StaticResourceProvider->ProvideStaticPlaybackDataForURL(StaticRequest);
				while(!HasReadBeenAborted())
				{
					if (StaticRequest->WaitDone(1000 * 100))
					{
						TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> KeyData = StaticRequest->GetData();
						if (KeyData.IsValid())
						{
							// Copy the response over into the receive buffer as if it was received through the http request.
							ReadBuffer.ReceiveBuffer->Buffer.Reserve(KeyData->Num());
							ReadBuffer.ReceiveBuffer->Buffer.PushData(KeyData->GetData(), KeyData->Num());
							ReadBuffer.ReceiveBuffer->Buffer.SetEOD();
							bHaveStaticResponse = true;
						}
						break;
					}
				}
			}

			// When we did not get a static key response we have to issue the request for real.
			if (!bHaveStaticResponse)
			{
				TSharedPtrTS<IElectraHttpManager::FProgressListener>	ProgressListener(new IElectraHttpManager::FProgressListener);
				ProgressListener->ProgressDelegate   = IElectraHttpManager::FProgressListener::FProgressDelegate::CreateRaw(this, &FStreamHandler::HTTPProgressCallback);
				ProgressListener->CompletionDelegate = IElectraHttpManager::FProgressListener::FCompletionDelegate::CreateRaw(this, &FStreamHandler::HTTPCompletionCallback);
				HTTP->ProgressListener = ProgressListener;
				PlayerSessionService->GetHTTPManager()->AddRequest(HTTP, false);
				while(!HasReadBeenAborted())
				{
					if (DownloadCompleteSignal.WaitTimeout(1000 * 100))
					{
						break;
					}
				}
				ProgressListener.Reset();
				// Note: It is only safe to access the connection info when the HTTP request has completed or the request been removed.
				PlayerSessionService->GetHTTPManager()->RemoveRequest(HTTP, false);
			}

			InRequest->ConnectionInfo = HTTP->ConnectionInfo;
			if (!HTTP->ConnectionInfo.StatusInfo.ErrorDetail.IsError())
			{
				// Notify license key download ok.
				PlayerSessionService->SendMessageToPlayer(FLicenseKeyMessage::Create(FLicenseKeyMessage::EReason::LicenseKeyDownload, FErrorDetail(), &HTTP->ConnectionInfo));

				// There is not much we can validate here. The key is the direct key data without any wrapping.
				if (ReadBuffer.ReceiveBuffer->Buffer.Num() == 16)
				{
					LicenseKeyData = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>();
					LicenseKeyData->AddUninitialized(ReadBuffer.ReceiveBuffer->Buffer.Num());
					FMemory::Memcpy(LicenseKeyData->GetData(), ReadBuffer.ReceiveBuffer->Buffer.GetLinearReadData(), ReadBuffer.ReceiveBuffer->Buffer.GetLinearReadSize());
					if (InRequest->LicenseKeyCache.IsValid())
					{
						InRequest->LicenseKeyCache->AddLicenseKey(LicenseKeyData, LicenseKeyInfo, FTimeValue::GetPositiveInfinity());
					}
					OutLicenseKeyData = LicenseKeyData;
					return FStreamReaderHLSfmp4::FStreamHandler::ELicenseKeyResult::Ok;
				}
				else
				{
					// Notify license key download failure.
					PlayerSessionService->SendMessageToPlayer(FLicenseKeyMessage::Create(FLicenseKeyMessage::EReason::LicenseKeyData,
																						 FErrorDetail().SetError(UEMEDIA_ERROR_FORMAT_ERROR).SetFacility(Facility::EFacility::LicenseKey).SetCode((uint16)FStreamReaderHLSfmp4::FStreamHandler::ELicenseKeyResult::FormatError).SetMessage("Invalid license key length"),
																						 &HTTP->ConnectionInfo));
					return FStreamReaderHLSfmp4::FStreamHandler::ELicenseKeyResult::FormatError;
				}
			}
			else
			{
				// Notify license key download failure.
				PlayerSessionService->SendMessageToPlayer(FLicenseKeyMessage::Create(FLicenseKeyMessage::EReason::LicenseKeyDownload,
																					 FErrorDetail().SetError(UEMEDIA_ERROR_READ_ERROR).SetFacility(Facility::EFacility::LicenseKey).SetCode((uint16)FStreamReaderHLSfmp4::FStreamHandler::ELicenseKeyResult::DownloadError).SetMessage("License key download failure"),
																					 &HTTP->ConnectionInfo));
				return FStreamReaderHLSfmp4::FStreamHandler::ELicenseKeyResult::DownloadError;
			}
		}
	}
	return FStreamReaderHLSfmp4::FStreamHandler::ELicenseKeyResult::DownloadError;
}


FStreamReaderHLSfmp4::FStreamHandler::EInitSegmentResult FStreamReaderHLSfmp4::FStreamHandler::GetInitSegment(FErrorDetail& OutErrorDetail, TSharedPtrTS<const IParserISO14496_12>& OutMP4InitSegment, const TSharedPtrTS<FStreamSegmentRequestHLSfmp4>& Request)
{
	bParsingInitSegment = true;
	bInvalidMP4 = false;
	if (Request->InitSegmentInfo.IsValid() && Request->InitSegmentInfo->URI.Len())
	{
		TSharedPtrTS<const IParserISO14496_12> MP4InitSegment = Request->InitSegmentCache->GetInitSegmentFor(Request->InitSegmentInfo);
		if (!MP4InitSegment.IsValid())
		{
			TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> LicenseKeyData;
			// Is the init segment encrypted?
			if (Request->InitSegmentInfo->DRMKeyInfo.IsValid())
			{
				ELicenseKeyResult LicenseKeyResult = GetLicenseKey(OutErrorDetail, LicenseKeyData, Request, Request->InitSegmentInfo->DRMKeyInfo);
				if (LicenseKeyResult == FStreamReaderHLSfmp4::FStreamHandler::ELicenseKeyResult::DownloadError)
				{
					return FStreamReaderHLSfmp4::FStreamHandler::EInitSegmentResult::DownloadError;
				}
				else if (LicenseKeyResult == FStreamReaderHLSfmp4::FStreamHandler::ELicenseKeyResult::FormatError)
				{
					return FStreamReaderHLSfmp4::FStreamHandler::EInitSegmentResult::LicenseKeyError;
				}
			}

			TSharedPtrTS<IElectraHttpManager::FRequest> 			HTTP(new IElectraHttpManager::FRequest);
			TSharedPtrTS<IElectraHttpManager::FProgressListener>	ProgressListener(new IElectraHttpManager::FProgressListener);
			ProgressListener->ProgressDelegate   = IElectraHttpManager::FProgressListener::FProgressDelegate::CreateRaw(this, &FStreamHandler::HTTPProgressCallback);
			ProgressListener->CompletionDelegate = IElectraHttpManager::FProgressListener::FCompletionDelegate::CreateRaw(this, &FStreamHandler::HTTPCompletionCallback);
			ReadBuffer.Reset();
			ReadBuffer.ReceiveBuffer = MakeSharedTS<IElectraHttpManager::FReceiveBuffer>();

			FString RequestURL = Request->InitSegmentInfo->URI;

			Metrics::FSegmentDownloadStats& ds = Request->DownloadStats;
			ds.URL  	   = RequestURL;
			ds.SegmentType = Metrics::ESegmentType::Init;

			HTTP->Parameters.URL   = RequestURL;
			HTTP->ReceiveBuffer    = ReadBuffer.ReceiveBuffer;
			HTTP->ProgressListener = ProgressListener;
			if (Request->InitSegmentInfo->ByteRange.IsSet())
			{
				HTTP->Parameters.Range.Start		= Request->InitSegmentInfo->ByteRange.GetStart();
				HTTP->Parameters.Range.EndIncluding = Request->InitSegmentInfo->ByteRange.GetEnd();
			}

			ProgressReportCount = 0;
			DownloadCompleteSignal.Reset();
			PlayerSessionService->GetHTTPManager()->AddRequest(HTTP, false);

			while(!HasReadBeenAborted())
			{
				if (DownloadCompleteSignal.WaitTimeout(1000 * 100))
				{
					break;
				}
			}

			ProgressListener.Reset();
			// Note: It is only safe to access the connection info when the HTTP request has completed or the request been removed.
			PlayerSessionService->GetHTTPManager()->RemoveRequest(HTTP, false);
			Request->ConnectionInfo = HTTP->ConnectionInfo;

			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_HLS_StreamReader);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, HLS_StreamReader);
			if (!HTTP->ConnectionInfo.StatusInfo.ErrorDetail.IsError())
			{
// TODO: If encrypted we must now decrypt it!!
//		 The question is how to do this if there is no explicit IV and the media sequence number is to be used
//		 with the init segment being static and not having one.
//		 Presently the assumption is that the init segment is not encrypted.

				TSharedPtrTS<IParserISO14496_12> InitSegmentParser = IParserISO14496_12::CreateParser();
				UEMediaError parseError = InitSegmentParser->ParseHeader(this, this, PlayerSessionService, nullptr);
				if (!bInvalidMP4)
				{
					if (parseError == UEMEDIA_ERROR_OK || parseError == UEMEDIA_ERROR_END_OF_STREAM)
					{
						// Parse the tracks of the init segment. We do this mainly to get to the CSD we might need should we have to insert filler data later.
						parseError = InitSegmentParser->PrepareTracks(PlayerSessionService, TSharedPtrTS<const IParserISO14496_12>());
						if (parseError == UEMEDIA_ERROR_OK)
						{
							Request->InitSegmentCache->AddInitSegment(InitSegmentParser, Request->InitSegmentInfo, FTimeValue::GetPositiveInfinity());
							OutMP4InitSegment = InitSegmentParser;
							return FStreamReaderHLSfmp4::FStreamHandler::EInitSegmentResult::Ok;
						}
						else
						{
							return FStreamReaderHLSfmp4::FStreamHandler::EInitSegmentResult::ParseError;
						}
					}
					else
					{
						return FStreamReaderHLSfmp4::FStreamHandler::EInitSegmentResult::ParseError;
					}
				}
				else
				{
					return FStreamReaderHLSfmp4::FStreamHandler::EInitSegmentResult::InvalidFormat;
				}
			}
			else
			{
				return FStreamReaderHLSfmp4::FStreamHandler::EInitSegmentResult::DownloadError;
			}
		}
		else
		{
			OutMP4InitSegment = MP4InitSegment;
		}
	}
	return FStreamReaderHLSfmp4::FStreamHandler::EInitSegmentResult::AlreadyCached;
}


void FStreamReaderHLSfmp4::FStreamHandler::HandleRequest()
{
	UEMediaError								Error;
	bool										bIsEmptyFillerSegment = false;

	// Get the request into a local TSharedPtrTS to hold on to it.
	TSharedPtrTS<FStreamSegmentRequestHLSfmp4> Request = CurrentRequest;

	// FIXME: We need to hold on to the retry info across several segment attempts.
	//        The connection info gets set and cleared in here a few times and we actually need
	//        to make sure the retry info is not modified.
	TSharedPtrTS<HTTP::FRetryInfo> CurrentRetryInfo = CurrentRequest->ConnectionInfo.RetryInfo;

	TSharedPtrTS<FBufferSourceInfo> SourceBufferInfo = MakeSharedTS<FBufferSourceInfo>(*Request->SourceBufferInfo);
	SourceBufferInfo->PlaybackSequenceID = Request->GetPlaybackSequenceID();

	Metrics::FSegmentDownloadStats& ds = CurrentRequest->DownloadStats;
	ds.StatsID = FMediaInterlockedIncrement(UniqueDownloadID);

	ds.FailureReason.Empty();
	ds.bWasSuccessful      = true;
	ds.bWasAborted  	   = false;
	ds.bDidTimeout  	   = false;
	ds.HTTPStatusCode      = 0;
	ds.StreamType   	   = Request->GetType();
	ds.SegmentType  	   = Metrics::ESegmentType::Media;
	ds.PresentationTime    = Request->AbsoluteDateTime.GetAsSeconds();
	ds.Bitrate  		   = Request->Bitrate;
	ds.Duration 		   = Request->SegmentDuration.GetAsSeconds();
	ds.DurationDownloaded  = 0.0;
	ds.DurationDelivered   = 0.0;
	ds.TimeToFirstByte     = 0.0;
	ds.TimeToDownload      = 0.0;
	ds.ByteSize 		   = -1;
	ds.NumBytesDownloaded  = 0;
	ds.bInsertedFillerData = false;

	ds.MediaAssetID 	= Request->MediaAsset.IsValid() ? Request->MediaAsset->GetUniqueIdentifier() : "";
	ds.AdaptationSetID  = Request->AdaptationSet.IsValid() ? Request->AdaptationSet->GetUniqueIdentifier() : "";
	ds.RepresentationID = Request->Representation.IsValid() ? Request->Representation->GetUniqueIdentifier() : "";
	ds.URL  			= Request->URL;
	ds.CDN  			= Request->CDN;
	ds.RetryNumber  	= Request->NumOverallRetries;

	bIsEmptyFillerSegment = Request->bInsertFillerData;

	TSharedPtrTS<FAccessUnit::CodecData>		CSD(new FAccessUnit::CodecData);
	TSharedPtrTS<const IParserISO14496_12>		MP4InitSegment;
	FErrorDetail								InitSegmentErrorDetail;
	EInitSegmentResult							InitSegmentResult = EInitSegmentResult::AlreadyCached;

	bHasErrored 		   = false;
	bAbortedByABR   	   = false;
	bAllowEarlyEmitting    = false;
	bFillRemainingDuration = false;
	ABRAbortReason.Empty();
	DurationSuccessfullyRead.SetToZero();
	DurationSuccessfullyDelivered.SetToZero();
	AccessUnitFIFO.Clear();

	Parameters.EventListener->OnFragmentOpen(CurrentRequest);

	if (!bIsEmptyFillerSegment)
	{
		InitSegmentResult = GetInitSegment(InitSegmentErrorDetail, MP4InitSegment, Request);
		// If we just downloaded the init segment successfully let the stream selector know.
		if (InitSegmentResult == EInitSegmentResult::Ok)
		{
			StreamSelector->ReportDownloadEnd(ds);
		}
	}
	else
	{
		// See if we have the init segment in the cache already. We won't request it if it is not as perhaps we are to insert
		// filler data because the init segment has already failed.
		if (Request->InitSegmentCache.IsValid() && Request->InitSegmentInfo.IsValid())
		{
			MP4InitSegment = Request->InitSegmentCache->GetInitSegmentFor(Request->InitSegmentInfo);
			if (MP4InitSegment.IsValid())
			{
				// Get the CSD from track 0. This is identical to what we are doing  further down with the actual track.
				check(MP4InitSegment->GetNumberOfTracks() == 1);
				if (MP4InitSegment->GetNumberOfTracks() == 1)
				{
					const IParserISO14496_12::ITrack* Track = MP4InitSegment->GetTrackByIndex(0);
					check(Track);
					if (Track)
					{
						CSD->CodecSpecificData = Track->GetCodecSpecificData();
						CSD->RawCSD = Track->GetCodecSpecificDataRAW();
						CSD->ParsedInfo = Track->GetCodecInformation();
						// Set information from the master playlist that is not available on the init segment.
						CSD->ParsedInfo.SetBitrate(Request->Bitrate);
					}
				}
			}
		}
	}

	bParsingInitSegment = false;
	bInvalidMP4 = false;
	FTimeValue NextExpectedDTS;
	FTimeValue LastKnownAUDuration;
	if (!bIsEmptyFillerSegment)
	{
		if (InitSegmentResult == EInitSegmentResult::Ok || InitSegmentResult == EInitSegmentResult::AlreadyCached)
		{
			// If the segment is encrypted we need to get the license key to decrypt it.
			TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> LicenseKeyData;
			// Is the init segment encrypted?
			if (Request->LicenseKeyInfo.IsValid())
			{
				ELicenseKeyResult LicenseKeyResult = GetLicenseKey(InitSegmentErrorDetail, LicenseKeyData, Request, Request->LicenseKeyInfo);
				if (LicenseKeyResult == FStreamReaderHLSfmp4::FStreamHandler::ELicenseKeyResult::DownloadError)
				{
					ds.FailureReason = FString::Printf(TEXT("Failed to download license key"));
					LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
					bHasErrored = true;
				}
				else if (LicenseKeyResult == FStreamReaderHLSfmp4::FStreamHandler::ELicenseKeyResult::FormatError)
				{
					ds.FailureReason = FString::Printf(TEXT("License key format error"));
					LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
					bHasErrored = true;
				}

				// Check that the encryption is AES-128 for the time being.
				if (Request->LicenseKeyInfo->Method != FManifestHLSInternal::FMediaStream::FDRMKeyInfo::EMethod::None &&
					Request->LicenseKeyInfo->Method != FManifestHLSInternal::FMediaStream::FDRMKeyInfo::EMethod::AES128)
				{
					ds.FailureReason = FString::Printf(TEXT("Unsupported encryption method"));
					LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
					bHasErrored = true;
				}

				// Create the decrypter.
				if (!bHasErrored)
				{
					if (LicenseKeyData.IsValid())
					{
						ElectraCDM::IStreamDecrypterAES128::EResult DecrypterResult;
						Decrypter = ElectraCDM::IStreamDecrypterAES128::Create();

						// Set up the IV for this segment which is either explicitly provided or the media sequence number.
						TArray<uint8> IV;
						if (Request->LicenseKeyInfo->IV.Len())
						{
							DecrypterResult = ElectraCDM::IStreamDecrypterAES128::ConvHexStringToBin(IV, TCHAR_TO_ANSI(*Request->LicenseKeyInfo->IV));
							if (DecrypterResult != ElectraCDM::IStreamDecrypterAES128::EResult::Ok)
							{
								ds.FailureReason = FString::Printf(TEXT("Bad explicit IV value"));
								LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
								bHasErrored = true;
							}
						}
						else
						{
							ElectraCDM::IStreamDecrypterAES128::MakePaddedIVFromUInt64(IV, Request->MediaSequence);
						}
						if (!bHasErrored)
						{
							DecrypterResult = Decrypter->CBCInit(*LicenseKeyData, &IV);
							if (DecrypterResult != ElectraCDM::IStreamDecrypterAES128::EResult::Ok)
							{
								ds.FailureReason = FString::Printf(TEXT("Received bad license key"));
								LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
								bHasErrored = true;
							}
						}
					}
					else
					{
						ds.FailureReason = FString::Printf(TEXT("No valid license key"));
						LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
						bHasErrored = true;
					}
				}
			}

			if (!bHasErrored)
			{
				ReadBuffer.Reset();
				ReadBuffer.ReceiveBuffer = MakeSharedTS<IElectraHttpManager::FReceiveBuffer>();

				// Start downloading the segment. Clear any stats that may have been set by the init segment download.
				FString RequestURL = Request->URL;

				ds.FailureReason.Empty();
				ds.URL  			   = RequestURL;
				ds.SegmentType  	   = Metrics::ESegmentType::Media;
				ds.bWasSuccessful      = true;
				ds.bWasAborted  	   = false;
				ds.bDidTimeout  	   = false;
				ds.HTTPStatusCode      = 0;
				ds.TimeToFirstByte     = 0.0;
				ds.TimeToDownload      = 0.0;
				ds.ByteSize 		   = -1;
				ds.NumBytesDownloaded  = 0;

				// Clear out the current connection info which may now be populated with the init segment fetch results.
				CurrentRequest->ConnectionInfo = {};

				TSharedPtrTS<IElectraHttpManager::FProgressListener>	ProgressListener(new IElectraHttpManager::FProgressListener);
				ProgressListener->ProgressDelegate   = IElectraHttpManager::FProgressListener::FProgressDelegate::CreateRaw(this, &FStreamHandler::HTTPProgressCallback);
				ProgressListener->CompletionDelegate = IElectraHttpManager::FProgressListener::FCompletionDelegate::CreateRaw(this, &FStreamHandler::HTTPCompletionCallback);
				TSharedPtrTS<IElectraHttpManager::FRequest> HTTP(new IElectraHttpManager::FRequest);
				HTTP->Parameters.URL   = RequestURL;
				HTTP->ReceiveBuffer    = ReadBuffer.ReceiveBuffer;
				HTTP->ProgressListener = ProgressListener;
				if (Request->Range.IsSet())
				{
					HTTP->Parameters.Range = Request->Range;
				}
				HTTP->ResponseCache = PlayerSessionService->GetHTTPResponseCache();

				ProgressReportCount = 0;
				DownloadCompleteSignal.Reset();
				PlayerSessionService->GetHTTPManager()->AddRequest(HTTP, false);

				MP4Parser = IParserISO14496_12::CreateParser();
				NumMOOFBoxesFound = 0;

				FTimeValue	BaseMediaDecodeTime;
				FTimeValue	TimeMappingOffset;
				bool		bDone = false;
				bool		bTimeOffsetsSet = false;
				int64		LastSuccessfulFilePos = 0;
				bool		bReadPastLastPTS = false;

				bool bIsFirstAU = true;
				while(!bDone && !HasErrored() && !HasReadBeenAborted())
				{
					Metrics::FSegmentDownloadStats::FMovieChunkInfo MoofInfo;
					MoofInfo.HeaderOffset = GetCurrentOffset();

					UEMediaError parseError = MP4Parser->ParseHeader(this, this, PlayerSessionService, MP4InitSegment.Get());
					if (parseError == UEMEDIA_ERROR_OK && !bInvalidMP4)
					{
						{
							SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_HLS_StreamReader);
							CSV_SCOPED_TIMING_STAT(ElectraPlayer, HLS_StreamReader);
							parseError = MP4Parser->PrepareTracks(PlayerSessionService, MP4InitSegment);
						}
						if (parseError == UEMEDIA_ERROR_OK)
						{
							// For the time being we only want to have a single track in the movie segments.
							check(MP4Parser->GetNumberOfTracks() == 1);
							if (MP4Parser->GetNumberOfTracks() == 1)
							{
								const IParserISO14496_12::ITrack* Track = MP4Parser->GetTrackByIndex(0);
								check(Track);
								if (Track)
								{
									CSD->CodecSpecificData = Track->GetCodecSpecificData();
									CSD->RawCSD = Track->GetCodecSpecificDataRAW();
									CSD->ParsedInfo = Track->GetCodecInformation();
									// Set information from the master playlist that is not available on the init segment.
									CSD->ParsedInfo.SetBitrate(Request->Bitrate);

									// TODO: Check that the track format matches the one we're expecting (video, audio, etc)
									IParserISO14496_12::ITrackIterator* TrackIterator = Track->CreateIterator();

									if (!bTimeOffsetsSet)
									{
										bTimeOffsetsSet 	= true;
										BaseMediaDecodeTime = FTimeValue().SetFromND(TrackIterator->GetBaseMediaDecodeTime(), TrackIterator->GetTimescale());
										TimeMappingOffset   = Request->AbsoluteDateTime - BaseMediaDecodeTime;
									}

									FTimeValue DTS;
									FTimeValue PTS;
									FTimeValue Duration;
									for(Error = TrackIterator->StartAtFirst(false); Error == UEMEDIA_ERROR_OK; Error = TrackIterator->Next())
									{
										// Get the DTS and PTS. Those are 0-based in a fragment and offset by the base media decode time of the fragment.
										DTS.SetFromND(TrackIterator->GetDTS(), TrackIterator->GetTimescale());
										PTS.SetFromND(TrackIterator->GetPTS(), TrackIterator->GetTimescale());
										DTS += TimeMappingOffset;
										PTS += TimeMappingOffset;

										// Create access unit
										FAccessUnit *AccessUnit = FAccessUnit::Create(Parameters.MemoryProvider);
										check(AccessUnit);

										Duration.SetFromND(TrackIterator->GetDuration(), TrackIterator->GetTimescale());

										AccessUnit->ESType			 = Request->GetType();
										AccessUnit->Duration  		 = Duration;
										AccessUnit->AUSize			 = (uint32) TrackIterator->GetSampleSize();
										AccessUnit->AUData   		 = AccessUnit->AllocatePayloadBuffer(AccessUnit->AUSize);
										check(AccessUnit->AUData);
										AccessUnit->bIsFirstInSequence = bIsFirstAU;
										AccessUnit->bIsSyncSample 	 = TrackIterator->IsSyncSample();
										AccessUnit->bIsDummyData  	 = false;
										AccessUnit->AUCodecData		 = CSD;

										// Calculate the drop on the fragment local DTS/PTS.
										AccessUnit->DropState = FAccessUnit::EDropState::None;
										if (PTS + Duration < Request->EarliestPTS)
										{
											AccessUnit->DropState |= FAccessUnit::EDropState::TooEarly;
										}
										if (Request->LastPTS.IsValid() && PTS >= Request->LastPTS)
										{
											AccessUnit->DropState |= FAccessUnit::EDropState::TooLate;
										}

										// Offset the AU's DTS and PTS to the time mapping of the segment.
										AccessUnit->DTS = DTS;
										AccessUnit->PTS = PTS;
										AccessUnit->SequenceIndex = Request->TimestampSequenceIndex;
										AccessUnit->DTS.SetSequenceIndex(Request->TimestampSequenceIndex);
										AccessUnit->PTS.SetSequenceIndex(Request->TimestampSequenceIndex);

										AccessUnit->EarliestPTS = Request->EarliestPTS;
										AccessUnit->EarliestPTS.SetSequenceIndex(Request->TimestampSequenceIndex);
										AccessUnit->LatestPTS = Request->LastPTS;
										AccessUnit->LatestPTS.SetSequenceIndex(Request->TimestampSequenceIndex);

										AccessUnit->BufferSourceInfo = SourceBufferInfo;

										// There should not be any gaps!
								// TODO: what if there are?
										check(GetCurrentOffset() == TrackIterator->GetSampleFileOffset());

										if (MoofInfo.PayloadStartOffset == 0)
										{
											MoofInfo.PayloadStartOffset = GetCurrentOffset();
										}

										int64 NumRead = ReadData(AccessUnit->AUData, AccessUnit->AUSize);
										if (NumRead == AccessUnit->AUSize)
										{
											MoofInfo.ContentDuration += Duration;
											DurationSuccessfullyRead += Duration;
											NextExpectedDTS = AccessUnit->DTS + Duration;
											LastKnownAUDuration = Duration;
											LastSuccessfulFilePos = GetCurrentOffset();
										}
										else
										{
											// Did not get the number of bytes we needed. Either because of a read error or because we got aborted.
											FAccessUnit::Release(AccessUnit);
											AccessUnit = nullptr;
											bDone = true;
											break;
										}

										// Check if the AU is outside the time range we are allowed to read.
										// The last one (the one that is already outside the range, actually) is tagged as such and sent into the buffer.
										// The respective decoder has to handle this flag if necessary and/or drop the AU.
										// We need to send at least one AU down so the FMultiTrackAccessUnitBuffer does not stay empty for this period!
										if (AccessUnit)
										{
											// Already sent the last one?
											if (bReadPastLastPTS)
											{
												// Yes. Release this AU and do not forward it. Continue reading however.
												FAccessUnit::Release(AccessUnit);
												AccessUnit = nullptr;
											}
											else if ((AccessUnit->DropState & FAccessUnit::EDropState::TooLate) == FAccessUnit::EDropState::TooLate)
											{
												// Tag the last one and send it off, but stop doing so for the remainder of the segment.
												// Note: we continue reading this segment all the way to the end on purpose in case there are further 'emsg' boxes.
												AccessUnit->bIsLastInPeriod = true;
												bReadPastLastPTS = true;
											}
										}

										if (AccessUnit)
										{
											AccessUnitFIFO.Push(AccessUnit);
											// Clear the pointer since we must not touch this AU again after we handed it off.
											AccessUnit = nullptr;
										}

										// Shall we pass on any AUs we already read?
										if (bAllowEarlyEmitting)
										{
											SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_HLS_StreamReader);
											CSV_SCOPED_TIMING_STAT(ElectraPlayer, HLS_StreamReader);
											while(AccessUnitFIFO.Num() && !HasReadBeenAborted())
											{
												FAccessUnit* pNext = AccessUnitFIFO.FrontRef();
												if (Parameters.EventListener->OnFragmentAccessUnitReceived(pNext))
												{
													DurationSuccessfullyDelivered += pNext->Duration;
													AccessUnitFIFO.Pop();
												}
												else
												{
													break;
												}
											}
										}

										bIsFirstAU = false;
									}
									delete TrackIterator;

									MoofInfo.PayloadEndOffset = LastSuccessfulFilePos;
									/*
										Not adding this at the moment since it is not needed. We are not gathering download timing traces to
										make use of the moof chunks. Segments are expected to be downloaded in one fell swoop.

									ds.MovieChunkInfos.Emplace(MoveTemp(MoofInfo));
									*/

									if (Error != UEMEDIA_ERROR_OK && Error != UEMEDIA_ERROR_END_OF_STREAM)
									{
										// error iterating
										LogMessage(IInfoLog::ELevel::Error, FString::Printf(TEXT("Failed to iterate over segment \"%s\""), *Request->URL));
										bHasErrored = true;
									}

									// Check if we are done or if there is additional data that needs parsing, like more moof boxes.
									if (HasReadBeenAborted() || HasReachedEOF())
									{
										bDone = true;
									}
								}
								else
								{
									// can't really happen. would indicate an internal screw up
									LogMessage(IInfoLog::ELevel::Error, FString::Printf(TEXT("Segment \"%s\" has no track"), *Request->URL));
									bHasErrored = true;
								}
							}
							else
							{
								// more than 1 track
								LogMessage(IInfoLog::ELevel::Error, FString::Printf(TEXT("Segment \"%s\" has more than one track"), *Request->URL));
								bHasErrored = true;
							}
						}
						else
						{
							// error preparing track for iterating
							LogMessage(IInfoLog::ELevel::Error, FString::Printf(TEXT("Failed to prepare segment \"%s\" for iterating"), *Request->URL));
							bHasErrored = true;
						}
					}
					else if (parseError == UEMEDIA_ERROR_END_OF_STREAM)
					{
						bDone = true;
					}
					else
					{
						// failed to parse the segment (in general)
						if (!HasReadBeenAborted())
						{
							if (!bInvalidMP4)
							{
								LogMessage(IInfoLog::ELevel::Error, FString::Printf(TEXT("Failed to download segment \"%s\""), *Request->URL));
							}
							else
							{
								LogMessage(IInfoLog::ELevel::Error, FString::Printf(TEXT("Media segment \"%s\" is not a valid mp4"), *Request->URL));
							}
							bHasErrored = true;
						}
					}
				}
				ProgressListener.Reset();
				// Note: It is only safe to access the connection info when the HTTP request has completed or the request been removed.
				PlayerSessionService->GetHTTPManager()->RemoveRequest(HTTP, false);
				CurrentRequest->ConnectionInfo = HTTP->ConnectionInfo;
				HTTP.Reset();
			}
		}
		else
		{
			if (!HasReadBeenAborted())
			{
				// Init segment failed to download or parse.
				CurrentRequest->ConnectionInfo.StatusInfo.ErrorDetail.SetFacility(Facility::EFacility::HLSFMP4Reader);
				if (InitSegmentResult == EInitSegmentResult::InvalidFormat)
				{
					CurrentRequest->ConnectionInfo.StatusInfo.ErrorDetail.SetMessage(TEXT("Init segment is not a valid mp4.")).SetCode(INTERNAL_ERROR_INIT_SEGMENT_UNSUPPORTED_FORMAT);
					ds.bParseFailure  = true;
				}
				else if (InitSegmentResult == EInitSegmentResult::ParseError)
				{
					CurrentRequest->ConnectionInfo.StatusInfo.ErrorDetail.SetMessage(TEXT("Init segment parse error")).SetCode(INTERNAL_ERROR_INIT_SEGMENT_PARSE_ERROR);
					ds.bParseFailure  = true;
				}
				else if (InitSegmentResult == EInitSegmentResult::LicenseKeyError)
				{
					CurrentRequest->ConnectionInfo.StatusInfo.ErrorDetail.SetMessage(FString::Printf(TEXT("Init segment license key format error"))).SetCode(INTERNAL_ERROR_INIT_SEGMENT_LICENSEKEY_ERROR);
				}
				else
				{
					// This is either a download failure of the init segment or its license key.
					CurrentRequest->ConnectionInfo.StatusInfo.ErrorDetail.SetMessage(FString::Printf(TEXT("Init segment download error: %s"), *CurrentRequest->ConnectionInfo.StatusInfo.ErrorDetail.GetMessage())).SetCode(INTERNAL_ERROR_INIT_SEGMENT_DOWNLOAD_ERROR);
				}
				bHasErrored = true;
			}
		}
	}

	// Do we need to fill remaining duration with dummy data?
	if (bIsEmptyFillerSegment || bFillRemainingDuration)
	{
		// Inserting dummy data means this request was successful.
		CurrentRequest->ConnectionInfo.StatusInfo.Empty();
		CurrentRequest->ConnectionInfo.StatusInfo.HTTPStatus = Request->Range.IsSet() ? 206 : 200;
		ds.HTTPStatusCode = CurrentRequest->ConnectionInfo.StatusInfo.HTTPStatus;
		// If this is a prefetch segment we will not fill in dummy data as the actual duration is not yet known
		// and an approximation only. If it is too long we would create an overlap with the next segment which
		// is not desirable.
		if (!Request->bIsPrefetch)
		{
			// Get the supposed segment duration.
			FTimeValue SegmentDurationToGo = Request->SegmentDuration;

			// Did we get anything so far?
			FTimeValue DefaultDuration;
			if (NextExpectedDTS.IsValid())
			{
				check(DurationSuccessfullyRead.IsValid());
				check(LastKnownAUDuration.IsValid());
				SegmentDurationToGo -= DurationSuccessfullyRead;
				DefaultDuration = LastKnownAUDuration;
				NextExpectedDTS.SetSequenceIndex(Request->TimestampSequenceIndex);
			}
			else
			{
				// No. We need to start with the segment time.
				NextExpectedDTS = Request->AbsoluteDateTime;
				NextExpectedDTS.SetSequenceIndex(Request->TimestampSequenceIndex);
				switch(Request->GetType())
				{
					case EStreamType::Video:
					{
						DefaultDuration.SetFromND(1, 60);
						break;
					}
					case EStreamType::Audio:
					{
						int64 n = 1024;
						uint32 d = 48000;
						if (CSD.IsValid() && CSD->ParsedInfo.GetSamplingRate())
						{
							d = (uint32) CSD->ParsedInfo.GetSamplingRate();
							switch(CSD->ParsedInfo.GetCodec())
							{
								case FStreamCodecInformation::ECodec::AAC:
								{
									n = CSD->ParsedInfo.GetExtras().GetValue("samples_per_block").SafeGetInt64(1024);
									break;
								}
							}
						}
						DefaultDuration.SetFromND(n, d);
						break;
					}
					default:
					{
						DefaultDuration.SetFromND(1, 10);
						break;
					}
				}
			}

			ds.bInsertedFillerData = SegmentDurationToGo > FTimeValue::GetZero();
			while(SegmentDurationToGo > FTimeValue::GetZero())
			{
				FAccessUnit *AccessUnit = FAccessUnit::Create(Parameters.MemoryProvider);
				if (!AccessUnit)
				{
					break;
				}
				if (DefaultDuration > SegmentDurationToGo)
				{
					DefaultDuration = SegmentDurationToGo;
				}

				AccessUnit->ESType = Request->GetType();
				AccessUnit->Duration = DefaultDuration;
				AccessUnit->AUSize = 0;
				AccessUnit->AUData = nullptr;
				AccessUnit->bIsDummyData = true;
				AccessUnit->BufferSourceInfo = SourceBufferInfo;
				if (CSD.IsValid() && CSD->CodecSpecificData.Num())
				{
					AccessUnit->AUCodecData = CSD;
				}

				AccessUnit->SequenceIndex = Request->TimestampSequenceIndex;
				AccessUnit->DTS = NextExpectedDTS;
				AccessUnit->PTS = NextExpectedDTS;
				AccessUnit->EarliestPTS = Request->EarliestPTS;
				AccessUnit->EarliestPTS.SetSequenceIndex(Request->TimestampSequenceIndex);
				AccessUnit->LatestPTS = Request->LastPTS;
				AccessUnit->LatestPTS.SetSequenceIndex(Request->TimestampSequenceIndex);

				// Calculate the drop on the fragment local NextExpectedDTS/PTS.
				AccessUnit->DropState = FAccessUnit::EDropState::None;
				if (AccessUnit->PTS + AccessUnit->Duration < Request->EarliestPTS)
				{
					AccessUnit->DropState |= FAccessUnit::EDropState::TooEarly;
				}
				if (Request->LastPTS.IsValid() && AccessUnit->PTS >= Request->LastPTS)
				{
					AccessUnit->DropState |= FAccessUnit::EDropState::TooLate;
					AccessUnit->bIsLastInPeriod = true;
				}

				NextExpectedDTS += DefaultDuration;
				SegmentDurationToGo -= DefaultDuration;

				// Add to the FIFO. We do not need to check for early emitting here as we are not waiting for any
				// data to be read. We can just shove all the synthesized dummy AUs in there.
				AccessUnitFIFO.Push(AccessUnit);
				if (AccessUnit->bIsLastInPeriod)
				{
					break;
				}
			}
		}
	}

	bool bFillWithDummyData = bIsEmptyFillerSegment || bFillRemainingDuration;
	while(AccessUnitFIFO.Num() && !bTerminate && (!HasReadBeenAborted() || bFillWithDummyData))
	{
		FAccessUnit* pNext = AccessUnitFIFO.FrontRef();
		while(!bTerminate && (!HasReadBeenAborted() || bFillWithDummyData))
		{
			if (Parameters.EventListener->OnFragmentAccessUnitReceived(pNext))
			{
				DurationSuccessfullyDelivered += pNext->Duration;
				AccessUnitFIFO.Pop();
				break;
			}
			else
			{
				FMediaRunnable::SleepMicroseconds(1000 * 20);
			}
		}
	}
	// Anything not handed over after an abort we delete
	while(AccessUnitFIFO.Num())
	{
		FAccessUnit::Release(AccessUnitFIFO.Pop());
	}


	// Set up remaining download stat fields.
	if (ds.FailureReason.Len() == 0)
	{
		ds.FailureReason = CurrentRequest->ConnectionInfo.StatusInfo.ErrorDetail.GetMessage();
	}
	if (bAbortedByABR)
	{
		// If aborted set the reason as the download failure.
		ds.FailureReason = ABRAbortReason;
	}
	ds.bWasAborted  	  = bAbortedByABR;
	ds.bWasSuccessful     = !bHasErrored && !bAbortedByABR;
	ds.URL  			  = CurrentRequest->ConnectionInfo.EffectiveURL;
	ds.HTTPStatusCode     = CurrentRequest->ConnectionInfo.StatusInfo.HTTPStatus;
	ds.DurationDownloaded = DurationSuccessfullyRead.GetAsSeconds();
	ds.DurationDelivered  = DurationSuccessfullyDelivered.GetAsSeconds();
	ds.TimeToFirstByte    = CurrentRequest->ConnectionInfo.TimeUntilFirstByte;
	ds.TimeToDownload     = (CurrentRequest->ConnectionInfo.RequestEndTime - CurrentRequest->ConnectionInfo.RequestStartTime).GetAsSeconds();
	ds.ByteSize 		  = CurrentRequest->ConnectionInfo.ContentLength;
	ds.NumBytesDownloaded = CurrentRequest->ConnectionInfo.BytesReadSoFar;
	ds.bIsCachedResponse  = CurrentRequest->ConnectionInfo.bIsCachedResponse;
	CurrentRequest->ConnectionInfo.GetTimingTraces(ds.TimingTraces);

	if (!bSilentCancellation)
	{
		StreamSelector->ReportDownloadEnd(ds);
	}

	// Restore the original retry info that may have been reset in all the changes and assignments in here.
	CurrentRequest->ConnectionInfo.RetryInfo = CurrentRetryInfo;
	// Clean out everything before reporting OnFragmentClose().
	TSharedPtrTS<FStreamSegmentRequestHLSfmp4> FinishedRequest = CurrentRequest;
	CurrentRequest.Reset();
	ReadBuffer.Reset();
	MP4Parser.Reset();
	Decrypter.Reset();

	if (!bSilentCancellation)
	{
		Parameters.EventListener->OnFragmentClose(FinishedRequest);
	}
}


bool FStreamReaderHLSfmp4::FStreamHandler::HasErrored() const
{
	return bHasErrored;
}




/**
 * Read n bytes of data into the provided buffer.
 *
 * Reading must return the number of bytes asked to get, if necessary by blocking.
 * If a read error prevents reading the number of bytes -1 must be returned.
 *
 * @param IntoBuffer Buffer into which to store the data bytes. If nullptr is passed the data must be skipped over.
 * @param NumBytesToRead The number of bytes to read. Must not read more bytes and no less than requested.
 * @return The number of bytes read or -1 on a read error.
 */
int64 FStreamReaderHLSfmp4::FStreamHandler::ReadData(void* IntoBuffer, int64 NumBytesToRead)
{
	FWaitableBuffer& SourceBuffer = ReadBuffer.ReceiveBuffer->Buffer;
	// Make sure the buffer will have the amount of data we need.
	while(1)
	{
		// Check if a HTTP reader progress event fired in the meantime.
		if (ProgressReportCount)
		{
			ProgressReportCount = 0;
			if (CurrentRequest.IsValid())
			{
				MetricUpdateLock.Lock();
				Metrics::FSegmentDownloadStats currentDownloadStats = CurrentRequest->DownloadStats;
				currentDownloadStats.DurationDownloaded = DurationSuccessfullyRead.GetAsSeconds();
				currentDownloadStats.DurationDelivered  = DurationSuccessfullyDelivered.GetAsSeconds();
				MetricUpdateLock.Unlock();

				Metrics::FSegmentDownloadStats& ds = CurrentRequest->DownloadStats;
				FABRDownloadProgressDecision StreamSelectorDecision = StreamSelector->ReportDownloadProgress(currentDownloadStats);
				if ((StreamSelectorDecision.Flags & FABRDownloadProgressDecision::EDecisionFlags::eABR_EmitPartialData) != 0)
				{
					SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_HLS_StreamReader);
					CSV_SCOPED_TIMING_STAT(ElectraPlayer, HLS_StreamReader);

					bAllowEarlyEmitting = true;
					// Deliver all enqueued AUs right now. Unless the request also gets aborted we could be stuck
					// in here for a while longer.
					while(AccessUnitFIFO.Num())
					{
						FAccessUnit* pNext = AccessUnitFIFO.FrontRef();
						if (Parameters.EventListener->OnFragmentAccessUnitReceived(pNext))
						{
							DurationSuccessfullyDelivered += pNext->Duration;
							AccessUnitFIFO.Pop();
						}
						else
						{
							break;
						}
					}
				}
				if ((StreamSelectorDecision.Flags & FABRDownloadProgressDecision::EDecisionFlags::eABR_InsertFillerData) != 0)
				{
					bFillRemainingDuration = true;
				}
				if ((StreamSelectorDecision.Flags & FABRDownloadProgressDecision::EDecisionFlags::eABR_AbortDownload) != 0)
				{
					// When aborted and early emitting did place something into the buffers we need to fill
					// the remainder no matter what.
					if (DurationSuccessfullyDelivered > FTimeValue::GetZero())
					{
						bFillRemainingDuration = true;
					}
					ABRAbortReason = StreamSelectorDecision.Reason;
					bAbortedByABR = true;
					return -1;
				}
			}
		}

		// Reading from an encrypted segment?
		if (Decrypter.IsValid())
		{
			// We are handling full segment encryption here only (AES-128) and not sample encryption or any other scheme.
			// Because AES is a block cipher we can only decrypt multiple of 16 byte chunks. Due to PKCS7 padding the encrypted segment
			// will also be 1 to 16 bytes larger than its unencrypted original which we have to consider and remove as to not make
			// the excess available to the caller here.

			// Get the encrypted size we need to have in order to decrypt it and get the data we need.
			int32 RequiredEncryptedSize = Decrypter->CBCGetEncryptionDataSize(ReadBuffer.ParsePos + NumBytesToRead);

			// Reading data from a network stream has no reliable end-of-data marker since the data can be unbounded in length
			// when using chunked transfer encoding. EOD is only signaled in the receive buffer when the connection is closed.
			// To ensure we get a reliable EOD signal we wait for more data to arrive in the buffer than we actually want since
			// the wait will either be satisfied with enough data (so what we want to have cannot be at the end yet) or when
			// the EOD flag gets set at the end of the transfer (with fewer data than we waited for but ideally the amount we
			// wanted to read (unless an error occurred)).
			// One AES block size is sufficient here. It could be set to higher values (1K or 16K even) with little harm other
			// than this will wait a tiny bit longer for new data then as long as we are not actually at EOD.
			const int32 NumExtraRequiredToCatchEOD = 16;

			if (!SourceBuffer.WaitUntilSizeAvailable(RequiredEncryptedSize + NumExtraRequiredToCatchEOD, 1000 * 100))
			{
				if (HasErrored() || HasReadBeenAborted() || SourceBuffer.WasAborted())
				{
					return -1;
				}
			}
			else
			{
				SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_HLS_StreamReader);
				CSV_SCOPED_TIMING_STAT(ElectraPlayer, HLS_StreamReader);

				SourceBuffer.Lock();
				// Check the available size. If the read was aborted there may not be enough in here as the wait got released early.
				if (SourceBuffer.Num() >= RequiredEncryptedSize)
				{
					int64 ReadUpToPos = ReadBuffer.ParsePos + NumBytesToRead;
					// Have enough data. See if it needs to be decrypted.
					if (ReadUpToPos > ReadBuffer.DecryptedPos)
					{
						// Decrypt from the last pos to the new pos now.
						check((ReadBuffer.DecryptedPos & 15) == 0);
						check((RequiredEncryptedSize & 15) == 0);
						check(RequiredEncryptedSize > ReadBuffer.DecryptedPos);
						uint8* EncryptedData = SourceBuffer.GetLinearReadData() + ReadBuffer.DecryptedPos;
						int32 EncryptedSize = RequiredEncryptedSize - ReadBuffer.DecryptedPos;
						bool bIsFinalBlock = false;
						// See comment above on NumExtraRequiredToCatchEOD why we can check for EOD on the buffer here.
						if (SourceBuffer.GetEOD() && RequiredEncryptedSize >= SourceBuffer.Num())
						{
							bIsFinalBlock = true;
						}

						// Decrypt data in place
						ElectraCDM::IStreamDecrypterAES128::EResult DecrypterResult;
						int32 NumDecryptedBytes = 0;
						DecrypterResult = Decrypter->CBCDecryptInPlace(NumDecryptedBytes, EncryptedData, EncryptedSize, bIsFinalBlock);
						// This cannot fail since we ensured it to be set up correctly and pass only properly aligned data, but just in case.
						if (DecrypterResult == ElectraCDM::IStreamDecrypterAES128::EResult::Ok)
						{
							// Advance the decrypted pos by the entire encrypted block size, not the amount of decrypted bytes!
							// This is required for the RequiredEncryptedSize test above to work correctly.
							ReadBuffer.DecryptedPos += EncryptedSize;
							if (bIsFinalBlock)
							{
								// On the final block adjust the maximum parse position to the end of the decrypted data
								// which is less than the encrypted total size due to padding.
								// This is needed for HasReachedEOF() to work correctly and not allow to read data from the padded area!
								ReadBuffer.MaxParsePos = ReadBuffer.DecryptedPos - EncryptedSize + NumDecryptedBytes;
							}
						}
						else
						{
							SourceBuffer.Unlock();
							LogMessage(IInfoLog::ELevel::Error, FString::Printf(TEXT("Failed to decrypt (%s)"), ElectraCDM::IStreamDecrypterAES128::GetResultText(DecrypterResult)));
							return -1;
						}
					}

					// Enough decrypted data available?
					if (ReadUpToPos <= ReadBuffer.DecryptedPos)
					{
						// Trying to read past the end of the decrypted data which would get into the padding area?
						if (ReadUpToPos <= ReadBuffer.MaxParsePos)
						{
							// No, read is ok.
							if (IntoBuffer)
							{
								FMemory::Memcpy(IntoBuffer, SourceBuffer.GetLinearReadData() + ReadBuffer.ParsePos, NumBytesToRead);
							}
							SourceBuffer.Unlock();
							ReadBuffer.ParsePos += NumBytesToRead;
							return NumBytesToRead;
						}
						else
						{
							// This means EOF now. Return 0 or -1 on error.
							SourceBuffer.Unlock();
							return HasErrored() ? -1 : 0;
						}
					}
				}
				else
				{
					// Return 0 at EOF and -1 on error.
					SourceBuffer.Unlock();
					return HasErrored() ? -1 : 0;
				}
				SourceBuffer.Unlock();
			}
		}
		else
		{
			if (!SourceBuffer.WaitUntilSizeAvailable(ReadBuffer.ParsePos + NumBytesToRead, 1000 * 100))
			{
				if (HasErrored() || HasReadBeenAborted() || SourceBuffer.WasAborted())
				{
					return -1;
				}
			}
			else
			{
				SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_HLS_StreamReader);
				CSV_SCOPED_TIMING_STAT(ElectraPlayer, HLS_StreamReader);
				SourceBuffer.Lock();
				if (SourceBuffer.Num() >= ReadBuffer.ParsePos + NumBytesToRead)
				{
					if (IntoBuffer)
					{
						FMemory::Memcpy(IntoBuffer, SourceBuffer.GetLinearReadData() + ReadBuffer.ParsePos, NumBytesToRead);
					}
					SourceBuffer.Unlock();
					ReadBuffer.ParsePos += NumBytesToRead;
					return NumBytesToRead;
				}
				else
				{
					// Return 0 at EOF and -1 on error.
					SourceBuffer.Unlock();
					return HasErrored() ? -1 : 0;
				}
			}
		}
	}
	return -1;
}

/**
 * Checks if the data source has reached the End Of File (EOF) and cannot provide any additional data.
 *
 * @return If EOF has been reached returns true, otherwise false.
 */
bool FStreamReaderHLSfmp4::FStreamHandler::HasReachedEOF() const
{
	const FWaitableBuffer& SourceBuffer = ReadBuffer.ReceiveBuffer->Buffer;
	return !HasErrored() && SourceBuffer.GetEOD() && (ReadBuffer.ParsePos >= SourceBuffer.Num() || ReadBuffer.ParsePos >= ReadBuffer.MaxParsePos);
}

/**
 * Checks if reading of the file and therefor parsing has been aborted.
 *
 * @return true if reading/parsing has been aborted, false otherwise.
 */
bool FStreamReaderHLSfmp4::FStreamHandler::HasReadBeenAborted() const
{
	return bTerminate || bRequestCanceled || bAbortedByABR;
}

/**
 * Returns the current read offset.
 *
 * The first read offset is not necessarily zero. It could be anywhere inside the source.
 *
 * @return The current byte offset in the source.
 */
int64 FStreamReaderHLSfmp4::FStreamHandler::GetCurrentOffset() const
{
	return ReadBuffer.ParsePos;
}


IParserISO14496_12::IBoxCallback::EParseContinuation FStreamReaderHLSfmp4::FStreamHandler::OnFoundBox(IParserISO14496_12::FBoxType Box, int64 BoxSizeInBytes, int64 FileDataOffset, int64 BoxDataOffset)
{
	if (bParsingInitSegment)
	{
		// We require the very first box to be an 'ftyp' box.
		if (FileDataOffset == 0 && Box != IParserISO14496_12::BoxType_ftyp)
		{
			bInvalidMP4 = true;
			return IParserISO14496_12::IBoxCallback::EParseContinuation::Stop;
		}
	}
	else
	{
		static const TArray<IParserISO14496_12::FBoxType> PermittedFirstBoxes =
		{
			IParserISO14496_12::BoxType_ftyp,
			IParserISO14496_12::BoxType_styp,
			IParserISO14496_12::BoxType_sidx,
			IParserISO14496_12::BoxType_moov,
			IParserISO14496_12::BoxType_moof,
			IParserISO14496_12::BoxType_prft,
			IParserISO14496_12::BoxType_free
		};
		if (FileDataOffset == 0 && !PermittedFirstBoxes.Contains(Box))
		{
			bInvalidMP4 = true;
			return IParserISO14496_12::IBoxCallback::EParseContinuation::Stop;
		}
	}

	// Check which box is being parsed next.
	switch(Box)
	{
		case IParserISO14496_12::BoxType_moov:
		case IParserISO14496_12::BoxType_sidx:
		{
			return IParserISO14496_12::IBoxCallback::EParseContinuation::Continue;
		}
		case IParserISO14496_12::BoxType_moof:
		{
			++NumMOOFBoxesFound;
			return IParserISO14496_12::IBoxCallback::EParseContinuation::Continue;
		}
		case IParserISO14496_12::BoxType_mdat:
		{
			return IParserISO14496_12::IBoxCallback::EParseContinuation::Stop;
		}
		default:
		{
			return IParserISO14496_12::IBoxCallback::EParseContinuation::Continue;
		}
	}
}

IParserISO14496_12::IBoxCallback::EParseContinuation FStreamReaderHLSfmp4::FStreamHandler::OnEndOfBox(IParserISO14496_12::FBoxType Box, int64 BoxSizeInBytes, int64 FileDataOffset, int64 BoxDataOffset)
{
	return IParserISO14496_12::IBoxCallback::EParseContinuation::Continue;
}


} // namespace Electra

