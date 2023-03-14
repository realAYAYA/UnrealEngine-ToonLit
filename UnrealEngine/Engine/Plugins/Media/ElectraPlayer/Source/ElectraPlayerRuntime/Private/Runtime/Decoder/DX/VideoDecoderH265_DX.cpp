// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef ELECTRA_ENABLE_MFDECODER

#include "PlayerCore.h"
#include "PlayerRuntimeGlobal.h"
#include "ElectraPlayerPrivate.h"
#include "ElectraPlayerPrivate_Platform.h"

#include "StreamAccessUnitBuffer.h"
#include "Decoder/VideoDecoderH265.h"
#include "Renderer/RendererBase.h"
#include "Renderer/RendererVideo.h"
#include "Player/PlayerSessionServices.h"
#include "Utilities/Utilities.h"
#include "Utilities/StringHelpers.h"
#include "Utilities/UtilsMPEGVideo.h"

#if ELECTRA_PLATFORM_HAS_H265_DECODER

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include "HAL/LowLevelMemTracker.h"

THIRD_PARTY_INCLUDES_START
#include "mftransform.h"
#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"
THIRD_PARTY_INCLUDES_END

#include "Decoder/DX/MediaFoundationGUIDs.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "DecoderErrors_DX.h"
#include "VideoDecoderH265_DX.h"


DECLARE_CYCLE_STAT(TEXT("FVideoDecoderH265::Decode()"), STAT_ElectraPlayer_VideoH265Decode, STATGROUP_ElectraPlayer);
DECLARE_CYCLE_STAT(TEXT("FVideoDecoderH265::ConvertOutput()"), STAT_ElectraPlayer_VideoH265ConvertOutput, STATGROUP_ElectraPlayer);


namespace Electra {

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

namespace ConstantsMF265
{
// Note: keep those set at 1080p.
static const int32 MaxHWDecodeW = 1920;
static const int32 MaxHWDecodeH = 1088;
}

bool IVideoDecoderH265::Startup(const FParamDict& Options)
{
	return FVideoDecoderH265::Startup(Options);
}

void IVideoDecoderH265::Shutdown()
{
	FVideoDecoderH265::Shutdown();
}

bool IVideoDecoderH265::GetStreamDecodeCapability(FStreamDecodeCapability& OutResult, const FStreamDecodeCapability& InStreamParameter)
{
	return FVideoDecoderH265::GetStreamDecodeCapability(OutResult, InStreamParameter);
}

IVideoDecoderH265::FInstanceConfiguration::FInstanceConfiguration()
	: MaxDecodedFrames(8)														// FIXME: how many do we need exactly?
{
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
bool FVideoDecoderH265::Startup(const FParamDict& Options)
{
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Decoder system shutdown.
 */
void FVideoDecoderH265::Shutdown()
{
}


//-----------------------------------------------------------------------------
/**
 * Constructor
 */
FVideoDecoderH265::FVideoDecoderH265()
	: FMediaThread("Electra::H265 decoder")
	, bThreadStarted(false)
	, bDrainForCodecChange(false)
	, PlayerSessionServices(nullptr)
	, Renderer(nullptr)
	, InputBufferListener(nullptr)
	, ReadyBufferListener(nullptr)
	, bIsHardwareAccelerated(true)
	, NumFramesInDecoder(0)
	, bError(false)
	, CurrentRenderOutputBuffer(nullptr)
	, bHaveDecoder(false)
	, MaxDecodeBufferSize(0)
{
}


//-----------------------------------------------------------------------------
/**
 * Destructor
 */
FVideoDecoderH265::~FVideoDecoderH265()
{
	Close();
}


//-----------------------------------------------------------------------------
/**
 * Sets an AU input buffer listener.
 *
 * @param InListener
 */
void FVideoDecoderH265::SetAUInputBufferListener(IAccessUnitBufferListener* InListener)
{
	FMediaCriticalSection::ScopedLock lock(ListenerMutex);
	InputBufferListener = InListener;
}


//-----------------------------------------------------------------------------
/**
 * Sets a buffer-ready listener.
 *
 * @param InListener
 */
void FVideoDecoderH265::SetReadyBufferListener(IDecoderOutputBufferListener* InListener)
{
	FMediaCriticalSection::ScopedLock lock(ListenerMutex);
	ReadyBufferListener = InListener;
}


//-----------------------------------------------------------------------------
/**
 * Sets the owning player's session service interface.
 *
 * @param InSessionServices
 *
 * @return
 */
void FVideoDecoderH265::SetPlayerSessionServices(IPlayerSessionServices* SessionServices)
{
	PlayerSessionServices = SessionServices;
}


//-----------------------------------------------------------------------------
/**
 * Opens a decoder instance
 *
 * @param InConfig
 *
 * @return
 */
void FVideoDecoderH265::Open(const IVideoDecoderH265::FInstanceConfiguration& InConfig)
{
	Config = InConfig;

	MaxDecodeDim = FIntPoint(Config.MaxFrameWidth, Config.MaxFrameHeight);

	StartThread();
}


//-----------------------------------------------------------------------------
/**
 * Closes the decoder instance.
 */
void FVideoDecoderH265::Close()
{
	StopThread();
}


//-----------------------------------------------------------------------------
/**
 * Drains the decoder of all enqueued input and ends it, after which the decoder must send an FDecoderMessage to the player
 * to signal completion.
 */
void FVideoDecoderH265::DrainForCodecChange()
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
 * precendence and force a decoder capable of decoding it!
 *
 * @param MaxTier
 * @param MaxWidth
 * @param MaxHeight
 * @param MaxProfile
 * @param MaxProfileLevel
 * @param AdditionalOptions
 */
void FVideoDecoderH265::SetMaximumDecodeCapability(int32 MaxTier, int32 MaxWidth, int32 MaxHeight, int32 MaxProfile, int32 MaxProfileLevel, const FParamDict& AdditionalOptions)
{
	// Recall max resolution so we can allocate properly sized sample buffers
	MaxDecodeDim = FIntPoint(MaxWidth, MaxHeight);
}


//-----------------------------------------------------------------------------
/**
 * Sets a new renderer.
 *
 * @param InRenderer
 */
void FVideoDecoderH265::SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer)
{
	Renderer = InRenderer;
}


void FVideoDecoderH265::SetResourceDelegate(const TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>& InResourceDelegate)
{
	ResourceDelegate = InResourceDelegate;
}

//-----------------------------------------------------------------------------
/**
 * Creates and runs the decoder thread.
 */
void FVideoDecoderH265::StartThread()
{
	ThreadStart(FMediaRunnable::FStartDelegate::CreateRaw(this, &FVideoDecoderH265::WorkerThread));
	bThreadStarted = true;
}


//-----------------------------------------------------------------------------
/**
 * Stops the decoder thread.
 */
void FVideoDecoderH265::StopThread()
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
void FVideoDecoderH265::PostError(int32 ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error)
{
	bError = true;
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
void FVideoDecoderH265::LogMessage(IInfoLog::ELevel Level, const FString& Message)
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
bool FVideoDecoderH265::CreateDecodedImagePool()
{
	check(Renderer);
	FParamDict poolOpts;

	poolOpts.Set("num_buffers", FVariantValue((int64) Config.MaxDecodedFrames));
	if (!bIsHardwareAccelerated)
	{
		poolOpts.Set("sw_texture", FVariantValue(true));
	}
	UEMediaError Error = Renderer->CreateBufferPool(poolOpts);
	check(Error == UEMEDIA_ERROR_OK);

	MaxDecodeBufferSize = (int32) Renderer->GetBufferPoolProperties().GetValue("max_buffers").GetInt64();

	if (Error != UEMEDIA_ERROR_OK)
	{
		PostError(0, "Failed to create image pool", ERRCODE_INTERNAL_COULD_NOT_CREATE_IMAGE_POOL, Error);
	}

	return Error == UEMEDIA_ERROR_OK;
}


//-----------------------------------------------------------------------------
/**
 * Destroys the pool of decoded images.
 */
void FVideoDecoderH265::DestroyDecodedImagePool()
{
	Renderer->ReleaseBufferPool();
}


//-----------------------------------------------------------------------------
/**
 * Called to receive a new input access unit for decoding.
 *
 * @param InAccessUnit
 */
void FVideoDecoderH265::AUdataPushAU(FAccessUnit* InAccessUnit)
{
	InAccessUnit->AddRef();

	TSharedPtrTS<FDecoderInput> NextAU = MakeSharedTS<FDecoderInput>();
	NextAU->AccessUnit = InAccessUnit;
	NextAccessUnits.Enqueue(MoveTemp(NextAU));
}


//-----------------------------------------------------------------------------
/**
 * "Pushes" an End Of Data marker indicating no further access units will be added.
 */
void FVideoDecoderH265::AUdataPushEOD()
{
	NextAccessUnits.SetEOD();
}

void FVideoDecoderH265::AUdataClearEOD()
{
	NextAccessUnits.ClearEOD();
}


//-----------------------------------------------------------------------------
/**
 * Flushes the decoder and clears the input access unit buffer.
 */
void FVideoDecoderH265::AUdataFlushEverything()
{
	FlushDecoderSignal.Signal();
	DecoderFlushedSignal.WaitAndReset();
}


//-----------------------------------------------------------------------------
/**
 *	Reconfigures decoder for software mode.
 */
bool FVideoDecoderH265::ReconfigureForSwDecoding(FString Reason)
{
	PostError(S_OK, Reason, ERRCODE_INTERNAL_INCOMPATIBLE_DECODER);
	return false;
}


//-----------------------------------------------------------------------------
/**
 * Configures the decoder input and output media types.
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH265::Configure()
{
	// Setup media input type
	if (!DecoderSetInputType())
	{
		return false;
	}
	// Setup media output type
	if (!DecoderSetOutputType())
	{
		return false;
	}
	// Verify status
	if (!DecoderVerifyStatus())
	{
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Starts decoder processing.
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH265::StartStreaming()
{
	HRESULT res;
	// Start it!
	VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0), "Failed to notify video decoder of stream begin", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_BEGIN);
	VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0), "Failed to notify video decoder of start start", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_START);
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Configures the decoder input media type.
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH265::DecoderSetInputType()
{
	TRefCountPtr<IMFMediaType>	InputMediaType;
	HRESULT					res;

	// https://docs.microsoft.com/en-us/windows/win32/medfound/h-265---hevc-video-decoder
	VERIFY_HR(MFCreateMediaType(InputMediaType.GetInitReference()), "Failed to create video decoder input media type", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUT_MEDIA_TYPE);
	VERIFY_HR(InputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video decoder input media type major", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUT_MEDIA_TYPE_MAJOR);
	VERIFY_HR(InputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_HEVC), "Failed to set video decoder input media sub type", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUT_MEDIA_TYPE_SUBTYPE);

	int32 configW, configH;
	if (bIsHardwareAccelerated)
	{
		configW = ConstantsMF265::MaxHWDecodeW;
		configH = ConstantsMF265::MaxHWDecodeH;
	}
	else
	{
		configW = CurrentSampleInfo.GetResolution().Width;
		configH = CurrentSampleInfo.GetResolution().Height;
	}

	if (CurrentAccessUnit.IsValid() && CurrentAccessUnit->AccessUnit->AUCodecData.IsValid() && CurrentAccessUnit->AccessUnit->AUCodecData->RawCSD.Num())
	{
		VERIFY_HR(InputMediaType->SetBlob(MF_MT_USER_DATA, CurrentAccessUnit->AccessUnit->AUCodecData->RawCSD.GetData(), (UINT32) CurrentAccessUnit->AccessUnit->AUCodecData->RawCSD.Num()), "Failed to set video decoder input media type codec blob", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_MEDIA_TYPE_CODEC_BLOB);
	}

	VERIFY_HR(MFSetAttributeSize(InputMediaType, MF_MT_FRAME_SIZE, configW, configH), "Failed to set video decoder input media type resolution", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUT_MEDIA_TYPE_RESOLUTION);

	VERIFY_HR(DecoderTransform->SetInputType(0, InputMediaType, 0), "Failed to set video decoder input type", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_MEDIA_TYPE)

	return true;
}


//-----------------------------------------------------------------------------
/**
 * Configures the decoder output media type.
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH265::DecoderSetOutputType()
{
	TRefCountPtr<IMFMediaType>	OutputMediaType;
	GUID					OutputMediaMajorType;
	GUID					OutputMediaSubtype;
	HRESULT					res;

	// Supposedly calling GetOutputAvailableType() returns following output media subtypes:
	// MFVideoFormat_NV12, MFVideoFormat_YV12, MFVideoFormat_IYUV, MFVideoFormat_I420, MFVideoFormat_YUY2
	for(int32 TypeIndex=0; ; ++TypeIndex)
	{
		VERIFY_HR(DecoderTransform->GetOutputAvailableType(0, TypeIndex, OutputMediaType.GetInitReference()), "Failed to get video decoder available output type", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_MEDIA_TYPE);
		VERIFY_HR(OutputMediaType->GetGUID(MF_MT_MAJOR_TYPE, &OutputMediaMajorType), "Failed to get video decoder available output media type", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_MEDIA_TYPE_MAJOR);
		VERIFY_HR(OutputMediaType->GetGUID(MF_MT_SUBTYPE, &OutputMediaSubtype), "Failed to get video decoder available output media subtype", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_MEDIA_TYPE_SUBTYPE);
		if (OutputMediaMajorType == MFMediaType_Video && OutputMediaSubtype == MFVideoFormat_NV12)
		{
			VERIFY_HR(DecoderTransform->SetOutputType(0, OutputMediaType, 0), "Failed to set video decoder output type", ERRCODE_INTERNAL_COULD_NOT_SET_OUTPUT_MEDIA_TYPE);
			CurrentOutputMediaType = OutputMediaType;
			return true;
		}
	}
	PostError(S_OK, "Failed to set video decoder output type to desired format", ERRCODE_INTERNAL_COULD_NOT_SET_OUTPUT_DESIRED_MEDIA_TYPE);
	return false;
}


//-----------------------------------------------------------------------------
/**
 * Checks that the decoder is in the state we think it should be and good to go.
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH265::DecoderVerifyStatus()
{
	HRESULT		res;
	DWORD		NumInputStreams;
	DWORD		NumOutputStreams;

	VERIFY_HR(DecoderTransform->GetStreamCount(&NumInputStreams, &NumOutputStreams), "Failed to get video decoder stream count", ERRCODE_INTERNAL_COULD_NOT_GET_STREAM_COUNT);
	if (NumInputStreams != 1 || NumOutputStreams != 1)
	{
		PostError(S_OK, FString::Printf(TEXT("Unexpected number of streams: input %lu, output %lu"), NumInputStreams, NumOutputStreams), ERRCODE_INTERNAL_UNEXPECTED_NUMBER_OF_STREAMS);
		return false;
	}

	DWORD DecoderStatus = 0;
	VERIFY_HR(DecoderTransform->GetInputStatus(0, &DecoderStatus), "Failed to get video decoder input status", ERRCODE_INTERNAL_COULD_NOT_GET_INPUT_STATUS);
	if (MFT_INPUT_STATUS_ACCEPT_DATA != DecoderStatus)
	{
		PostError(S_OK, FString::Printf(TEXT("Decoder doesn't accept data, status %lu"), DecoderStatus), ERRCODE_INTERNAL_DECODER_NOT_ACCEPTING_DATA);
		return false;
	}

	VERIFY_HR(DecoderTransform->GetOutputStreamInfo(0, &DecoderOutputStreamInfo), "Failed to get video decoder output stream info", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_STREAM_INFO);
	if (!(DecoderOutputStreamInfo.dwFlags & MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE))
	{
		return ReconfigureForSwDecoding(TEXT("Incompatible H.265 decoder: fixed sample size expected"));
	}
	if (!(DecoderOutputStreamInfo.dwFlags & MFT_OUTPUT_STREAM_WHOLE_SAMPLES))
	{
		return ReconfigureForSwDecoding(TEXT("Incompatible H.265 decoder: whole samples expected"));
	}
	if (bIsHardwareAccelerated && !(DecoderOutputStreamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES))
	{
		// theoretically we can handle this situation with H/W decoder, but we can't reproduce it locally for testing so we aren't sure if H/W
		// decoder would work in this case
		return ReconfigureForSwDecoding(TEXT("Incompatible H.265 decoder: h/w accelerated decoder is expected to provide output samples"));
	}
	if (!bIsHardwareAccelerated && (DecoderOutputStreamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES))
	{
		PostError(S_OK, "Incompatible H.265 decoder: s/w decoder is expected to require preallocated output samples", ERRCODE_INTERNAL_INCOMPATIBLE_DECODER);
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Destroys the current decoder instance.
 */
void FVideoDecoderH265::InternalDecoderDestroy()
{
	DecoderTransform = nullptr;
	CurrentOutputMediaType = nullptr;
	bHaveDecoder = false;
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
void FVideoDecoderH265::NotifyReadyBufferListener(bool bHaveOutput)
{
	if (ReadyBufferListener)
	{
		IDecoderOutputBufferListener::FDecodeReadyStats stats;
		stats.MaxDecodedElementsReady = MaxDecodeBufferSize;
		stats.NumElementsInDecoder    = NumFramesInDecoder;
		stats.bOutputStalled		  = !bHaveOutput;
		stats.bEODreached   		  = NextAccessUnits.ReachedEOD() && stats.NumDecodedElementsReady == 0 && stats.NumElementsInDecoder == 0;
		ListenerMutex.Lock();
		if (ReadyBufferListener)
		{
			ReadyBufferListener->DecoderOutputReady(stats);
		}
		ListenerMutex.Unlock();
	}
}



void FVideoDecoderH265::PrepareAU(TSharedPtrTS<FDecoderInput> AU)
{
	if (!AU->bHasBeenPrepared)
	{
		AU->bHasBeenPrepared = true;

		if (!AU->AccessUnit->bIsDummyData)
		{
			// Process NALUs
			AU->bIsDiscardable = false;
			AU->bIsIDR = AU->AccessUnit->bIsSyncSample;
			// Replace the NALU lengths with the startcode.
			uint32* NALU = (uint32*)AU->AccessUnit->AUData;
			uint32* End  = (uint32*)Electra::AdvancePointer(NALU, AU->AccessUnit->AUSize);
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

			// Is there codec specific data?
			if (AU->AccessUnit->AUCodecData.IsValid())
			{
				// Yes.
				if (AU->bIsIDR || AU->AccessUnit->bIsSyncSample || AU->AccessUnit->bIsFirstInSequence)
				{
					NewSampleInfo = AU->AccessUnit->AUCodecData->ParsedInfo;

					// Have to re-allocate the AU memory to preprend the codec data
					if (AU->AccessUnit->AUCodecData->CodecSpecificData.Num())
					{
						uint64 nb = AU->AccessUnit->AUSize + AU->AccessUnit->AUCodecData->CodecSpecificData.Num();
						void* PayloadBuffer = AU->AccessUnit->AllocatePayloadBuffer(nb);
						check(PayloadBuffer);
						void* PayloadData = PayloadBuffer;
						FMemory::Memcpy(PayloadData, AU->AccessUnit->AUCodecData->CodecSpecificData.GetData(), AU->AccessUnit->AUCodecData->CodecSpecificData.Num());
						PayloadData = Electra::AdvancePointer(PayloadData, AU->AccessUnit->AUCodecData->CodecSpecificData.Num());
						FMemory::Memcpy(PayloadData, AU->AccessUnit->AUData, AU->AccessUnit->AUSize);
						AU->AccessUnit->AdoptNewPayloadBuffer(PayloadBuffer, nb);
					}
					AU->bIsDiscardable = false;
				}
			}
		}

		// Does this AU fall (partially) outside the range for rendering?
		FTimeValue StartTime = AU->AccessUnit->PTS;
		FTimeValue EndTime = AU->AccessUnit->PTS + AU->AccessUnit->Duration;
		AU->PTS = StartTime.GetAsHNS();		// The PTS we give the decoder no matter any adjustment.
		AU->EndPTS = EndTime.GetAsHNS();	// End PTS we need to check the PTS value returned by the decoder against.
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
	}
}


void FVideoDecoderH265::SetupBufferAcquisitionProperties()
{
	BufferAcquireOptions.Clear();
	int32 w = CurrentSampleInfo.GetResolution().Width;
	int32 h = CurrentSampleInfo.GetResolution().Height;
	BufferAcquireOptions.Set("width",  FVariantValue((int64) w));
	BufferAcquireOptions.Set("height", FVariantValue((int64) h));
}



void FVideoDecoderH265::ReturnUnusedFrame()
{
	CurrentDecoderOutputBuffer.Reset();
	if (CurrentRenderOutputBuffer)
	{
		Electra::FParamDict DummySampleProperties;
		Renderer->ReturnBuffer(CurrentRenderOutputBuffer, false, DummySampleProperties);
		CurrentRenderOutputBuffer = nullptr;
	}
}


bool FVideoDecoderH265::AcquireOutputBuffer(bool bForNonDisplay)
{
	// If we still have one we're done.
	if (CurrentRenderOutputBuffer)
	{
		return true;
	}
	while(!TerminateThreadSignal.IsSignaled())
	{
		if (FlushDecoderSignal.IsSignaled())
		{
			break;
		}

		UEMediaError bufResult = Renderer->AcquireBuffer(CurrentRenderOutputBuffer, 0, BufferAcquireOptions);
		check(bufResult == UEMEDIA_ERROR_OK || bufResult == UEMEDIA_ERROR_INSUFFICIENT_DATA);
		if (bufResult != UEMEDIA_ERROR_OK && bufResult != UEMEDIA_ERROR_INSUFFICIENT_DATA)
		{
			PostError(S_OK, "Failed to acquire sample buffer", ERRCODE_INTERNAL_COULD_NOT_GET_SAMPLE_BUFFER, bufResult);
			return false;
		}

		bool bHaveOutputBuffer = CurrentRenderOutputBuffer != nullptr;
		NotifyReadyBufferListener(bHaveOutputBuffer || bForNonDisplay);
		if (bHaveOutputBuffer)
		{
			break;
		}
		else
		{
			// No available buffer. Sleep for a bit on the flush signal. This gets set before the termination signal.
			FlushDecoderSignal.WaitTimeout(1000 * 5);
		}
	}
	return true;

}

bool FVideoDecoderH265::FindAndUpdateDecoderInput(TSharedPtrTS<FDecoderInput>& OutMatchingInput, int64 InPTSFromDecoder)
{
	if (InDecoderInput.Num())
	{
		OutMatchingInput = InDecoderInput[0];
		InDecoderInput.RemoveAt(0);
		return true;
	}
	LogMessage(IInfoLog::ELevel::Warning, FString::Printf(TEXT("No decoder input found for decoded image PTS %lld. List is empty!"), (long long int)InPTSFromDecoder));
	return false;
}

bool FVideoDecoderH265::ConvertDecodedImage(const TRefCountPtr<IMFSample>& DecodedOutputSample)
{
	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265ConvertOutput);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265ConvertOutput);

	TUniquePtr<FParamDict> OutputBufferSampleProperties = MakeUnique<FParamDict>();

	HRESULT			res;
	LONGLONG		llTimeStamp = 0;
	LONGLONG		llDuration = 0;
	MFVideoArea		videoArea = {};
	UINT32			uiPanScanEnabled = 0;
	FTimeValue		TimeStamp, Duration;
	bool			bRender = true;

	VERIFY_HR(DecodedOutputSample->GetSampleTime(&llTimeStamp), "Failed to get video decoder output sample timestamp", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_SAMPLE_TIME);
	TimeStamp.SetFromHNS((int64) llTimeStamp);
	VERIFY_HR(DecodedOutputSample->GetSampleDuration(&llDuration), "Failed to get video decoder output sample duration", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_SAMPLE_DURATION);
	Duration.SetFromHNS((int64) llDuration);

	// Attempt to locate the information we saved for this image when we sent it for decoding.
	TSharedPtrTS<FDecoderInput> MatchingInput;
	if (FindAndUpdateDecoderInput(MatchingInput, (int64) llTimeStamp))
	{
		// If this image is not for display, return the buffer immediately and bail.
		if (!MatchingInput->AdjustedPTS.IsValid())
		{
			bRender = false;
			#if 1
				// Note: For some reason there will be COM errors thrown when we do not "use" the IMFSample in some way
				//       This does not appear to be a problem but is strange anyway. This block can be turned to #if 0
				//       to go through all the notions of accessing the sample and then not actually render it, which
				//       gets rid of the COM exceptions but is slower.
				if (CurrentRenderOutputBuffer != nullptr)
				{
					Renderer->ReturnBuffer(CurrentRenderOutputBuffer, false, *OutputBufferSampleProperties);
					CurrentRenderOutputBuffer = nullptr;
				}
				return true;
			#endif
		}
		TimeStamp = MatchingInput->AdjustedPTS;
		Duration = MatchingInput->AdjustedDuration;
	}

	OutputBufferSampleProperties->Set("pts", FVariantValue(TimeStamp));
	OutputBufferSampleProperties->Set("duration", FVariantValue(Duration));

	if (SUCCEEDED(res = CurrentOutputMediaType->GetUINT32(MF_MT_PAN_SCAN_ENABLED, &uiPanScanEnabled)) && uiPanScanEnabled)
	{
		res = CurrentOutputMediaType->GetBlob(MF_MT_PAN_SCAN_APERTURE, (UINT8*)&videoArea, sizeof(MFVideoArea), nullptr);
	}

	UINT32	dwInputWidth = 0;
	UINT32	dwInputHeight = 0;
	res = MFGetAttributeSize(CurrentOutputMediaType.GetReference(), MF_MT_FRAME_SIZE, &dwInputWidth, &dwInputHeight);
	check(SUCCEEDED(res));
	res = CurrentOutputMediaType->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&videoArea, sizeof(MFVideoArea), nullptr);
	if (FAILED(res))
	{
		res = CurrentOutputMediaType->GetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&videoArea, sizeof(MFVideoArea), nullptr);
		if (FAILED(res))
		{
			videoArea.OffsetX.fract = 0;
			videoArea.OffsetX.value = 0;
			videoArea.OffsetY.fract = 0;
			videoArea.OffsetY.value = 0;
			videoArea.Area.cx = dwInputWidth;
			videoArea.Area.cy = dwInputHeight;
		}
	}

	// Width and height is the cropped area!
	OutputBufferSampleProperties->Set("width",  FVariantValue((int64) videoArea.Area.cx));
	OutputBufferSampleProperties->Set("height", FVariantValue((int64) videoArea.Area.cy));

	if (!bIsHardwareAccelerated || (FDXDeviceInfo::s_DXDeviceInfo->DxVersion == FDXDeviceInfo::ED3DVersion::Version11Win8))
	{
		// Cropping is applied to immediate decoder output and never visible to outside code
		OutputBufferSampleProperties->Set("crop_left", FVariantValue((int64)0));
		OutputBufferSampleProperties->Set("crop_right", FVariantValue((int64)0));
		OutputBufferSampleProperties->Set("crop_top", FVariantValue((int64)0));
		OutputBufferSampleProperties->Set("crop_bottom", FVariantValue((int64)0));
	}
	else
	{
		// Note: WMF decoder will always crop only at the lower, right borders
		OutputBufferSampleProperties->Set("crop_left", FVariantValue((int64)0));
		OutputBufferSampleProperties->Set("crop_right", FVariantValue((int64)(dwInputWidth - videoArea.Area.cx)));
		OutputBufferSampleProperties->Set("crop_top", FVariantValue((int64)0));
		OutputBufferSampleProperties->Set("crop_bottom", FVariantValue((int64)(dwInputHeight - videoArea.Area.cy)));
	}

	// Try to get the stride. Defaults to 0 should it not be obtainable.
	UINT32 stride = MFGetAttributeUINT32(CurrentOutputMediaType, MF_MT_DEFAULT_STRIDE, 0);
	OutputBufferSampleProperties->Set("pitch",  FVariantValue((int64) stride));

	// Try to get the frame rate ratio
	UINT32 num=0, denom=0;
	MFGetAttributeRatio(CurrentOutputMediaType, MF_MT_FRAME_RATE, &num, &denom);
	OutputBufferSampleProperties->Set("fps_num",   FVariantValue((int64) num ));
	OutputBufferSampleProperties->Set("fps_denom", FVariantValue((int64) denom ));

	// Try to get the pixel aspect ratio
	num=0, denom=0;
	MFGetAttributeRatio(CurrentOutputMediaType, MF_MT_PIXEL_ASPECT_RATIO, &num, &denom);
	if (!num || !denom)
	{
		num   = 1;
		denom = 1;
	}
	OutputBufferSampleProperties->Set("aspect_ratio", FVariantValue((double) num / (double)denom));
	OutputBufferSampleProperties->Set("aspect_w",     FVariantValue((int64) num));
	OutputBufferSampleProperties->Set("aspect_h",     FVariantValue((int64) denom));

	OutputBufferSampleProperties->Set("pixelfmt",	  FVariantValue((int64)EPixelFormat::PF_NV12));

	if (CurrentRenderOutputBuffer != nullptr)
	{
		if (!SetupDecodeOutputData(FIntPoint(videoArea.Area.cx, videoArea.Area.cy), DecodedOutputSample, OutputBufferSampleProperties.Get()))
		{
			return false;
		}

		Renderer->ReturnBuffer(CurrentRenderOutputBuffer, bRender, *OutputBufferSampleProperties);
		CurrentRenderOutputBuffer = nullptr;
		OutputBufferSampleProperties.Release();
	}
	return true;
}


bool FVideoDecoderH265::Decode(TSharedPtrTS<FDecoderInput> AU)
{
	// No decoder, nothing to do.
	if (DecoderTransform.GetReference() == nullptr)
	{
		return true;
	}

	HRESULT res;
	TRefCountPtr<IMFSample>	InputSample;
	bool bFlush = !AU.IsValid();

	if (bFlush)
	{
		VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0), "Failed to set video decoder end of stream notification", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_ENDOFSTREAM);
		VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0), "Failed to issue video decoder drain command", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_DRAINCOMMAND);
	}
	else
	{
		// Create the input sample.
		TRefCountPtr<IMFMediaBuffer> InputSampleBuffer;
		BYTE*					pbNewBuffer = nullptr;
		DWORD					dwMaxBufferSize = 0;
		DWORD					dwSize = 0;
		LONGLONG				llSampleTime = 0;

		SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
		CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);

		VERIFY_HR(MFCreateSample(InputSample.GetInitReference()), "Failed to create video decoder input sample", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUT_SAMPLE);
		VERIFY_HR(MFCreateMemoryBuffer((DWORD) AU->AccessUnit->AUSize, InputSampleBuffer.GetInitReference()), "Failed to create video decoder input sample memory buffer", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUTBUFFER);
		VERIFY_HR(InputSample->AddBuffer(InputSampleBuffer.GetReference()), "Failed to set video decoder input buffer with sample", ERRCODE_INTERNAL_COULD_NOT_ADD_INPUT_BUFFER_TO_SAMPLE);
		VERIFY_HR(InputSampleBuffer->Lock(&pbNewBuffer, &dwMaxBufferSize, &dwSize), "Failed to lock video decoder input sample buffer", ERRCODE_INTERNAL_COULD_NOT_LOCK_INPUT_BUFFER);
		FMemory::Memcpy(pbNewBuffer, AU->AccessUnit->AUData, AU->AccessUnit->AUSize);
		VERIFY_HR(InputSampleBuffer->Unlock(), "Failed to unlock video decoder input sample buffer", ERRCODE_INTERNAL_COULD_NOT_UNLOCK_INPUT_BUFFER);
		VERIFY_HR(InputSampleBuffer->SetCurrentLength((DWORD) AU->AccessUnit->AUSize), "Failed to set video decoder input sample buffer length", ERRCODE_INTERNAL_COULD_NOT_SET_BUFFER_CURRENT_LENGTH);
		// Set sample attributes
		llSampleTime = AU->PTS;
		VERIFY_HR(InputSample->SetSampleTime(llSampleTime), "Failed to set video decoder input sample presentation time", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_SAMPLE_TIME);
		llSampleTime = AU->AccessUnit->Duration.GetAsHNS();
		VERIFY_HR(InputSample->SetSampleDuration(llSampleTime), "Failed to set video decode intput sample duration", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_SAMPLE_DURATION);
		llSampleTime = AU->AccessUnit->DTS.GetAsHNS();
		VERIFY_HR(InputSample->SetUINT64(MFSampleExtension_DecodeTimestamp, llSampleTime), "Failed to set video decoder input sample decode time", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_SAMPLE_DECODETIME);
		VERIFY_HR(InputSample->SetUINT32(MFSampleExtension_CleanPoint, AU->bIsIDR ? 1 : 0), "Failed to set video decoder input sample clean point", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_SAMPLE_KEYFRAME);
	}

	while(!TerminateThreadSignal.IsSignaled())
	{
		// Tick output buffer pool to handle state progression of samples in flight
		Renderer->TickOutputBufferPool();
		// Tick platform code in case something needs doing
		PlatformTick();

		// Check if to be flushed now. This may have resulted in AcquireOutputBuffer() to return with no buffer!
		if (FlushDecoderSignal.IsSignaled())
		{
			break;
		}

		if (!CurrentDecoderOutputBuffer.IsValid())
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);

			if (!CreateDecoderOutputBuffer())
			{
				return false;
			}
		}

		CurrentDecoderOutputBuffer->PrepareForProcess();
		DWORD	dwStatus = 0;
		res = DecoderTransform->ProcessOutput(0, 1, &CurrentDecoderOutputBuffer->mOutputBuffer, &dwStatus);
		CurrentDecoderOutputBuffer->UnprepareAfterProcess();

		if (res == MF_E_TRANSFORM_NEED_MORE_INPUT)
		{
			if (bFlush)
			{
				// This means we have received all pending output and are done now.
				// Issue a flush for completeness sake.
				SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
				CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);
				VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0), "Failed to issue video decoder flush command", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_FLUSHCOMMAND);
				// And start over.
				VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0), "Failed to start video decoder", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_START);
				CurrentDecoderOutputBuffer.Reset();
				NumFramesInDecoder = 0;
				InDecoderInput.Empty();
				return true;
			}
			else if (InputSample.IsValid())
			{
				VERIFY_HR(DecoderTransform->ProcessInput(0, InputSample.GetReference(), 0), "Failed to process video decoder input", ERRCODE_INTERNAL_COULD_NOT_PROCESS_INPUT);
				// Used this sample. Have no further input data for now, but continue processing to produce output if possible.
				InputSample = nullptr;
				++NumFramesInDecoder;

				// Add to the list of inputs passed to the decoder.
				InDecoderInput.Add(AU);
				InDecoderInput.Sort([](const TSharedPtrTS<FDecoderInput>& e1, const TSharedPtrTS<FDecoderInput>& e2)
				{
					return e1->PTS < e2->PTS;
				});
			}
			else
			{
				// Need more input but have none right now.
				return true;
			}
		}
		else if (res == MF_E_TRANSFORM_STREAM_CHANGE)
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);
			// Update output type.
			if (!DecoderSetOutputType())
			{
				return false;
			}
		}
		else if (SUCCEEDED(res))
		{
			TRefCountPtr<IMFSample> DecodedOutputSample = CurrentDecoderOutputBuffer->DetachOutputSample();
			CurrentDecoderOutputBuffer.Reset();

			if (DecodedOutputSample)
			{
				// HW accelerated decoding needs to get a buffer from the renderer only now after decoding.
				// SW decoding required it earlier for the call to ProcessOutput().
				if (bIsHardwareAccelerated)
				{
					if (!AcquireOutputBuffer(AU.IsValid() && !AU->AdjustedPTS.IsValid()))
					{
						return false;
					}
				}

				// Check if to be flushed now. This may have resulted in AcquireOutputBuffer() to return with no buffer!
				if (FlushDecoderSignal.IsSignaled())
				{
					break;
				}

				if (!ConvertDecodedImage(DecodedOutputSample))
				{
					return false;
				}
			}
			--NumFramesInDecoder;
		}
		else
		{
			// Error!
			VERIFY_HR(res, "Failed to process video decoder output", ERRCODE_INTERNAL_COULD_NOT_PROCESS_OUTPUT);
			return false;
		}
	}
	return true;
}


bool FVideoDecoderH265::DecodeDummy(TSharedPtrTS<FDecoderInput> AU)
{
	if (AU.IsValid() && AU->AdjustedPTS.IsValid())
	{
		FParamDict OutputBufferSampleProperties;

		OutputBufferSampleProperties.Set("pts", FVariantValue(AU->AdjustedPTS));
		OutputBufferSampleProperties.Set("duration", FVariantValue(AU->AdjustedDuration));
		OutputBufferSampleProperties.Set("is_dummy", FVariantValue(true));

		if (!AcquireOutputBuffer(AU.IsValid() && !AU->AdjustedPTS.IsValid()))
		{
			return false;
		}

		if (CurrentRenderOutputBuffer != nullptr)
		{
			//TODO: THIS BEING A DUMMY - THE DICT IN ANY DECODER OUTPUT BUFFER (is there any?) WILL BE OUTDATED THIS WAY!!
			Renderer->ReturnBuffer(CurrentRenderOutputBuffer, true, OutputBufferSampleProperties);
			CurrentRenderOutputBuffer = nullptr;
		}
	}
	return true;
}


void FVideoDecoderH265::DecoderCreate()
{
	if (InternalDecoderCreate())
	{
		// Configure the decoder with our default values.
		Configure();
		if (!bError)
		{
			StartStreaming();
			bHaveDecoder = true;
		}
		else
		{
			InternalDecoderDestroy();
		}
	}
	else
	{
		bError = true;
		PostError(S_OK, "Failed to create H.265 decoder transform", ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
	}
}


//-----------------------------------------------------------------------------
/**
 * Creates a decoder and checks if it will use hardware or software decoding.
 */
void FVideoDecoderH265::TestHardwareDecoding()
{
	// No-op.
}


//-----------------------------------------------------------------------------
/**
 * Application has entered foreground.
 */
void FVideoDecoderH265::HandleApplicationHasEnteredForeground()
{
	ApplicationRunningSignal.Signal();
}


//-----------------------------------------------------------------------------
/**
 * Application goes into background.
 */
void FVideoDecoderH265::HandleApplicationWillEnterBackground()
{
	ApplicationSuspendConfirmedSignal.Reset();
	ApplicationRunningSignal.Reset();
}


//-----------------------------------------------------------------------------
/**
 * H265 video decoder main threaded decode loop
 */
void FVideoDecoderH265::WorkerThread()
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);

	ApplicationRunningSignal.Signal();
	ApplicationSuspendConfirmedSignal.Reset();

	TSharedPtrTS<FFGBGNotificationHandlers> FGBGHandlers = MakeSharedTS<FFGBGNotificationHandlers>();
	FGBGHandlers->WillEnterBackground = [this]() { HandleApplicationWillEnterBackground(); };
	FGBGHandlers->HasEnteredForeground = [this]() { HandleApplicationHasEnteredForeground(); };
	AddBGFGNotificationHandler(FGBGHandlers);

	CurrentRenderOutputBuffer = nullptr;
	bHaveDecoder = false;
	NumFramesInDecoder = 0;
	bError = false;

	// Set the configured maximum resolution as the current resolution which is used to create the decoder transform.
	CurrentSampleInfo.SetResolution(FStreamCodecInformation::FResolution(Align(Config.MaxFrameWidth, 16), Align(Config.MaxFrameHeight, 16)));
	NewSampleInfo = CurrentSampleInfo;
	SetupBufferAcquisitionProperties();

	// Create the decoded image pool.
	if (!CreateDecodedImagePool())
	{
		bError = true;
	}

	bool bDone = false;
	bool bInDummyDecodeMode = false;
	bool bGotLastSequenceAU = false;
	TOptional<int64> SequenceIndex;

	while(!TerminateThreadSignal.IsSignaled())
	{
		// If in background, wait until we get activated again.
		if (!ApplicationRunningSignal.IsSignaled())
		{
			UE_LOG(LogElectraPlayer, Log, TEXT("FVideoDecoderH265(%p): OnSuspending"), this);
			ApplicationSuspendConfirmedSignal.Signal();
			while(!ApplicationRunningSignal.WaitTimeout(100 * 1000) && !TerminateThreadSignal.IsSignaled())
			{
			}
			UE_LOG(LogElectraPlayer, Log, TEXT("FVideoDecoderH265(%p): OnResuming"), this);
			continue;
		}

		if (!bDrainForCodecChange)
		{
			// Ask the buffer listener for an AU.
			if (!bError && InputBufferListener && NextAccessUnits.IsEmpty())
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
			// Wait for data.
			bool bHaveData = NextAccessUnits.Wait(1000 * 5);
			// When there is data, even and especially after a previous EOD, we are no longer done and idling.
			if (bHaveData && bDone)
			{
				bDone = false;
			}

			// Tick output buffer pool to handle state progression of samples in flight
			Renderer->TickOutputBufferPool();
			// Tick platform code in case something needs doing
			PlatformTick();

			if (bHaveData && Renderer->CanReceiveOutputFrames(1)) // note: the check for full output buffers ignores the NumFramesInDecoder value as we don't seem to receive new data if we don't feed new input
			{
				bool bOk = NextAccessUnits.Dequeue(CurrentAccessUnit);
				MEDIA_UNUSED_VAR(bOk);
				check(bOk);

				if (!bError)
				{
					{
						SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
						CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);
						PrepareAU(CurrentAccessUnit);
					}
					if (!CurrentAccessUnit->AccessUnit->bIsDummyData)
					{
						if (!SequenceIndex.IsSet())
						{
							SequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
						}

						// If this AU falls outside the range where it is to be rendered and it is also discardable
						// we do not need to concern ourselves with it at all.
						if (CurrentAccessUnit->bIsDiscardable && !CurrentAccessUnit->AdjustedPTS.IsValid())
						{
							CurrentAccessUnit.Reset();
							continue;
						}

						// Decode
						if (!bError)
						{
							// Update the buffer acquisition after a change in streams?
							bool bFormatChangedJustNow = CurrentSampleInfo.IsDifferentFromOtherVideo(NewSampleInfo) || bGotLastSequenceAU || SequenceIndex.GetValue() != CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();

							if (bInDummyDecodeMode)
							{
								bFormatChangedJustNow = true;
								bInDummyDecodeMode = false;
							}

							if (!bHaveDecoder)
							{
								SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
								CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);
								DecoderCreate();
							}

							if (bFormatChangedJustNow)
							{
								if (!Decode(nullptr))
								{
									// FIXME: Is this error fatal or safe to ignore?
								}
								NumFramesInDecoder = 0;
								InDecoderInput.Empty();
								CurrentSampleInfo = NewSampleInfo;
								SetupBufferAcquisitionProperties();
							}
							bGotLastSequenceAU = CurrentAccessUnit->AccessUnit->bIsLastInPeriod;
							SequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
							if (!Decode(CurrentAccessUnit))
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
							Decode(nullptr);
							NumFramesInDecoder = 0;
							InDecoderInput.Empty();
						}

						if (!DecodeDummy(CurrentAccessUnit))
						{
							bError = true;
						}
						// DecodeDummy() went through most of the regular path, but has returned the output buffer immediately
						// and can thus always get a new one with no waiting. To avoid draining the player buffer by consuming
						// the dummy AUs at rapid pace we put ourselves to sleep for the duration the AU was supposed to last.
						FMediaRunnable::SleepMicroseconds(CurrentAccessUnit->AdjustedDuration.GetAsMicroseconds());
					}
				}

				// AU payload is no longer needed, just the information. Release the payload.
				if (CurrentAccessUnit.IsValid())
				{
					CurrentAccessUnit->ReleasePayload();
				}
				CurrentAccessUnit.Reset();
			}
			else
			{
				if (!bHaveData)
				{
					// No data. Is the buffer at EOD?
					if (NextAccessUnits.ReachedEOD())
					{
						NotifyReadyBufferListener(true);
						// Are we done yet?
						if (!bDone && !bError)
						{
							bError = !Decode(nullptr);
							NumFramesInDecoder = 0;
							InDecoderInput.Empty();
						}
						bDone = true;
						FMediaRunnable::SleepMilliseconds(10);
					}
				}
				else
				{
					// We could not decode new data as the output buffers are full... notify listener & wait a bit...
					NotifyReadyBufferListener(false);
					FMediaRunnable::SleepMilliseconds(10);
				}
			}
		}
		else
		{
			bError = !Decode(nullptr);
			break;
		}

		// Flush?
		if (FlushDecoderSignal.IsSignaled())
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);

			// Destroy and re-create.
			InternalDecoderDestroy();

			ReturnUnusedFrame();
			NextAccessUnits.Empty();
			CurrentAccessUnit.Reset();

			FlushDecoderSignal.Reset();
			DecoderFlushedSignal.Signal();
			NumFramesInDecoder = 0;
			InDecoderInput.Empty();
			SequenceIndex.Reset();
			CurrentSampleInfo.SetResolution(FStreamCodecInformation::FResolution(Align(Config.MaxFrameWidth, 16), Align(Config.MaxFrameHeight, 16)));
			NewSampleInfo = CurrentSampleInfo;

			// Reset done state.
			bDone = false;
		}
	}

	InternalDecoderDestroy();
	ReturnUnusedFrame();
	DestroyDecodedImagePool();
	NextAccessUnits.Empty();
	InDecoderInput.Empty();

	RemoveBGFGNotificationHandler(FGBGHandlers);

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

} // namespace Electra

#endif // ELECTRA_ENABLE_MFDECODER


#endif
