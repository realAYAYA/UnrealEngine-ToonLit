// Copyright Epic Games, Inc. All Rights Reserved.

#include "StreamReaderDASH.h"
#include "PlayerCore.h"
#include "HTTP/HTTPManager.h"
#include "Demuxer/ParserISO14496-12.h"
#include "Demuxer/ParserISO14496-12_Utils.h"
#include "Demuxer/ParserMKV.h"
#include "Demuxer/ParserMKV_Utils.h"
#include "StreamAccessUnitBuffer.h"
#include "HAL/LowLevelMemTracker.h"
#include "ElectraPlayerPrivate.h"
#include "Player/AdaptiveStreamingPlayerABR.h"
#include "Player/AdaptivePlayerOptionKeynames.h"
#include "Player/PlayerEntityCache.h"
#include "Player/DASH/PlaylistReaderDASH.h"
#include "Player/DASH/OptionKeynamesDASH.h"
#include "Player/DASH/PlayerEventDASH.h"
#include "Player/DASH/PlayerEventDASH_Internal.h"
#include "Player/DRM/DRMManager.h"
#include "Utilities/Utilities.h"
#include "Utilities/TimeUtilities.h"
#include "Utilities/UtilsMP4.h"
#include "Utils/Google/ElectraUtilsVPxVideo.h"
#include "Async/Async.h"

#define INTERNAL_ERROR_INIT_SEGMENT_DOWNLOAD_ERROR					1
#define INTERNAL_ERROR_INIT_SEGMENT_PARSE_ERROR						2


DECLARE_CYCLE_STAT(TEXT("FStreamReaderDASH::HandleRequest"), STAT_ElectraPlayer_DASH_StreamReader, STATGROUP_ElectraPlayer);

namespace Electra
{

FStreamSegmentRequestDASH::FStreamSegmentRequestDASH()
{
}

FStreamSegmentRequestDASH::~FStreamSegmentRequestDASH()
{
}

void FStreamSegmentRequestDASH::SetPlaybackSequenceID(uint32 PlaybackSequenceID)
{
	CurrentPlaybackSequenceID = PlaybackSequenceID;
}

uint32 FStreamSegmentRequestDASH::GetPlaybackSequenceID() const
{
	return CurrentPlaybackSequenceID;
}

void FStreamSegmentRequestDASH::SetExecutionDelay(const FTimeValue& UTCNow, const FTimeValue& ExecutionDelay)
{
	// If there is a delay specified and the current time is already past the availability time
	// then this is an old segment before the Live edge since we had paused or seeked backwards.
	// In that case, or if there is no availability time due to VoD, set the availability time
	// as the provided current time to apply the delay to.
	if (UTCNow.IsValid() && ExecutionDelay > FTimeValue::GetZero())
	{
		if (!ASAST.IsValid() || UTCNow > ASAST)
		{
			ASAST = UTCNow;
		}
	}
	DownloadDelayTime = ExecutionDelay;
}

FTimeValue FStreamSegmentRequestDASH::GetExecuteAtUTCTime() const
{
	FTimeValue When = ASAST;
	if (DownloadDelayTime.IsValid())
	{
		When += DownloadDelayTime;
	}
	return When;
}


EStreamType FStreamSegmentRequestDASH::GetType() const
{
	return StreamType;
}

void FStreamSegmentRequestDASH::GetDependentStreams(TArray<TSharedPtrTS<IStreamSegment>>& OutDependentStreams) const
{
	OutDependentStreams.Empty();
	for(auto& Stream : DependentStreams)
	{
		OutDependentStreams.Emplace(Stream);
	}
}

void FStreamSegmentRequestDASH::GetRequestedStreams(TArray<TSharedPtrTS<IStreamSegment>>& OutRequestedStreams)
{
	OutRequestedStreams.Empty();
	for(auto& Stream : DependentStreams)
	{
		OutRequestedStreams.Emplace(Stream);
	}
}

void FStreamSegmentRequestDASH::GetEndedStreams(TArray<TSharedPtrTS<IStreamSegment>>& OutAlreadyEndedStreams)
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

FTimeValue FStreamSegmentRequestDASH::GetFirstPTS() const
{
	return AST + AdditionalAdjustmentTime + PeriodStart + FTimeValue((Segment.bFrameAccuracyRequired ? Segment.MediaLocalFirstPTS : Segment.Time) - Segment.PTO, Segment.Timescale);
}

int32 FStreamSegmentRequestDASH::GetQualityIndex() const
{
	return Representation->GetQualityIndex();
}

int32 FStreamSegmentRequestDASH::GetBitrate() const
{
	return Representation->GetBitrate();
}

void FStreamSegmentRequestDASH::GetDownloadStats(Metrics::FSegmentDownloadStats& OutStats) const
{
	OutStats = DownloadStats;
}


bool FStreamSegmentRequestDASH::GetStartupDelay(FTimeValue& OutStartTime, FTimeValue& OutTimeIntoSegment, FTimeValue& OutSegmentDuration) const
{
	check(DependentStreams.Num());
	if (DependentStreams.Num())
	{
		OutTimeIntoSegment.SetFromND(DependentStreams[0]->Segment.MediaLocalFirstAUTime - DependentStreams[0]->Segment.Time, DependentStreams[0]->Segment.Timescale, 0);
		OutSegmentDuration.SetFromND(DependentStreams[0]->Segment.Duration, DependentStreams[0]->Segment.Timescale, 0);
		OutStartTime = DependentStreams[0]->GetFirstPTS();
		return true;
	}
	return false;
}

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

FStreamReaderDASH::FStreamReaderDASH()
{
}


FStreamReaderDASH::~FStreamReaderDASH()
{
	Close();
}

UEMediaError FStreamReaderDASH::Create(IPlayerSessionServices* InPlayerSessionService, const IStreamReader::CreateParam& InCreateParam)
{
	check(InPlayerSessionService);
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
		StreamHandlers[i].bTerminate		   = false;
		StreamHandlers[i].bWasStarted		   = false;
		StreamHandlers[i].bRequestCanceled     = false;
		StreamHandlers[i].bSilentCancellation  = false;
		StreamHandlers[i].bHasErrored   	   = false;
		StreamHandlers[i].IsIdleSignal.Signal();
#if 0
	// Disabled because the thread pool is timing sensitive and does not really allow for jobs like this.
		// Subtitles get fetched running on the thread pool.
		if (i == 2)
		{
			StreamHandlers[i].bRunOnThreadPool = true;
		}
#endif
		StreamHandlers[i].ThreadSetName(i==0 ? "ElectraPlayer::DASH Video" :
										i==1 ? "ElectraPlayer::DASH Audio" :
											   "ElectraPlayer::DASH Subtitle");
	}
	return UEMEDIA_ERROR_OK;
}

void FStreamReaderDASH::Close()
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
			if (StreamHandlers[i].bRunOnThreadPool)
			{
				StreamHandlers[i].IsIdleSignal.Wait();
			}
			else if (StreamHandlers[i].bWasStarted)
			{
				StreamHandlers[i].ThreadWaitDone();
				StreamHandlers[i].ThreadReset();
			}
		}
	}
}

IStreamReader::EAddResult FStreamReaderDASH::AddRequest(uint32 CurrentPlaybackSequenceID, TSharedPtrTS<IStreamSegment> InRequest)
{
	TSharedPtrTS<FStreamSegmentRequestDASH> Request = StaticCastSharedPtr<FStreamSegmentRequestDASH>(InRequest);

	if (Request->bIsInitialStartRequest)
	{
		PostError(PlayerSessionService, TEXT("Initial start request segments cannot be enqueued!"), 0);
		return IStreamReader::EAddResult::Error;
	}
	else
	{
		// Get the handler for the main request.
		FStreamHandler* Handler = nullptr;
		switch(Request->GetType())
		{
			case EStreamType::Video:
				Handler = &StreamHandlers[0];
				break;
			case EStreamType::Audio:
				Handler = &StreamHandlers[1];
				break;
			case EStreamType::Subtitle:
				Handler = &StreamHandlers[2];
				break;
			default:
				break;
		}
		if (!Handler)
		{
			ErrorDetail.SetMessage(FString::Printf(TEXT("No handler for stream type")));
			return IStreamReader::EAddResult::Error;
		}
		// Is the handler busy?
		bool bIsIdle = Handler->IsIdleSignal.WaitTimeout(1000 * 1000);
		if (!bIsIdle)
		{
			ErrorDetail.SetMessage(FString::Printf(TEXT("The handler for this stream type is busy!?")));
			return IStreamReader::EAddResult::Error;
		}

		Request->SetPlaybackSequenceID(CurrentPlaybackSequenceID);
		if (Handler->bRunOnThreadPool)
		{
			Handler->IsIdleSignal.Reset();
			Async(EAsyncExecution::ThreadPool, [Handler]()
			{
				Handler->RunInThreadPool();
			});
		}
		else
		{
			if (!Handler->bWasStarted)
			{
				Handler->ThreadStart(FMediaRunnable::FStartDelegate::CreateRaw(Handler, &FStreamHandler::WorkerThread));
				Handler->bWasStarted = true;
			}
		}

		Handler->bRequestCanceled = false;
		Handler->bSilentCancellation = false;
		Handler->CurrentRequest = Request;
		Handler->SignalWork();
	}
	return IStreamReader::EAddResult::Added;
}

void FStreamReaderDASH::CancelRequest(EStreamType StreamType, bool bSilent)
{
	if (StreamType == EStreamType::Video)
	{
		StreamHandlers[0].Cancel(bSilent);
	}
	else if (StreamType == EStreamType::Audio)
	{
		StreamHandlers[1].Cancel(bSilent);
	}
	else if (StreamType == EStreamType::Subtitle)
	{
		StreamHandlers[2].Cancel(bSilent);
	}
}

void FStreamReaderDASH::CancelRequests()
{
	for(int32 i=0; i<FMEDIA_STATIC_ARRAY_COUNT(StreamHandlers); ++i)
	{
		StreamHandlers[i].Cancel(false);
	}
}





uint32 FStreamReaderDASH::FStreamHandler::UniqueDownloadID = 1;

FStreamReaderDASH::FStreamHandler::FStreamHandler()
{ }

FStreamReaderDASH::FStreamHandler::~FStreamHandler()
{
	// NOTE: The thread will have been terminated by the enclosing FStreamReaderDASH's Close() method!
	//       Also, this may have run on the thread pool instead of a dedicated worker thread.
}

void FStreamReaderDASH::FStreamHandler::Cancel(bool bSilent)
{
	bSilentCancellation = bSilent;
	bRequestCanceled = true;
}

void FStreamReaderDASH::FStreamHandler::SignalWork()
{
	WorkSignal.Release();
}

void FStreamReaderDASH::FStreamHandler::WorkerThread()
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);

	StreamSelector = PlayerSessionService->GetStreamSelector();
	if (StreamSelector.IsValid())
	{
		while(!bTerminate)
		{
			WorkSignal.Obtain();
			if (!bTerminate)
			{
				if (CurrentRequest.IsValid())
				{
					IsIdleSignal.Reset();
					if (!bRequestCanceled)
					{
						HandleRequest();
					}
					else
					{
						CurrentRequest.Reset();
					}
					IsIdleSignal.Signal();
				}
				bRequestCanceled = false;
				bSilentCancellation = false;
			}
		}
	}
	StreamSelector.Reset();
}

void FStreamReaderDASH::FStreamHandler::RunInThreadPool()
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);

	StreamSelector = PlayerSessionService->GetStreamSelector();
	if (StreamSelector.IsValid())
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
	IsIdleSignal.Signal();
}



void FStreamReaderDASH::FStreamHandler::LogMessage(IInfoLog::ELevel Level, const FString& Message)
{
	PlayerSessionService->PostLog(Facility::EFacility::DASHStreamReader, Level, Message);
}


int32 FStreamReaderDASH::FStreamHandler::HTTPProgressCallback(const IElectraHttpManager::FRequest* Request)
{
	HTTPUpdateStats(MEDIAutcTime::Current(), Request);
	++ProgressReportCount;

	// Aborted?
	return HasReadBeenAborted() ? 1 : 0;
}

void FStreamReaderDASH::FStreamHandler::HTTPCompletionCallback(const IElectraHttpManager::FRequest* Request)
{
	HTTPUpdateStats(FTimeValue::GetInvalid(), Request);

	bHasErrored = Request->ConnectionInfo.StatusInfo.ErrorDetail.IsError();
	DownloadCompleteSignal.Signal();
}

void FStreamReaderDASH::FStreamHandler::HTTPUpdateStats(const FTimeValue& CurrentTime, const IElectraHttpManager::FRequest* Request)
{
	TSharedPtrTS<FStreamSegmentRequestDASH> SegmentRequest = CurrentRequest;
	if (SegmentRequest.IsValid())
	{
		// Only update elements that are needed by the ABR here.
		FMediaCriticalSection::ScopedLock lock(MetricUpdateLock);
		SegmentRequest->ConnectionInfo.RequestStartTime = Request->ConnectionInfo.RequestStartTime;
		Metrics::FSegmentDownloadStats& ds = SegmentRequest->DownloadStats;
		if (Request->ConnectionInfo.EffectiveURL.Len())
		{
			ds.URL = Request->ConnectionInfo.EffectiveURL;
		}
		ds.HTTPStatusCode = Request->ConnectionInfo.StatusInfo.HTTPStatus;
		ds.TimeToFirstByte = Request->ConnectionInfo.TimeUntilFirstByte;
		ds.TimeToDownload = ((CurrentTime.IsValid() ? CurrentTime : Request->ConnectionInfo.RequestEndTime) - Request->ConnectionInfo.RequestStartTime).GetAsSeconds();
		ds.ByteSize = Request->ConnectionInfo.ContentLength;
		ds.NumBytesDownloaded = Request->ConnectionInfo.BytesReadSoFar;
	}
}




FErrorDetail FStreamReaderDASH::FStreamHandler::LoadInitSegment(TSharedPtrTS<FMPDLoadRequestDASH>& OutLoadRequest, Metrics::FSegmentDownloadStats& ds, const TSharedPtrTS<FStreamSegmentRequestDASH>& Request)
{
	// Get the manifest reader. We need it to handle the init segment download.
	TSharedPtrTS<IPlaylistReader> ManifestReader = PlayerSessionService->GetManifestReader();
	if (!ManifestReader.IsValid())
	{
		return PostError(PlayerSessionService, TEXT("Entity loader disappeared"), 0);
	}

	// Create a download finished structure we can wait upon for completion.
	struct FLoadResult
	{
		FMediaEvent Event;
		bool bSuccess = false;
	};
	TSharedPtrTS<FLoadResult> LoadResult = MakeSharedTS<FLoadResult>();

	// Create the download request.
	OutLoadRequest = MakeSharedTS<FMPDLoadRequestDASH>();
	OutLoadRequest->LoadType = FMPDLoadRequestDASH::ELoadType::Segment;
	OutLoadRequest->SegmentStreamType = Request->StreamType;
	OutLoadRequest->SegmentQualityIndex = Request->QualityIndex;
	OutLoadRequest->SegmentQualityIndexMax = Request->MaxQualityIndex;
	OutLoadRequest->URL = Request->Segment.InitializationURL.URL;
	OutLoadRequest->Range = Request->Segment.InitializationURL.Range;
	if (Request->Segment.InitializationURL.CustomHeader.Len())
	{
		OutLoadRequest->Headers.Emplace(HTTP::FHTTPHeader({DASH::HTTPHeaderOptionName, Request->Segment.InitializationURL.CustomHeader}));
	}
	OutLoadRequest->PlayerSessionServices = PlayerSessionService;
	OutLoadRequest->CompleteCallback.BindLambda([LoadResult](TSharedPtrTS<FMPDLoadRequestDASH> LoadRequest, bool bSuccess)
	{
		LoadResult->bSuccess = bSuccess;
		LoadResult->Event.Signal();
	});
	// Issue the download request.
	check(ManifestReader->GetPlaylistType().Equals(TEXT("dash")));
	IPlaylistReaderDASH* Reader = static_cast<IPlaylistReaderDASH*>(ManifestReader.Get());
	Reader->AddElementLoadRequests(TArray<TWeakPtrTS<FMPDLoadRequestDASH>>({OutLoadRequest}));

	// Wait for completion or abort
	while(!HasReadBeenAborted())
	{
		if (LoadResult->Event.WaitTimeout(1000 * 100))
		{
			break;
		}
	}
	if (HasReadBeenAborted())
	{
		return FErrorDetail();
	}

	// Set up download stats to be sent to the stream selector.
	const HTTP::FConnectionInfo* ci = OutLoadRequest->GetConnectionInfo();
	ds.StatsID = FMediaInterlockedIncrement(UniqueDownloadID);
	ds.StreamType = Request->GetType();
	ds.SegmentType = Metrics::ESegmentType::Init;
	ds.URL = Request->Segment.InitializationURL.URL;
	ds.Range = Request->Segment.InitializationURL.Range;
	ds.CDN = Request->Segment.InitializationURL.CDN;
	ds.bWasSuccessful = LoadResult->bSuccess;
	ds.HTTPStatusCode = ci->StatusInfo.HTTPStatus;
	ds.TimeToFirstByte = ci->TimeUntilFirstByte;
	ds.TimeToDownload = (ci->RequestEndTime - ci->RequestStartTime).GetAsSeconds();
	ds.ByteSize = ci->ContentLength;
	ds.NumBytesDownloaded = ci->BytesReadSoFar;
	ds.MediaAssetID = Request->Period.IsValid() ? Request->Period->GetUniqueIdentifier() : "";
	ds.AdaptationSetID = Request->AdaptationSet.IsValid() ? Request->AdaptationSet->GetUniqueIdentifier() : "";
	ds.RepresentationID = Request->Representation.IsValid() ? Request->Representation->GetUniqueIdentifier() : "";
	ds.Bitrate = Request->GetBitrate();
	ds.QualityIndex = Request->QualityIndex;
	ds.HighestQualityIndex = Request->MaxQualityIndex;

	if (!LoadResult->bSuccess)
	{
		Request->ConnectionInfo = *ci;
		StreamSelector->ReportDownloadEnd(ds);
		return CreateError(FString::Printf(TEXT("Init segment download error: %s"),	*ci->StatusInfo.ErrorDetail.GetMessage()), INTERNAL_ERROR_INIT_SEGMENT_DOWNLOAD_ERROR);
	}
	return FErrorDetail();
}

FErrorDetail FStreamReaderDASH::FStreamHandler::GetInitSegment(TSharedPtrTS<const IParserISO14496_12>& OutMP4InitSegment, const TSharedPtrTS<FStreamSegmentRequestDASH>& Request)
{
	// Is an init segment required?
	if (Request->Segment.InitializationURL.URL.IsEmpty())
	{
		// No init segment required. We're done.
		return FErrorDetail();
	}
	// Get the entity cache. If it's not there we return.
	TSharedPtrTS<IPlayerEntityCache> EntityCache = PlayerSessionService ? PlayerSessionService->GetEntityCache() : nullptr;
	if (!EntityCache.IsValid())
	{
		return FErrorDetail();
	}
	// Check if we already have this cached.
	IPlayerEntityCache::FCacheItem CachedItem;
	if (EntityCache->GetCachedEntity(CachedItem, Request->Segment.InitializationURL.URL, Request->Segment.InitializationURL.Range))
	{
		// Already cached. Use it.
		OutMP4InitSegment = CachedItem.Parsed14496_12Data;
		return FErrorDetail();
	}
	TSharedPtrTS<FMPDLoadRequestDASH> LoadReq;
	Metrics::FSegmentDownloadStats ds;
	FErrorDetail LoadError = LoadInitSegment(LoadReq, ds, Request);
	if (!LoadReq.IsValid() || LoadError.IsSet())
	{
		return LoadError;
	}

	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_DASH_StreamReader);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, DASH_StreamReader);

	TSharedPtrTS<IParserISO14496_12> Init = IParserISO14496_12::CreateParser();
	UEMediaError parseError = UEMEDIA_ERROR_FORMAT_ERROR;
	if (LoadReq->Request.IsValid() && LoadReq->Request->GetResponseBuffer().IsValid())
	{
		FMP4StaticDataReader StaticDataReader;
		StaticDataReader.SetParseData(LoadReq->Request->GetResponseBuffer());
		parseError = Init->ParseHeader(&StaticDataReader, this, PlayerSessionService, nullptr);
	}
	if (parseError == UEMEDIA_ERROR_OK || parseError == UEMEDIA_ERROR_END_OF_STREAM)
	{
		// Parse the tracks of the init segment. We do this mainly to get to the CSD we might need should we have to insert filler data later.
		parseError = Init->PrepareTracks(PlayerSessionService, TSharedPtrTS<const IParserISO14496_12>());
		if (parseError == UEMEDIA_ERROR_OK)
		{
			// Add this to the entity cache in case it needs to be retrieved again.
			IPlayerEntityCache::FCacheItem CacheItem;
			CacheItem.URL = LoadReq->URL;
			CacheItem.Range = LoadReq->Range;
			CacheItem.Parsed14496_12Data = Init;
			EntityCache->CacheEntity(CacheItem);
			OutMP4InitSegment = Init;

			StreamSelector->ReportDownloadEnd(ds);
			return FErrorDetail();
		}
		else
		{
			ds.bParseFailure = true;
			Request->ConnectionInfo = *LoadReq->GetConnectionInfo();
			StreamSelector->ReportDownloadEnd(ds);
			return CreateError(FString::Printf(TEXT("Track preparation of init segment \"%s\" failed"), *LoadReq->URL), INTERNAL_ERROR_INIT_SEGMENT_PARSE_ERROR);
		}
	}
	else
	{
		ds.bParseFailure = true;
		StreamSelector->ReportDownloadEnd(ds);
		return CreateError(FString::Printf(TEXT("Parse error of init segment \"%s\""), *LoadReq->URL), INTERNAL_ERROR_INIT_SEGMENT_PARSE_ERROR);
	}
}

FErrorDetail FStreamReaderDASH::FStreamHandler::GetInitSegment(TSharedPtrTS<const IParserMKV>& OutMKVInitSegment, const TSharedPtrTS<FStreamSegmentRequestDASH>& Request)
{
	// Is an init segment required?
	if (Request->Segment.InitializationURL.URL.IsEmpty())
	{
		// No init segment required. We're done.
		return FErrorDetail();
	}
	// Get the entity cache. If it's not there we return.
	TSharedPtrTS<IPlayerEntityCache> EntityCache = PlayerSessionService ? PlayerSessionService->GetEntityCache() : nullptr;
	if (!EntityCache.IsValid())
	{
		return FErrorDetail();
	}
	// Check if we already have this cached.
	IPlayerEntityCache::FCacheItem CachedItem;
	if (EntityCache->GetCachedEntity(CachedItem, Request->Segment.InitializationURL.URL, Request->Segment.InitializationURL.Range))
	{
		// Already cached. Use it.
		OutMKVInitSegment = CachedItem.ParsedMatroskaData;
		return FErrorDetail();
	}
	TSharedPtrTS<FMPDLoadRequestDASH> LoadReq;
	Metrics::FSegmentDownloadStats ds;
	FErrorDetail LoadError = LoadInitSegment(LoadReq, ds, Request);
	if (!LoadReq.IsValid() || LoadError.IsSet())
	{
		return LoadError;
	}

	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_DASH_StreamReader);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, DASH_StreamReader);

	TSharedPtrTS<IParserMKV> Init = IParserMKV::CreateParser(nullptr);
	if (LoadReq->Request.IsValid() && LoadReq->Request->GetResponseBuffer().IsValid())
	{
		FMKVStaticDataReader StaticDataReader;
		StaticDataReader.SetParseData(LoadReq->Request->GetResponseBuffer());
		FErrorDetail parseError = Init->ParseHeader(&StaticDataReader, static_cast<Electra::IParserMKV::EParserFlags>(IParserMKV::EParserFlags::ParseFlag_OnlyTracks | IParserMKV::EParserFlags::ParseFlag_SuppressCueWarning));
		if (parseError.IsOK())
		{
			parseError = Init->PrepareTracks();
			if (parseError.IsOK())
			{
				// Add this to the entity cache in case it needs to be retrieved again.
				IPlayerEntityCache::FCacheItem CacheItem;
				CacheItem.URL = LoadReq->URL;
				CacheItem.Range = LoadReq->Range;
				CacheItem.ParsedMatroskaData = Init;
				EntityCache->CacheEntity(CacheItem);
				OutMKVInitSegment = Init;

				StreamSelector->ReportDownloadEnd(ds);
				return FErrorDetail();
			}
			else
			{
				ds.bParseFailure = true;
				Request->ConnectionInfo = *LoadReq->GetConnectionInfo();
				StreamSelector->ReportDownloadEnd(ds);
				return CreateError(FString::Printf(TEXT("Track preparation of init segment \"%s\" failed. %d"), *LoadReq->URL, *parseError.GetMessage()), INTERNAL_ERROR_INIT_SEGMENT_PARSE_ERROR);
			}
		}
		else
		{
			ds.bParseFailure = true;
			StreamSelector->ReportDownloadEnd(ds);
			return CreateError(FString::Printf(TEXT("Parse error of init segment \"%s\". %s"), *LoadReq->URL, *parseError.GetMessage()), INTERNAL_ERROR_INIT_SEGMENT_PARSE_ERROR);
		}
	}
	else
	{
		ds.bParseFailure = true;
		StreamSelector->ReportDownloadEnd(ds);
		return CreateError(FString::Printf(TEXT("Parse error of init segment \"%s\". Could not create MKV parser."), *LoadReq->URL), INTERNAL_ERROR_INIT_SEGMENT_PARSE_ERROR);
	}
}


FErrorDetail FStreamReaderDASH::FStreamHandler::RetrieveSideloadedFile(TSharedPtrTS<const TArray<uint8>>& OutData, const TSharedPtrTS<FStreamSegmentRequestDASH>& Request)
{
	if (Request->Segment.MediaURL.URL.IsEmpty())
	{
		return FErrorDetail();
	}
	// Get the entity cache. If it's not there we return.
	TSharedPtrTS<IPlayerEntityCache> EntityCache = PlayerSessionService ? PlayerSessionService->GetEntityCache() : nullptr;
	if (!EntityCache.IsValid())
	{
		return FErrorDetail();
	}
	// Check if we already have this cached.
	IPlayerEntityCache::FCacheItem CachedItem;
	if (EntityCache->GetCachedEntity(CachedItem, Request->Segment.MediaURL.URL, Request->Segment.MediaURL.Range))
	{
		// Already cached. Use it.
		OutData = CachedItem.RawPayloadData;
		return FErrorDetail();
	}

	// Get the manifest reader. We need it to handle the download.
	TSharedPtrTS<IPlaylistReader> ManifestReader = PlayerSessionService->GetManifestReader();
	if (!ManifestReader.IsValid())
	{
		return PostError(PlayerSessionService, TEXT("Entity loader disappeared"), 0);
	}

	// Create a download finished structure we can wait upon for completion.
	struct FLoadResult
	{
		FMediaEvent Event;
		bool bSuccess = false;
	};
	TSharedPtrTS<FLoadResult> LoadResult = MakeSharedTS<FLoadResult>();

	// Create the download request.
	TSharedPtrTS<FMPDLoadRequestDASH> LoadReq = MakeSharedTS<FMPDLoadRequestDASH>();
	LoadReq->LoadType = FMPDLoadRequestDASH::ELoadType::Sideload;
	LoadReq->URL = Request->Segment.MediaURL.URL;
	LoadReq->Range = Request->Segment.MediaURL.Range;
	if (Request->Segment.MediaURL.CustomHeader.Len())
	{
		LoadReq->Headers.Emplace(HTTP::FHTTPHeader({DASH::HTTPHeaderOptionName, Request->Segment.MediaURL.CustomHeader}));
	}
	LoadReq->PlayerSessionServices = PlayerSessionService;
	LoadReq->CompleteCallback.BindLambda([LoadResult](TSharedPtrTS<FMPDLoadRequestDASH> LoadRequest, bool bSuccess)
	{
		LoadResult->bSuccess = bSuccess;
		LoadResult->Event.Signal();
	});
	// Issue the download request.
	check(ManifestReader->GetPlaylistType().Equals(TEXT("dash")));
	IPlaylistReaderDASH* Reader = static_cast<IPlaylistReaderDASH*>(ManifestReader.Get());
	Reader->AddElementLoadRequests(TArray<TWeakPtrTS<FMPDLoadRequestDASH>>({LoadReq}));

	// Wait for completion or abort
	while(!HasReadBeenAborted())
	{
		if (LoadResult->Event.WaitTimeout(1000 * 100))
		{
			break;
		}
	}
	if (HasReadBeenAborted())
	{
		return FErrorDetail();
	}

	// Set up download stats to be sent to the stream selector.
	const HTTP::FConnectionInfo* ci = LoadReq->GetConnectionInfo();
	Metrics::FSegmentDownloadStats ds;
	ds.StatsID = FMediaInterlockedIncrement(UniqueDownloadID);
	ds.StreamType = Request->GetType();
	ds.SegmentType = Metrics::ESegmentType::Media;
	ds.URL = Request->Segment.MediaURL.URL;
	ds.Range = Request->Segment.MediaURL.Range;
	ds.CDN = Request->Segment.MediaURL.CDN;
	ds.bWasSuccessful = LoadResult->bSuccess;
	ds.HTTPStatusCode = ci->StatusInfo.HTTPStatus;
	ds.TimeToFirstByte = ci->TimeUntilFirstByte;
	ds.TimeToDownload = (ci->RequestEndTime - ci->RequestStartTime).GetAsSeconds();
	ds.ByteSize = ci->ContentLength;
	ds.NumBytesDownloaded = ci->BytesReadSoFar;
	ds.MediaAssetID = Request->Period.IsValid() ? Request->Period->GetUniqueIdentifier() : "";
	ds.AdaptationSetID = Request->AdaptationSet.IsValid() ? Request->AdaptationSet->GetUniqueIdentifier() : "";
	ds.RepresentationID = Request->Representation.IsValid() ? Request->Representation->GetUniqueIdentifier() : "";
	ds.Bitrate = Request->GetBitrate();

	if (!LoadResult->bSuccess)
	{
		Request->ConnectionInfo = *ci;
		StreamSelector->ReportDownloadEnd(ds);
		return CreateError(FString::Printf(TEXT("Sideloaded media download error: %s"), *ci->StatusInfo.ErrorDetail.GetMessage()), INTERNAL_ERROR_INIT_SEGMENT_DOWNLOAD_ERROR);
	}

	OutData = MakeSharedTS<const TArray<uint8>>(TArrayView<uint8>(LoadReq->Request->GetResponseBuffer()->Buffer.GetLinearReadData(), LoadReq->Request->GetResponseBuffer()->Buffer.GetLinearReadSize()));

	// Add this to the entity cache in case it needs to be retrieved again.
	IPlayerEntityCache::FCacheItem CacheItem;
	CacheItem.URL = LoadReq->URL;
	CacheItem.Range = LoadReq->Range;
	CacheItem.RawPayloadData = OutData;
	EntityCache->CacheEntity(CacheItem);

	StreamSelector->ReportDownloadEnd(ds);
	return FErrorDetail();
}


void FStreamReaderDASH::FStreamHandler::CheckForInbandDASHEvents()
{
	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_DASH_StreamReader);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, DASH_StreamReader);
	bool bHasInbandEvent = false;
	if (!CurrentRequest->bIsEOSSegment)
	{
		for(int32 i=0; i<CurrentRequest->Segment.InbandEventStreams.Num(); ++i)
		{
			const FManifestDASHInternal::FSegmentInformation::FInbandEventStream& ibs = CurrentRequest->Segment.InbandEventStreams[i];
			if (ibs.SchemeIdUri.Equals(DASH::Schemes::ManifestEvents::Scheme_urn_mpeg_dash_event_2012))
			{
				bHasInbandEvent = true;
				break;
			}
		}
	}
	TSharedPtrTS<IPlaylistReader> ManifestReader = PlayerSessionService->GetManifestReader();
	IPlaylistReaderDASH* Reader = static_cast<IPlaylistReaderDASH*>(ManifestReader.Get());
	if (Reader)
	{
		Reader->SetStreamInbandEventUsage(CurrentRequest->GetType(), bHasInbandEvent);
	}
}

void FStreamReaderDASH::FStreamHandler::HandleEventMessages()
{
	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_DASH_StreamReader);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, DASH_StreamReader);
	// Are there 'emsg' boxes we need to handle?
	if (MP4Parser->GetNumberOfEventMessages())
	{
		FTimeValue AbsPeriodStart = CurrentRequest->PeriodStart + CurrentRequest->AST + CurrentRequest->AdditionalAdjustmentTime;
		// We may need the EPT from the 'sidx' if there is one.
		const IParserISO14496_12::ISegmentIndex* Sidx = MP4Parser->GetSegmentIndexByIndex(0);
		for(int32 nEmsg=0, nEmsgs=MP4Parser->GetNumberOfEventMessages(); nEmsg<nEmsgs; ++nEmsg)
		{
			const IParserISO14496_12::IEventMessage* Emsg = MP4Parser->GetEventMessageByIndex(nEmsg);
			// This event must be described by an <InbandEventStream> in order to be processed.
			for(int32 i=0; i<CurrentRequest->Segment.InbandEventStreams.Num(); ++i)
			{
				const FManifestDASHInternal::FSegmentInformation::FInbandEventStream& ibs = CurrentRequest->Segment.InbandEventStreams[i];
				if (ibs.SchemeIdUri.Equals(Emsg->GetSchemeIdUri()) &&
					(ibs.Value.IsEmpty() || ibs.Value.Equals(Emsg->GetValue())))
				{
					TSharedPtrTS<DASH::FPlayerEvent> NewEvent = MakeSharedTS<DASH::FPlayerEvent>();
					NewEvent->SetOrigin(IAdaptiveStreamingPlayerAEMSEvent::EOrigin::InbandEventStream);
					NewEvent->SetSchemeIdUri(Emsg->GetSchemeIdUri());
					NewEvent->SetValue(Emsg->GetValue());
					NewEvent->SetID(LexToString(Emsg->GetID()));
					uint32 Timescale = Emsg->GetTimescale();
					uint32 Duration = Emsg->GetEventDuration();
					FTimeValue PTS;
					if (Emsg->GetVersion() == 0)
					{
						// Version 0 uses a presentation time delta relative to the EPT of the SIDX, if it exists, or if not
						// to the PTS of the first AU, which should be the same as the segment media start time.
						FTimeValue PTD((int64)Emsg->GetPresentationTimeDelta(), Timescale);
						FTimeValue EPT;
						if (Sidx)
						{
							EPT.SetFromND((int64)Sidx->GetEarliestPresentationTime(), Sidx->GetTimescale());
						}
						else
						{
							EPT.SetFromND((int64)CurrentRequest->Segment.Time, CurrentRequest->Segment.Timescale);
						}
						FTimeValue PTO(CurrentRequest->Segment.PTO, CurrentRequest->Segment.Timescale);
						PTS = AbsPeriodStart - PTO + EPT + PTD;
					}
					else if (Emsg->GetVersion() == 1)
					{
						FTimeValue EventTime(Emsg->GetPresentationTime(), Timescale);
						FTimeValue PTO(FTimeValue::GetZero());
						PTS = AbsPeriodStart - PTO + EventTime;
					}
					NewEvent->SetPresentationTime(PTS);
					if (Duration != 0xffffffff)
					{
						NewEvent->SetDuration(FTimeValue((int64)Duration, Timescale));
					}
					NewEvent->SetMessageData(Emsg->GetMessageData());
					NewEvent->SetPeriodID(CurrentRequest->Period->GetUniqueIdentifier());
					// Add the event to the handler.
					if (PTS.IsValid())
					{
						/*
							Check that we have no seen this event in this segment already. This is for the case where the 'emsg' appears
							inbetween multiple 'moof' boxes. As per ISO/IEC 23009-1:2019 Section 5.10.3.3.1 General:
								A Media Segment if based on the ISO BMFF container may contain one or more event message ('emsg') boxes. If present, any 'emsg' box shall be placed as follows:
								- It may be placed before the first 'moof' box of the segment.
								- It may be placed in between any 'mdat' and 'moof' box. In this case, an equivalent 'emsg' with the same id value shall be present before the first 'moof' box of any Segment.
						*/
						int32 EventIdx = SegmentEventsFound.IndexOfByPredicate([&NewEvent](const TSharedPtrTS<DASH::FPlayerEvent>& This) {
							return NewEvent->GetSchemeIdUri().Equals(This->GetSchemeIdUri()) &&
								   NewEvent->GetID().Equals(This->GetID()) &&
								   (NewEvent->GetValue().IsEmpty() || NewEvent->GetValue().Equals(This->GetValue()));
						});
						if (EventIdx == INDEX_NONE)
						{
							PlayerSessionService->GetAEMSEventHandler()->AddEvent(NewEvent, CurrentRequest->Period->GetUniqueIdentifier(), IAdaptiveStreamingPlayerAEMSHandler::EEventAddMode::AddIfNotExists);
							SegmentEventsFound.Emplace(MoveTemp(NewEvent));
						}
					}
					break;
				}
			}
		}
	}
}

void FStreamReaderDASH::FStreamHandler::HandleRequest()
{
	TSharedPtrTS<FStreamSegmentRequestDASH> Request = CurrentRequest;
	if (Request.IsValid())
	{
		if (Request->Segment.ContainerType == FManifestDASHInternal::FSegmentInformation::EContainerType::ISO14496_12)
		{
			HandleRequestMP4();
		}
		else
		{
			HandleRequestMKV();
		}
	}
}

void FStreamReaderDASH::FStreamHandler::HandleRequestMP4()
{
	// Get the request into a local shared pointer to hold on to it.
	TSharedPtrTS<FStreamSegmentRequestDASH> Request = CurrentRequest;
	FTimeValue SegmentDuration = FTimeValue().SetFromND(Request->Segment.Duration, Request->Segment.Timescale);
	FString RequestURL = Request->Segment.MediaURL.URL;
	TSharedPtrTS<const IParserISO14496_12> MP4InitSegment;
	FErrorDetail InitSegmentError;
	UEMediaError Error;
	bool bIsEmptyFillerSegment = Request->bInsertFillerData;
	bool bIsLastSegment = Request->Segment.bIsLastInPeriod;

	ActiveTrackData.Reset();
	ActiveTrackData.CSD = MakeSharedTS<FAccessUnit::CodecData>();
	// Copy the source buffer info into a new instance and set the playback sequence ID in it.
	ActiveTrackData.BufferSourceInfo = MakeSharedTS<FBufferSourceInfo>(*Request->SourceBufferInfo);
	ActiveTrackData.BufferSourceInfo->PlaybackSequenceID = Request->GetPlaybackSequenceID();
	ActiveTrackData.StreamType = Request->GetType();

	Metrics::FSegmentDownloadStats& ds = CurrentRequest->DownloadStats;
	ds.StatsID = FMediaInterlockedIncrement(UniqueDownloadID);
	ds.FailureReason.Empty();
	ds.bWasSuccessful      = true;
	ds.bWasAborted  	   = false;
	ds.bDidTimeout  	   = false;
	ds.HTTPStatusCode      = 0;
	ds.StreamType   	   = Request->GetType();
	// Assume we need to fetch the init segment first. This gets changed to 'media' when we load the media segment.
	ds.SegmentType  	   = Metrics::ESegmentType::Init;
	ds.PresentationTime    = Request->GetFirstPTS().GetAsSeconds();
	ds.Bitrate  		   = Request->GetBitrate();
	ds.QualityIndex        = Request->QualityIndex;
	ds.HighestQualityIndex = Request->MaxQualityIndex;
	ds.Duration 		   = SegmentDuration.GetAsSeconds();
	ds.DurationDownloaded  = 0.0;
	ds.DurationDelivered   = 0.0;
	ds.TimeToFirstByte     = 0.0;
	ds.TimeToDownload      = 0.0;
	ds.ByteSize 		   = -1;
	ds.NumBytesDownloaded  = 0;
	ds.bInsertedFillerData = false;

	ds.MediaAssetID 	= Request->Period.IsValid() ? Request->Period->GetUniqueIdentifier() : "";
	ds.AdaptationSetID  = Request->AdaptationSet.IsValid() ? Request->AdaptationSet->GetUniqueIdentifier() : "";
	ds.RepresentationID = Request->Representation.IsValid() ? Request->Representation->GetUniqueIdentifier() : "";
	ds.URL  			= Request->Segment.MediaURL.URL;
	ds.Range  			= Request->Segment.MediaURL.Range;
	ds.CDN  			= Request->Segment.MediaURL.CDN;
	ds.RetryNumber  	= Request->NumOverallRetries;

	// Clear out the list of events found the last time.
	SegmentEventsFound.Empty();
	CheckForInbandDASHEvents();
	/*
		Actually handle the request only if this is not an EOS segment.
		A segment that is already at EOS is not meant to be loaded as it does not exist and there
		would not be another segment following it either. They are meant to indicate to the player
		that a stream has ended and will not be delivering any more data.

		NOTE:
		  We had to handle this request up to detecting the use of inband event streams in order to
		  signal that this stream has now ended and will not be receiving any further inband events!
	*/
	if (Request->bIsEOSSegment)
	{
		CurrentRequest.Reset();
		return;
	}

	// Clear internal work variables.
	bHasErrored 		   = false;
	bAbortedByABR   	   = false;
	bAllowEarlyEmitting    = false;
	bFillRemainingDuration = false;
	ABRAbortReason.Empty();

	FTimeValue NextExpectedDTS;
	FTimeValue LastKnownAUDuration;
	FTimeValue TimeOffset = Request->PeriodStart + Request->AST + Request->AdditionalAdjustmentTime;

	bool bDoNotTruncateAtPresentationEnd = PlayerSessionService->GetOptionValue(OptionKeyDoNotTruncateAtPresentationEnd).SafeGetBool(false);

	// Get the init segment if there is one. Either gets it from the entity cache or requests it now and adds it to the cache.
	InitSegmentError = GetInitSegment(MP4InitSegment, Request);
	// Get the CSD from the init segment in case there is a failure with the segment later.
	if (MP4InitSegment.IsValid())
	{
		if (MP4InitSegment->GetNumberOfTracks() == 1)
		{
			const IParserISO14496_12::ITrack* Track = MP4InitSegment->GetTrackByIndex(0);
			if (Track)
			{
				ActiveTrackData.CSD->CodecSpecificData = Track->GetCodecSpecificData();
				ActiveTrackData.CSD->RawCSD = Track->GetCodecSpecificDataRAW();
				ActiveTrackData.CSD->ParsedInfo = Track->GetCodecInformation();
				// Set information from the MPD codec information that is not available on the init segment.
				ActiveTrackData.CSD->ParsedInfo.SetBitrate(Request->CodecInfo.GetBitrate());
			}
		}
	}

	Parameters.EventListener->OnFragmentOpen(CurrentRequest);

	if (InitSegmentError.IsOK() && !HasReadBeenAborted())
	{
		// Tending to the media segment now.
		ds.SegmentType = Metrics::ESegmentType::Media;

		if (Request->Segment.bIsSideload)
		{
			FErrorDetail SideloadError;
			TSharedPtrTS<const TArray<uint8>> SideloadedData;
			SideloadError = RetrieveSideloadedFile(SideloadedData, Request);
			if (SideloadError.IsOK())
			{
				// CSD is only partially available for sideloaded files.
				ActiveTrackData.CSD->ParsedInfo = Request->CodecInfo;

				uint32 TrackTimescale = Request->Segment.Timescale;

				// Create an access unit
				FAccessUnit *AccessUnit = FAccessUnit::Create(Parameters.MemoryProvider);
				AccessUnit->ESType = Request->GetType();
				AccessUnit->AUSize = (uint32) SideloadedData->Num();
				AccessUnit->AUData = AccessUnit->AllocatePayloadBuffer(AccessUnit->AUSize);
				FMemory::Memcpy(AccessUnit->AUData, SideloadedData->GetData(), SideloadedData->Num());
				AccessUnit->AUCodecData = ActiveTrackData.CSD;
				AccessUnit->bIsFirstInSequence = true;
				AccessUnit->bIsSyncSample = true;
				AccessUnit->bIsDummyData = false;
				AccessUnit->bIsSideloaded = true;
				AccessUnit->BufferSourceInfo = ActiveTrackData.BufferSourceInfo;
				AccessUnit->SequenceIndex = Request->TimestampSequenceIndex;
				AccessUnit->DropState = FAccessUnit::EDropState::None;

				// Sideloaded files coincide with the period start
				AccessUnit->DTS = TimeOffset;
				AccessUnit->PTS = TimeOffset;
				AccessUnit->PTO.SetFromND(Request->Segment.PTO, TrackTimescale);
				AccessUnit->DTS.SetSequenceIndex(Request->TimestampSequenceIndex);
				AccessUnit->PTS.SetSequenceIndex(Request->TimestampSequenceIndex);

				FTimeValue Duration(Request->Segment.Duration, TrackTimescale);
				AccessUnit->Duration = Duration;

				ActiveTrackData.DurationSuccessfullyRead += Duration;
				NextExpectedDTS = AccessUnit->DTS + Duration;
				LastKnownAUDuration = Duration;
				ActiveTrackData.AddAccessUnit(AccessUnit);
				FAccessUnit::Release(AccessUnit);
				AccessUnit = nullptr;
			}
			else
			{
				CurrentRequest->ConnectionInfo.StatusInfo.ErrorDetail = SideloadError;
				bHasErrored = true;
			}
		}
		else if (!bIsEmptyFillerSegment)
		{
			ReadBuffer.Reset();
			ReadBuffer.ReceiveBuffer = MakeSharedTS<IElectraHttpManager::FReceiveBuffer>();

			// Start downloading the segment.
			TSharedPtrTS<IElectraHttpManager::FProgressListener>	ProgressListener(new IElectraHttpManager::FProgressListener);
			ProgressListener->ProgressDelegate   = IElectraHttpManager::FProgressListener::FProgressDelegate::CreateRaw(this, &FStreamHandler::HTTPProgressCallback);
			ProgressListener->CompletionDelegate = IElectraHttpManager::FProgressListener::FCompletionDelegate::CreateRaw(this, &FStreamHandler::HTTPCompletionCallback);
			TSharedPtrTS<IElectraHttpManager::FRequest> HTTP(new IElectraHttpManager::FRequest);
			HTTP->Parameters.URL = RequestURL;
			HTTP->Parameters.StreamType = Request->StreamType;
			HTTP->Parameters.QualityIndex = Request->QualityIndex;
			HTTP->Parameters.MaxQualityIndex = Request->MaxQualityIndex;
			HTTP->ReceiveBuffer = ReadBuffer.ReceiveBuffer;
			HTTP->ProgressListener = ProgressListener;
			HTTP->ResponseCache = PlayerSessionService->GetHTTPResponseCache();
			HTTP->ExternalDataReader = PlayerSessionService->GetExternalDataReader();
			HTTP->Parameters.Range.Set(Request->Segment.MediaURL.Range);
			HTTP->Parameters.AcceptEncoding.Set(TEXT("identity"));
			if (Request->Segment.MediaURL.CustomHeader.Len())
			{
				HTTP->Parameters.RequestHeaders.Emplace(HTTP::FHTTPHeader({DASH::HTTPHeaderOptionName, Request->Segment.MediaURL.CustomHeader}));
			}
			HTTP->Parameters.bCollectTimingTraces = Request->Segment.bLowLatencyChunkedEncodingExpected;
			// Set timeouts for media segment retrieval
			HTTP->Parameters.ConnectTimeout = PlayerSessionService->GetOptionValue(DASH::OptionKeyMediaSegmentConnectTimeout).SafeGetTimeValue(FTimeValue().SetFromMilliseconds(1000 * 4));
			HTTP->Parameters.NoDataTimeout = PlayerSessionService->GetOptionValue(DASH::OptionKeyMediaSegmentNoDataTimeout).SafeGetTimeValue(FTimeValue().SetFromMilliseconds(1000 * 4));

			ProgressReportCount = 0;
			DownloadCompleteSignal.Reset();
			PlayerSessionService->GetHTTPManager()->AddRequest(HTTP, false);

			MP4Parser = IParserISO14496_12::CreateParser();
			NumMOOFBoxesFound = 0;

			int64 LastSuccessfulFilePos = 0;
			uint32 TrackTimescale = 0;
			bool bDone = false;
			FTimeValue TimelineOffset;

			// Encrypted?
			TSharedPtr<ElectraCDM::IMediaCDMDecrypter, ESPMode::ThreadSafe> Decrypter;
			if (Request->DrmClient.IsValid())
			{
				if (Request->DrmClient->CreateDecrypter(Decrypter, Request->DrmMimeType) != ElectraCDM::ECDMError::Success)
				{
					bDone = true;
					bHasErrored = true;
					ds.FailureReason = FString::Printf(TEXT("Failed to create decrypter for segment. %s"), *Request->DrmClient->GetLastErrorMessage());
					LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
				}
			}


			while(!bDone && !HasErrored() && !HasReadBeenAborted())
			{
				Metrics::FSegmentDownloadStats::FMovieChunkInfo MoofInfo;
				MoofInfo.HeaderOffset = GetCurrentOffset();
				UEMediaError parseError = MP4Parser->ParseHeader(this, this, PlayerSessionService, MP4InitSegment.Get());
				if (parseError == UEMEDIA_ERROR_OK)
				{
					{
						SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_DASH_StreamReader);
						CSV_SCOPED_TIMING_STAT(ElectraPlayer, DASH_StreamReader);
						Request->Segment.bSawLMSG = MP4Parser->HasBrand(IParserISO14496_12::BrandType_lmsg);
						parseError = MP4Parser->PrepareTracks(PlayerSessionService, MP4InitSegment);
					}
					if (parseError == UEMEDIA_ERROR_OK)
					{
						// Get the metadata from the movie fragment or the init segment.
						const IParserISO14496_12::IMetadata* MoofMetadata = MP4Parser->GetMetadata(IParserISO14496_12::EBaseBoxType::Moof);
						const IParserISO14496_12::IMetadata* MoovMetadata = !MoofMetadata ? MP4InitSegment.IsValid() ? MP4InitSegment->GetMetadata(IParserISO14496_12::EBaseBoxType::Moov) : nullptr : nullptr;
						if (MoofMetadata || MoovMetadata)
						{
							const IParserISO14496_12::IMetadata* md = MoofMetadata ? MoofMetadata : MoovMetadata;
							uint32 hdlr = md->GetHandler();
							uint32 res0 = md->GetReserved(0);
							TArray<UtilsMP4::FMetadataParser::FBoxInfo> Boxes;
							for(int32 i=0, iMax=md->GetNumChildBoxes(); i<iMax; ++i)
							{
								Boxes.Emplace(UtilsMP4::FMetadataParser::FBoxInfo(md->GetChildBoxType(i), md->GetChildBoxData(i), md->GetChildBoxDataSize(i)));
							}
							TSharedPtrTS<UtilsMP4::FMetadataParser> MediaMetadata = MakeSharedTS<UtilsMP4::FMetadataParser>();
							if (MediaMetadata->Parse(hdlr, res0, Boxes) == UtilsMP4::FMetadataParser::EResult::Success)
							{
								FTimeValue StartTime = md == MoofMetadata ? Request->GetFirstPTS() : TimeOffset;
								StartTime.SetSequenceIndex(Request->TimestampSequenceIndex);
  								PlayerSessionService->SendMessageToPlayer(FPlaylistMetadataUpdateMessage::Create(StartTime, MediaMetadata));
							}
						}

						// For the time being we only want to have a single track in the movie segments.
						if (MP4Parser->GetNumberOfTracks() == 1)
						{
							const IParserISO14496_12::ITrack* Track = MP4Parser->GetTrackByIndex(0);
							check(Track);
							if (Track)
							{
								HandleEventMessages();

								ActiveTrackData.CSD->CodecSpecificData = Track->GetCodecSpecificData();
								ActiveTrackData.CSD->RawCSD = Track->GetCodecSpecificDataRAW();
								ActiveTrackData.CSD->ParsedInfo = Track->GetCodecInformation();
								// Set information from the MPD codec information that is not available on the init segment.
								ActiveTrackData.CSD->ParsedInfo.SetBitrate(Request->CodecInfo.GetBitrate());

								IParserISO14496_12::ITrackIterator* TrackIterator = Track->CreateIterator();
								TrackTimescale = TrackIterator->GetTimescale();

								int64 MediaLocalFirstAUTime = Request->Segment.MediaLocalFirstAUTime;
								int64 MediaLocalLastAUTime = bDoNotTruncateAtPresentationEnd ? TNumericLimits<int64>::Max() : Request->Segment.MediaLocalLastAUTime;
								int64 PTO = Request->Segment.PTO;

								if (TrackTimescale != Request->Segment.Timescale)
								{
									// Need to rescale the AU times from the MPD timescale to the media timescale.
									MediaLocalFirstAUTime = FTimeFraction(Request->Segment.MediaLocalFirstAUTime, Request->Segment.Timescale).GetAsTimebase(TrackTimescale);
									MediaLocalLastAUTime = MediaLocalLastAUTime == TNumericLimits<int64>::Max() ? MediaLocalLastAUTime : FTimeFraction(Request->Segment.MediaLocalLastAUTime, Request->Segment.Timescale).GetAsTimebase(TrackTimescale);
									PTO = FTimeFraction(Request->Segment.PTO, Request->Segment.Timescale).GetAsTimebase(TrackTimescale);

									if (!Request->bWarnedAboutTimescale)
									{
										Request->bWarnedAboutTimescale = true;
										LogMessage(IInfoLog::ELevel::Verbose, FString::Printf(TEXT("Track timescale %u differs from timescale of %u in MPD or segment index. This may cause playback problems!"), TrackTimescale, Request->Segment.Timescale));
									}
								}

								// If the track uses encryption we update the DRM system with the PSSH boxes that are currently in use.
								if (Decrypter.IsValid())
								{
									TArray<TArray<uint8>> PSSHBoxes;
									Track->GetPSSHBoxes(PSSHBoxes, true, true);
									Decrypter->UpdateInitDataFromMultiplePSSH(PSSHBoxes);
								}

								// Producer reference time?
								struct FProducerTime
								{
									FTimeValue	Base;
									int64		Media = 0;
								};
								FProducerTime ProducerTime;
								if (Request->Segment.MeasureLatencyViaReferenceTimeInfoID >= 0 && Request->Segment.ProducerReferenceTimeInfos.Num())
								{
									// We only look at inband 'prtf' boxes if the <ProducerReferenceTime> element in the MPD tells us to.
									// This is similar to events that are not to be considered any more if the MPD doesn't specify them.
									for(auto &MPDPrtf : Request->Segment.ProducerReferenceTimeInfos)
									{
										if (MPDPrtf.GetID() == (uint32)Request->Segment.MeasureLatencyViaReferenceTimeInfoID)
										{
											// Use the inband 'prft' boxes?
											if (MPDPrtf.bInband)
											{
												TArray<IParserISO14496_12::ITrack::FProducerReferenceTime> PRFTBoxes;
												Track->GetPRFTBoxes(PRFTBoxes);
												bool bFound = false;
												for(auto &MP4Prtf : PRFTBoxes)
												{
													if ((MPDPrtf.Type == FManifestDASHInternal::FProducerReferenceTimeInfo::EType::Encoder && MP4Prtf.Reference == IParserISO14496_12::ITrack::FProducerReferenceTime::EReferenceType::Encoder) ||
														(MPDPrtf.Type == FManifestDASHInternal::FProducerReferenceTimeInfo::EType::Captured && MP4Prtf.Reference == IParserISO14496_12::ITrack::FProducerReferenceTime::EReferenceType::Captured))
													{
														RFC5905::ParseNTPTime(ProducerTime.Base, MP4Prtf.NtpTimestamp);
														ProducerTime.Media = MP4Prtf.MediaTime;
														bFound = true;
														break;
													}
												}
												// When the MPD says that there are inband prtf's then this has to be so. If for some reason this is not the case
												// then what are we to do?
												if (!bFound)
												{
													// We take the values from the MPD here, which may be better than nothing?!
													ProducerTime.Base = MPDPrtf.WallclockTime;
													ProducerTime.Media = MPDPrtf.PresentationTime;
												}
											}
											else
											{
												// Use values from MPD
												ProducerTime.Base = MPDPrtf.WallclockTime;
												ProducerTime.Media = MPDPrtf.PresentationTime;
											}
											break;
										}
									}
								}

								// Iterate the moof
								for(Error = TrackIterator->StartAtFirst(false); Error == UEMEDIA_ERROR_OK; Error = TrackIterator->Next())
								{
									if (ActiveTrackData.bIsFirstInSequence)
									{
										FTimeValue BaseMediaDecodeTime;
										BaseMediaDecodeTime.SetFromND(TrackIterator->GetBaseMediaDecodeTime(), TrackTimescale);
										TimelineOffset = BaseMediaDecodeTime - Request->Segment.PeriodLocalSegmentStartTime;
										//LogMessage(IInfoLog::ELevel::Info, FString::Printf(TEXT("Segment to timeline delta = %.3fs"), TimelineOffset.GetAsSeconds()));
									}

									// Get the DTS and PTS. Those are 0-based in a fragment and offset by the base media decode time of the fragment.
									const int64 AUDTS = TrackIterator->GetDTS();
									const int64 AUPTS = TrackIterator->GetPTS();
									const int64 AUDuration = TrackIterator->GetDuration();

									// Create access unit
									FAccessUnit *AccessUnit = FAccessUnit::Create(Parameters.MemoryProvider);
									AccessUnit->ESType = Request->GetType();
									AccessUnit->AUSize = (uint32) TrackIterator->GetSampleSize();
									AccessUnit->AUData = AccessUnit->AllocatePayloadBuffer(AccessUnit->AUSize);
									AccessUnit->bIsFirstInSequence = ActiveTrackData.bIsFirstInSequence;
									AccessUnit->bIsSyncSample = TrackIterator->IsSyncSample();
									AccessUnit->bIsDummyData = false;
									AccessUnit->AUCodecData = ActiveTrackData.CSD;
									AccessUnit->BufferSourceInfo = ActiveTrackData.BufferSourceInfo;

									AccessUnit->DropState = FAccessUnit::EDropState::None;
									// Is this AU entirely before the time we want?
									int64 Overlap = AUPTS + AUDuration - MediaLocalFirstAUTime;
									if (Overlap <= 0)
									{
										AccessUnit->DropState |= FAccessUnit::EDropState::TooEarly;
									}
									// Entirely past the time allowed?
									if (bIsLastSegment || Request->Segment.bFrameAccuracyRequired)
									{
										if (AUDTS >= MediaLocalLastAUTime && AUPTS >= MediaLocalLastAUTime)
										{
											AccessUnit->DropState |= FAccessUnit::EDropState::TooLate;
										}
										/*
										if (AUPTS + AUDuration >= MediaLocalLastAUTime)
										{
											// Note that we cannot stop reading here. For one we need to continue in case there are additional tracks here
											// that have not yet finished (although multiple tracks are not presently supported) and we also need to make
											// it all the way to the end in case there are additional boxes that we have to read (like 'emsg').
										}
										*/
									}
									FTimeValue Duration(AUDuration, TrackTimescale);
									AccessUnit->Duration = Duration;

									// Offset the AU's DTS and PTS to the time mapping of the segment.
									AccessUnit->DTS.SetFromND(AUDTS - PTO, TrackTimescale);
									AccessUnit->DTS += TimeOffset;
									AccessUnit->PTS.SetFromND(AUPTS - PTO, TrackTimescale);
									AccessUnit->PTS += TimeOffset;
									AccessUnit->PTO.SetFromND(PTO, TrackTimescale);

									// Set the sequence index member and update all timestamps with it as well.
									AccessUnit->SequenceIndex = Request->TimestampSequenceIndex;
									AccessUnit->DTS.SetSequenceIndex(Request->TimestampSequenceIndex);
									AccessUnit->PTS.SetSequenceIndex(Request->TimestampSequenceIndex);

									if (Request->Segment.bFrameAccuracyRequired)
									{
										if (Request->FrameAccurateStartTime.IsValid())
										{
											AccessUnit->EarliestPTS = Request->FrameAccurateStartTime;
										}
										else
										{
											AccessUnit->EarliestPTS.SetFromND(MediaLocalFirstAUTime - PTO, TrackTimescale);
											AccessUnit->EarliestPTS += TimeOffset;
										}
										AccessUnit->EarliestPTS.SetSequenceIndex(Request->TimestampSequenceIndex);
									}
									if (MediaLocalLastAUTime != TNumericLimits<int64>::Max())
									{
										AccessUnit->LatestPTS.SetFromND(MediaLocalLastAUTime - PTO, TrackTimescale);
									}
									else
									{
										AccessUnit->LatestPTS.SetToPositiveInfinity();
									}
									AccessUnit->LatestPTS += TimeOffset;
									AccessUnit->LatestPTS.SetSequenceIndex(Request->TimestampSequenceIndex);

									AccessUnit->OffsetFromSegmentStart = TimelineOffset;

									AccessUnit->ProducerReferenceTime = ProducerTime.Base + FTimeValue(AUDTS - ProducerTime.Media, TrackTimescale);

									ElectraCDM::FMediaCDMSampleInfo SampleEncryptionInfo;
									bool bIsSampleEncrypted = TrackIterator->GetEncryptionInfo(SampleEncryptionInfo);

									// There should not be any gaps!
									int64 NumBytesToSkip = TrackIterator->GetSampleFileOffset() - GetCurrentOffset();
									if (NumBytesToSkip < 0)
									{
										// Current read position is already farther than where the data is supposed to be.
										FAccessUnit::Release(AccessUnit);
										AccessUnit = nullptr;
										ds.FailureReason = FString::Printf(TEXT("Read position already at %lld but data starts at %lld in segment \"%s\""), (long long int)GetCurrentOffset(), (long long int)TrackIterator->GetSampleFileOffset(), *RequestURL);
										LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
										bHasErrored = true;
										bDone = true;
										break;
									}
									else if (NumBytesToSkip > 0)
									{
										int64 NumSkipped = ReadData(nullptr, NumBytesToSkip);
										if (NumSkipped != NumBytesToSkip)
										{
											FAccessUnit::Release(AccessUnit);
											AccessUnit = nullptr;
											ds.FailureReason = FString::Printf(TEXT("Failed to skip over %lld bytes in segment \"%s\""), (long long int)NumBytesToSkip, *RequestURL);
											LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
											bHasErrored = true;
											bDone = true;
											break;
										}
									}

									if (MoofInfo.PayloadStartOffset == 0)
									{
										MoofInfo.PayloadStartOffset = GetCurrentOffset();
									}

									int64 NumRead = ReadData(AccessUnit->AUData, AccessUnit->AUSize);
									if (NumRead == AccessUnit->AUSize)
									{
										MoofInfo.NumKeyframeBytes += AccessUnit->bIsSyncSample ? AccessUnit->AUSize : 0;
										MoofInfo.ContentDuration += Duration;
										ActiveTrackData.DurationSuccessfullyRead += Duration;
										NextExpectedDTS = AccessUnit->DTS + Duration;
										LastKnownAUDuration = Duration;
										LastSuccessfulFilePos = GetCurrentOffset();

										// If we need to decrypt we have to wait for the decrypter to become ready.
										if (bIsSampleEncrypted && Decrypter.IsValid())
										{
											while(!bTerminate && !HasReadBeenAborted() &&
												  (Decrypter->GetState() == ElectraCDM::ECDMState::WaitingForKey || Decrypter->GetState() == ElectraCDM::ECDMState::Idle))
											{
												FMediaRunnable::SleepMilliseconds(100);
											}
											ElectraCDM::ECDMError DecryptResult = ElectraCDM::ECDMError::Failure;
											if (Decrypter->GetState() == ElectraCDM::ECDMState::Ready)
											{
												SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_DASH_StreamReader);
												CSV_SCOPED_TIMING_STAT(ElectraPlayer, DASH_StreamReader);
												DecryptResult = Decrypter->DecryptInPlace((uint8*) AccessUnit->AUData, (int32) AccessUnit->AUSize, SampleEncryptionInfo);
											}
											if (DecryptResult != ElectraCDM::ECDMError::Success)
											{
												FAccessUnit::Release(AccessUnit);
												AccessUnit = nullptr;
												ds.FailureReason = FString::Printf(TEXT("Failed to decrypt segment \"%s\" with error %d (%s)"), *RequestURL, (int32)DecryptResult, *Decrypter->GetLastErrorMessage());
												LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
												bHasErrored = true;
												bDone = true;
												break;
											}
										}
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
										if (ActiveTrackData.bReadPastLastPTS)
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
											ActiveTrackData.bReadPastLastPTS = true;
										}
									}

									if (AccessUnit)
									{
										ActiveTrackData.AddAccessUnit(AccessUnit);
										FAccessUnit::Release(AccessUnit);
										AccessUnit = nullptr;
									}

									// Shall we pass on any AUs we already read?
									if (bAllowEarlyEmitting)
									{
										EmitSamples(EEmitType::UntilBlocked, Request);
									}

									ActiveTrackData.bIsFirstInSequence = false;
								}
								delete TrackIterator;

								MoofInfo.PayloadEndOffset = LastSuccessfulFilePos;
								if (Request->Segment.bLowLatencyChunkedEncodingExpected)
								{
									ds.MovieChunkInfos.Emplace(MoveTemp(MoofInfo));
								}

								if (Error != UEMEDIA_ERROR_OK && Error != UEMEDIA_ERROR_END_OF_STREAM)
								{
									// error iterating
									ds.FailureReason = FString::Printf(TEXT("Failed to iterate over segment \"%s\""), *RequestURL);
									LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
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
								ds.FailureReason = FString::Printf(TEXT("Segment \"%s\" has no track"), *RequestURL);
								LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
								bHasErrored = true;
							}
						}
						else
						{
							// more than 1 track
							ds.FailureReason = FString::Printf(TEXT("Segment \"%s\" has more than one track"), *RequestURL);
							LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
							bHasErrored = true;
						}
					}
					else
					{
						// error preparing track for iterating
						ds.FailureReason = FString::Printf(TEXT("Failed to prepare segment \"%s\" for iterating"), *RequestURL);
						LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
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
					if (!HasReadBeenAborted() && !HasErrored())
					{
						ds.FailureReason = FString::Printf(TEXT("Failed to parse segment \"%s\""), *RequestURL);
						LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
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
		// Init segment error.
		if (!HasReadBeenAborted())
		{
			CurrentRequest->ConnectionInfo.StatusInfo.ErrorDetail = InitSegmentError;
			ds.bParseFailure = InitSegmentError.GetCode() == INTERNAL_ERROR_INIT_SEGMENT_PARSE_ERROR;
			bHasErrored = true;
		}
	}

	// Do we need to fill remaining duration with dummy data?
	if (bIsEmptyFillerSegment || bFillRemainingDuration)
	{
		// Inserting dummy data means this request was successful.
		CurrentRequest->ConnectionInfo.StatusInfo.Empty();
		CurrentRequest->ConnectionInfo.StatusInfo.HTTPStatus = Request->Segment.MediaURL.Range.Len() ? 206 : 200;
		ds.HTTPStatusCode = CurrentRequest->ConnectionInfo.StatusInfo.HTTPStatus;

		// Get the supposed segment duration.
		FTimeValue SegmentDurationToGo = SegmentDuration;
		FTimeValue PTO;
		PTO.SetFromND(Request->Segment.PTO, Request->Segment.Timescale);
		FTimeValue DiscardBefore = TimeOffset + FTimeValue(Request->Segment.MediaLocalFirstAUTime - Request->Segment.PTO, Request->Segment.Timescale);
		FTimeValue DiscardAfter = TimeOffset + FTimeValue(Request->Segment.MediaLocalLastAUTime - Request->Segment.PTO, Request->Segment.Timescale);

		// Did we get anything so far?
		FTimeValue DefaultDuration;
		if (NextExpectedDTS.IsValid())
		{
			check(ActiveTrackData.DurationSuccessfullyRead.IsValid());
			check(LastKnownAUDuration.IsValid());
			SegmentDurationToGo -= ActiveTrackData.DurationSuccessfullyRead;
			DefaultDuration = LastKnownAUDuration;
			NextExpectedDTS.SetSequenceIndex(Request->TimestampSequenceIndex);
		}
		else
		{
			// No. We need to start with the segment time. Not the adjusted media local time since we would be generating
			// too many dummy AUs as we go over the entire segment duration!
			NextExpectedDTS.SetFromND(Request->Segment.Time - Request->Segment.PTO, Request->Segment.Timescale);
			NextExpectedDTS += TimeOffset;
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
					if (ActiveTrackData.CSD.IsValid() && ActiveTrackData.CSD->ParsedInfo.GetSamplingRate())
					{
						d = (uint32) ActiveTrackData.CSD->ParsedInfo.GetSamplingRate();
						switch(ActiveTrackData.CSD->ParsedInfo.GetCodec())
						{
							case FStreamCodecInformation::ECodec::AAC:
							{
								n = ActiveTrackData.CSD->ParsedInfo.GetExtras().GetValue(StreamCodecInformationOptions::SamplesPerBlock).SafeGetInt64(1024);
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
			AccessUnit->BufferSourceInfo = ActiveTrackData.BufferSourceInfo;
			AccessUnit->Duration = DefaultDuration;
			AccessUnit->AUSize = 0;
			AccessUnit->AUData = nullptr;
			AccessUnit->bIsDummyData = true;
			if (ActiveTrackData.CSD.IsValid() && ActiveTrackData.CSD->CodecSpecificData.Num())
			{
				AccessUnit->AUCodecData = ActiveTrackData.CSD;
			}

			// Set the sequence index member and update all timestamps with it as well.
			AccessUnit->SequenceIndex = Request->TimestampSequenceIndex;
			AccessUnit->DTS = NextExpectedDTS;
			AccessUnit->PTS = NextExpectedDTS;
			AccessUnit->DTS.SetSequenceIndex(Request->TimestampSequenceIndex);
			AccessUnit->PTS.SetSequenceIndex(Request->TimestampSequenceIndex);
			AccessUnit->PTO = PTO;

			if (Request->Segment.bFrameAccuracyRequired)
			{
				AccessUnit->EarliestPTS = DiscardBefore;
				AccessUnit->EarliestPTS.SetSequenceIndex(Request->TimestampSequenceIndex);
			}
			AccessUnit->LatestPTS = DiscardAfter;
			AccessUnit->LatestPTS.SetSequenceIndex(Request->TimestampSequenceIndex);

			//AccessUnit->OffsetFromSegmentStart = TimelineOffset;
			//AccessUnit->ProducerReferenceTime = ProducerTime.Base + FTimeValue(AUDTS - ProducerTime.Media, TrackTimescale);

			// Calculate the drop on the fragment local NextExpectedDTS/PTS.
			AccessUnit->DropState = FAccessUnit::EDropState::None;
			if (NextExpectedDTS + AccessUnit->Duration < DiscardBefore)
			{
				AccessUnit->DropState |= FAccessUnit::EDropState::TooEarly;
			}
			bool bIsLast = false;
			if (NextExpectedDTS > DiscardAfter)
			{
				AccessUnit->DropState |= FAccessUnit::EDropState::TooLate;
				bIsLast = AccessUnit->bIsLastInPeriod = true;
			}

			NextExpectedDTS += DefaultDuration;
			SegmentDurationToGo -= DefaultDuration;
			ActiveTrackData.AddAccessUnit(AccessUnit);
			FAccessUnit::Release(AccessUnit);
			AccessUnit = nullptr;

			if (bIsLast)
			{
				break;
			}
		}
	}

	// We got all the samples there are to get.
	ActiveTrackData.bGotAllSamples = true;
	// Emit all remaining pending AUs
	EmitSamples(EEmitType::AllRemaining, Request);
	ActiveTrackData.AccessUnitFIFO.Empty();
	ActiveTrackData.SortedAccessUnitFIFO.Empty();

	// If we did not set a specific failure message yet set the one from the downloader.
	if (ds.FailureReason.Len() == 0)
	{
		ds.FailureReason = CurrentRequest->ConnectionInfo.StatusInfo.ErrorDetail.GetMessage();
	}
	// If the ABR aborted this takes precedence in the failure message. Overwrite it.
	if (bAbortedByABR)
	{
		ds.FailureReason = ABRAbortReason;
	}
	// Set up remaining download stat fields.
	ds.bWasAborted  	  = bAbortedByABR;
	ds.bWasSuccessful     = !bHasErrored && !bAbortedByABR;
	ds.URL  			  = CurrentRequest->ConnectionInfo.EffectiveURL;
	ds.HTTPStatusCode     = CurrentRequest->ConnectionInfo.StatusInfo.HTTPStatus;
	ds.DurationDownloaded = ActiveTrackData.DurationSuccessfullyRead.GetAsSeconds();
	ds.DurationDelivered  = ActiveTrackData.DurationSuccessfullyDelivered.GetAsSeconds();
	ds.TimeToFirstByte    = CurrentRequest->ConnectionInfo.TimeUntilFirstByte;
	ds.TimeToDownload     = (CurrentRequest->ConnectionInfo.RequestEndTime - CurrentRequest->ConnectionInfo.RequestStartTime).GetAsSeconds();
	ds.ByteSize 		  = CurrentRequest->ConnectionInfo.ContentLength;
	ds.NumBytesDownloaded = CurrentRequest->ConnectionInfo.BytesReadSoFar;
	ds.bIsCachedResponse  = CurrentRequest->ConnectionInfo.bIsCachedResponse;
	CurrentRequest->ConnectionInfo.GetTimingTraces(ds.TimingTraces);

	// Was this request for a segment that might potentially be missing and it did?
	if (Request->Segment.bMayBeMissing && (ds.HTTPStatusCode == 404 || ds.HTTPStatusCode == 416))
	{
		// This is not an actual error then. Pretend all was well.
		ds.bWasSuccessful = true;
		ds.HTTPStatusCode = 200;
		ds.bIsMissingSegment = true;
		ds.FailureReason.Empty();
		CurrentRequest->DownloadStats.bWasSuccessful = true;
		CurrentRequest->DownloadStats.HTTPStatusCode = 200;
		CurrentRequest->DownloadStats.bIsMissingSegment = true;
		CurrentRequest->DownloadStats.FailureReason.Empty();
		CurrentRequest->ConnectionInfo.StatusInfo.Empty();
		// Take note of the missing segment in the segment info as well so the search for the next segment
		// can return quicker.
		CurrentRequest->Segment.bIsMissing = true;
	}

	// If we had to wait for the segment to become available and we got a 404 back we might have been trying to fetch
	// the segment before the server made it available.
	if (Request->ASAST.IsValid() && (ds.HTTPStatusCode == 404 || ds.HTTPStatusCode == 416))
	{
		FTimeValue Now = PlayerSessionService->GetSynchronizedUTCTime()->GetTime();
		if ((ds.AvailibilityDelay = (Request->ASAST - Now).GetAsSeconds()) == 0.0)
		{
			// In the extremely unlikely event this comes out to zero exactly set a small value so the ABR knows there was a delay.
			ds.AvailibilityDelay = -0.01;
		}
	}

	// If we failed to get the segment and there is an inband DASH event stream which triggers MPD events and
	// we did not get such an event in the 'emsg' boxes, then we err on the safe side and assume this segment
	// would have carried an MPD update event and fire an artificial event.
	if (!ds.bWasSuccessful && CurrentRequest->Segment.InbandEventStreams.FindByPredicate([](const FManifestDASHInternal::FSegmentInformation::FInbandEventStream& This){ return This.SchemeIdUri.Equals(DASH::Schemes::ManifestEvents::Scheme_urn_mpeg_dash_event_2012); }))
	{
		if (SegmentEventsFound.IndexOfByPredicate([](const TSharedPtrTS<DASH::FPlayerEvent>& This){return This->GetSchemeIdUri().Equals(DASH::Schemes::ManifestEvents::Scheme_urn_mpeg_dash_event_2012);}) == INDEX_NONE)
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_DASH_StreamReader);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, DASH_StreamReader);
			TSharedPtrTS<DASH::FPlayerEvent> NewEvent = MakeSharedTS<DASH::FPlayerEvent>();
			NewEvent->SetOrigin(IAdaptiveStreamingPlayerAEMSEvent::EOrigin::InbandEventStream);
			NewEvent->SetSchemeIdUri(DASH::Schemes::ManifestEvents::Scheme_urn_mpeg_dash_event_2012);
			NewEvent->SetValue(TEXT("1"));
			NewEvent->SetID("$missed$");
			FTimeValue EPT((int64)CurrentRequest->Segment.Time, CurrentRequest->Segment.Timescale);
			FTimeValue PTO(CurrentRequest->Segment.PTO, CurrentRequest->Segment.Timescale);
			NewEvent->SetPresentationTime(TimeOffset - PTO + EPT);
			NewEvent->SetPeriodID(CurrentRequest->Period->GetUniqueIdentifier());
			PlayerSessionService->GetAEMSEventHandler()->AddEvent(NewEvent, CurrentRequest->Period->GetUniqueIdentifier(), IAdaptiveStreamingPlayerAEMSHandler::EEventAddMode::AddIfNotExists);
		}
	}

	// Clean out everything before reporting OnFragmentClose().
	TSharedPtrTS<FStreamSegmentRequestDASH> FinishedRequest = CurrentRequest;
	CurrentRequest.Reset();
	ReadBuffer.Reset();
	MP4Parser.Reset();
	SegmentEventsFound.Empty();

	if (!bSilentCancellation)
	{
		StreamSelector->ReportDownloadEnd(ds);
		Parameters.EventListener->OnFragmentClose(FinishedRequest);
	}
}



void FStreamReaderDASH::FStreamHandler::HandleRequestMKV()
{
	// Get the request into a local shared pointer to hold on to it.
	TSharedPtrTS<FStreamSegmentRequestDASH> Request = CurrentRequest;
	FTimeValue SegmentDuration = FTimeValue().SetFromND(Request->Segment.Duration, Request->Segment.Timescale);
	FString RequestURL = Request->Segment.MediaURL.URL;
	TSharedPtrTS<const IParserMKV> MKVParser;
	FErrorDetail InitSegmentError;
	bool bIsEmptyFillerSegment = Request->bInsertFillerData;
	bool bIsLastSegment = Request->Segment.bIsLastInPeriod;
	FTimeValue TimeOffset = Request->PeriodStart + Request->AST + Request->AdditionalAdjustmentTime;

	ActiveTrackData.Reset();
	ActiveTrackData.CSD = MakeSharedTS<FAccessUnit::CodecData>();
	// Copy the source buffer info into a new instance and set the playback sequence ID in it.
	ActiveTrackData.BufferSourceInfo = MakeSharedTS<FBufferSourceInfo>(*Request->SourceBufferInfo);
	ActiveTrackData.BufferSourceInfo->PlaybackSequenceID = Request->GetPlaybackSequenceID();
	// Video needs to be held back to recalculate the frame duration and possibly the DTS.
	ActiveTrackData.StreamType = Request->GetType();
	ActiveTrackData.bNeedToRecalculateDurations = Request->GetType() == EStreamType::Video;


	Metrics::FSegmentDownloadStats& ds = CurrentRequest->DownloadStats;
	ds.StatsID = FMediaInterlockedIncrement(UniqueDownloadID);
	ds.FailureReason.Empty();
	ds.bWasSuccessful = true;
	ds.bWasAborted = false;
	ds.bDidTimeout = false;
	ds.HTTPStatusCode = 0;
	ds.StreamType = Request->GetType();
	// Assume we need to fetch the init segment first. This gets changed to 'media' when we load the media segment.
	ds.SegmentType = Metrics::ESegmentType::Init;
	ds.PresentationTime = Request->GetFirstPTS().GetAsSeconds();
	ds.Bitrate = Request->GetBitrate();
	ds.Duration = SegmentDuration.GetAsSeconds();
	ds.DurationDownloaded = 0.0;
	ds.DurationDelivered = 0.0;
	ds.TimeToFirstByte = 0.0;
	ds.TimeToDownload = 0.0;
	ds.ByteSize = -1;
	ds.NumBytesDownloaded = 0;
	ds.bInsertedFillerData = false;

	ds.MediaAssetID = Request->Period.IsValid() ? Request->Period->GetUniqueIdentifier() : TEXT("");
	ds.AdaptationSetID  = Request->AdaptationSet.IsValid() ? Request->AdaptationSet->GetUniqueIdentifier() : TEXT("");
	ds.RepresentationID = Request->Representation.IsValid() ? Request->Representation->GetUniqueIdentifier() : TEXT("");
	ds.URL = Request->Segment.MediaURL.URL;
	ds.Range = Request->Segment.MediaURL.Range;
	ds.CDN = Request->Segment.MediaURL.CDN;
	ds.RetryNumber = Request->NumOverallRetries;

	// Clear out the list of events found the last time.
	SegmentEventsFound.Empty();
	CheckForInbandDASHEvents();
	/*
		Actually handle the request only if this is not an EOS segment.
		A segment that is already at EOS is not meant to be loaded as it does not exist and there
		would not be another segment following it either. They are meant to indicate to the player
		that a stream has ended and will not be delivering any more data.

		NOTE:
		  We had to handle this request up to detecting the use of inband event streams in order to
		  signal that this stream has now ended and will not be receiving any further inband events!
	*/
	if (Request->bIsEOSSegment)
	{
		CurrentRequest.Reset();
		return;
	}

	// Clear internal work variables.
	bHasErrored = false;
	bAbortedByABR = false;
	bAllowEarlyEmitting = false;
	bFillRemainingDuration = false;
	ABRAbortReason.Empty();

	bool bDoNotTruncateAtPresentationEnd = PlayerSessionService->GetOptionValue(OptionKeyDoNotTruncateAtPresentationEnd).SafeGetBool(false);

	// Get the init segment if there is one. Either gets it from the entity cache or requests it now and adds it to the cache.
	InitSegmentError = GetInitSegment(MKVParser, Request);
	// Get the CSD from the init segment in case there is a failure with the segment later.
	if (MKVParser.IsValid())
	{
		if (MKVParser->GetNumberOfTracks() == 1)
		{
			const IParserMKV::ITrack* Track = MKVParser->GetTrackByIndex(0);
			if (Track)
			{
				ActiveTrackData.CSD->ParsedInfo = Track->GetCodecInformation();
				ActiveTrackData.CSD->CodecSpecificData = ActiveTrackData.CSD->ParsedInfo.GetCodecSpecificData();
				FVariantValue dcr = ActiveTrackData.CSD->ParsedInfo.GetExtras().GetValue(StreamCodecInformationOptions::DecoderConfigurationRecord);
				if (dcr.IsValid() && dcr.IsType(FVariantValue::EDataType::TypeU8Array))
				{
					ActiveTrackData.CSD->RawCSD = dcr.GetArray();
				}
				// Set information from the MPD codec information that is not available on the init segment.
				ActiveTrackData.CSD->ParsedInfo.SetBitrate(Request->CodecInfo.GetBitrate());
			}
		}
		else
		{
			// more than 1 track
			if (MKVParser->GetNumberOfTracks() > 1)
			{
				ds.FailureReason = FString::Printf(TEXT("Segment \"%s\" has more than one track"), *RequestURL);
			}
			else
			{
				ds.FailureReason = FString::Printf(TEXT("Segment \"%s\" has no usable track"), *RequestURL);
			}
			LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
			bHasErrored = true;
		}
	}
	else
	{
		ds.FailureReason = FString::Printf(TEXT("Initialization segment \"%s\" failed"), *RequestURL);
		LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
		bHasErrored = true;
	}

	Parameters.EventListener->OnFragmentOpen(CurrentRequest);

	if (!HasErrored() && InitSegmentError.IsOK() && !HasReadBeenAborted())
	{
		// Tending to the media segment now.
		ds.SegmentType = Metrics::ESegmentType::Media;

		if (!bIsEmptyFillerSegment)
		{
			ReadBuffer.Reset();
			ReadBuffer.ReceiveBuffer = MakeSharedTS<IElectraHttpManager::FReceiveBuffer>();

			// Start downloading the segment.
			TSharedPtrTS<IElectraHttpManager::FProgressListener> ProgressListener(new IElectraHttpManager::FProgressListener);
			ProgressListener->ProgressDelegate = IElectraHttpManager::FProgressListener::FProgressDelegate::CreateRaw(this, &FStreamHandler::HTTPProgressCallback);
			ProgressListener->CompletionDelegate = IElectraHttpManager::FProgressListener::FCompletionDelegate::CreateRaw(this, &FStreamHandler::HTTPCompletionCallback);
			TSharedPtrTS<IElectraHttpManager::FRequest> HTTP(new IElectraHttpManager::FRequest);
			HTTP->Parameters.URL = RequestURL;
			HTTP->Parameters.StreamType = Request->StreamType;
			HTTP->Parameters.QualityIndex = Request->QualityIndex;
			HTTP->Parameters.MaxQualityIndex = Request->MaxQualityIndex;
			HTTP->ReceiveBuffer = ReadBuffer.ReceiveBuffer;
			HTTP->ProgressListener = ProgressListener;
			HTTP->ResponseCache = PlayerSessionService->GetHTTPResponseCache();
			HTTP->ExternalDataReader = PlayerSessionService->GetExternalDataReader();
			HTTP->Parameters.Range.Set(Request->Segment.MediaURL.Range);
			HTTP->Parameters.AcceptEncoding.Set(TEXT("identity"));
			if (Request->Segment.MediaURL.CustomHeader.Len())
			{
				HTTP->Parameters.RequestHeaders.Emplace(HTTP::FHTTPHeader({DASH::HTTPHeaderOptionName, Request->Segment.MediaURL.CustomHeader}));
			}
			HTTP->Parameters.bCollectTimingTraces = Request->Segment.bLowLatencyChunkedEncodingExpected;
			// Set timeouts for media segment retrieval
			HTTP->Parameters.ConnectTimeout = PlayerSessionService->GetOptionValue(DASH::OptionKeyMediaSegmentConnectTimeout).SafeGetTimeValue(FTimeValue().SetFromMilliseconds(1000 * 4));
			HTTP->Parameters.NoDataTimeout = PlayerSessionService->GetOptionValue(DASH::OptionKeyMediaSegmentNoDataTimeout).SafeGetTimeValue(FTimeValue().SetFromMilliseconds(1000 * 4));

			ProgressReportCount = 0;
			DownloadCompleteSignal.Reset();
			PlayerSessionService->GetHTTPManager()->AddRequest(HTTP, false);

			bool bDone = false;

			// Encrypted?
			TSharedPtr<ElectraCDM::IMediaCDMDecrypter, ESPMode::ThreadSafe> Decrypter;
			if (Request->DrmClient.IsValid())
			{
				if (Request->DrmClient->CreateDecrypter(Decrypter, Request->DrmMimeType) != ElectraCDM::ECDMError::Success)
				{
					bDone = true;
					bHasErrored = true;
					ds.FailureReason = FString::Printf(TEXT("Failed to create decrypter for segment. %s"), *Request->DrmClient->GetLastErrorMessage());
					LogMessage(IInfoLog::ELevel::Error, ds.FailureReason);
				}
			}


			TArray<uint64> TrackIDsToParse;
			if (MKVParser->GetTrackByIndex(0))
			{
				TrackIDsToParse.Emplace(MKVParser->GetTrackByIndex(0)->GetID());
			}
			TSharedPtrTS<IParserMKV::IClusterParser> ClusterParser = MKVParser->CreateClusterParser(this, TrackIDsToParse, IParserMKV::EClusterParseFlags::ClusterParseFlag_AllowFullDocument);
			check(ClusterParser.IsValid());

			FAccessUnit* AccessUnit = nullptr;
			bool bIsParserError = false;
			bool bIsReadError = false;

			FTimeValue PTO(Request->Segment.PTO, Request->Segment.Timescale);
			FTimeValue EarliestPTS(Request->Segment.MediaLocalFirstAUTime, Request->Segment.Timescale, Request->TimestampSequenceIndex);
			EarliestPTS += TimeOffset - PTO;
			FTimeValue LastPTS;
			if (!bDoNotTruncateAtPresentationEnd && Request->Segment.MediaLocalLastAUTime != TNumericLimits<int64>::Max())
			{
				LastPTS.SetFromND(Request->Segment.MediaLocalLastAUTime, Request->Segment.Timescale, Request->TimestampSequenceIndex);
				LastPTS += TimeOffset - PTO;
			}
			else
			{
				LastPTS.SetToPositiveInfinity();
			}

			auto PrepareAccessUnit = [this, &AccessUnit](int64 NumToRead) -> void*
			{
				if (!AccessUnit)
				{
					AccessUnit = FAccessUnit::Create(Parameters.MemoryProvider);
					AccessUnit->AUSize = NumToRead;
					AccessUnit->AUData = AccessUnit->AllocatePayloadBuffer(AccessUnit->AUSize);
					return AccessUnit->AUData;
				}
				else
				{
					void* NewBuffer = AccessUnit->AllocatePayloadBuffer(AccessUnit->AUSize + NumToRead);
					FMemory::Memcpy(NewBuffer, AccessUnit->AUData, AccessUnit->AUSize);
					void* ReadTo = AdvancePointer(NewBuffer, AccessUnit->AUSize);
					AccessUnit->AdoptNewPayloadBuffer(NewBuffer, AccessUnit->AUSize + NumToRead);
					return ReadTo;
				}
			};

			bool bIsFirstFrame = true;

			while(!bDone && !HasErrored() && !HasReadBeenAborted())
			{
				IParserMKV::IClusterParser::EParseAction ParseAction = ClusterParser->NextParseAction();
				switch(ParseAction)
				{
					case IParserMKV::IClusterParser::EParseAction::ReadFrameData:
					{
						const IParserMKV::IClusterParser::IActionReadFrameData* Action = static_cast<const IParserMKV::IClusterParser::IActionReadFrameData*>(ClusterParser->GetAction());
						check(Action);

						int64 NumToRead = Action->GetNumBytesToRead();
						void* ReadTo = PrepareAccessUnit(NumToRead);
						int64 nr = ReadData(ReadTo, NumToRead);
						if (nr != NumToRead)
						{
							bHasErrored = true;
							bIsReadError = true;
						}
						break;
					}
					case IParserMKV::IClusterParser::EParseAction::FrameDone:
					{
						const IParserMKV::IClusterParser::IActionFrameDone* Action = static_cast<const IParserMKV::IClusterParser::IActionFrameDone*>(ClusterParser->GetAction());
						check(Action);
						if (AccessUnit)
						{
							// Do a test on the first frame's PTS to see if this is always zero, which
							// is indicative of a bad DASH segmenter for MKV/WEBM.
							if (bIsFirstFrame)
							{
								FTimeValue st(Request->Segment.Time, Request->Segment.Timescale, (int64)0);
								if (Action->GetPTS().IsZero() && !st.IsZero())
								{
									// If the delta is greater than 0.5 seconds
									if (Utils::AbsoluteValue(st.GetAsSeconds() - Action->GetPTS().GetAsSeconds()) >= 0.5 && !Request->bWarnedAboutTimescale)
									{
										Request->bWarnedAboutTimescale = true;
										LogMessage(IInfoLog::ELevel::Verbose, FString::Printf(TEXT("Cluster timestamp is zero while MPD time says it should be %#7.4f. Using MPD time as start value, but this may cause playback problems!"), st.GetAsSeconds()));
									}
									TimeOffset += st;
								}
								bIsFirstFrame = false;
							}


							AccessUnit->ESType = Request->GetType();
							AccessUnit->PTS = Action->GetPTS();
							AccessUnit->PTS += TimeOffset;
							AccessUnit->PTS.SetSequenceIndex(Request->TimestampSequenceIndex);
							AccessUnit->DTS = Action->GetDTS();
							AccessUnit->DTS += TimeOffset;
							AccessUnit->DTS.SetSequenceIndex(Request->TimestampSequenceIndex);
							AccessUnit->Duration = Action->GetDuration();
							if (Request->Segment.bFrameAccuracyRequired)
							{
								AccessUnit->EarliestPTS = EarliestPTS;
							}
							AccessUnit->LatestPTS = LastPTS;

							AccessUnit->bIsFirstInSequence = ActiveTrackData.bIsFirstInSequence;
							AccessUnit->bIsSyncSample = Action->IsKeyFrame();
							AccessUnit->bIsDummyData = false;
							AccessUnit->AUCodecData = ActiveTrackData.CSD;
							AccessUnit->BufferSourceInfo = ActiveTrackData.BufferSourceInfo;
							AccessUnit->DropState = FAccessUnit::EDropState::None;
							AccessUnit->SequenceIndex = Request->TimestampSequenceIndex;

							// VP9 codec?
							if (ActiveTrackData.CSD->ParsedInfo.GetCodec4CC() == Utils::Make4CC('v','p','0','9'))
							{
								// We cannot trust the keyframe indicator from the demuxer.
								ElectraDecodersUtil::VPxVideo::FVP9UncompressedHeader Header;
								if (ElectraDecodersUtil::VPxVideo::ParseVP9UncompressedHeader(Header, AccessUnit->AUData, AccessUnit->AUSize))
								{
									AccessUnit->bIsSyncSample = Header.IsKeyframe();
								}

								// Any additional data?
								const TMap<uint64, TArray<uint8>>& BlockAdditionalData = Action->GetBlockAdditionalData();
								// What type of additional data is there?
								if (BlockAdditionalData.Contains(4))	// VP9 ITU T.35 metadata?
								{
									AccessUnit->DynamicSidebandData = MakeUnique<TMap<uint32, TArray<uint8>>>();
									AccessUnit->DynamicSidebandData->Emplace(Utils::Make4CC('i','t','3','5'), BlockAdditionalData[4]);
								}
							}
							// VP8 codec?
							else if (ActiveTrackData.CSD->ParsedInfo.GetCodec4CC() == Utils::Make4CC('v','p','0','8'))
							{
								ElectraDecodersUtil::VPxVideo::FVP8UncompressedHeader Header;
								if (ElectraDecodersUtil::VPxVideo::ParseVP8UncompressedHeader(Header, AccessUnit->AUData, AccessUnit->AUSize))
								{
									AccessUnit->bIsSyncSample = Header.IsKeyframe();
								}
							}

							ActiveTrackData.bIsFirstInSequence = false;

							// Add to the track AU FIFO unless we already reached the last sample of the time range.
							if (!ActiveTrackData.bReadPastLastPTS)
							{
								ActiveTrackData.AddAccessUnit(AccessUnit);
							}
							ActiveTrackData.DurationSuccessfullyRead = ActiveTrackData.LargestPTS - ActiveTrackData.SmallestPTS + ActiveTrackData.AverageDuration;
						}
						FAccessUnit::Release(AccessUnit);
						AccessUnit = nullptr;
						break;
					}
					case IParserMKV::IClusterParser::EParseAction::SkipOver:
					{
						const IParserMKV::IClusterParser::IActionSkipOver* Action = static_cast<const IParserMKV::IClusterParser::IActionSkipOver*>(ClusterParser->GetAction());
						check(Action);
						int64 NumBytesToSkip = Action->GetNumBytesToSkip();
						int64 nr = ReadData(nullptr, NumBytesToSkip);
						if (nr != NumBytesToSkip)
						{
							bHasErrored = true;
							bIsReadError = true;
						}
						break;
					}
					case IParserMKV::IClusterParser::EParseAction::PrependData:
					{
						const IParserMKV::IClusterParser::IActionPrependData* Action = static_cast<const IParserMKV::IClusterParser::IActionPrependData*>(ClusterParser->GetAction());
						check(Action);
						int64 NumToRead = Action->GetPrependData().Num();
						void* ReadTo = PrepareAccessUnit(NumToRead);
						FMemory::Memcpy(ReadTo, Action->GetPrependData().GetData(), Action->GetPrependData().Num());
						break;
					}
					case IParserMKV::IClusterParser::EParseAction::DecryptData:
					{
						break;
					}
					case IParserMKV::IClusterParser::EParseAction::EndOfData:
					{
						bDone = true;
						break;
					}
					default:
					case IParserMKV::IClusterParser::EParseAction::Failure:
					{
						bIsParserError = true;
						bDone = true;
						break;
					}
				}

				// Shall we pass on any AUs we already read?
				if (bAllowEarlyEmitting)
				{
					EmitSamples(EEmitType::UntilBlocked, Request);
				}
			}
			// Any open access unit that we may have failed on we delete.
			FAccessUnit::Release(AccessUnit);
			AccessUnit = nullptr;

			ProgressListener.Reset();
			// Note: It is only safe to access the connection info when the HTTP request has completed or the request been removed.
			PlayerSessionService->GetHTTPManager()->RemoveRequest(HTTP, false);
			CurrentRequest->ConnectionInfo = HTTP->ConnectionInfo;
			HTTP.Reset();
		}
	}
	else
	{
		// Init segment error.
		if (!HasReadBeenAborted())
		{
			CurrentRequest->ConnectionInfo.StatusInfo.ErrorDetail = InitSegmentError;
			ds.bParseFailure = InitSegmentError.GetCode() == INTERNAL_ERROR_INIT_SEGMENT_PARSE_ERROR;
			bHasErrored = true;
		}
	}

	// Do we need to fill remaining duration with dummy data?
	if (bIsEmptyFillerSegment || bFillRemainingDuration)
	{
		// Inserting dummy data means this request was successful.
		CurrentRequest->ConnectionInfo.StatusInfo.Empty();
		CurrentRequest->ConnectionInfo.StatusInfo.HTTPStatus = Request->Segment.MediaURL.Range.Len() ? 206 : 200;
		ds.HTTPStatusCode = CurrentRequest->ConnectionInfo.StatusInfo.HTTPStatus;

		// Get the supposed segment duration.
		FTimeValue SegmentDurationToGo = SegmentDuration;
		FTimeValue PTO;
		PTO.SetFromND(Request->Segment.PTO, Request->Segment.Timescale);
		FTimeValue DiscardBefore = TimeOffset + FTimeValue(Request->Segment.MediaLocalFirstAUTime - Request->Segment.PTO, Request->Segment.Timescale);
		FTimeValue DiscardAfter = TimeOffset + FTimeValue(Request->Segment.MediaLocalLastAUTime - Request->Segment.PTO, Request->Segment.Timescale);

		FTimeValue DefaultDuration = ActiveTrackData.AverageDuration;
		if (!DefaultDuration.IsValid() || DefaultDuration.IsZero())
		{
			switch(Request->GetType())
			{
				case EStreamType::Video:
				{
					DefaultDuration.SetFromND(1, 60);
					break;
				}
				case EStreamType::Audio:
				{
					DefaultDuration.SetFromND(1024, ActiveTrackData.CSD.IsValid() && ActiveTrackData.CSD->ParsedInfo.GetSamplingRate() ? (uint32) ActiveTrackData.CSD->ParsedInfo.GetSamplingRate() : 48000U);
					break;
				}
				default:
				{
					DefaultDuration.SetFromND(1, 10);
					break;
				}
			}
		}

		SegmentDurationToGo -= ActiveTrackData.DurationSuccessfullyRead;
		ds.bInsertedFillerData = SegmentDurationToGo > FTimeValue::GetZero();
		FTimeValue NextExpectedPTS = ActiveTrackData.LargestPTS;
		if (!NextExpectedPTS.IsValid())
		{
			NextExpectedPTS.SetFromND(Request->Segment.Time - Request->Segment.PTO, Request->Segment.Timescale);
			NextExpectedPTS += TimeOffset;
			NextExpectedPTS.SetSequenceIndex(Request->TimestampSequenceIndex);
		}
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
			AccessUnit->BufferSourceInfo = ActiveTrackData.BufferSourceInfo;
			AccessUnit->Duration = DefaultDuration;
			AccessUnit->AUSize = 0;
			AccessUnit->AUData = nullptr;
			AccessUnit->bIsDummyData = true;
			if (ActiveTrackData.CSD.IsValid() && ActiveTrackData.CSD->CodecSpecificData.Num())
			{
				AccessUnit->AUCodecData = ActiveTrackData.CSD;
			}

			// Set the sequence index member and update all timestamps with it as well.
			AccessUnit->SequenceIndex = Request->TimestampSequenceIndex;
			AccessUnit->DTS = NextExpectedPTS;
			AccessUnit->PTS = NextExpectedPTS;
			AccessUnit->DTS.SetSequenceIndex(Request->TimestampSequenceIndex);
			AccessUnit->PTS.SetSequenceIndex(Request->TimestampSequenceIndex);
			AccessUnit->PTO = PTO;

			if (Request->Segment.bFrameAccuracyRequired)
			{
				AccessUnit->EarliestPTS = DiscardBefore;
				AccessUnit->EarliestPTS.SetSequenceIndex(Request->TimestampSequenceIndex);
			}
			AccessUnit->LatestPTS = DiscardAfter;
			AccessUnit->LatestPTS.SetSequenceIndex(Request->TimestampSequenceIndex);

			// Calculate the drop on the fragment local NextExpectedPTS/PTS.
			AccessUnit->DropState = FAccessUnit::EDropState::None;
			if (NextExpectedPTS + AccessUnit->Duration < DiscardBefore)
			{
				AccessUnit->DropState |= FAccessUnit::EDropState::TooEarly;
			}
			bool bIsLast = false;
			if (NextExpectedPTS > DiscardAfter)
			{
				AccessUnit->DropState |= FAccessUnit::EDropState::TooLate;
				bIsLast = AccessUnit->bIsLastInPeriod = true;
			}

			NextExpectedPTS += DefaultDuration;
			SegmentDurationToGo -= DefaultDuration;

			ActiveTrackData.AddAccessUnit(AccessUnit);
			FAccessUnit::Release(AccessUnit);
			AccessUnit = nullptr;

			if (bIsLast)
			{
				break;
			}
		}
	}

	// We got all the samples there are to get.
	ActiveTrackData.bGotAllSamples = true;
	// Emit all remaining pending AUs
	EmitSamples(EEmitType::AllRemaining, Request);
	ActiveTrackData.AccessUnitFIFO.Empty();
	ActiveTrackData.SortedAccessUnitFIFO.Empty();

	// If we did not set a specific failure message yet set the one from the downloader.
	if (ds.FailureReason.Len() == 0)
	{
		ds.FailureReason = CurrentRequest->ConnectionInfo.StatusInfo.ErrorDetail.GetMessage();
	}
	// If the ABR aborted this takes precedence in the failure message. Overwrite it.
	if (bAbortedByABR)
	{
		ds.FailureReason = ABRAbortReason;
	}
	// Set up remaining download stat fields.
	ds.bWasAborted  	  = bAbortedByABR;
	ds.bWasSuccessful     = !bHasErrored && !bAbortedByABR;
	ds.URL  			  = CurrentRequest->ConnectionInfo.EffectiveURL;
	ds.HTTPStatusCode     = CurrentRequest->ConnectionInfo.StatusInfo.HTTPStatus;
	ds.DurationDelivered  = ActiveTrackData.DurationSuccessfullyDelivered.GetAsSeconds();
	ds.DurationDownloaded = Utils::Max(Utils::Min(ds.Duration, ActiveTrackData.DurationSuccessfullyRead.GetAsSeconds()), ds.DurationDelivered);

	ds.TimeToFirstByte    = CurrentRequest->ConnectionInfo.TimeUntilFirstByte;
	ds.TimeToDownload     = (CurrentRequest->ConnectionInfo.RequestEndTime - CurrentRequest->ConnectionInfo.RequestStartTime).GetAsSeconds();
	ds.ByteSize 		  = CurrentRequest->ConnectionInfo.ContentLength;
	ds.NumBytesDownloaded = CurrentRequest->ConnectionInfo.BytesReadSoFar;
	ds.bIsCachedResponse  = CurrentRequest->ConnectionInfo.bIsCachedResponse;
	CurrentRequest->ConnectionInfo.GetTimingTraces(ds.TimingTraces);

	// Was this request for a segment that might potentially be missing and it did?
	if (Request->Segment.bMayBeMissing && (ds.HTTPStatusCode == 404 || ds.HTTPStatusCode == 416))
	{
		// This is not an actual error then. Pretend all was well.
		ds.bWasSuccessful = true;
		ds.HTTPStatusCode = 200;
		ds.bIsMissingSegment = true;
		ds.FailureReason.Empty();
		CurrentRequest->DownloadStats.bWasSuccessful = true;
		CurrentRequest->DownloadStats.HTTPStatusCode = 200;
		CurrentRequest->DownloadStats.bIsMissingSegment = true;
		CurrentRequest->DownloadStats.FailureReason.Empty();
		CurrentRequest->ConnectionInfo.StatusInfo.Empty();
		// Take note of the missing segment in the segment info as well so the search for the next segment
		// can return quicker.
		CurrentRequest->Segment.bIsMissing = true;
	}

	// If we had to wait for the segment to become available and we got a 404 back we might have been trying to fetch
	// the segment before the server made it available.
	if (Request->ASAST.IsValid() && (ds.HTTPStatusCode == 404 || ds.HTTPStatusCode == 416))
	{
		FTimeValue Now = PlayerSessionService->GetSynchronizedUTCTime()->GetTime();
		if ((ds.AvailibilityDelay = (Request->ASAST - Now).GetAsSeconds()) == 0.0)
		{
			// In the extremely unlikely event this comes out to zero exactly set a small value so the ABR knows there was a delay.
			ds.AvailibilityDelay = -0.01;
		}
	}

	// If we failed to get the segment and there is an inband DASH event stream which triggers MPD events and
	// we did not get such an event in the 'emsg' boxes, then we err on the safe side and assume this segment
	// would have carried an MPD update event and fire an artificial event.
	if (!ds.bWasSuccessful && CurrentRequest->Segment.InbandEventStreams.FindByPredicate([](const FManifestDASHInternal::FSegmentInformation::FInbandEventStream& This){ return This.SchemeIdUri.Equals(DASH::Schemes::ManifestEvents::Scheme_urn_mpeg_dash_event_2012); }))
	{
		if (SegmentEventsFound.IndexOfByPredicate([](const TSharedPtrTS<DASH::FPlayerEvent>& This){return This->GetSchemeIdUri().Equals(DASH::Schemes::ManifestEvents::Scheme_urn_mpeg_dash_event_2012);}) == INDEX_NONE)
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_DASH_StreamReader);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, DASH_StreamReader);
			TSharedPtrTS<DASH::FPlayerEvent> NewEvent = MakeSharedTS<DASH::FPlayerEvent>();
			NewEvent->SetOrigin(IAdaptiveStreamingPlayerAEMSEvent::EOrigin::InbandEventStream);
			NewEvent->SetSchemeIdUri(DASH::Schemes::ManifestEvents::Scheme_urn_mpeg_dash_event_2012);
			NewEvent->SetValue(TEXT("1"));
			NewEvent->SetID("$missed$");
			FTimeValue EPT((int64)CurrentRequest->Segment.Time, CurrentRequest->Segment.Timescale);
			FTimeValue PTO(CurrentRequest->Segment.PTO, CurrentRequest->Segment.Timescale);
			NewEvent->SetPresentationTime(TimeOffset - PTO + EPT);
			NewEvent->SetPeriodID(CurrentRequest->Period->GetUniqueIdentifier());
			PlayerSessionService->GetAEMSEventHandler()->AddEvent(NewEvent, CurrentRequest->Period->GetUniqueIdentifier(), IAdaptiveStreamingPlayerAEMSHandler::EEventAddMode::AddIfNotExists);
		}
	}

	// Clean out everything before reporting OnFragmentClose().
	TSharedPtrTS<FStreamSegmentRequestDASH> FinishedRequest = CurrentRequest;
	CurrentRequest.Reset();
	ReadBuffer.Reset();
	SegmentEventsFound.Empty();

	if (!bSilentCancellation)
	{
		StreamSelector->ReportDownloadEnd(ds);
		Parameters.EventListener->OnFragmentClose(FinishedRequest);
	}
}


void FStreamReaderDASH::FStreamHandler::UpdateAUDropState(FAccessUnit* InAU, const TSharedPtrTS<FStreamSegmentRequestDASH>& InRequest)
{
	if (InAU->PTS + InAU->Duration < InAU->EarliestPTS)
	{
		InAU->DropState |= FAccessUnit::EDropState::TooEarly;
	}
	if (InAU->PTS >= InAU->LatestPTS)
	{
		InAU->DropState |= FAccessUnit::EDropState::TooLate;
	}
}


FStreamReaderDASH::FStreamHandler::EEmitResult FStreamReaderDASH::FStreamHandler::EmitSamples(EEmitType InEmitType, const TSharedPtrTS<FStreamSegmentRequestDASH>& InRequest)
{
	EEmitResult Result = EEmitResult::SentNothing;

	while(ActiveTrackData.AccessUnitFIFO.Num() && !bTerminate && !HasReadBeenAborted())
	{
		if (ActiveTrackData.bNeedToRecalculateDurations)
		{
			// Need to have a certain amount of upcoming samples to be able to
			// (more or less) safely calculate timestamp differences.
			// NOTE: min to check depends on codec and B frame distance
			int32 NumToCheck = !ActiveTrackData.bGotAllSamples ? 10 : ActiveTrackData.AccessUnitFIFO.Num();
			if (ActiveTrackData.AccessUnitFIFO.Num() < NumToCheck)
			{
				break;
			}
			// Locate the sample in the time-sorted list
			for(int32 i=0; i<ActiveTrackData.SortedAccessUnitFIFO.Num(); ++i)
			{
				if (ActiveTrackData.SortedAccessUnitFIFO[i].PTS == ActiveTrackData.AccessUnitFIFO[0].PTS)
				{
					if (i < ActiveTrackData.SortedAccessUnitFIFO.Num()-1)
					{
						ActiveTrackData.AccessUnitFIFO[0].AU->Duration = ActiveTrackData.SortedAccessUnitFIFO[i+1].PTS - ActiveTrackData.SortedAccessUnitFIFO[i].PTS;
						UpdateAUDropState(ActiveTrackData.AccessUnitFIFO[0].AU, InRequest);
						if (!ActiveTrackData.AverageDuration.IsValid() || ActiveTrackData.AverageDuration.IsZero())
						{
							ActiveTrackData.AverageDuration = ActiveTrackData.AccessUnitFIFO[0].AU->Duration;
						}
					}
					else
					{
						if (InEmitType == EEmitType::KnownDurationOnly)
						{
							ActiveTrackData.bReachedEndOfKnownDuration = true;
							break;
						}
						else if (InEmitType == EEmitType::AllRemaining)
						{
							ActiveTrackData.AccessUnitFIFO[i].AU->Duration = ActiveTrackData.AverageDuration;
						}
					}
					ActiveTrackData.SortedAccessUnitFIFO[i].Release();
					break;
				}
			}
			// Reduce the sorted list
			for(int32 i=0; i<ActiveTrackData.SortedAccessUnitFIFO.Num(); ++i)
			{
				if (ActiveTrackData.SortedAccessUnitFIFO[i].AU)
				{
					if (i)
					{
						ActiveTrackData.SortedAccessUnitFIFO.RemoveAt(0, i);
					}
					break;
				}
			}
		}
		else
		{
			UpdateAUDropState(ActiveTrackData.AccessUnitFIFO[0].AU, InRequest);
		}

		// Could not recalculate the duration of the remaining samples, so we do not emit them now
		// and hold on to them until the next segment brings in more samples.
		if (ActiveTrackData.bReachedEndOfKnownDuration)
		{
			break;
		}

		FAccessUnit* pNext = ActiveTrackData.AccessUnitFIFO[0].AU;
		// Check if this is the last access unit in the requested time range.
		if (!ActiveTrackData.bTaggedLastSample && pNext->PTS >= pNext->LatestPTS)
		{
			/*
				Because of B frames the last frame that must be decoded could actually be
				a later frame in decode order.
				Suppose the sequence IPBB with timestamps 0,3,1,2 respectively. Even though the
				P frame with timestamp 3 is "the last" one in presentation order, it will enter
				the decoder before the B frames.
				As such we need to tag the last B frame (2) as "the last one" even though its timestamp
				is before the last time requested.
				This would be easy if we had access to reliable DTS, but Matroska files only provide PTS.
				Note: This may seem superfluous since we are tagging as "last" which happens to be the
				      actual last element in the list, but there could really be even later frames in
				      the list that we will then remove to avoid sending frames into the decoder that
				      will be discarded after decoding, which is a waste of decode cycles.
			*/

			const FTimeValue NextPTS(pNext->PTS);
			// Sort the remaining access units by ascending PTS
			ActiveTrackData.AccessUnitFIFO.Sort([](const FActiveTrackData::FSample& a, const FActiveTrackData::FSample& b){return a.PTS < b.PTS;});
			// Go backwards over the list and drop all access units that _follow_ the next one.
			for(int32 i=ActiveTrackData.AccessUnitFIFO.Num()-1; i>0; --i)
			{
				if (ActiveTrackData.AccessUnitFIFO[i].PTS > NextPTS)
				{
					for(int32 j=0; j<ActiveTrackData.SortedAccessUnitFIFO.Num(); ++j)
					{
						if (ActiveTrackData.SortedAccessUnitFIFO[j].PTS == ActiveTrackData.AccessUnitFIFO[i].PTS)
						{
							ActiveTrackData.SortedAccessUnitFIFO.RemoveAt(j);
							break;
						}
					}
					ActiveTrackData.AccessUnitFIFO.RemoveAt(i);
				}
				else
				{
					break;
				}
			}
			// Sort the list back to index order.
			ActiveTrackData.AccessUnitFIFO.Sort([](const FActiveTrackData::FSample& a, const FActiveTrackData::FSample& b){return a.SequentialIndex < b.SequentialIndex;});
			// Whichever element is the last in the list now is the one that needs to be tagged as such.
			ActiveTrackData.AccessUnitFIFO.Last().AU->bIsLastInPeriod = true;
			ActiveTrackData.bReadPastLastPTS = true;
			ActiveTrackData.bTaggedLastSample = true;

			check(pNext == ActiveTrackData.AccessUnitFIFO[0].AU);
			pNext = ActiveTrackData.AccessUnitFIFO[0].AU;
		}

		while(!bTerminate && !HasReadBeenAborted())
		{
			if (Parameters.EventListener->OnFragmentAccessUnitReceived(pNext))
			{
				ActiveTrackData.DurationSuccessfullyDelivered += pNext->Duration;
				ActiveTrackData.AccessUnitFIFO[0].AU = nullptr;
				ActiveTrackData.AccessUnitFIFO.RemoveAt(0);
				Result = Result == EEmitResult::SentNothing ? EEmitResult::Sent : Result;
				break;
			}
			// If emitting as much as we can we leave this loop now that the receiver is blocked.
			else if (InEmitType == EEmitType::UntilBlocked)
			{
				return Result;
			}
			else
			{
				FMediaRunnable::SleepMilliseconds(100);
			}
		}
	}

	// At EOS?
	Result = ActiveTrackData.bReadPastLastPTS ? EEmitResult::AllReachedEOS : Result;
	return Result;
}


bool FStreamReaderDASH::FStreamHandler::HasErrored() const
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
int64 FStreamReaderDASH::FStreamHandler::ReadData(void* IntoBuffer, int64 NumBytesToRead)
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
				Metrics::FSegmentDownloadStats& currentDownloadStats = CurrentRequest->DownloadStats;
				currentDownloadStats.DurationDelivered = ActiveTrackData.DurationSuccessfullyDelivered.GetAsSeconds();
				currentDownloadStats.DurationDownloaded = ActiveTrackData.DurationSuccessfullyRead.GetAsSeconds();
				currentDownloadStats.TimeToDownload = (MEDIAutcTime::Current() - CurrentRequest->ConnectionInfo.RequestStartTime).GetAsSeconds();
				FABRDownloadProgressDecision StreamSelectorDecision = StreamSelector->ReportDownloadProgress(currentDownloadStats);
				MetricUpdateLock.Unlock();

				if ((StreamSelectorDecision.Flags & FABRDownloadProgressDecision::EDecisionFlags::eABR_EmitPartialData) != 0)
				{
					SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_DASH_StreamReader);
					CSV_SCOPED_TIMING_STAT(ElectraPlayer, DASH_StreamReader);
					bAllowEarlyEmitting = true;
					// Deliver all enqueued AUs right now. Unless the request also gets aborted we could be stuck
					// in here for a while longer.
					EmitSamples(EEmitType::UntilBlocked, CurrentRequest);
				}
				if ((StreamSelectorDecision.Flags & FABRDownloadProgressDecision::EDecisionFlags::eABR_InsertFillerData) != 0)
				{
					bFillRemainingDuration = true;
				}
				if ((StreamSelectorDecision.Flags & FABRDownloadProgressDecision::EDecisionFlags::eABR_AbortDownload) != 0)
				{
					// When aborted and early emitting did place something into the buffers we need to fill
					// the remainder no matter what.
					if (ActiveTrackData.DurationSuccessfullyDelivered > FTimeValue::GetZero())
					{
						bFillRemainingDuration = true;
					}
					ABRAbortReason = StreamSelectorDecision.Reason;
					bAbortedByABR = true;
					return -1;
				}
			}
		}

		if (!SourceBuffer.WaitUntilSizeAvailable(ReadBuffer.ParsePos + NumBytesToRead, 1000 * 100))
		{
			if (HasErrored() || HasReadBeenAborted() || SourceBuffer.WasAborted())
			{
				return -1;
			}
		}
		else
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_DASH_StreamReader);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, DASH_StreamReader);
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
	return -1;
}

/**
 * Checks if the data source has reached the End Of File (EOF) and cannot provide any additional data.
 *
 * @return If EOF has been reached returns true, otherwise false.
 */
bool FStreamReaderDASH::FStreamHandler::HasReachedEOF() const
{
	const FWaitableBuffer& SourceBuffer = ReadBuffer.ReceiveBuffer->Buffer;
	return !HasErrored() && SourceBuffer.GetEOD() && (ReadBuffer.ParsePos >= SourceBuffer.Num() || ReadBuffer.ParsePos >= ReadBuffer.MaxParsePos);
}

/**
 * Checks if reading of the file and therefor parsing has been aborted.
 *
 * @return true if reading/parsing has been aborted, false otherwise.
 */
bool FStreamReaderDASH::FStreamHandler::HasReadBeenAborted() const
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
int64 FStreamReaderDASH::FStreamHandler::GetCurrentOffset() const
{
	return ReadBuffer.ParsePos;
}


IParserISO14496_12::IBoxCallback::EParseContinuation FStreamReaderDASH::FStreamHandler::OnFoundBox(IParserISO14496_12::FBoxType Box, int64 BoxSizeInBytes, int64 FileDataOffset, int64 BoxDataOffset)
{
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

IParserISO14496_12::IBoxCallback::EParseContinuation FStreamReaderDASH::FStreamHandler::OnEndOfBox(IParserISO14496_12::FBoxType Box, int64 BoxSizeInBytes, int64 FileDataOffset, int64 BoxDataOffset)
{
	return IParserISO14496_12::IBoxCallback::EParseContinuation::Continue;
}



int64 FStreamReaderDASH::FStreamHandler::MKVReadData(void* InDestinationBuffer, int64 InNumBytesToRead, int64 InFromOffset)
{
	check(InFromOffset == MKVGetCurrentFileOffset());
	if (InFromOffset != MKVGetCurrentFileOffset())
	{
		return -1;
	}
	return ReadData(InDestinationBuffer, InNumBytesToRead);
}

int64 FStreamReaderDASH::FStreamHandler::MKVGetCurrentFileOffset() const
{
	return GetCurrentOffset();
}

int64 FStreamReaderDASH::FStreamHandler::MKVGetTotalSize()
{
	if (CurrentRequest.IsValid())
	{
		Metrics::FSegmentDownloadStats& ds = CurrentRequest->DownloadStats;
		if (ds.ByteSize > 0)
		{
			return ds.ByteSize;
		}
	}
	return TNumericLimits<int64>::Max();
}

bool FStreamReaderDASH::FStreamHandler::MKVHasReadBeenAborted() const
{
	return HasReadBeenAborted();
}



} // namespace Electra

