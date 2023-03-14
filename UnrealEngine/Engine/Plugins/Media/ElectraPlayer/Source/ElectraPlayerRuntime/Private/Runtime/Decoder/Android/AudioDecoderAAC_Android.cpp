// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCore.h"
#include "PlayerRuntimeGlobal.h"

#include "StreamAccessUnitBuffer.h"
#include "Decoder/AudioDecoderAAC.h"
#include "Renderer/RendererBase.h"
#include "Player/PlayerSessionServices.h"
#include "Utilities/Utilities.h"
#include "Utilities/UtilsMPEGAudio.h"
#include "Utilities/StringHelpers.h"
#include "Utilities/AudioChannelMapper.h"
#include "DecoderErrors_Android.h"
#include "HAL/LowLevelMemTracker.h"
#include "ElectraPlayerPrivate.h"

#include "AudioDecoderAAC_JavaWrapper_Android.h"

DECLARE_CYCLE_STAT(TEXT("FAudioDecoderAAC::Decode()"), STAT_ElectraPlayer_AudioAACDecode, STATGROUP_ElectraPlayer);
DECLARE_CYCLE_STAT(TEXT("FAudioDecoderAAC::ConvertOutput()"), STAT_ElectraPlayer_AudioAACConvertOutput, STATGROUP_ElectraPlayer);


namespace Electra
{

namespace AACDecoderChannelOutputMappingAndroid
{
	#define CP FAudioChannelMapper::EChannelPosition
	static const CP _1[] = { CP::C };
	static const CP _2[] = { CP::L, CP::R };
	static const CP _3[] = { CP::L, CP::R, CP::C };
	static const CP _4[] = { CP::L, CP::R, CP::C, CP::Cs };
	static const CP _5[] = { CP::L, CP::R, CP::C, CP::Ls, CP::Rs };
	static const CP _6[] = { CP::L, CP::R, CP::C, CP::LFE, CP::Ls, CP::Rs };
	static const CP _7[] = { CP::L, CP::R, CP::C, CP::LFE, CP::Ls, CP::Rs, CP::Cs };
	static const CP _8[] = { CP::L, CP::R, CP::C, CP::LFE, CP::Ls, CP::Rs, CP::Lsr, CP::Rsr };
	static const CP * const Order[] = { _1, _2, _3, _4, _5, _6, _7, _8 };
	#undef CP
};


/**
 * AAC audio decoder class implementation.
**/
class FAudioDecoderAAC : public IAudioDecoderAAC, public FMediaThread
{
public:
	static bool Startup(const IAudioDecoderAAC::FSystemConfiguration& InConfig);
	static void Shutdown();

	static bool CanDecodeStream(const FStreamCodecInformation& InCodecInfo);

	FAudioDecoderAAC();
	virtual ~FAudioDecoderAAC();

	virtual void SetPlayerSessionServices(IPlayerSessionServices* SessionServices) override;

	virtual void Open(const FInstanceConfiguration& InConfig) override;
	virtual void Close() override;

	virtual void SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer) override;

	virtual void SetAUInputBufferListener(IAccessUnitBufferListener* Listener) override;

	virtual void SetReadyBufferListener(IDecoderOutputBufferListener* Listener) override;

	virtual void AUdataPushAU(FAccessUnit* InAccessUnit) override;
	virtual void AUdataPushEOD() override;
	virtual void AUdataClearEOD() override;
	virtual void AUdataFlushEverything() override;

	virtual void Android_SuspendOrResumeDecoder(bool bSuspend) override;

private:
	struct FDecoderInput
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
		int64			PTS = 0;
		int64			EndPTS = 0;
		FTimeValue		AdjustedPTS;
		FTimeValue		AdjustedDuration;
		FTimeValue		StartOverlapDuration;
		FTimeValue		EndOverlapDuration;
	};

	enum class EDecodeResult
	{
		Ok,
		Fail,
		SessionLost,
		TryAgainLater,
	};

	enum class EOutputResult
	{
		Ok,
		Fail,
		TryAgainLater,
		EOS
	};

	enum class EDecodingState
	{
		Regular,				//!< Regular decoding mode
		Dummy,					//!< Dummy decoding mode
		Draining				//!< Draining decoder to get all pending output.
	};

	void StartThread();
	void StopThread();
	void WorkerThread();

	bool InternalDecoderCreate();
	void InternalDecoderDestroy();

	bool CreateDecodedSamplePool();
	void DestroyDecodedSamplePool();

	bool AcquireOutputBuffer();
	void ReturnUnusedOutputBuffer();

	void NotifyReadyBufferListener(bool bHaveOutput);

	bool IsDifferentFormat();
	void PrepareAU(TSharedPtrTS<FDecoderInput> InAccessUnit);
	void GetAndPrepareInputAU();
	bool ParseConfigRecord();

	bool CheckForFlush();
	EOutputResult GetOutput();
	EDecodeResult Decode();
	EDecodeResult DecodeDummy();
	EDecodeResult DrainDecoder();
	bool DecodeAU(TSharedPtrTS<FDecoderInput> AU);

	void PostError(int32 ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error = UEMEDIA_ERROR_OK);
	void LogMessage(IInfoLog::ELevel Level, const FString& Message);

	void HandleApplicationHasEnteredForeground();
	void HandleApplicationWillEnterBackground();

	FInstanceConfiguration													Config;

	TAccessUnitQueue<TSharedPtrTS<FDecoderInput>>							NextAccessUnits;

	FMediaEvent																ApplicationRunningSignal;
	FMediaEvent																ApplicationSuspendConfirmedSignal;
	int32																	ApplicationSuspendCount = 0;

	FMediaEvent																TerminateThreadSignal;
	FMediaEvent																FlushDecoderSignal;
	FMediaEvent																DecoderFlushedSignal;
	bool																	bThreadStarted = false;

	TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe>							Renderer;
	int32																	MaxDecodeBufferSize = 0;

	FMediaCriticalSection													ListenerMutex;
	IAccessUnitBufferListener*												InputBufferListener = nullptr;
	IDecoderOutputBufferListener*											ReadyBufferListener = nullptr;

	IPlayerSessionServices* 												SessionServices = nullptr;

	TSharedPtr<MPEG::FAACDecoderConfigurationRecord, ESPMode::ThreadSafe>	ConfigRecord;
	TSharedPtrTS<FAccessUnit::CodecData>									CurrentCodecData;

	TSharedPtr<IAndroidJavaAACAudioDecoder, ESPMode::ThreadSafe>			DecoderInstance;
	IAndroidJavaAACAudioDecoder::FOutputFormatInfo							CurrentOutputFormatInfo;
	bool																	bIsOutputFormatInfoValid = false;
	EDecodingState															DecodingState = EDecodingState::Regular;
	int64																	LastPushedPresentationTimeUs = 0;
	bool																	bGotEOS = false;

	TArray<TSharedPtrTS<FDecoderInput>>										InDecoderInput;
	TSharedPtrTS<FDecoderInput>												CurrentAccessUnit;
	TOptional<int64>														CurrentSequenceIndex;

	IMediaRenderer::IBuffer*												CurrentOutputBuffer = nullptr;
	FParamDict																BufferAcquireOptions;
	FParamDict																OutputBufferSampleProperties;

	int32																	PCMBufferSize = 0;
	int16*																	PCMBuffer = nullptr;
	FAudioChannelMapper														ChannelMapper;
	static const uint8														NumChannelsForConfig[16];
public:
	static FSystemConfiguration												SystemConfig;
};

const uint8								FAudioDecoderAAC::NumChannelsForConfig[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 7, 8, 0, 8, 0 };
IAudioDecoderAAC::FSystemConfiguration	FAudioDecoderAAC::SystemConfig;


/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

IAudioDecoderAAC::FSystemConfiguration::FSystemConfiguration()
{
	ThreadConfig.Decoder.Priority = TPri_Normal;
	ThreadConfig.Decoder.StackSize = 65536;
	ThreadConfig.Decoder.CoreAffinity = -1;
}

IAudioDecoderAAC::FInstanceConfiguration::FInstanceConfiguration()
	: ThreadConfig(FAudioDecoderAAC::SystemConfig.ThreadConfig)
{
}

bool IAudioDecoderAAC::Startup(const IAudioDecoderAAC::FSystemConfiguration& InConfig)
{
	return FAudioDecoderAAC::Startup(InConfig);
}

void IAudioDecoderAAC::Shutdown()
{
	FAudioDecoderAAC::Shutdown();
}

bool IAudioDecoderAAC::CanDecodeStream(const FStreamCodecInformation& InCodecInfo)
{
	return FAudioDecoderAAC::CanDecodeStream(InCodecInfo);
}

IAudioDecoderAAC* IAudioDecoderAAC::Create()
{
	return new FAudioDecoderAAC;
}

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/


//-----------------------------------------------------------------------------
/**
 * Decoder system startup
 *
 * @param InConfig
 *
 * @return
 */
bool FAudioDecoderAAC::Startup(const IAudioDecoderAAC::FSystemConfiguration& InConfig)
{
	SystemConfig = InConfig;
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Decoder system shutdown.
 */
void FAudioDecoderAAC::Shutdown()
{
}


//-----------------------------------------------------------------------------
/**
 * Determines if this stream can be decoded based on its channel configuration.
 * 
 * @return
 */
bool FAudioDecoderAAC::CanDecodeStream(const FStreamCodecInformation& InCodecInfo)
{
	return InCodecInfo.GetChannelConfiguration() != 0;
}


//-----------------------------------------------------------------------------
/**
 * Constructor
 */
FAudioDecoderAAC::FAudioDecoderAAC()
	: FMediaThread("ElectraPlayer::AAC decoder")
{
}


//-----------------------------------------------------------------------------
/**
 * Destructor
 */
FAudioDecoderAAC::~FAudioDecoderAAC()
{
	Close();
}


//-----------------------------------------------------------------------------
/**
 * Sets the owning player's session service interface.
 *
 * @param InSessionServices
 *
 * @return
 */
void FAudioDecoderAAC::SetPlayerSessionServices(IPlayerSessionServices* InSessionServices)
{
	SessionServices = InSessionServices;
}


//-----------------------------------------------------------------------------
/**
 * Creates the decoded sample blocks output buffers.
 *
 * @return
 */
bool FAudioDecoderAAC::CreateDecodedSamplePool()
{
	check(Renderer);
	FParamDict poolOpts;
	uint32 frameSize = sizeof(int16) * 8 * 2048;
	poolOpts.Set("max_buffer_size", FVariantValue((int64) frameSize));
	poolOpts.Set("num_buffers", FVariantValue((int64) 8));
	poolOpts.Set("samples_per_block", FVariantValue((int64) 2048));
	poolOpts.Set("max_channels", FVariantValue((int64) 8));

	UEMediaError Error = Renderer->CreateBufferPool(poolOpts);
	check(Error == UEMEDIA_ERROR_OK);

	if (Error != UEMEDIA_ERROR_OK)
	{
		PostError(0, "Failed to create sample pool", ERRCODE_INTERNAL_ANDROID_COULD_NOT_CREATE_SAMPLE_POOL, Error);
	}

	MaxDecodeBufferSize = (int32) Renderer->GetBufferPoolProperties().GetValue("max_buffers").GetInt64();

	return Error == UEMEDIA_ERROR_OK;
}


//-----------------------------------------------------------------------------
/**
 * Destroys the pool of decoded sample blocks.
 */
void FAudioDecoderAAC::DestroyDecodedSamplePool()
{
	Renderer->ReleaseBufferPool();
}


//-----------------------------------------------------------------------------
/**
 * Opens a decoder instance
 *
 * @param InConfig
 */
void FAudioDecoderAAC::Open(const IAudioDecoderAAC::FInstanceConfiguration& InConfig)
{
	Config = InConfig;
	StartThread();
}


//-----------------------------------------------------------------------------
/**
 * Closes the decoder instance.
 */
void FAudioDecoderAAC::Close()
{
	StopThread();
}


//-----------------------------------------------------------------------------
/**
 * Sets a new renderer.
 *
 * @param InRenderer
 */
void FAudioDecoderAAC::SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer)
{
	Renderer = InRenderer;
}


//-----------------------------------------------------------------------------
/**
 * Sets an AU input buffer listener.
 *
 * @param InListener
 */
void FAudioDecoderAAC::SetAUInputBufferListener(IAccessUnitBufferListener* InListener)
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
void FAudioDecoderAAC::SetReadyBufferListener(IDecoderOutputBufferListener* InListener)
{
	FMediaCriticalSection::ScopedLock lock(ListenerMutex);
	ReadyBufferListener = InListener;
}


//-----------------------------------------------------------------------------
/**
 * Creates and runs the decoder thread.
 */
void FAudioDecoderAAC::StartThread()
{
	ThreadSetPriority(Config.ThreadConfig.Decoder.Priority);
	ThreadSetStackSize(Config.ThreadConfig.Decoder.StackSize);
	ThreadSetCoreAffinity(Config.ThreadConfig.Decoder.CoreAffinity);
	ThreadStart(FMediaRunnable::FStartDelegate::CreateRaw(this, &FAudioDecoderAAC::WorkerThread));
	bThreadStarted = true;
}


//-----------------------------------------------------------------------------
/**
 * Stops the decoder thread.
 */
void FAudioDecoderAAC::StopThread()
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
 * Called to receive a new input access unit for decoding.
 *
 * @param InAccessUnit
 */
void FAudioDecoderAAC::AUdataPushAU(FAccessUnit* InAccessUnit)
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
void FAudioDecoderAAC::AUdataPushEOD()
{
	NextAccessUnits.SetEOD();
}


//-----------------------------------------------------------------------------
/**
 * Notifies the decoder that there may be further access units.
 */
void FAudioDecoderAAC::AUdataClearEOD()
{
	NextAccessUnits.ClearEOD();
}


//-----------------------------------------------------------------------------
/**
 * Flushes the decoder and clears the input access unit buffer.
 */
void FAudioDecoderAAC::AUdataFlushEverything()
{
	FlushDecoderSignal.Signal();
	DecoderFlushedSignal.WaitAndReset();
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
void FAudioDecoderAAC::NotifyReadyBufferListener(bool bHaveOutput)
{
	if (ReadyBufferListener)
	{
		IDecoderOutputBufferListener::FDecodeReadyStats stats;
		stats.MaxDecodedElementsReady = MaxDecodeBufferSize;
		stats.NumElementsInDecoder = CurrentOutputBuffer ? 1 : 0;
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
 * Posts an error to the session service error listeners.
 *
 * @param ApiReturnValue
 * @param Message
 * @param Code
 * @param Error
 */
void FAudioDecoderAAC::PostError(int32 ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error)
{
	check(SessionServices);	// there better be a session service interface to receive the error!
	if (SessionServices)
	{
		FErrorDetail err;
		err.SetError(Error != UEMEDIA_ERROR_OK ? Error : UEMEDIA_ERROR_DETAIL);
		err.SetFacility(Facility::EFacility::AACDecoder);
		err.SetCode(Code);
		err.SetMessage(Message);
		err.SetPlatformMessage(FString::Printf(TEXT("%d (0x%08x)"), (int32) ApiReturnValue, (int32) ApiReturnValue));
		SessionServices->PostError(err);
	}
}


//-----------------------------------------------------------------------------
/**
 * Sends a log message to the session service log.
 *
 * @param Level
 * @param Message
 */
void FAudioDecoderAAC::LogMessage(IInfoLog::ELevel Level, const FString& Message)
{
	if (SessionServices)
	{
		SessionServices->PostLog(Facility::EFacility::AACDecoder, Level, Message);
	}
}


//-----------------------------------------------------------------------------
/**
 * Creates an audio decoder.
 *
 * Note: This requires the configuration record to have been parsed successfully.
 * 
 * @return
 */
bool FAudioDecoderAAC::InternalDecoderCreate()
{
	if (!ConfigRecord.IsValid())
	{
		PostError(0, "No CSD to create audio decoder with", ERRCODE_INTERNAL_ANDROID_COULD_NOT_CREATE_AUDIO_DECODER);
		return false;
	}

	InternalDecoderDestroy();

	DecoderInstance = IAndroidJavaAACAudioDecoder::Create();
	bIsOutputFormatInfoValid = false;
	int32 result = DecoderInstance->InitializeDecoder(*ConfigRecord);
	if (result)
	{
		PostError(result, "Failed to create decoder", ERRCODE_INTERNAL_ANDROID_COULD_NOT_CREATE_AUDIO_DECODER);
		return false;
	}
	// Start it.
	result = DecoderInstance->Start();
	if (result)
	{
		PostError(result, "Failed to start decoder", ERRCODE_INTERNAL_ANDROID_COULD_NOT_START_DECODER);
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Destroys the audio decoder.
 */
void FAudioDecoderAAC::InternalDecoderDestroy()
{
	if (DecoderInstance.IsValid())
	{
		DecoderInstance->Flush();
		DecoderInstance->Stop();
		DecoderInstance->ReleaseDecoder();
		DecoderInstance.Reset();
	}
}


//-----------------------------------------------------------------------------
/**
 * Gets a new output buffer if we currently have none.
 * 
 * @return
 */
bool FAudioDecoderAAC::AcquireOutputBuffer()
{
	// Need a new output buffer?
	if (CurrentOutputBuffer == nullptr)
	{
		UEMediaError bufResult = Renderer->AcquireBuffer(CurrentOutputBuffer, 0, BufferAcquireOptions);
		check(bufResult == UEMEDIA_ERROR_OK || bufResult == UEMEDIA_ERROR_INSUFFICIENT_DATA);
		if (bufResult != UEMEDIA_ERROR_OK && bufResult != UEMEDIA_ERROR_INSUFFICIENT_DATA)
		{
			PostError(0, "Failed to acquire sample buffer", ERRCODE_INTERNAL_ANDROID_COULD_NOT_GET_RENDER_BUFFER, bufResult);
			return false;
		}
	}
	// We may not have gotten a new buffer, but there was no failure, so returning true.
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Returns the acquired sample output buffer back to the renderer without having it rendered.
 */
void FAudioDecoderAAC::ReturnUnusedOutputBuffer()
{
	if (CurrentOutputBuffer)
	{
		OutputBufferSampleProperties.Clear();
		Renderer->ReturnBuffer(CurrentOutputBuffer, false, OutputBufferSampleProperties);
		CurrentOutputBuffer = nullptr;
	}
}


//-----------------------------------------------------------------------------
/**
 * Checks if the codec specific format has changed.
 *
 * @return
 */
bool FAudioDecoderAAC::IsDifferentFormat()
{
	if (CurrentAccessUnit.IsValid() && CurrentAccessUnit->AccessUnit->AUCodecData.IsValid() && (!CurrentCodecData.IsValid() || CurrentCodecData->CodecSpecificData != CurrentAccessUnit->AccessUnit->AUCodecData->CodecSpecificData))
	{
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
/**
 * Prepares the AU for decoding and calculates sample trimming time values.
 */
void FAudioDecoderAAC::PrepareAU(TSharedPtrTS<FDecoderInput> AU)
{
	if (!AU->bHasBeenPrepared)
	{
		AU->bHasBeenPrepared = true;

		// Does this AU fall (partially) outside the range for rendering?
		FTimeValue StartTime = AU->AccessUnit->PTS;
		FTimeValue EndTime = AU->AccessUnit->PTS + AU->AccessUnit->Duration;
		AU->PTS = StartTime.GetAsMicroseconds();		// The PTS we give the decoder no matter any adjustment.
		AU->EndPTS = EndTime.GetAsMicroseconds();	// End PTS we need to check the PTS value returned by the decoder against.
		AU->StartOverlapDuration.SetToZero();
		AU->EndOverlapDuration.SetToZero();
		if (AU->AccessUnit->EarliestPTS.IsValid())
		{
			// If the end time of the AU is before the earliest render PTS we do not need to decode it.
			if (EndTime <= AU->AccessUnit->EarliestPTS)
			{
				StartTime.SetToInvalid();
			}
			else if (StartTime < AU->AccessUnit->EarliestPTS)
			{
				AU->StartOverlapDuration = AU->AccessUnit->EarliestPTS - StartTime;
				StartTime = AU->AccessUnit->EarliestPTS;
			}
		}
		if (StartTime.IsValid() && AU->AccessUnit->LatestPTS.IsValid())
		{
			// If the start time is behind the latest render PTS we donot need to decode it.
			if (StartTime >= AU->AccessUnit->LatestPTS)
			{
				StartTime.SetToInvalid();
			}
			else if (EndTime >= AU->AccessUnit->LatestPTS)
			{
				AU->EndOverlapDuration = EndTime - AU->AccessUnit->LatestPTS;
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


//-----------------------------------------------------------------------------
/**
 * Tries to get decoded output.
 *
 * @return
 */
FAudioDecoderAAC::EOutputResult FAudioDecoderAAC::GetOutput()
{
	// Without a decoder there is nothing pulled right now.
	if (!DecoderInstance.IsValid())
	{
		return EOutputResult::TryAgainLater;
	}

	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACConvertOutput);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACConvertOutput);

	// Get a buffer to put the output into.
	if (!AcquireOutputBuffer())
	{
		return EOutputResult::Fail;
	}

	// Did we get a buffer?
	bool bHaveAvailSmpBlk = CurrentOutputBuffer != nullptr;
	// If the renderer cannot currently take on another buffer we pretend we did not
	// get a buffer in order to let the buffer listener know the output is stalled.
	if (Renderer.IsValid() && !Renderer->CanReceiveOutputFrames(1))
	{
		bHaveAvailSmpBlk = false;
	}
	NotifyReadyBufferListener(bHaveAvailSmpBlk);
	if (!bHaveAvailSmpBlk)
	{
		return EOutputResult::TryAgainLater;
	}

	// Check if there is available output from the decoder.
	IAndroidJavaAACAudioDecoder::FOutputBufferInfo OutputBufferInfo;
	int32 Result = DecoderInstance->DequeueOutputBuffer(OutputBufferInfo, 0);
	if (Result != 0)
	{
		PostError(Result, "Failed to get decoder output buffer", ERRCODE_INTERNAL_ANDROID_COULD_NOT_GET_OUTPUT_BUFFER);
		return EOutputResult::Fail;
	}

	if (OutputBufferInfo.BufferIndex >= 0)
	{
		// Is this an empty EOS buffer?
		if (OutputBufferInfo.bIsEOS && OutputBufferInfo.Size == 0)
		{
			void* CopyBuffer = (void*)PCMBuffer;
			Result = DecoderInstance->GetOutputBufferAndRelease(CopyBuffer, PCMBufferSize, OutputBufferInfo);
			LogMessage(IInfoLog::ELevel::Info, FString::Printf(TEXT("Got an EOS buffer, release buffer returned %d"), Result));
			return EOutputResult::EOS;
		}

		// Check if we got a MediaCodec_INFO_OUTPUT_FORMAT_CHANGED with valid format information.
		if (!bIsOutputFormatInfoValid)
		{
			IAndroidJavaAACAudioDecoder::FOutputFormatInfo OutputFormatInfo;
			LogMessage(IInfoLog::ELevel::Info, TEXT("Got a buffer without a preceeding format info message, querying current buffer for properties."));
			Result = DecoderInstance->GetOutputFormatInfo(OutputFormatInfo, OutputBufferInfo.BufferIndex);
			if (Result == 0)
			{
				CurrentOutputFormatInfo = OutputFormatInfo;
				bIsOutputFormatInfoValid = CurrentOutputFormatInfo.NumChannels > 0 && CurrentOutputFormatInfo.SampleRate > 0;
				LogMessage(IInfoLog::ELevel::Info, FString::Printf(TEXT("Info from current buffer: %d channels @ %d Hz; size=%d, eos=%d, cfg=%d"), CurrentOutputFormatInfo.NumChannels, CurrentOutputFormatInfo.SampleRate, OutputBufferInfo.Size, OutputBufferInfo.bIsEOS, OutputBufferInfo.bIsConfig));
			}
			else
			{
				LogMessage(IInfoLog::ELevel::Info, TEXT("Failed to get properties from current buffer."));
				if (ConfigRecord.IsValid())
				{
					CurrentOutputFormatInfo.NumChannels = ConfigRecord->PSSignal > 0 ? 2 : NumChannelsForConfig[ConfigRecord->ChannelConfiguration];
					CurrentOutputFormatInfo.SampleRate = ConfigRecord->ExtSamplingFrequency ? ConfigRecord->ExtSamplingFrequency : ConfigRecord->SamplingRate;
					bIsOutputFormatInfoValid = CurrentOutputFormatInfo.NumChannels > 0 && CurrentOutputFormatInfo.SampleRate > 0;
					LogMessage(IInfoLog::ELevel::Info, FString::Printf(TEXT("Info from config record: %d channels @ %d Hz; sbr=%d, ps=%d; size=%d, eos=%d, cfg=%d"), CurrentOutputFormatInfo.NumChannels, CurrentOutputFormatInfo.SampleRate, ConfigRecord->SBRSignal, ConfigRecord->PSSignal, OutputBufferInfo.Size, OutputBufferInfo.bIsEOS, OutputBufferInfo.bIsConfig));
				}
			}
		}

		bool bEOS = OutputBufferInfo.bIsEOS;
		// Get the frontmost AU info that should correspond to this output.
		TSharedPtrTS<FDecoderInput> MatchingInput;
		if (InDecoderInput.Num())
		{
			MatchingInput = InDecoderInput[0];
			InDecoderInput.RemoveAt(0);
		}
		else
		{
			// When we have no info then this should better be an EOS buffer.
			check(bEOS);
			// Force EOS regardless.
			bEOS = true;
		}

		int32 SamplingRate = CurrentOutputFormatInfo.SampleRate;
		int32 NumberOfChannels = CurrentOutputFormatInfo.NumChannels;
		int32 OutputByteCount = OutputBufferInfo.Size;
		int32 nBytesPerSample = NumberOfChannels * sizeof(int16);
		int32 nSamplesProduced = OutputByteCount / nBytesPerSample;

		check(PCMBufferSize >= OutputByteCount);
		if (PCMBufferSize >= OutputByteCount)
		{
			void* CopyBuffer = (void*)PCMBuffer;
			Result = DecoderInstance->GetOutputBufferAndRelease(CopyBuffer, PCMBufferSize, OutputBufferInfo);
			if (Result == 0)
			{
				if (!ChannelMapper.IsInitialized())
				{
					if (NumberOfChannels)
					{
						TArray<FAudioChannelMapper::FSourceLayout> Layout;
						const FAudioChannelMapper::EChannelPosition* ChannelPositions = AACDecoderChannelOutputMappingAndroid::Order[NumberOfChannels - 1];
						for(uint32 i=0; i<NumberOfChannels; ++i)
						{
							FAudioChannelMapper::FSourceLayout lo;
							lo.ChannelPosition = ChannelPositions[i];
							Layout.Emplace(MoveTemp(lo));
						}
						if (ChannelMapper.Initialize(sizeof(int16), Layout))
						{
							// Ok.
						}
						else
						{
							PostError(0, "Failed to initialize audio channel mapper", ERRCODE_INTERNAL_ANDROID_UNSUPPORTED_NUMBER_OF_AUDIO_CHANNELS);
							return EOutputResult::Fail;
						}
					}
					else
					{
						PostError(0, "Bad number (0) of decoded audio channels", ERRCODE_INTERNAL_ANDROID_UNSUPPORTED_NUMBER_OF_AUDIO_CHANNELS);
						return EOutputResult::Fail;
					}
				}

				int32 ByteOffsetToFirstSample = 0;
				if (MatchingInput.IsValid() && (MatchingInput->StartOverlapDuration.GetAsHNS() || MatchingInput->EndOverlapDuration.GetAsHNS()))
				{
					int32 SkipStartSampleNum = (int32) (MatchingInput->StartOverlapDuration.GetAsHNS() * SamplingRate / 10000000);
					int32 SkipEndSampleNum = (int32) (MatchingInput->EndOverlapDuration.GetAsHNS() * SamplingRate / 10000000);

					if (SkipStartSampleNum + SkipEndSampleNum < nSamplesProduced)
					{
						ByteOffsetToFirstSample = SkipStartSampleNum * nBytesPerSample;
						nSamplesProduced -= SkipStartSampleNum;
						nSamplesProduced -= SkipEndSampleNum;
					}
					else
					{
						nSamplesProduced = 0;
					}
				}

				if (nSamplesProduced)
				{
					FTimeValue Duration;
					Duration.SetFromND(nSamplesProduced, SamplingRate);
					FTimeValue PTS = MatchingInput.IsValid() ? MatchingInput->AdjustedPTS : FTimeValue(FTimeValue::MicrosecondsToHNS(OutputBufferInfo.PresentationTimestamp), CurrentSequenceIndex.Get(0));
    
					void* OutputBufferAddr = CurrentOutputBuffer->GetBufferProperties().GetValue("address").GetPointer();
					uint32 OutputBufferSize = (uint32)CurrentOutputBuffer->GetBufferProperties().GetValue("size").GetInt64();
					ChannelMapper.MapChannels(OutputBufferAddr, OutputBufferSize, AdvancePointer(PCMBuffer, ByteOffsetToFirstSample), nSamplesProduced * nBytesPerSample, nSamplesProduced);
    
					OutputBufferSampleProperties.Clear();
					OutputBufferSampleProperties.Set("num_channels",  FVariantValue((int64)ChannelMapper.GetNumTargetChannels()));
					OutputBufferSampleProperties.Set("byte_size",     FVariantValue((int64)(ChannelMapper.GetNumTargetChannels() * nSamplesProduced * sizeof(int16))));
					OutputBufferSampleProperties.Set("sample_rate",   FVariantValue((int64) SamplingRate));
					OutputBufferSampleProperties.Set("duration",      FVariantValue(Duration));
					OutputBufferSampleProperties.Set("pts",           FVariantValue(PTS));
    
					Renderer->ReturnBuffer(CurrentOutputBuffer, true, OutputBufferSampleProperties);
					CurrentOutputBuffer = nullptr;
				}
				else
				{
					ReturnUnusedOutputBuffer();
				}
				return bEOS ? EOutputResult::EOS : EOutputResult::Ok;
			}
			else
			{
				PostError(0, "Failed to get decoder output buffer", ERRCODE_INTERNAL_ANDROID_COULD_NOT_GET_OUTPUT_BUFFER);
				return EOutputResult::Fail;
			}
		}
		else
		{
			PostError(0, "Output buffer not large enough", ERRCODE_INTERNAL_ANDROID_RENDER_BUFFER_TOO_SMALL);
			return EOutputResult::Fail;
		}
	}
	else if (OutputBufferInfo.BufferIndex == IAndroidJavaAACAudioDecoder::FOutputBufferInfo::EBufferIndexValues::MediaCodec_INFO_TRY_AGAIN_LATER)
	{
	}
	else if (OutputBufferInfo.BufferIndex == IAndroidJavaAACAudioDecoder::FOutputBufferInfo::EBufferIndexValues::MediaCodec_INFO_OUTPUT_FORMAT_CHANGED)
	{
		IAndroidJavaAACAudioDecoder::FOutputFormatInfo OutputFormatInfo;
		Result = DecoderInstance->GetOutputFormatInfo(OutputFormatInfo, -1);
		if (Result == 0)
		{
			CurrentOutputFormatInfo = OutputFormatInfo;
			bIsOutputFormatInfoValid = CurrentOutputFormatInfo.NumChannels > 0 && CurrentOutputFormatInfo.SampleRate > 0;
			LogMessage(IInfoLog::ELevel::Info, FString::Printf(TEXT("Output format: %d channels @ %d Hz"), CurrentOutputFormatInfo.NumChannels, CurrentOutputFormatInfo.SampleRate));
		}
		else
		{
			PostError(Result, "Failed to get decoder output format", ERRCODE_INTERNAL_ANDROID_COULD_NOT_GET_OUTPUT_FORMAT);
			return EOutputResult::Fail;
		}
	}
	else if (OutputBufferInfo.BufferIndex == IAndroidJavaAACAudioDecoder::FOutputBufferInfo::EBufferIndexValues::MediaCodec_INFO_OUTPUT_BUFFERS_CHANGED)
	{
		// No-op as this is the Result of a deprecated API we are not using.
	}
	else
	{
		// What new value might this be?
		PostError(OutputBufferInfo.BufferIndex, "Unhandled output buffer index value", ERRCODE_INTERNAL_ANDROID_INTERNAL);
		return EOutputResult::Fail;
	}

	return EOutputResult::TryAgainLater;
}


//-----------------------------------------------------------------------------
/**
 * Decodes an access unit
 *
 * @return
 */
FAudioDecoderAAC::EDecodeResult FAudioDecoderAAC::Decode()
{
	// To decode we need to have a decoder.
	if (!DecoderInstance.IsValid())
	{
		return EDecodeResult::Fail;
	}
	// No input, nothing to do.
	if (!CurrentAccessUnit.IsValid())
	{
		return EDecodeResult::Ok;
	}
	// Must be actual data!
	if (CurrentAccessUnit->AccessUnit->bIsDummyData)
	{
		return EDecodeResult::Fail;
	}

	// Check if there is an available input buffer.
	int32 InputBufferIndex = DecoderInstance->DequeueInputBuffer(0);
	if (InputBufferIndex >= 0)
	{
		int32 Result = DecoderInstance->QueueInputBuffer(InputBufferIndex, CurrentAccessUnit->AccessUnit->AUData, CurrentAccessUnit->AccessUnit->AUSize, CurrentAccessUnit->PTS);
		if (Result == 0)
		{
			LastPushedPresentationTimeUs = CurrentAccessUnit->PTS;
			InDecoderInput.Add(CurrentAccessUnit);
			InDecoderInput.Sort([](const TSharedPtr<FDecoderInput, ESPMode::ThreadSafe>& a, const TSharedPtr<FDecoderInput, ESPMode::ThreadSafe>& b) { return a->PTS < b->PTS; });
			return EDecodeResult::Ok;
		}
		else
		{
			PostError(Result, "Failed to submit decoder input buffer", ERRCODE_INTERNAL_ANDROID_FAILED_TO_DECODE_AUDIO);
			return EDecodeResult::Fail;
		}
	}
	else if (InputBufferIndex == -1)
	{
		// No available input buffer. Try later.
		return EDecodeResult::TryAgainLater;
	}
	else
	{
		PostError(InputBufferIndex, "Failed to get a decoder input buffer", ERRCODE_INTERNAL_ANDROID_COULD_NOT_GET_INPUT_BUFFER);
		return EDecodeResult::Fail;
	}
}


//-----------------------------------------------------------------------------
/**
 * Send an EOS to the decoder to drain it and get all pending output.
 *
 * @return
 */
FAudioDecoderAAC::EDecodeResult FAudioDecoderAAC::DrainDecoder()
{
	// If there is no decoder we cannot send and EOS. We need to fail this since without a decoder
	// we will not get the EOS back to stay in the regular flow.
	if (!DecoderInstance.IsValid())
	{
		return EDecodeResult::Fail;
	}

	int32 Result = -1;
	int32 InputBufferIndex = DecoderInstance->DequeueInputBuffer(0);
	if (InputBufferIndex >= 0)
	{
		Result = DecoderInstance->QueueEOSInputBuffer(InputBufferIndex, LastPushedPresentationTimeUs);
		check(Result == 0);
		if (Result == 0)
		{
			return EDecodeResult::Ok;
		}
		else
		{
			PostError(Result, "Failed to submit decoder EOS input buffer", ERRCODE_INTERNAL_ANDROID_FAILED_TO_DECODE_AUDIO);
			return EDecodeResult::Fail;
		}
	}
	else if (InputBufferIndex == -1)
	{
		// No available input buffer. Try later.
		return EDecodeResult::TryAgainLater;
	}
	else
	{
		PostError(InputBufferIndex, "Failed to get a decoder input buffer for EOS", ERRCODE_INTERNAL_ANDROID_COULD_NOT_GET_INPUT_BUFFER);
		return EDecodeResult::Fail;
	}
}


//-----------------------------------------------------------------------------
/**
 * "Decodes" a dummy access unit
 *
 * @return
 */
FAudioDecoderAAC::EDecodeResult FAudioDecoderAAC::DecodeDummy()
{
	if (!CurrentAccessUnit.IsValid())
	{
		return EDecodeResult::TryAgainLater;
	}

	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACConvertOutput);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACConvertOutput);

	// Get a buffer to put the output into.
	if (!AcquireOutputBuffer())
	{
		return EDecodeResult::Fail;
	}
	// Did we get a buffer?
	bool bHaveAvailSmpBlk = CurrentOutputBuffer != nullptr;
	// If the renderer cannot currently take on another buffer we pretend we did not
	// get a buffer in order to let the buffer listener know the output is stalled.
	if (Renderer.IsValid() && !Renderer->CanReceiveOutputFrames(1))
	{
		bHaveAvailSmpBlk = false;
	}
	NotifyReadyBufferListener(bHaveAvailSmpBlk);
	if (!bHaveAvailSmpBlk)
	{
		return EDecodeResult::TryAgainLater;
	}

	// Clear to silence
	FMemory::Memzero(CurrentOutputBuffer->GetBufferProperties().GetValue("address").GetPointer(), CurrentOutputBuffer->GetBufferProperties().GetValue("size").GetInt64());

	// Assume sensible defaults.
	int64 NumChannels = 2;
	int64 SampleRate = 48000;
	int64 SamplesPerBlock = 1024;

	// With a valid configuration record we can use the actual values.
	if (ConfigRecord.IsValid())
	{
		// Parameteric stereo results in stereo (2 channels).
		NumChannels = ConfigRecord->PSSignal > 0 ? 2 : NumChannelsForConfig[ConfigRecord->ChannelConfiguration];
		SampleRate = ConfigRecord->ExtSamplingFrequency ? ConfigRecord->ExtSamplingFrequency : ConfigRecord->SamplingRate;
		SamplesPerBlock = ConfigRecord->SBRSignal > 0 ? 2048 : 1024;
	}

	// A partial sample block has fewer samples.
	if (CurrentAccessUnit->AdjustedDuration.IsValid())
	{
		SamplesPerBlock = CurrentAccessUnit->AdjustedDuration.GetAsHNS() * SampleRate / 10000000;
	}

	if (SamplesPerBlock)
	{
		FTimeValue Duration;
		Duration.SetFromND(SamplesPerBlock, (uint32) SampleRate);

		OutputBufferSampleProperties.Clear();
		OutputBufferSampleProperties.Set("num_channels", FVariantValue(NumChannels));
		OutputBufferSampleProperties.Set("sample_rate", FVariantValue(SampleRate));
		OutputBufferSampleProperties.Set("byte_size", FVariantValue((int64)(NumChannels * sizeof(int16) * SamplesPerBlock)));
		OutputBufferSampleProperties.Set("duration", FVariantValue(Duration));
		OutputBufferSampleProperties.Set("pts", FVariantValue(CurrentAccessUnit->AdjustedPTS));

		Renderer->ReturnBuffer(CurrentOutputBuffer, true, OutputBufferSampleProperties);
		CurrentOutputBuffer = nullptr;
	}
	else
	{
		ReturnUnusedOutputBuffer();
	}
	return EDecodeResult::Ok;
}


//-----------------------------------------------------------------------------
/**
 * Gets an input access unit and prepares it for use.
 *
 * @return
 */
void FAudioDecoderAAC::GetAndPrepareInputAU()
{
	// Need new input?
	if (!CurrentAccessUnit.IsValid() && InputBufferListener && NextAccessUnits.IsEmpty())
	{
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);

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
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);

			bool bOk = NextAccessUnits.Dequeue(CurrentAccessUnit);
			MEDIA_UNUSED_VAR(bOk);
			check(bOk);

			PrepareAU(CurrentAccessUnit);
		}
	}

	// Set the current sequence index if it is not set yet.
	if (!CurrentSequenceIndex.IsSet() && CurrentAccessUnit.IsValid())
	{
		CurrentSequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
	}
}


//-----------------------------------------------------------------------------
/**
 * Parse codec specific data from configuration record.
 */
bool FAudioDecoderAAC::ParseConfigRecord()
{
	if (!ConfigRecord.IsValid() || IsDifferentFormat())
	{
		SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
		CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);
		if (CurrentAccessUnit->AccessUnit->AUCodecData.IsValid())
		{
			CurrentCodecData = CurrentAccessUnit->AccessUnit->AUCodecData;
			ConfigRecord = MakeShared<MPEG::FAACDecoderConfigurationRecord, ESPMode::ThreadSafe>();
			if (!ConfigRecord->ParseFrom(CurrentCodecData->CodecSpecificData.GetData(), CurrentCodecData->CodecSpecificData.Num()))
			{
				ConfigRecord.Reset();
				CurrentCodecData.Reset();
				PostError(0, "Failed to parse AAC configuration record", ERRCODE_INTERNAL_ANDROID_FAILED_TO_PARSE_CSD);
				return false;
			}
			else
			{
				// Channel configuration must be specified. Internal CPE/SCE elements are not supported.
				if (ConfigRecord->ChannelConfiguration == 0)
				{
					ConfigRecord.Reset();
					CurrentCodecData.Reset();
					PostError(0, "Unsupported AAC channel configuration", ERRCODE_INTERNAL_ANDROID_UNSUPPORTED_NUMBER_OF_AUDIO_CHANNELS);
					return false;
				}
			}
		}
	}
	return true;
}


void FAudioDecoderAAC::Android_SuspendOrResumeDecoder(bool bSuspend)
{
	if (bSuspend)
	{
		HandleApplicationWillEnterBackground();
		ApplicationSuspendConfirmedSignal.Wait();
	}
	else
	{
		HandleApplicationHasEnteredForeground();
	}
}


//-----------------------------------------------------------------------------
/**
 * Application has entered foreground.
 */
void FAudioDecoderAAC::HandleApplicationHasEnteredForeground()
{
	int32 Count = FPlatformAtomics::InterlockedDecrement(&ApplicationSuspendCount);
	if (Count == 0)
	{
		ApplicationRunningSignal.Signal();
	}
}


//-----------------------------------------------------------------------------
/**
 * Application goes into background.
 */
void FAudioDecoderAAC::HandleApplicationWillEnterBackground()
{
	int32 Count = FPlatformAtomics::InterlockedIncrement(&ApplicationSuspendCount);
	if (Count == 1)
	{
		ApplicationRunningSignal.Reset();
	}
}


//-----------------------------------------------------------------------------
/**
 * Checks if the decoder needs to be flushed.
 */
bool FAudioDecoderAAC::CheckForFlush()
{
	// Flush?
	if (FlushDecoderSignal.IsSignaled())
	{
		SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
		CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);
		ReturnUnusedOutputBuffer();
		NextAccessUnits.Empty();
		InDecoderInput.Empty();
		CurrentSequenceIndex.Reset();
		LastPushedPresentationTimeUs = 0;
		DecodingState = EDecodingState::Regular;
		bGotEOS = false;

		InternalDecoderDestroy();
		ReturnUnusedOutputBuffer();
		ConfigRecord.Reset();
		CurrentCodecData.Reset();
		ChannelMapper.Reset();

		FlushDecoderSignal.Reset();
		DecoderFlushedSignal.Signal();
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
/**
 * AAC audio decoder main threaded decode loop
 */
void FAudioDecoderAAC::WorkerThread()
{
	LLM_SCOPE(ELLMTag::MediaStreaming);

	ApplicationRunningSignal.Signal();
	ApplicationSuspendConfirmedSignal.Reset();

	TSharedPtrTS<FFGBGNotificationHandlers> FGBGHandlers = MakeSharedTS<FFGBGNotificationHandlers>();
	FGBGHandlers->WillEnterBackground = [this]() { HandleApplicationWillEnterBackground(); };
	FGBGHandlers->HasEnteredForeground = [this]() { HandleApplicationHasEnteredForeground(); };
	if (AddBGFGNotificationHandler(FGBGHandlers))
	{
		HandleApplicationWillEnterBackground();
	}

	bool bError = false;

	bGotEOS = false;
	CurrentOutputBuffer = nullptr;
	LastPushedPresentationTimeUs = 0;
	DecodingState = EDecodingState::Regular;

	PCMBufferSize = sizeof(int16) * 8 * 2048;
	PCMBuffer = (int16*)FMemory::Malloc(PCMBufferSize);

	bError = !CreateDecodedSamplePool();
	check(!bError);

	EDecodeResult DecodeResult;
	int64 TimeLast = MEDIAutcTime::CurrentMSec();
	while(!TerminateThreadSignal.IsSignaled())
	{
		// If in background, wait until we get activated again.
		if (!ApplicationRunningSignal.IsSignaled())
		{
			UE_LOG(LogElectraPlayer, Log, TEXT("FAudioDecoderAAC(%p): OnSuspending"), this);
			ApplicationSuspendConfirmedSignal.Signal();
			while(!ApplicationRunningSignal.WaitTimeout(100 * 1000) && !TerminateThreadSignal.IsSignaled())
			{
				// Check if there is a decoder flush pending. While we cannot flush right now we
				// have to at least pretend we do
				if (FlushDecoderSignal.IsSignaled())
				{
					DecoderFlushedSignal.Signal();
				}
			}
			UE_LOG(LogElectraPlayer, Log, TEXT("FAudioDecoderAAC(%p): OnResuming"), this);
			ApplicationSuspendConfirmedSignal.Reset();
			continue;
		}

		// Is there a pending flush? If so, execute the flush and go back to the top to check if we must terminate now.
		if (CheckForFlush())
		{
			continue;
		}

		// Because of the different paths this loop can take there is a possibility that it may go very fast and not wait for any resources.
		// To prevent this from becoming a tight loop we make sure to sleep at least some time  here to throttle down.
		int64 TimeNow = MEDIAutcTime::CurrentMSec();
		int64 elapsedMS = TimeNow - TimeLast;
		const int32 kTotalSleepTimeMsec = 10;
		if (elapsedMS < kTotalSleepTimeMsec)
		{
			FMediaRunnable::SleepMilliseconds(kTotalSleepTimeMsec - elapsedMS);
		}
		TimeLast = TimeNow;

		// Get input if available and prepare it.
		GetAndPrepareInputAU();

		// When in error state pace ourselves in consuming any more data until we are being stopped.
		if (bError && CurrentAccessUnit.IsValid())
		{
			FMediaRunnable::SleepMicroseconds(CurrentAccessUnit->AccessUnit->Duration.GetAsMicroseconds());
			CurrentAccessUnit.Reset();
			continue;
		}

		// Get output if available.
		EOutputResult OutputResult = GetOutput();
		if (OutputResult == EOutputResult::Fail)
		{
			bError = true;
		}

		// Are we currently draining the decoder?
		if (DecodingState == EDecodingState::Draining)
		{
			// Did we get the final output?
			if (OutputResult == EOutputResult::EOS)
			{
				SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
				CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);

				// Done draining.
				bGotEOS = true;
				InDecoderInput.Empty();

				// We have to assume the next data will be of a different format for which we need a new decoder.
				InternalDecoderDestroy();
				ReturnUnusedOutputBuffer();
				ConfigRecord.Reset();
				CurrentCodecData.Reset();
				ChannelMapper.Reset();
				LastPushedPresentationTimeUs = 0;
			}
			else if (OutputResult == EOutputResult::Fail)
			{
				bError = true;
			}
			// At EOS now?
			if (bGotEOS)
			{
				// We now need to check if there is another AU to be decoded and if so, if it's a regular or a dummy AU.
				if (CurrentAccessUnit.IsValid())
				{
					DecodingState = CurrentAccessUnit->AccessUnit->bIsDummyData ? EDecodingState::Dummy : EDecodingState::Regular;
				}
				// No further data.
				else if (NextAccessUnits.ReachedEOD())
				{
					// We stay in this state until we get flushed or new data becomes available.
					NotifyReadyBufferListener(true);
					FMediaRunnable::SleepMilliseconds(20);
				}
			}
		}
		// Not an else-if so we can get right in here after having received the EOS without going through another loop iteration and wait time.
		if (DecodingState == EDecodingState::Regular)
		{
			// Check if this AU requires a new decoder
			if (CurrentAccessUnit.IsValid())
			{
				if (CurrentAccessUnit->AccessUnit->bTrackChangeDiscontinuity || IsDifferentFormat() || CurrentAccessUnit->AccessUnit->bIsDummyData || CurrentSequenceIndex.GetValue() != CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex())
				{
					// If we have a decoder we need to drain it now to get all pending output.
					if (DecoderInstance.IsValid())
					{
						DecodeResult = DrainDecoder();
						if (DecodeResult == EDecodeResult::Ok)
						{
							// EOS was sent. Now wait until draining is complete.
							DecodingState = EDecodingState::Draining;
							bGotEOS = false;
							continue;
						}
						else if (DecodeResult == EDecodeResult::TryAgainLater)
						{
							// Could not enqueue the EOS buffer. We will try this again on the next iteration.
							continue;
						}
						else if (DecodeResult == EDecodeResult::Fail)
						{
							bError = true;
						}
						else if (DecodeResult == EDecodeResult::SessionLost)
						{
							// This cannot happen at the moment.
						}
						else
						{
							checkNoEntry();
							bError = true;
						}
					}
					// With no decoder, if we need to decode dummy data we can switch into dummy mode now.
					else if (CurrentAccessUnit->AccessUnit->bIsDummyData)
					{
						DecodingState = EDecodingState::Dummy;
						continue;
					}
				}

				// Parse the CSD into a configuration record.
				if (!ParseConfigRecord())
				{
					bError = true;
				}

				// Is this audio packet to be dropped?
				if (!CurrentAccessUnit->AdjustedPTS.IsValid())
				{
					CurrentAccessUnit.Reset();
					continue;
				}

				// Need to create a decoder instance?
				if (!bError && !DecoderInstance.IsValid())
				{
					SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
					CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);
					if (!InternalDecoderCreate())
					{
						bError = true;
					}
				}

				// Decode.
				if (!bError && DecoderInstance.IsValid())
				{
					SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
					CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);

					DecodeResult = Decode();
					if (DecodeResult == EDecodeResult::Ok)
					{
						// Update the sequence index.
						CurrentSequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
						// Done with this AU.
						CurrentAccessUnit->ReleasePayload();
						CurrentAccessUnit.Reset();
					}
					else if (DecodeResult == EDecodeResult::TryAgainLater)
					{
					}
					else if (DecodeResult == EDecodeResult::Fail)
					{
						bError = true;
					}
					else if (DecodeResult == EDecodeResult::SessionLost)
					{
						// This cannot happen at the moment.
					}
					else
					{
						checkNoEntry();
						bError = true;
					}
				}
			}
		}
		else if (DecodingState == EDecodingState::Dummy)
		{
			if (CurrentAccessUnit.IsValid())
			{
				// Parse the CSD into a configuration record.
				if (!ParseConfigRecord())
				{
					bError = true;
				}

				// Update the sequence index.
				CurrentSequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();

				// Is this audio packet to be dropped?
				if (!CurrentAccessUnit->AdjustedPTS.IsValid())
				{
					CurrentAccessUnit.Reset();
					continue;
				}

				// Is this a regular AU now?
				if (!CurrentAccessUnit->AccessUnit->bIsDummyData)
				{
					DecodingState = EDecodingState::Regular;
					continue;
				}

				DecodeResult = DecodeDummy();
				// Dummy decoding either succeeds or fails.
				if (DecodeResult == EDecodeResult::Ok)
				{
					CurrentAccessUnit.Reset();
				}
				else if (DecodeResult == EDecodeResult::Fail)
				{
					bError = true;
				}
			}
		}

		// Check if we reached end-of-data
		if (!CurrentAccessUnit.IsValid() && NextAccessUnits.ReachedEOD())
		{
			// Need to drain the decoder to get all remaining output?
			if (DecodingState == EDecodingState::Regular)
			{
				if (DecoderInstance.IsValid())
				{
					DecodeResult = DrainDecoder();
					if (DecodeResult == EDecodeResult::Ok)
					{
						// EOS was sent. Now wait until draining is complete.
						DecodingState = EDecodingState::Draining;
						bGotEOS = false;
						continue;
					}
					else if (DecodeResult == EDecodeResult::TryAgainLater)
					{
						// Could not enqueue the EOS buffer. We will try this again on the next iteration.
					}
					else if (DecodeResult == EDecodeResult::Fail)
					{
						bError = true;
					}
					else if (DecodeResult == EDecodeResult::SessionLost)
					{
						// This cannot happen at the moment.
					}
					else
					{
						checkNoEntry();
						bError = true;
					}
				}
			}
			else if (DecodingState == EDecodingState::Dummy)
			{
				// Reached end-of-data while dummy decoding. Since there is no need to flush the decoder
				// we merely put ourselves into a finished-draining state as if we ended with regular data.
				DecodingState = EDecodingState::Draining;
				bGotEOS = true;
				InDecoderInput.Empty();
			}
		}
	}

	ReturnUnusedOutputBuffer();
	// Close the decoder.
	InternalDecoderDestroy();
	DestroyDecodedSamplePool();

	CurrentCodecData.Reset();
	ConfigRecord.Reset();

	// Flush any remaining input data.
	NextAccessUnits.Empty();
	InDecoderInput.Empty();
	FMemory::Free(PCMBuffer);

	RemoveBGFGNotificationHandler(FGBGHandlers);
}


} // namespace Electra


