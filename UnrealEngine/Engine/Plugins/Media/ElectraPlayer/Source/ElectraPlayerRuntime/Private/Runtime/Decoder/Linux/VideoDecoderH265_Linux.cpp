// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/Platform.h"

#include "PlayerCore.h"
#include "StreamAccessUnitBuffer.h"
#include "Decoder/VideoDecoderH265.h"
#include "Renderer/RendererBase.h"
#include "Player/PlayerSessionServices.h"
#include "Utilities/StringHelpers.h"
#include "HAL/LowLevelMemTracker.h"
#include "ElectraPlayerPrivate.h"
#include "DecoderErrors_Linux.h"

#include "Linux/MediaVideoDecoderOutputLinux.h"
#include "Renderer/RendererVideo.h"
#include "ElectraVideoDecoder_Linux.h"

#include "libav_Decoder_Common.h"
#include "libav_Decoder_H265.h"

/***************************************************************************************************************************************************/

DECLARE_CYCLE_STAT(TEXT("FVideoDecoderH265::Decode()"), STAT_ElectraPlayer_VideoH265Decode, STATGROUP_ElectraPlayer);
DECLARE_CYCLE_STAT(TEXT("FVideoDecoderH265::ConvertOutput()"), STAT_ElectraPlayer_VideoH265ConvertOutput, STATGROUP_ElectraPlayer);

namespace Electra
{

/**
 * H265 video decoder class implementation.
**/
class FVideoDecoderH265LinuxLibavcodec : public IVideoDecoderH265, public FMediaThread, public ILibavDecoderVideoResourceAllocator
{
public:
	static bool Startup(const FParamDict& Options);
	static void Shutdown();

	FVideoDecoderH265LinuxLibavcodec();
	virtual ~FVideoDecoderH265LinuxLibavcodec();

	void SetPlayerSessionServices(IPlayerSessionServices* SessionServices) override;

	void Open(const FInstanceConfiguration& InConfig) override;
	void Close() override;
	void DrainForCodecChange() override;

	void SetMaximumDecodeCapability(int32 MaxTier, int32 MaxWidth, int32 MaxHeight, int32 MaxProfile, int32 MaxProfileLevel, const FParamDict& AdditionalOptions) override;

	void SetAUInputBufferListener(IAccessUnitBufferListener* Listener) override;

	void SetReadyBufferListener(IDecoderOutputBufferListener* Listener) override;

	void SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer) override;

    void SetResourceDelegate(const TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>& ResourceDelegate) override;

	void AUdataPushAU(FAccessUnit* AccessUnit) override;
	void AUdataPushEOD() override;
	void AUdataClearEOD() override;
	void AUdataFlushEverything() override;

private:
	// Add methods from ILibavDecoderVideoResourceAllocator here
	// ...

	enum
	{
		MaxPendingOutputImages = 2
	};

	enum EDecodeResult
	{
		Ok,
		Fail
	};

	struct FDecoderInput : public TSharedFromThis<FDecoderInput, ESPMode::ThreadSafe>
	{
		~FDecoderInput()
		{
			ReleasePayload();
		}
		void ReleasePayload()
		{
			FAccessUnit::Release(AccessUnit);
			AccessUnit = nullptr;
		}

		FAccessUnit*	AccessUnit = nullptr;
		bool			bHasBeenPrepared = false;
		bool			bIsIDR = false;
		bool			bIsDiscardable = false;
		int64			PTS = 0;
		int64			EndPTS = 0;
		int64			SequenceIndex = 0;
		FTimeValue		AdjustedPTS;
		FTimeValue		AdjustedDuration;

		int32			Width = 0;
		int32			Height = 0;
		int32			TotalWidth = 0;
		int32			TotalHeight = 0;
		int32			CropLeft = 0;
		int32			CropRight = 0;
		int32			CropTop = 0;
		int32			CropBottom = 0;
		int32			AspectX = 0;
		int32			AspectY = 0;
	};

	struct FDecoderFormatInfo
	{
		void Reset()
		{
			CurrentCodecData.Reset();
		}
		bool IsDifferentFrom(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU);

		TSharedPtr<const FAccessUnit::CodecData, ESPMode::ThreadSafe> CurrentCodecData;
	};

	struct FDecodedImage
	{
		FDecodedImage() = default;
		FDecodedImage(const FDecodedImage& rhs)
		{
			InternalCopy(rhs);
		}

		FDecodedImage& operator=(const FDecodedImage& rhs)
		{
			if (this != &rhs)
			{
				InternalCopy(rhs);
			}
			return *this;
		}

		bool operator<(const FDecodedImage& rhs) const
		{
			if (!SourceInfo.IsValid() || !rhs.SourceInfo.IsValid())
			{
				return false;
			}
			return (SourceInfo->SequenceIndex < rhs.SourceInfo->SequenceIndex) || (SourceInfo->SequenceIndex == rhs.SourceInfo->SequenceIndex && SourceInfo->PTS < rhs.SourceInfo->PTS);
		}

		TSharedPtr<FDecoderInput, ESPMode::ThreadSafe>	SourceInfo;
		TSharedPtr<ILibavDecoderDecodedImage, ESPMode::ThreadSafe> DecodedImage;
		ILibavDecoderVideoCommon::FOutputInfo ImageInfo;
	private:
		void InternalCopy(const FDecodedImage& rhs)
		{
			SourceInfo = rhs.SourceInfo;
			DecodedImage = rhs.DecodedImage;
			ImageInfo = rhs.ImageInfo;
		}
	};


	bool InternalDecoderCreate();
	void InternalDecoderDestroy();
	void StartThread();
	void StopThread();
	void WorkerThread();

	bool CreateDecodedImagePool();
	void DestroyDecodedImagePool();

	void NotifyReadyBufferListener(bool bHaveOutput);

	bool AcquireOutputBuffer(IMediaRenderer::IBuffer*& RenderOutputBuffer);

	void PrepareAU(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU);
	void GetAndPrepareInputAU();

	void ProcessOutput(bool bFlush = false);

	bool FlushDecoder();
	void ClearInDecoderInfos();
	void FlushPendingImages();

	EDecodeResult Decode(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU);
	bool DecodeDummy(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU);

	void PostError(int32_t ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error = UEMEDIA_ERROR_OK);
	void LogMessage(IInfoLog::ELevel Level, const FString& Message);

	void ConvertDecodedImageToNV12(TArray<uint8>& OutNV12Buffer, FIntPoint OutBufDim, const FDecodedImage& InImage);

	FInstanceConfiguration Config;

	FMediaEvent TerminateThreadSignal;
	FMediaEvent FlushDecoderSignal;
	FMediaEvent DecoderFlushedSignal;
	bool bThreadStarted = false;

	FMediaEvent ApplicationRunningSignal;
	FMediaEvent ApplicationSuspendConfirmedSignal;

	IPlayerSessionServices* PlayerSessionServices = nullptr;

	TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> Renderer;

    TWeakPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> ResourceDelegate;

	TAccessUnitQueue<TSharedPtr<FDecoderInput, ESPMode::ThreadSafe>> NextAccessUnits;
	TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> CurrentAccessUnit;
	bool bDrainForCodecChange = false;

	FCriticalSection ListenerMutex;
	IAccessUnitBufferListener* InputBufferListener = nullptr;
	IDecoderOutputBufferListener* ReadyBufferListener = nullptr;

	FDecoderFormatInfo CurrentStreamFormatInfo;
	TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> DecoderInstance;
	TArray<TSharedPtr<FDecoderInput, ESPMode::ThreadSafe>> InDecoderInput;

	int32 MaxDecodeBufferSize = 0;
	bool bError = false;

	TArray<FDecodedImage> ReadyImages;
};

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

bool IVideoDecoderH265::Startup(const FParamDict& Options)
{
	return FVideoDecoderH265LinuxLibavcodec::Startup(Options);
}

void IVideoDecoderH265::Shutdown()
{
	FVideoDecoderH265LinuxLibavcodec::Shutdown();
}

bool IVideoDecoderH265::GetStreamDecodeCapability(FStreamDecodeCapability& OutResult, const FStreamDecodeCapability& InStreamParameter)
{
	OutResult = InStreamParameter;
	OutResult.DecoderSupportType = ILibavDecoderH265::IsAvailable() ? FStreamDecodeCapability::ESupported::HardAndSoftware : FStreamDecodeCapability::ESupported::NotSupported;

	// For now we handle only 8 bit encodings. If Main-10 profile, or only Main-10 profile compatibility is signaled we refuse this stream.
	if (InStreamParameter.Profile > 1 || (InStreamParameter.CompatibilityFlags & 0x60000000) == 0x20000000)
	{
		OutResult.DecoderSupportType = FStreamDecodeCapability::ESupported::NotSupported;
	}
	return true;
}

IVideoDecoderH265::FInstanceConfiguration::FInstanceConfiguration()
	: MaxDecodedFrames(8)
{
}

IVideoDecoderH265* IVideoDecoderH265::Create()
{
	return new FVideoDecoderH265LinuxLibavcodec;
}


/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

//-----------------------------------------------------------------------------
/**
 * Decoder system startup
 *
 * @param Options
 *
 * @return
 */
bool FVideoDecoderH265LinuxLibavcodec::Startup(const FParamDict& Options)
{
	ILibavDecoder::Startup();
	return ILibavDecoder::IsLibAvAvailable();
}


//-----------------------------------------------------------------------------
/**
 * Decoder system shutdown.
 */
void FVideoDecoderH265LinuxLibavcodec::Shutdown()
{
	ILibavDecoder::Shutdown();
}


//-----------------------------------------------------------------------------
/**
 * Constructor
 */
FVideoDecoderH265LinuxLibavcodec::FVideoDecoderH265LinuxLibavcodec()
	: FMediaThread("ElectraPlayer::H265 decoder")
{
}


//-----------------------------------------------------------------------------
/**
 * Destructor
 */
FVideoDecoderH265LinuxLibavcodec::~FVideoDecoderH265LinuxLibavcodec()
{
	Close();
}


//-----------------------------------------------------------------------------
/**
 * Sets an AU input buffer listener.
 *
 * @param Listener
 */
void FVideoDecoderH265LinuxLibavcodec::SetAUInputBufferListener(IAccessUnitBufferListener* Listener)
{
	FScopeLock lock(&ListenerMutex);
	InputBufferListener = Listener;
}


//-----------------------------------------------------------------------------
/**
 * Sets a buffer-ready listener.
 *
 * @param Listener
 */
void FVideoDecoderH265LinuxLibavcodec::SetReadyBufferListener(IDecoderOutputBufferListener* Listener)
{
	FScopeLock lock(&ListenerMutex);
	ReadyBufferListener = Listener;
}


//-----------------------------------------------------------------------------
/**
 * Sets the owning player's session service interface.
 *
 * @param InSessionServices
 *
 * @return
 */
void FVideoDecoderH265LinuxLibavcodec::SetPlayerSessionServices(IPlayerSessionServices* InSessionServices)
{
	PlayerSessionServices = InSessionServices;
}


//-----------------------------------------------------------------------------
/**
 * Opens a decoder instance
 *
 * @param InConfig
 *
 * @return
 */
void FVideoDecoderH265LinuxLibavcodec::Open(const IVideoDecoderH265::FInstanceConfiguration& InConfig)
{
	Config = InConfig;
	StartThread();
}


//-----------------------------------------------------------------------------
/**
 * Closes the decoder instance.
 */
void FVideoDecoderH265LinuxLibavcodec::Close()
{
	StopThread();
}


//-----------------------------------------------------------------------------
/**
 * Drains the decoder of all enqueued input and ends it, after which the decoder must send an FDecoderMessage to the player
 * to signal completion.
 */
void FVideoDecoderH265LinuxLibavcodec::DrainForCodecChange()
{
	bDrainForCodecChange = true;
}


//-----------------------------------------------------------------------------
/**
 * Sets a new decoder limit.
 * As soon as a new sequence starts matching this limit the decoder will be
 * destroyed and recreated to conserve memory.
 * The assumption is that the data being streamed will not exceed this limit,
 * but if it does, any access unit requiring a better decoder will take
 * precedence and force a decoder capable of decoding it!
 *
 * @param MaxTier
 * @param MaxWidth
 * @param MaxHeight
 * @param MaxProfile
 * @param MaxProfileLevel
 * @param AdditionalOptions
 */
void FVideoDecoderH265LinuxLibavcodec::SetMaximumDecodeCapability(int32 MaxTier, int32 MaxWidth, int32 MaxHeight, int32 MaxProfile, int32 MaxProfileLevel, const FParamDict& AdditionalOptions)
{
	// Not implemented
}


//-----------------------------------------------------------------------------
/**
 * Sets a new renderer.
 *
 * @param InRenderer
 */
void FVideoDecoderH265LinuxLibavcodec::SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer)
{
	Renderer = InRenderer;
}

void FVideoDecoderH265LinuxLibavcodec::SetResourceDelegate(const TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>& InResourceDelegate)
{
    ResourceDelegate = InResourceDelegate;
}

//-----------------------------------------------------------------------------
/**
 * Creates and runs the decoder thread.
 */
void FVideoDecoderH265LinuxLibavcodec::StartThread()
{
	ThreadStart(FMediaRunnable::FStartDelegate::CreateRaw(this, &FVideoDecoderH265LinuxLibavcodec::WorkerThread));
	bThreadStarted = true;
}


//-----------------------------------------------------------------------------
/**
 * Stops the decoder thread.
 */
void FVideoDecoderH265LinuxLibavcodec::StopThread()
{
	if (bThreadStarted)
	{
		TerminateThreadSignal.Signal();
		ThreadWaitDone();
		bThreadStarted = false;
	}
}


//-----------------------------------------------------------------------------
/**
 * Posts an error to the session service error listeners.
 *
 * @param ApiReturnValue
 * @param Message
 * @param Code
 * @param Error
 */
void FVideoDecoderH265LinuxLibavcodec::PostError(int32_t ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error)
{
	check(PlayerSessionServices);
	if (PlayerSessionServices)
	{
		FErrorDetail err;
		err.SetError(Error != UEMEDIA_ERROR_OK ? Error : UEMEDIA_ERROR_DETAIL);
		err.SetFacility(Facility::EFacility::H265Decoder);
		err.SetCode(Code);
		err.SetMessage(Message);
		err.SetPlatformMessage(FString::Printf(TEXT("%d (0x%08x)"), ApiReturnValue, ApiReturnValue));
		PlayerSessionServices->PostError(err);
	}
}


//-----------------------------------------------------------------------------
/**
 * Sends a log message to the session service log.
 *
 * @param Level
 * @param Message
 */
void FVideoDecoderH265LinuxLibavcodec::LogMessage(IInfoLog::ELevel Level, const FString& Message)
{
	if (PlayerSessionServices)
	{
		PlayerSessionServices->PostLog(Facility::EFacility::H265Decoder, Level, Message);
	}
}


//-----------------------------------------------------------------------------
/**
 * Create a pool of decoded images for the decoder.
 *
 * @return
 */
bool FVideoDecoderH265LinuxLibavcodec::CreateDecodedImagePool()
{
	check(Renderer);
	FParamDict poolOpts;

	poolOpts.Set("num_buffers", FVariantValue((int64) Config.MaxDecodedFrames));

	UEMediaError Error = Renderer->CreateBufferPool(poolOpts);
	check(Error == UEMEDIA_ERROR_OK);

	MaxDecodeBufferSize = (int32) Renderer->GetBufferPoolProperties().GetValue("max_buffers").GetInt64();

	if (Error != UEMEDIA_ERROR_OK)
	{
		PostError(0, "Failed to create image pool", ERRCODE_INTERNAL_LINUX_COULD_NOT_CREATE_IMAGE_POOL, Error);
	}

	return Error == UEMEDIA_ERROR_OK;
}


//-----------------------------------------------------------------------------
/**
 * Destroys the pool of decoded images.
 */
void FVideoDecoderH265LinuxLibavcodec::DestroyDecodedImagePool()
{
	Renderer->ReleaseBufferPool();
}


//-----------------------------------------------------------------------------
/**
 * Called to receive a new input access unit for decoding.
 *
 * @param AccessUnit
 */
void FVideoDecoderH265LinuxLibavcodec::AUdataPushAU(FAccessUnit* InAccessUnit)
{
	InAccessUnit->AddRef();

	TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> NextAU = MakeShared<FDecoderInput, ESPMode::ThreadSafe>();
	NextAU->AccessUnit = InAccessUnit;
	NextAccessUnits.Enqueue(MoveTemp(NextAU));
}


//-----------------------------------------------------------------------------
/**
 * "Pushes" an End Of Data marker indicating no further access units will be added.
 */
void FVideoDecoderH265LinuxLibavcodec::AUdataPushEOD()
{
	NextAccessUnits.SetEOD();
}


//-----------------------------------------------------------------------------
/**
 * Notifies the decoder that there may be further access units.
 */
void FVideoDecoderH265LinuxLibavcodec::AUdataClearEOD()
{
	NextAccessUnits.ClearEOD();
}


//-----------------------------------------------------------------------------
/**
 * Flushes the decoder and clears the input access unit buffer.
 */
void FVideoDecoderH265LinuxLibavcodec::AUdataFlushEverything()
{
	FlushDecoderSignal.Signal();
	DecoderFlushedSignal.WaitAndReset();
}


//-----------------------------------------------------------------------------
/**
 * Create a decoder instance.
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH265LinuxLibavcodec::InternalDecoderCreate()
{
	InternalDecoderDestroy();

	if (ILibavDecoderH265::IsAvailable())
	{
		TMap<FString, FVariant> Options;
		Options.Add(TEXT("hw_priority"), FVariant(FString("vdpau;cuda;vaapi")));
		DecoderInstance = ILibavDecoderH265::Create(this, Options);
		if (!DecoderInstance.IsValid() || DecoderInstance->GetLastLibraryError())
		{
			InternalDecoderDestroy();
			PostError(-2, "libavcodec failed to open video decoder", ERRCODE_INTERNAL_LINUX_COULD_NOT_CREATE_VIDEO_DECODER);
			return false;
		}
		return true;
	}
	PostError(-1, "libavcodec does not support this video format", ERRCODE_INTERNAL_LINUX_VIDEO_DECODER_NOT_SUPPORTED);
	return false;
}


//-----------------------------------------------------------------------------
/**
 * Destroys the current decoder instance.
 */
void FVideoDecoderH265LinuxLibavcodec::InternalDecoderDestroy()
{
	DecoderInstance.Reset();
}


//-----------------------------------------------------------------------------
/**
 * Notifies the buffer-ready listener that we will now be producing output data
 * and need an output buffer.
 * This call is not intended to create or obtain an output buffer. It is merely
 * indicating that new output will be produced.
 *
 * @param bHaveOutput
 */
void FVideoDecoderH265LinuxLibavcodec::NotifyReadyBufferListener(bool bHaveOutput)
{
	if (ReadyBufferListener)
	{
		IDecoderOutputBufferListener::FDecodeReadyStats stats;
		stats.MaxDecodedElementsReady = MaxDecodeBufferSize;
		stats.NumElementsInDecoder = InDecoderInput.Num();
		stats.bOutputStalled = !bHaveOutput;
		stats.bEODreached = NextAccessUnits.ReachedEOD() && stats.NumDecodedElementsReady == 0 && stats.NumElementsInDecoder == 0;
		ListenerMutex.Lock();
		if (ReadyBufferListener)
		{
			ReadyBufferListener->DecoderOutputReady(stats);
		}
		ListenerMutex.Unlock();
	}
}


//-----------------------------------------------------------------------------
/**
 * Prepares the passed access unit to be sent to the decoder.
 *
 * @param AU
 */
void FVideoDecoderH265LinuxLibavcodec::PrepareAU(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU)
{
	if (!AU->bHasBeenPrepared)
	{
		AU->bHasBeenPrepared = true;

		const FStreamCodecInformation::FResolution& res = AU->AccessUnit->AUCodecData->ParsedInfo.GetResolution();
		const FStreamCodecInformation::FAspectRatio& ar = AU->AccessUnit->AUCodecData->ParsedInfo.GetAspectRatio();
		const FStreamCodecInformation::FCrop& crop = AU->AccessUnit->AUCodecData->ParsedInfo.GetCrop();
		AU->Width = res.Width;
		AU->Height = res.Height;
		AU->TotalWidth = res.Width + crop.Left + crop.Right;
		AU->TotalHeight = res.Height + crop.Top + crop.Bottom;
		AU->CropLeft = crop.Left;
		AU->CropRight = crop.Right;
		AU->CropTop = crop.Top;
		AU->CropBottom = crop.Bottom;
		AU->AspectX = ar.Width ? ar.Width : 1;
		AU->AspectY = ar.Height ? ar.Height : 1;

		if (!AU->AccessUnit->bIsDummyData)
		{
			// Process NALUs
			AU->bIsDiscardable = false;
			AU->bIsIDR = AU->AccessUnit->bIsSyncSample;
			uint32* NALU = (uint32 *)AU->AccessUnit->AUData;
			uint32* End  = (uint32 *)Electra::AdvancePointer(NALU, AU->AccessUnit->AUSize);
			while(NALU < End)
			{
				uint8 nut = *(const uint8 *)(NALU + 1);
				nut >>= 1;
				// IDR frame?
				if (nut == 19 /*IDR_W_RADL*/ || nut == 20 /*IDR_N_LP*/ || nut == 21 /*CRA_NUT*/)
				{
					AU->bIsIDR = true;
				}
				// One of TRAIL_N, TSA_N, STSA_N, RADL_N, RASL_N, RSV_VCL_N10, RSV_VCL_N12 or RSV_VCL_N14 ?
				else if (nut == 0 || nut == 2 || nut == 4 || nut == 6 || nut == 8 || nut == 10 || nut == 12 || nut == 14)
				{
					AU->bIsDiscardable = true;
				}

				uint32 naluLen = MEDIA_FROM_BIG_ENDIAN(*NALU) + 4;
				*NALU = MEDIA_TO_BIG_ENDIAN(0x00000001U);
				NALU = Electra::AdvancePointer(NALU, naluLen);
			}
		}

		// Does this AU fall (partially) outside the range for rendering?
		FTimeValue StartTime = AU->AccessUnit->PTS;
		FTimeValue EndTime = AU->AccessUnit->PTS + AU->AccessUnit->Duration;
		AU->PTS = StartTime.GetAsHNS();				// The PTS we give the decoder no matter any adjustment.
		AU->EndPTS = EndTime.GetAsHNS();			// End PTS we need to check the PTS value returned by the decoder against.
		AU->SequenceIndex = StartTime.GetSequenceIndex();
		if (AU->AccessUnit->EarliestPTS.IsValid())
		{
			// If the end time of the AU is before the earliest render PTS we need to decode it, but not display it.
			if (EndTime <= AU->AccessUnit->EarliestPTS)
			{
				StartTime.SetToInvalid();
			}
			else if (StartTime < AU->AccessUnit->EarliestPTS)
			{
				StartTime = AU->AccessUnit->EarliestPTS;
			}
		}
		if (StartTime.IsValid() && AU->AccessUnit->LatestPTS.IsValid())
		{
			// If the start time is behind the latest render PTS we do not need to decode at all.
			if (StartTime >= AU->AccessUnit->LatestPTS)
			{
				StartTime.SetToInvalid();
				AU->bIsDiscardable = true;
			}
			else if (EndTime >= AU->AccessUnit->LatestPTS)
			{
				EndTime = AU->AccessUnit->LatestPTS;
			}
		}
		AU->AdjustedPTS = StartTime;
		AU->AdjustedDuration = EndTime - StartTime;
		if (AU->AdjustedDuration <= FTimeValue::GetZero())
		{
			AU->AdjustedPTS.SetToInvalid();
		}

		// Is there codec specific data?
		if (!AU->AccessUnit->bIsDummyData && AU->AccessUnit->AUCodecData.IsValid())
		{
			// Yes.
			if (AU->bIsIDR || AU->AccessUnit->bIsSyncSample || AU->AccessUnit->bIsFirstInSequence)
			{
				// Have to re-allocate the AU memory to preprend the codec data
				if (AU->AccessUnit->AUCodecData->CodecSpecificData.Num())
				{
					uint64 nb = AU->AccessUnit->AUSize + AU->AccessUnit->AUCodecData->CodecSpecificData.Num();
					void* pD = AU->AccessUnit->AllocatePayloadBuffer(nb);
					void* pP = pD;
					FMemory::Memcpy(pP, AU->AccessUnit->AUCodecData->CodecSpecificData.GetData(), AU->AccessUnit->AUCodecData->CodecSpecificData.Num());
					pP = Electra::AdvancePointer(pP, AU->AccessUnit->AUCodecData->CodecSpecificData.Num());
					FMemory::Memcpy(pP, AU->AccessUnit->AUData, AU->AccessUnit->AUSize);
					AU->AccessUnit->AdoptNewPayloadBuffer(pD, nb);
				}
				AU->bIsDiscardable = false;
			}
		}
	}
}


//-----------------------------------------------------------------------------
/**
 * Gets an input access unit and prepares it for use.
 */
void FVideoDecoderH265LinuxLibavcodec::GetAndPrepareInputAU()
{
	if (bDrainForCodecChange)
	{
		return;
	}

	// Need new input?
	if (!CurrentAccessUnit.IsValid() && InputBufferListener && NextAccessUnits.IsEmpty())
	{
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);

			IAccessUnitBufferListener::FBufferStats	stats;
			stats.bEODSignaled = NextAccessUnits.GetEOD();
			stats.bEODReached = NextAccessUnits.ReachedEOD();
			ListenerMutex.Lock();
			if (InputBufferListener)
			{
				InputBufferListener->DecoderInputNeeded(stats);
			}
			ListenerMutex.Unlock();
		}

		// Get the AU to be decoded if one is there.
		bool bHaveData = NextAccessUnits.Wait(1000);
		if (bHaveData)
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);

			bool bOk = NextAccessUnits.Dequeue(CurrentAccessUnit);
			MEDIA_UNUSED_VAR(bOk);
			check(bOk);

			PrepareAU(CurrentAccessUnit);
		}
	}
}


//-----------------------------------------------------------------------------
/**
 * Gets an output buffer from the video renderer.
 *
 * @param RenderOutputBuffer
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH265LinuxLibavcodec::AcquireOutputBuffer(IMediaRenderer::IBuffer*& RenderOutputBuffer)
{
	RenderOutputBuffer = nullptr;
	FParamDict BufferAcquireOptions;
	while(!TerminateThreadSignal.IsSignaled() && !FlushDecoderSignal.IsSignaled())
	{
		UEMediaError bufResult = Renderer->AcquireBuffer(RenderOutputBuffer, 0, BufferAcquireOptions);
		check(bufResult == UEMEDIA_ERROR_OK || bufResult == UEMEDIA_ERROR_INSUFFICIENT_DATA);
		if (bufResult != UEMEDIA_ERROR_OK && bufResult != UEMEDIA_ERROR_INSUFFICIENT_DATA)
		{
			PostError(0, "Failed to acquire sample buffer", ERRCODE_INTERNAL_LINUX_COULD_NOT_GET_OUTPUT_BUFFER, bufResult);
			return false;
		}
		bool bHaveAvailSmpBlk = RenderOutputBuffer != nullptr;
		NotifyReadyBufferListener(bHaveAvailSmpBlk);
		if (bHaveAvailSmpBlk)
		{
			break;
		}
		else
		{
			// No available buffer. Sleep for a bit. Can't sleep on a signal since we have to check two: abort and flush
			FMediaRunnable::SleepMilliseconds(5);
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Flushes the decoder and passes all pending frames to the renderer.
 *
 * @return true if successful, false otherwise.
 */
bool FVideoDecoderH265LinuxLibavcodec::FlushDecoder()
{
	if (DecoderInstance.IsValid())
	{
		ILibavDecoder::EDecoderError DecErr = DecoderInstance->SendEndOfData();
		check(DecErr == ILibavDecoder::EDecoderError::None); (void)DecErr;
		ProcessOutput(true);
	}

	ClearInDecoderInfos();
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Clears out any leftover decoder source infos that were not picked by decoded frames.
 * This must be called after having flushed the decoder to ensure no delayed decode
 * callbacks will arrive.
 */
void FVideoDecoderH265LinuxLibavcodec::ClearInDecoderInfos()
{
	InDecoderInput.Empty();
}


//-----------------------------------------------------------------------------
/**
 * Creates a dummy output image that is not to be displayed and has no image data.
 * Dummy access units are created when stream data is missing to ensure the data
 * pipeline does not run dry and exhibits no gaps in the timeline.
 *
 * @param AU
 *
 * @return true if successful, false otherwise
 */
bool FVideoDecoderH265LinuxLibavcodec::DecodeDummy(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU)
{
	if (AU.IsValid() && AU->AdjustedPTS.IsValid())
	{
		FDecodedImage NextImage;
		NextImage.SourceInfo = AU;
		ReadyImages.Add(NextImage);
	}
    return true;
}

//-----------------------------------------------------------------------------
/**
 * Flushes all images not passed to the renderer from our list.
 */
void FVideoDecoderH265LinuxLibavcodec::FlushPendingImages()
{
	ReadyImages.Empty();
}

//-----------------------------------------------------------------------------
/**
 * Checks if the codec specific data has changed.
 *
 * @param AU
 *
 * @return false if the format is still the same, true if it has changed.
 */
bool FVideoDecoderH265LinuxLibavcodec::FDecoderFormatInfo::IsDifferentFrom(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU)
{
	if (AU->AccessUnit->AUCodecData.IsValid() && AU->AccessUnit->AUCodecData.Get() != CurrentCodecData.Get())
	{
		// Pointers are different. Is the content too?
		bool bDifferent = !CurrentCodecData.IsValid() || (CurrentCodecData.IsValid() && AU->AccessUnit->AUCodecData->CodecSpecificData != CurrentCodecData->CodecSpecificData);
		CurrentCodecData = AU->AccessUnit->AUCodecData;
		return bDifferent;
	}
	return false;
}


//-----------------------------------------------------------------------------
/**
 * Sends an access unit to the decoder for decoding.
 *
 * @param AU
 *
 * @return
 */
FVideoDecoderH265LinuxLibavcodec::EDecodeResult FVideoDecoderH265LinuxLibavcodec::Decode(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU)
{
	if (!DecoderInstance.IsValid())
	{
		return EDecodeResult::Fail;
	}

	bool bAddInput = true;
	// Decode
	ILibavDecoderVideoCommon::FInputAU DecAU;
	DecAU.Data = AU->AccessUnit->AUData;
	DecAU.DataSize = (int32) AU->AccessUnit->AUSize;
	DecAU.DTS = AU->AccessUnit->DTS.GetAsHNS();
	DecAU.PTS = AU->AccessUnit->PTS.GetAsHNS();
	DecAU.Duration = AU->AccessUnit->Duration.GetAsHNS();
	DecAU.UserValue = AU->PTS;
	DecAU.Flags = 0;
	if (!AU->AdjustedPTS.IsValid())
	{
		DecAU.Flags |= ILibavDecoderVideoCommon::FInputAU::EFlags::EVidAUFlag_DoNotOutput;
		bAddInput = false;
	}
	if (bAddInput)
	{
		InDecoderInput.Add(AU);
	}

	ILibavDecoder::EDecoderError DecErr = DecoderInstance->DecodeAccessUnit(DecAU);
	if (DecErr == ILibavDecoder::EDecoderError::None)
	{
		// Ok.
		return EDecodeResult::Ok;
	}
	else
	{
		InDecoderInput.RemoveSingle(AU);
		PostError(DecoderInstance->GetLastLibraryError(), FString::Printf(TEXT("Failed to decode video frame (%d)"),(int)DecErr), ERRCODE_INTERNAL_LINUX_FAILED_TO_DECODE_VIDEO);
		return EDecodeResult::Fail;
	}
}


//-----------------------------------------------------------------------------
/**
 * H265 video decoder main threaded decode loop
 */
void FVideoDecoderH265LinuxLibavcodec::WorkerThread()
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);

	ApplicationRunningSignal.Signal();
	ApplicationSuspendConfirmedSignal.Reset();

	TOptional<int64> SequenceIndex;
	bool bDone  = false;
    bool bGotLastSequenceAU = false;
	bool bInDummyDecodeMode = false;

	bError = false;

	// Create decoded image pool.
	if (!CreateDecodedImagePool())
	{
		bError = true;
	}

	while(!TerminateThreadSignal.IsSignaled())
	{
		// If in background, wait until we get activated again.
		if (!ApplicationRunningSignal.IsSignaled())
		{
			UE_LOG(LogElectraPlayer, Log, TEXT("FVideoDecoderH265LinuxLibavcodec(%p): OnSuspending"), this);
			ApplicationSuspendConfirmedSignal.Signal();
			while(!ApplicationRunningSignal.WaitTimeout(100 * 1000) && !TerminateThreadSignal.IsSignaled())
			{
			}
			UE_LOG(LogElectraPlayer, Log, TEXT("FVideoDecoderH265LinuxLibavcodec(%p): OnResuming"), this);
		}

		if (!bDrainForCodecChange)
		{
			GetAndPrepareInputAU();

			bool bHaveData = CurrentAccessUnit.IsValid();
			bool bTooManyImagesWaiting = false;
			if (bHaveData)
			{
				// When there is data, even and especially after a previous EOD, we are no longer done and idling.
				bDone = false;

				// If there is too much output still pending to be delivered to the renderer
				bTooManyImagesWaiting = ReadyImages.Num() > MaxPendingOutputImages;
				if (bTooManyImagesWaiting)
				{
					while(!TerminateThreadSignal.IsSignaled() && !FlushDecoderSignal.IsSignaled())
					{
						ProcessOutput();
						if ((bTooManyImagesWaiting = ReadyImages.Num() > MaxPendingOutputImages) == false)
						{
							break;
						}
						FMediaRunnable::SleepMilliseconds(10);
					}
				}
			}

			if (bHaveData && !bTooManyImagesWaiting)
			{
				// When there is data, even and especially after a previous EOD, we are no longer done and idling.
				bDone = false;
				if (!CurrentAccessUnit->AccessUnit->bIsDummyData)
				{
					bInDummyDecodeMode = false;

					bool bStreamFormatChanged = CurrentStreamFormatInfo.IsDifferentFrom(CurrentAccessUnit) || bGotLastSequenceAU;
					bool bNeedNewDecoder = false;

					if (!SequenceIndex.IsSet())
					{
						SequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
					}
					bool bSequenceIndexChanged = SequenceIndex.GetValue() != CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
					SequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();

					bGotLastSequenceAU = CurrentAccessUnit->AccessUnit->bIsLastInPeriod;
					if (bStreamFormatChanged || bSequenceIndexChanged || !DecoderInstance.IsValid())
					{
						SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
						CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);
						bNeedNewDecoder |= bSequenceIndexChanged || !DecoderInstance.IsValid();
						if (bNeedNewDecoder)
						{
							FlushDecoder();
							InternalDecoderDestroy();
							if (!bError && !InternalDecoderCreate())
							{
								bError = true;
							}
						}
					}

					// If this AU falls outside the range where it is to be rendered and it is also discardable
					// we do not need to concern ourselves with it at all.
					if (CurrentAccessUnit->bIsDiscardable && !CurrentAccessUnit->AdjustedPTS.IsValid())
					{
						CurrentAccessUnit.Reset();
						continue;
					}

					// Decode
					if (!bError && DecoderInstance.IsValid())
					{
						EDecodeResult DecRes = Decode(CurrentAccessUnit);

						// Process any output we might have pending
						ProcessOutput();

						// Decode ok?
						if (DecRes == EDecodeResult::Ok)
						{
							CurrentAccessUnit.Reset();
						}
						// Decode failed
						else
						{
							bError = true;
						}
					}
				}
				else
				{
					if (!bInDummyDecodeMode)
					{
						bInDummyDecodeMode = true;
						FlushDecoder();
					}

					if (!DecodeDummy(CurrentAccessUnit))
					{
						bError = true;
					}
					// DecodeDummy() went through most of the regular path, but has returned the output buffer immediately
					// and can thus always get a new one with no waiting. To avoid draining the player buffer by consuming
					// the dummy AUs at rapid pace we put ourselves to sleep for the duration the AU was supposed to last.
					FMediaRunnable::SleepMicroseconds(CurrentAccessUnit->AdjustedDuration.GetAsMicroseconds());

					ProcessOutput();
					CurrentAccessUnit.Reset();
				}
			}
			else
			{
				ProcessOutput();

				// No data. Is the buffer at EOD?
				if (!CurrentAccessUnit.IsValid() && NextAccessUnits.ReachedEOD())
				{
					NotifyReadyBufferListener(true);
					// Are we done yet?
					if (!bDone && !bError)
					{
						bError = !FlushDecoder();
					}
					bDone = true;
					FMediaRunnable::SleepMilliseconds(10);
					ProcessOutput(true);
				}
			}
		}
		else
		{
			ProcessOutput();
			NotifyReadyBufferListener(true);
			// Are we done yet?
			if (!bDone && !bError)
			{
				bError = !FlushDecoder();
			}
			bDone = true;
			ProcessOutput(true);
			break;
		}

		// Flush?
		if (FlushDecoderSignal.IsSignaled())
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);

			// Have to destroy the decoder!
			InternalDecoderDestroy();
			FlushPendingImages();
			ClearInDecoderInfos();
			NextAccessUnits.Empty();
			SequenceIndex.Reset();
			CurrentAccessUnit.Reset();

			FlushDecoderSignal.Reset();
			DecoderFlushedSignal.Signal();

			// Reset done state.
			bDone = false;
		}
	}

	InternalDecoderDestroy();
	FlushPendingImages();
	DestroyDecodedImagePool();
	ClearInDecoderInfos();
	NextAccessUnits.Empty();
	CurrentAccessUnit.Reset();

	if (bDrainForCodecChange)
	{
		// Notify the player that we have finished draining.
		PlayerSessionServices->SendMessageToPlayer(FDecoderMessage::Create(FDecoderMessage::EReason::DrainingFinished, this, EStreamType::Video, FStreamCodecInformation::ECodec::H265));
		// We need to wait to get terminated. Also check if flushing is requested and acknowledge if it is.
		while(!TerminateThreadSignal.IsSignaled())
		{
			if (FlushDecoderSignal.WaitTimeoutAndReset(1000 * 10))
			{
				DecoderFlushedSignal.Signal();
			}
		}
	}
}

//-----------------------------------------------------------------------------
/**
 */
void FVideoDecoderH265LinuxLibavcodec::ProcessOutput(bool bFlush)
{
	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265ConvertOutput);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265ConvertOutput);

	// Pull output from decoder
	while(DecoderInstance.IsValid())
	{
		FDecodedImage NextImage;

		// Output available?
		if (DecoderInstance->HaveOutput(NextImage.ImageInfo) != ILibavDecoderVideoCommon::EOutputStatus::Available)
		{
			break;
		}
		// Get the output.
		NextImage.DecodedImage = DecoderInstance->GetOutput();
		if (!NextImage.DecodedImage.IsValid())
		{
			break;
		}
		// Find the corresponding input source.
		int32 NumCurrentDecodeInputs = InDecoderInput.Num();
		for(int32 i=0; i<NumCurrentDecodeInputs; ++i)
		{
			if (InDecoderInput[i]->PTS == NextImage.ImageInfo.UserValue)
			{
				NextImage.SourceInfo = InDecoderInput[i];
				InDecoderInput.RemoveSingle(NextImage.SourceInfo);
				break;
			}
		}

		if (!NextImage.SourceInfo.IsValid())
		{
			LogMessage(IInfoLog::ELevel::Warning, FString::Printf(TEXT("FVideoDecoderH265::ProcessOutput(): No source info found for decoded PTS %lld in %d pending infos"), (long long int)NextImage.ImageInfo.UserValue, NumCurrentDecodeInputs));
			break;
		}

		// For safety we remove input that is older than what we got right now.
		// This is to ensure that if libav does not return decoded output to us for some reason
		// (like a corrupted frame or such) that we do not sit on stale input.
		if (InDecoderInput.Num() > MaxPendingOutputImages)
		{
			for(int32 i=0; i<InDecoderInput.Num(); ++i)
			{
				if (InDecoderInput[i]->PTS < NextImage.ImageInfo.UserValue)
				{
					LogMessage(IInfoLog::ELevel::Warning, FString::Printf(TEXT("FVideoDecoderH265::ProcessOutput(): Removing stale input PTS %lld for output PTS %lld"), (long long int)InDecoderInput[i]->PTS, (long long int)NextImage.ImageInfo.UserValue));
					InDecoderInput.RemoveAt(i);
					--i;
				}
			}
		}

		ReadyImages.Add(NextImage);
	}
	ReadyImages.Sort();

	// Send frames out to the renderer.
	IMediaRenderer::IBuffer* RenderOutputBuffer = nullptr;
	while(!TerminateThreadSignal.IsSignaled() && !FlushDecoderSignal.IsSignaled())
	{
		if (ReadyImages.Num() == 0)
		{
			break;
		}

		// When we are not flushing out the remaining frames and the renderer cannot take on
		// anything new we leave.
		if (!bFlush && !Renderer->CanReceiveOutputFrames(1))
		{
			NotifyReadyBufferListener(false);
			break;
		}

		// Need a new output buffer?
		if (!RenderOutputBuffer)
		{
			// If we failed to get a new buffer, leave the loop.
			if (!AcquireOutputBuffer(RenderOutputBuffer))
			{
				break;
			}
		}

		// Can we send the image to the renderer?
		bool bHaveAvailableImage = RenderOutputBuffer != nullptr;
		if (bFlush && !Renderer->CanReceiveOutputFrames(1))
		{
			bHaveAvailableImage = false;
		}
		NotifyReadyBufferListener(bHaveAvailableImage);
		if (bHaveAvailableImage)
		{
			FDecodedImage NextImage(ReadyImages[0]);
			ReadyImages.RemoveAt(0);

			FParamDict* OutputBufferSampleProperties = new FParamDict();
			FIntPoint Dim(NextImage.SourceInfo->Width, NextImage.SourceInfo->Height);

			if (NextImage.DecodedImage.IsValid())
			{
				int32 ax = NextImage.ImageInfo.AspectNum;
				int32 ay = NextImage.ImageInfo.AspectDenom;
				Dim.X = NextImage.ImageInfo.Width;
				Dim.Y = NextImage.ImageInfo.Height;

				OutputBufferSampleProperties->Set("width", FVariantValue((int64) NextImage.ImageInfo.Width));
				OutputBufferSampleProperties->Set("height", FVariantValue((int64) NextImage.ImageInfo.Height));
				OutputBufferSampleProperties->Set("crop_left", FVariantValue((int64) 0));
				OutputBufferSampleProperties->Set("crop_right", FVariantValue((int64) 0));
				OutputBufferSampleProperties->Set("crop_top", FVariantValue((int64) 0));
				OutputBufferSampleProperties->Set("crop_bottom", FVariantValue((int64) 0));
				OutputBufferSampleProperties->Set("aspect_ratio", FVariantValue((double)ax / (double)ay));
				OutputBufferSampleProperties->Set("aspect_w", FVariantValue((int64) ax));
				OutputBufferSampleProperties->Set("aspect_h", FVariantValue((int64) ay));
				OutputBufferSampleProperties->Set("fps_num", FVariantValue((int64) 0 ));
				OutputBufferSampleProperties->Set("fps_denom", FVariantValue((int64) 0 ));
				OutputBufferSampleProperties->Set("pixelfmt", FVariantValue((int64)EPixelFormat::PF_NV12));
			}
			else
			{
				OutputBufferSampleProperties->Set("is_dummy", FVariantValue(true));
			}
			OutputBufferSampleProperties->Set("pts", FVariantValue(NextImage.SourceInfo->AdjustedPTS));
			OutputBufferSampleProperties->Set("duration", FVariantValue(NextImage.SourceInfo->AdjustedDuration));

			bool bRender = NextImage.SourceInfo->AdjustedPTS.IsValid();
			TSharedPtr<FElectraPlayerVideoDecoderOutputLinux, ESPMode::ThreadSafe> DecoderOutput = RenderOutputBuffer->GetBufferProperties().GetValue("texture").GetSharedPointer<FElectraPlayerVideoDecoderOutputLinux>();
			FIntPoint BufferDim(NextImage.ImageInfo.Planes[0].Width, NextImage.ImageInfo.Planes[0].Height);
			if (DecoderOutput->InitializeForBuffer(BufferDim, EPixelFormat::PF_NV12, OutputBufferSampleProperties))
			{
				TArray<uint8>& ImgBuf = DecoderOutput->GetMutableBuffer();
				ConvertDecodedImageToNV12(ImgBuf, DecoderOutput->GetBufferDimensions(), NextImage);
				// Have the decoder output keep a reference to the decoded image in case it will
				// shared data with it at some point.
				DecoderOutput->SetDecodedImage(NextImage.DecodedImage);
			}

			// Return the buffer to the renderer.
			Renderer->ReturnBuffer(RenderOutputBuffer, bRender, *OutputBufferSampleProperties);
			RenderOutputBuffer = nullptr;
		}
		else
		{
			// Since we need to check the abort and the flush signal we sleep for a bit and check again.
			FMediaRunnable::SleepMilliseconds(10);
		}
	}
	if (RenderOutputBuffer)
	{
		FParamDict None;
		Renderer->ReturnBuffer(RenderOutputBuffer, false, None);
		RenderOutputBuffer = nullptr;
	}
}


void FVideoDecoderH265LinuxLibavcodec::ConvertDecodedImageToNV12(TArray<uint8>& OutNV12Buffer, FIntPoint OutBufDim, const FDecodedImage& InImage)
{
	if (OutNV12Buffer.GetData() && InImage.DecodedImage.IsValid())
	{
		if (InImage.ImageInfo.NumPlanes == 2 &&
			InImage.ImageInfo.Planes[0].Content == ILibavDecoderVideoCommon::FPlaneInfo::EContent::Luma &&
			InImage.ImageInfo.Planes[1].Content == ILibavDecoderVideoCommon::FPlaneInfo::EContent::ChromaUV)
		{
			const int32 w = InImage.ImageInfo.Planes[0].Width;
			const int32 h = InImage.ImageInfo.Planes[0].Height;
			const int32 aw = ((w + 1) / 2) * 2;
			const int32 ah = ((h + 1) / 2) * 2;
			uint8* DstY = OutNV12Buffer.GetData();
			uint8* DstUV = DstY + aw * ah;
			const uint8* SrcY = (const uint8*)InImage.ImageInfo.Planes[0].Address;
			const uint8* SrcUV = (const uint8*)InImage.ImageInfo.Planes[1].Address;
			// To simplify the conversion we require the output buffer to have the dimension of the planes.
			check(OutBufDim.X == aw && OutBufDim.Y == ah);
			if (!SrcY || !SrcUV || OutBufDim.X != aw || OutBufDim.Y != ah)
			{
				return;
			}
			if ((w & 1) == 0)
			{
				FMemory::Memcpy(DstY, SrcY, w*h);
				FMemory::Memcpy(DstUV, SrcUV, w*h/2);
			}
			else
			{
				for(int32 y=0; y<h; ++y)
				{
					FMemory::Memcpy(DstY, SrcY, w);
					DstY += aw;
					SrcY += w;
				}
				for(int32 y=0; y<h/2; ++y)
				{
					FMemory::Memcpy(DstUV, SrcUV, w);
					DstUV += aw;
					SrcUV += w;
				}
			}
		}
		else if (InImage.ImageInfo.NumPlanes == 3 &&
				 InImage.ImageInfo.Planes[0].Content == ILibavDecoderVideoCommon::FPlaneInfo::EContent::Luma &&
				 InImage.ImageInfo.Planes[1].Content == ILibavDecoderVideoCommon::FPlaneInfo::EContent::ChromaU &&
				 InImage.ImageInfo.Planes[2].Content == ILibavDecoderVideoCommon::FPlaneInfo::EContent::ChromaV)
		{
			const int32 w = InImage.ImageInfo.Planes[0].Width;
			const int32 h = InImage.ImageInfo.Planes[0].Height;
			const int32 aw = ((w + 1) / 2) * 2;
			const int32 ah = ((h + 1) / 2) * 2;
			uint8* DstY = OutNV12Buffer.GetData();
			uint8* DstUV = DstY + aw * ah;
			const uint8* SrcY = (const uint8*)InImage.ImageInfo.Planes[0].Address;
			const uint8* SrcU = (const uint8*)InImage.ImageInfo.Planes[1].Address;
			const uint8* SrcV = (const uint8*)InImage.ImageInfo.Planes[2].Address;
			// To simplify the conversion we require the output buffer to have the dimension of the planes.
			check(OutBufDim.X == aw && OutBufDim.Y == ah);
			if (!SrcY || !SrcU || !SrcV || OutBufDim.X != aw || OutBufDim.Y != ah)
			{
				return;
			}
			if ((w & 1) == 0)
			{
				FMemory::Memcpy(DstY, SrcY, w*h);
				for(int32 i=0, iMax=w*h/4; i<iMax; ++i)
				{
					*DstUV++ = *SrcU++;
					*DstUV++ = *SrcV++;
				}
			}
			else
			{
				for(int32 y=0; y<h; ++y)
				{
					FMemory::Memcpy(DstY, SrcY, w);
					DstY += aw;
					SrcY += w;
				}
				int32 padUV = (aw - w) * 2;
				for(int32 v=0; v<h/2; ++v)
				{
					for(int32 u=0; u<w/2; ++u)
					{
						*DstUV++ = *SrcU++;
						*DstUV++ = *SrcV++;
					}
					DstUV += padUV;
				}
			}
		}
	}
}


} // namespace Electra
