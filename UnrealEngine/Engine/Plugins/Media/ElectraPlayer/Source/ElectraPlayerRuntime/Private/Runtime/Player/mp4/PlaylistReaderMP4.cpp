// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCore.h"
#include "Player/Manifest.h"
#include "HTTP/HTTPManager.h"
#include "Demuxer/ParserISO14496-12.h"
#include "SynchronizedClock.h"
#include "PlaylistReaderMP4.h"
#include "Utilities/HashFunctions.h"
#include "Utilities/TimeUtilities.h"
#include "HAL/LowLevelMemTracker.h"
#include "ElectraPlayerPrivate.h"
#include "Player/AdaptiveStreamingPlayerResourceRequest.h"
#include "Player/PlayerStreamReader.h"
#include "Player/mp4/ManifestMP4.h"
#include "Player/mp4/OptionKeynamesMP4.h"

#define ERRCODE_MP4_INVALID_FILE	1
#define ERRCODE_MP4_DOWNLOAD_ERROR	2


DECLARE_CYCLE_STAT(TEXT("FPlaylistReaderMP4_WorkerThread"), STAT_ElectraPlayer_MP4_PlaylistWorker, STATGROUP_ElectraPlayer);


namespace Electra
{

/**
 * This class is responsible for downloading the mp4 non-mdat boxes and parsing them.
 */
class FPlaylistReaderMP4 : public IPlaylistReaderMP4, public IParserISO14496_12::IReader, public IParserISO14496_12::IBoxCallback, public FMediaThread
{
public:
	FPlaylistReaderMP4();
	void Initialize(IPlayerSessionServices* PlayerSessionServices);

	virtual ~FPlaylistReaderMP4();

	virtual void Close() override;
	virtual void HandleOnce() override;

	/**
	 * Returns the type of playlist format.
	 * For this implementation it will be "mp4".
	 *
	 * @return "mp4" to indicate this is an mp4 file.
	 */
	virtual const FString& GetPlaylistType() const override
	{
		static FString Type("mp4");
		return Type;
	}

	/**
	 * Loads and parses the playlist.
	 *
	 * @param URL     URL of the playlist to load
	 */
	virtual void LoadAndParse(const FString& URL) override;

	/**
	 * Returns the URL from which the playlist was loaded (or supposed to be loaded).
	 *
	 * @return The playlist URL
	 */
	virtual FString GetURL() const override;

	/**
	 * Returns an interface to the manifest created from the loaded mp4 playlists.
	 *
	 * @return A shared manifest interface pointer.
	 */
	virtual TSharedPtrTS<IManifest> GetManifest() override;

private:
	// Methods from IParserISO14496_12::IReader
	virtual int64 ReadData(void* IntoBuffer, int64 NumBytesToRead) override;
	virtual bool HasReachedEOF() const override;
	virtual bool HasReadBeenAborted() const override;
	virtual int64 GetCurrentOffset() const override;
	// Methods from IParserISO14496_12::IBoxCallback
	virtual IParserISO14496_12::IBoxCallback::EParseContinuation OnFoundBox(IParserISO14496_12::FBoxType Box, int64 BoxSizeInBytes, int64 FileDataOffset, int64 BoxDataOffset) override;
	virtual IParserISO14496_12::IBoxCallback::EParseContinuation OnEndOfBox(IParserISO14496_12::FBoxType Box, int64 BoxSizeInBytes, int64 FileDataOffset, int64 BoxDataOffset) override;

	void StartWorkerThread();
	void StopWorkerThread();
	void WorkerThread();

	void PostError(const FString& Message, uint16 Code, UEMediaError Error = UEMEDIA_ERROR_OK);
	void LogMessage(IInfoLog::ELevel Level, const FString& Message);

	FErrorDetail CreateErrorAndLog(const FString& Message, uint16 Code, UEMediaError Error = UEMEDIA_ERROR_OK);

	int32 HTTPProgressCallback(const IElectraHttpManager::FRequest* Request);
	void HTTPCompletionCallback(const IElectraHttpManager::FRequest* Request);

	void ClearRequest();

	void ReadNextChunk(int64 InFromOffset, int64 ChunkSize);

	bool HasErrored() const
	{
		return bHasErrored;
	}

	const int32 kChunkReadSize = 65536;

	IPlayerSessionServices*									PlayerSessionServices = nullptr;
	FString													MasterPlaylistURL;
	FString													URLFragment;
	FMediaEvent												WorkerThreadQuitSignal;
	bool													bIsWorkerThreadStarted = false;

	FCriticalSection										Lock;
	TSharedPtrTS<IElectraHttpManager::FRequest> 			Request;
	TSharedPtrTS<IElectraHttpManager::FReceiveBuffer> 		ReceiveBuffer;
	TSharedPtrTS<IElectraHttpManager::FProgressListener>	ProgressListener;
	HTTP::FConnectionInfo									ConnectionInfo;
	FWaitableBuffer 										Buffer;
	int64													ParsePos = 0;
	int64													ChunkReadOffset = 0;
	int64													FileSize = -1;
	bool													bChunkReadInProgress = false;

	bool													bAbort = false;
	bool													bHasErrored = false;

	TSharedPtrTS<IParserISO14496_12>						MP4Parser;
	bool													bFoundBoxFTYP = false;
	bool													bFoundBoxMOOV = false;
	bool													bFoundBoxSIDX = false;
	bool													bFoundBoxMOOF = false;
	bool													bFoundBoxMDAT = false;
	bool													bIsFastStartable = false;

	TSharedPtrTS<FManifestMP4Internal>						Manifest;
	FErrorDetail											LastErrorDetail;
};


/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

TSharedPtrTS<IPlaylistReader> IPlaylistReaderMP4::Create(IPlayerSessionServices* PlayerSessionServices)
{
	TSharedPtrTS<FPlaylistReaderMP4> PlaylistReader = MakeSharedTS<FPlaylistReaderMP4>();
	if (PlaylistReader)
	{
		PlaylistReader->Initialize(PlayerSessionServices);
	}
	return PlaylistReader;
}

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/


FPlaylistReaderMP4::FPlaylistReaderMP4()
	: FMediaThread("ElectraPlayer::MP4 Playlist")
{
}

FPlaylistReaderMP4::~FPlaylistReaderMP4()
{
	Close();
}

FString FPlaylistReaderMP4::GetURL() const
{
	return MasterPlaylistURL;
}

TSharedPtrTS<IManifest> FPlaylistReaderMP4::GetManifest()
{
	return Manifest;
}

void FPlaylistReaderMP4::Initialize(IPlayerSessionServices* InPlayerSessionServices)
{
	PlayerSessionServices = InPlayerSessionServices;
	Buffer.Reserve(kChunkReadSize);
}

void FPlaylistReaderMP4::Close()
{
	bAbort = true;
	ClearRequest();

	StopWorkerThread();
}

void FPlaylistReaderMP4::HandleOnce()
{
	// No-op. This class is using a dedicated thread to read data from the stream
	// which can stall at any moment and thus not lend itself to a tickable instance.
}

void FPlaylistReaderMP4::StartWorkerThread()
{
	check(!bIsWorkerThreadStarted);
	ThreadStart(FMediaRunnable::FStartDelegate::CreateRaw(this, &FPlaylistReaderMP4::WorkerThread));
	bIsWorkerThreadStarted = true;
}

void FPlaylistReaderMP4::StopWorkerThread()
{
	if (bIsWorkerThreadStarted)
	{
		WorkerThreadQuitSignal.Signal();
		ThreadWaitDone();
		ThreadReset();
		bIsWorkerThreadStarted = false;
	}
}

void FPlaylistReaderMP4::PostError(const FString& InMessage, uint16 InCode, UEMediaError InError)
{
	LastErrorDetail.Clear();
	LastErrorDetail.SetError(InError != UEMEDIA_ERROR_OK ? InError : UEMEDIA_ERROR_DETAIL);
	LastErrorDetail.SetFacility(Facility::EFacility::MP4PlaylistReader);
	LastErrorDetail.SetCode(InCode);
	LastErrorDetail.SetMessage(InMessage);
	check(PlayerSessionServices);
	if (PlayerSessionServices)
	{
		PlayerSessionServices->PostError(LastErrorDetail);
	}
}

FErrorDetail FPlaylistReaderMP4::CreateErrorAndLog(const FString& InMessage, uint16 InCode, UEMediaError InError)
{
	FErrorDetail err;
	err.SetError(InError != UEMEDIA_ERROR_OK ? InError : UEMEDIA_ERROR_DETAIL);
	err.SetFacility(Facility::EFacility::MP4PlaylistReader);
	err.SetCode(InCode);
	err.SetMessage(InMessage);
	check(PlayerSessionServices);
	if (PlayerSessionServices)
	{
		PlayerSessionServices->PostLog(Facility::EFacility::MP4PlaylistReader, IInfoLog::ELevel::Error, err.GetPrintable());
	}
	return err;
}

void FPlaylistReaderMP4::LogMessage(IInfoLog::ELevel Level, const FString& Message)
{
	if (PlayerSessionServices)
	{
		PlayerSessionServices->PostLog(Facility::EFacility::MP4PlaylistReader, Level, Message);
	}
}

void FPlaylistReaderMP4::LoadAndParse(const FString& URL)
{
	FURL_RFC3986 UrlParser;
	UrlParser.Parse(URL);
	MasterPlaylistURL = UrlParser.Get(true, false);
	URLFragment = UrlParser.GetFragment();

	StartWorkerThread();
}

int32 FPlaylistReaderMP4::HTTPProgressCallback(const IElectraHttpManager::FRequest* InRequest)
{
	// Aborted?
	return bAbort ? 1 : 0;
}

void FPlaylistReaderMP4::HTTPCompletionCallback(const IElectraHttpManager::FRequest* InRequest)
{
	bool bFailed = InRequest->ConnectionInfo.StatusInfo.ErrorDetail.IsError();
	ConnectionInfo = InRequest->ConnectionInfo;
	if (!bFailed)
	{
		// Set the size of the resource if we don't have it yet.
		if (FileSize < 0)
		{
			ElectraHTTPStream::FHttpRange crh;
			if (crh.ParseFromContentRangeResponse(InRequest->ConnectionInfo.ContentRangeHeader))
			{
				FileSize = crh.GetDocumentSize();
			}
		}

		if (ConnectionInfo.EffectiveURL.Len())
		{
			MasterPlaylistURL = ConnectionInfo.EffectiveURL;
		}

		// Copy the read data across.
		int32 NumRead = ReceiveBuffer->Buffer.Num();
		Buffer.PushData(ReceiveBuffer->Buffer.GetLinearReadData(), NumRead);
		if (FileSize >= 0 && ChunkReadOffset + NumRead >= FileSize)
		{
			Buffer.SetEOD();
		}
	}
	bHasErrored = bFailed;
	bChunkReadInProgress = false;
}

void FPlaylistReaderMP4::ClearRequest()
{
	FScopeLock lock(&Lock);
	if (Request.IsValid())
	{
		PlayerSessionServices->GetHTTPManager()->RemoveRequest(Request, false);
		Request.Reset();
	}
	ProgressListener.Reset();
	ReceiveBuffer.Reset();
}

void FPlaylistReaderMP4::ReadNextChunk(int64 InFromOffset, int64 ChunkSize)
{
	ClearRequest();

	FScopeLock lock(&Lock);
	// Asked to go beyond the size of the file?
	ChunkReadOffset = InFromOffset;
	if (FileSize >= 0 && InFromOffset >= FileSize)
	{
		Buffer.SetEOD();
		return;
	}
	Buffer.Reset();
	ProgressListener = MakeSharedTS<IElectraHttpManager::FProgressListener>();
	ProgressListener->CompletionDelegate = IElectraHttpManager::FProgressListener::FCompletionDelegate::CreateRaw(this, &FPlaylistReaderMP4::HTTPCompletionCallback);
	ProgressListener->ProgressDelegate   = IElectraHttpManager::FProgressListener::FProgressDelegate::CreateRaw(this, &FPlaylistReaderMP4::HTTPProgressCallback);

	ReceiveBuffer = MakeSharedTS<IElectraHttpManager::FReceiveBuffer>();
	ReceiveBuffer->Buffer.Reserve(ChunkSize);

	const FParamDict& Options = PlayerSessionServices->GetOptions();

	Request = MakeSharedTS<IElectraHttpManager::FRequest>();
	Request->Parameters.URL = MasterPlaylistURL;
	Request->Parameters.Range.SetStart(InFromOffset);
	int64 LastByte = InFromOffset + ChunkSize - 1;
	if (FileSize >= 0 && LastByte > FileSize-1)
	{
		LastByte = FileSize - 1;
	}
	Request->Parameters.Range.SetEndIncluding(LastByte);
	Request->Parameters.ConnectTimeout = Options.GetValue(MP4::OptionKeyMP4LoadConnectTimeout).SafeGetTimeValue(FTimeValue().SetFromMilliseconds(1000 * 8));
	Request->Parameters.NoDataTimeout = Options.GetValue(MP4::OptionKeyMP4LoadNoDataTimeout).SafeGetTimeValue(FTimeValue().SetFromMilliseconds(1000 * 6));
	Request->ReceiveBuffer = ReceiveBuffer;
	Request->ProgressListener = ProgressListener;
	Request->ResponseCache = PlayerSessionServices->GetHTTPResponseCache();
	PlayerSessionServices->GetHTTPManager()->AddRequest(Request, false);
}




void FPlaylistReaderMP4::WorkerThread()
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);

	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_MP4_PlaylistWorker);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, MP4_PlaylistWorker);

	MP4Parser = IParserISO14496_12::CreateParser();
	UEMediaError parseError = MP4Parser->ParseHeader(this, this, PlayerSessionServices, nullptr);
	ClearRequest();

	if (parseError != UEMEDIA_ERROR_ABORTED)
	{
		// Notify the download of the "master playlist". This indicates the download only, not the parsing thereof.
		PlayerSessionServices->SendMessageToPlayer(IPlaylistReader::PlaylistDownloadMessage::Create(&ConnectionInfo, Playlist::EListType::Master, Playlist::ELoadType::Initial));
		// Notify that the "master playlist" has been parsed, successfully or not.
		PlayerSessionServices->SendMessageToPlayer(IPlaylistReader::PlaylistLoadedMessage::Create(LastErrorDetail, &ConnectionInfo, Playlist::EListType::Master, Playlist::ELoadType::Initial));

		if (parseError == UEMEDIA_ERROR_OK || parseError == UEMEDIA_ERROR_END_OF_STREAM)
		{
			// See that we have parsed all the boxes we need.
			if (bFoundBoxFTYP && bFoundBoxMOOV)
			{
				if (!bIsFastStartable)
				{
					LogMessage(IInfoLog::ELevel::Info, FString::Printf(TEXT("The mp4 at \"%s\" is not fast-startable. Consider moving the 'moov' box in front of the 'mdat' for faster startup times."), *ConnectionInfo.EffectiveURL));
				}

				// Prepare the tracks in the stream that are of a supported codec.
				parseError = MP4Parser->PrepareTracks(PlayerSessionServices, TSharedPtrTS<const IParserISO14496_12>());
				if (parseError == UEMEDIA_ERROR_OK)
				{
					Manifest = MakeSharedTS<FManifestMP4Internal>(PlayerSessionServices);

					TArray<FURL_RFC3986::FQueryParam> URLFragmentComponents;
					FURL_RFC3986::GetQueryParams(URLFragmentComponents, URLFragment, false);	// The fragment is already URL escaped, so no need to do it again.
					Manifest->SetURLFragmentComponents(MoveTemp(URLFragmentComponents));

					LastErrorDetail = Manifest->Build(MP4Parser, MasterPlaylistURL, ConnectionInfo);

					// Notify that the "variant playlists" are ready. There are no variants in an mp4, but this is the trigger that the playlists are all set up and are good to go now.
					PlayerSessionServices->SendMessageToPlayer(IPlaylistReader::PlaylistLoadedMessage::Create(LastErrorDetail, &ConnectionInfo, Playlist::EListType::Variant, Playlist::ELoadType::Initial));
				}
				else
				{
					PostError(FString::Printf(TEXT("Failed to parse tracks in mp4 \"%s\" with error %u"), *ConnectionInfo.EffectiveURL, parseError), ERRCODE_MP4_INVALID_FILE, UEMEDIA_ERROR_FORMAT_ERROR);
				}
			}
			else
			{
				// No moov box usually means this is not a fast-start file.
				PostError(FString::Printf(TEXT("No moov box found in \"%s\". This is not a valid file."), *ConnectionInfo.EffectiveURL), ERRCODE_MP4_INVALID_FILE, UEMEDIA_ERROR_FORMAT_ERROR);
			}
		}
		else
		{
			// See if there was a download error
			if (ConnectionInfo.StatusInfo.ErrorDetail.IsError())
			{
				PostError(FString::Printf(TEXT("%s while downloading \"%s\""), *ConnectionInfo.StatusInfo.ErrorDetail.GetMessage(), *ConnectionInfo.EffectiveURL), ERRCODE_MP4_DOWNLOAD_ERROR, UEMEDIA_ERROR_READ_ERROR);
			}
			else
			{
				PostError(FString::Printf(TEXT("Failed to parse mp4 \"%s\" with error %u"), *ConnectionInfo.EffectiveURL, parseError), ERRCODE_MP4_INVALID_FILE, UEMEDIA_ERROR_FORMAT_ERROR);
			}
		}
	}

	// This thread's work is done. We only wait for termination now.
	WorkerThreadQuitSignal.Wait();
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
int64 FPlaylistReaderMP4::ReadData(void* IntoBuffer, int64 NumBytesToRead)
{
	uint8* OutputBuffer = (uint8*)IntoBuffer;
	// Do we have enough data in the buffer to satisfy the read?
	if (Buffer.Num() >= NumBytesToRead)
	{
		// Yes. Get the data and return.
		int32 NumGot = Buffer.PopData(OutputBuffer, NumBytesToRead);
		check(NumGot == NumBytesToRead);
		ParsePos += NumBytesToRead;
		return NumBytesToRead;
	}
	else
	{
		// Do not have enough data yet or we want to read more than the buffer can hold?
		int32 NumBytesToGo = NumBytesToRead;
		int64 NextChunkReadOffset = ParsePos;
		while(NumBytesToGo > 0)
		{
			if (bHasErrored || bAbort)
			{
				return -1;
			}
			// EOD?
			if (Buffer.IsEndOfData())
			{
				return 0;
			}

			// Get whatever amount of data is currently available to free up the buffer for receiving more data.
			int32 NumGot = Buffer.PopData(OutputBuffer, NumBytesToGo);
			if ((NumBytesToGo -= NumGot) > 0)
			{
				if (OutputBuffer)
				{
					OutputBuffer += NumGot;
				}
				// Trigger read of next chunk of data.
				if (!bChunkReadInProgress)
				{
					check(Buffer.IsEmpty());
					// Is the data to read actually used or is it skipped over?
					if (OutputBuffer)
					{
						NextChunkReadOffset += NumGot;
						ReadNextChunk(NextChunkReadOffset, kChunkReadSize);
						bChunkReadInProgress = true;
					}
					else
					{
						// Data is not used, so do not request it.
						break;
					}
				}
				// Wait for data to arrive in the ringbuffer.
				int32 WaitForBytes = NumBytesToGo > Buffer.Capacity() ? Buffer.Capacity() : NumBytesToGo;
				Buffer.WaitUntilSizeAvailable(WaitForBytes, 1000 * 100);
			}
		}
		ParsePos += NumBytesToRead;
		return NumBytesToRead;
	}
}

/**
 * Checks if the data source has reached the End Of File (EOF) and cannot provide any additional data.
 *
 * @return If EOF has been reached returns true, otherwise false.
 */
bool FPlaylistReaderMP4::HasReachedEOF() const
{
	return Buffer.IsEndOfData();
}

/**
 * Checks if reading of the file and therefor parsing has been aborted.
 *
 * @return true if reading/parsing has been aborted, false otherwise.
 */
bool FPlaylistReaderMP4::HasReadBeenAborted() const
{
	return bAbort;
}

/**
 * Returns the current read offset.
 *
 * The first read offset is not necessarily zero. It could be anywhere inside the source.
 *
 * @return The current byte offset in the source.
 */
int64 FPlaylistReaderMP4::GetCurrentOffset() const
{
	return ParsePos;
}


IParserISO14496_12::IBoxCallback::EParseContinuation FPlaylistReaderMP4::OnFoundBox(IParserISO14496_12::FBoxType Box, int64 BoxSizeInBytes, int64 FileDataOffset, int64 BoxDataOffset)
{
	// We require the very first box to be an 'ftyp' box.
	if (FileDataOffset == 0 && Box != IParserISO14496_12::BoxType_ftyp)
	{
		PostError("Invalid mp4 file: first box is not 'ftyp'", ERRCODE_MP4_INVALID_FILE, UEMEDIA_ERROR_FORMAT_ERROR);
		return IParserISO14496_12::IBoxCallback::EParseContinuation::Stop;
	}

	// Check which box is being parsed next.
	switch(Box)
	{
		case IParserISO14496_12::BoxType_ftyp:
			bFoundBoxFTYP = true;
			break;
		case IParserISO14496_12::BoxType_moov:
			bFoundBoxMOOV = true;
			bIsFastStartable = !bFoundBoxMDAT;
			break;
		case IParserISO14496_12::BoxType_sidx:
			bFoundBoxSIDX = true;
			break;
		case IParserISO14496_12::BoxType_moof:
			bFoundBoxMOOF = true;
			return IParserISO14496_12::IBoxCallback::EParseContinuation::Stop;
		case IParserISO14496_12::BoxType_mdat:
			bFoundBoxMDAT = true;
			break;
		default:
			break;
	}
	return IParserISO14496_12::IBoxCallback::EParseContinuation::Continue;
}

IParserISO14496_12::IBoxCallback::EParseContinuation FPlaylistReaderMP4::OnEndOfBox(IParserISO14496_12::FBoxType Box, int64 BoxSizeInBytes, int64 FileDataOffset, int64 BoxDataOffset)
{
	if (bFoundBoxMOOV)
	{
		return IParserISO14496_12::IBoxCallback::EParseContinuation::Stop;
	}
	return IParserISO14496_12::IBoxCallback::EParseContinuation::Continue;
}


} // namespace Electra


