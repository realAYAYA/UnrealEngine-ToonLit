// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/Platform.h"

#if PLATFORM_MAC || PLATFORM_IOS || PLATFORM_TVOS

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
#include "DecoderErrors_Apple.h"
#include "HAL/LowLevelMemTracker.h"
#include "ElectraPlayerPrivate.h"

#include <AudioToolbox/AudioToolbox.h>


#define MAGIC_INSUFFICIENT_INPUT_MARKER		4711

DECLARE_CYCLE_STAT(TEXT("FAudioDecoderAAC::Decode()"), STAT_ElectraPlayer_AudioAACDecode, STATGROUP_ElectraPlayer);
DECLARE_CYCLE_STAT(TEXT("FAudioDecoderAAC::ConvertOutput()"), STAT_ElectraPlayer_AudioAACConvertOutput, STATGROUP_ElectraPlayer);

namespace Electra
{

namespace AACDecoderChannelOutputMappingApple
{
	#define CP FAudioChannelMapper::EChannelPosition
	static const CP _1[] = { CP::C };
	static const CP _2[] = { CP::L, CP::R };
	static const CP _3[] = { CP::C, CP::L, CP::R };
	static const CP _4[] = { CP::C, CP::L, CP::R, CP::Cs };
	static const CP _5[] = { CP::C, CP::L, CP::R, CP::Ls, CP::Rs };
	static const CP _6[] = { CP::C, CP::L, CP::R, CP::Ls, CP::Rs, CP::LFE };
	static const CP _7[] = { CP::C, CP::L, CP::R, CP::Ls, CP::Rs, CP::Cs, CP::LFE };
	static const CP _8[] = { CP::C, CP::L, CP::R, CP::Ls, CP::Rs, CP::Lsr, CP::Rsr, CP::LFE };
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

	void StartThread();
	void StopThread();
	void WorkerThread();

	void HandleApplicationHasEnteredForeground();
	void HandleApplicationWillEnterBackground();

	bool InternalDecoderCreate();
	void InternalDecoderDestroy();

	bool CreateDecodedSamplePool();
	void DestroyDecodedSamplePool();

	void ReturnUnusedOutputBuffer();

	void NotifyReadyBufferListener(bool bHaveOutput);

	void PrepareAU(TSharedPtrTS<FDecoderInput> AU);
	bool Decode(TSharedPtrTS<FDecoderInput> AU);
	bool IsDifferentFormat(TSharedPtrTS<FDecoderInput> AU);

	void PostError(OSStatus ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error = UEMEDIA_ERROR_OK);
	void LogMessage(IInfoLog::ELevel Level, const FString& Message);

private:

	struct FAudioConverterInstance
	{
		FAudioConverterInstance()
		{
			ConverterRef = nullptr;
			InputDescr   = {};
			OutputDescr  = {};
			PacketDescr  = {};
		}
		struct FWorkData
		{
			FWorkData()
			{
				Clear();
			}
			void Clear()
			{
				InputData     = nullptr;
				InputSize     = 0;
				InputConsumed = 0;
			}
			void*	InputData;
			uint32	InputSize;
			uint32	InputConsumed;
		};
		AudioConverterRef				ConverterRef;
		AudioStreamBasicDescription		InputDescr;
		AudioStreamBasicDescription		OutputDescr;
		AudioStreamPacketDescription	PacketDescr;
		FWorkData						WorkData;

		// Methods for Audio Toolbox
		OSStatus AudioToolbox_ComplexInputCallback(AudioConverterRef InAudioConverter, UInt32* InOutNumberDataPackets, AudioBufferList* InOutData, AudioStreamPacketDescription** OutDataPacketDescription);
		static OSStatus _AudioToolbox_ComplexInputCallback(AudioConverterRef InAudioConverter, UInt32* InOutNumberDataPackets, AudioBufferList* InOutData, AudioStreamPacketDescription** OutDataPacketDescription, void* InUserData)
		{
			return static_cast<FAudioDecoderAAC::FAudioConverterInstance*>(InUserData)->AudioToolbox_ComplexInputCallback(InAudioConverter, InOutNumberDataPackets, InOutData, OutDataPacketDescription);
		}
	};

private:
	FInstanceConfiguration													Config;

	TAccessUnitQueue<TSharedPtrTS<FDecoderInput>>							NextAccessUnits;

	FMediaEvent																TerminateThreadSignal;
	FMediaEvent																FlushDecoderSignal;
	FMediaEvent																DecoderFlushedSignal;
	bool																	bThreadStarted;

	FMediaEvent																ApplicationRunningSignal;
	FMediaEvent																ApplicationSuspendConfirmedSignal;

	TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe>							Renderer;
	int32																	MaxDecodeBufferSize;

	FMediaCriticalSection													ListenerMutex;
	IAccessUnitBufferListener*												InputBufferListener;
	IDecoderOutputBufferListener*											ReadyBufferListener;

	IPlayerSessionServices* 												SessionServices;

	TSharedPtr<MPEG::FAACDecoderConfigurationRecord, ESPMode::ThreadSafe>	ConfigRecord;
	TSharedPtrTS<FAccessUnit::CodecData>									CurrentCodecData;
	bool																	bInDummyDecodeMode;
	bool																	bHaveDiscontinuity;

	FAudioConverterInstance*												DecoderInstance;

	TArray<TSharedPtrTS<FDecoderInput>>										InDecoderInput;

	IMediaRenderer::IBuffer*												CurrentOutputBuffer;
	FParamDict																BufferAcquireOptions;
	FParamDict																OutputBufferSampleProperties;
	int32																	PCMBufferSize;
	int16*																	PCMBuffer;
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
	ThreadConfig.Decoder.Priority 	   = TPri_Normal;
	ThreadConfig.Decoder.StackSize	   = 65536;
	ThreadConfig.Decoder.CoreAffinity    = -1;
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
 * @param config
 *
 * @return
 */
bool FAudioDecoderAAC::Startup(const IAudioDecoderAAC::FSystemConfiguration& config)
{
	SystemConfig = config;
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
 */
bool FAudioDecoderAAC::CanDecodeStream(const FStreamCodecInformation& InCodecInfo)
{
	return InCodecInfo.GetChannelConfiguration() == 1 ||		// Mono
		   InCodecInfo.GetChannelConfiguration() == 2 ||		// Stereo
		   InCodecInfo.GetChannelConfiguration() == 3 ||		// L/C/R
		   InCodecInfo.GetChannelConfiguration() == 6 ||		// 5.1
		   InCodecInfo.GetChannelConfiguration() == 12;			// 7.1
}


//-----------------------------------------------------------------------------
/**
 * Constructor
 */
FAudioDecoderAAC::FAudioDecoderAAC()
	: FMediaThread("ElectraPlayer::AAC decoder")
	, bThreadStarted(false)
	, Renderer(nullptr)
	, MaxDecodeBufferSize(0)
	, InputBufferListener(nullptr)
	, ReadyBufferListener(nullptr)
	, SessionServices(nullptr)
	, bInDummyDecodeMode(false)
	, bHaveDiscontinuity(false)
	, DecoderInstance(nullptr)
	, CurrentOutputBuffer(nullptr)
	, PCMBufferSize(0)
	, PCMBuffer(nullptr)
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
		PostError(0, "Failed to create sample pool", ERRCODE_INTERNAL_APPLE_COULD_NOT_CREATE_SAMPLE_POOL, Error);
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
 *
 * @return
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
void FAudioDecoderAAC::PostError(OSStatus ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error)
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
 * Creates an audio decoder ("converter" in AudioToolBox terminology)
 *
 * Note: This requires the configuration record to have been parsed successfully.
 */
bool FAudioDecoderAAC::InternalDecoderCreate()
{
	InternalDecoderDestroy();

	if (!ConfigRecord.IsValid())
	{
		PostError(0, "No CSD to create audio decoder with", ERRCODE_INTERNAL_APPLE_COULD_NOT_CREATE_AUDIO_DECODER);
		return false;
	}

	DecoderInstance = new FAudioConverterInstance;
	// Set input configuration specific to our current format.
	memset(&DecoderInstance->InputDescr, 0, sizeof(AudioStreamBasicDescription));
	DecoderInstance->InputDescr.mSampleRate		  = ConfigRecord->ExtSamplingFrequency ? ConfigRecord->ExtSamplingFrequency : ConfigRecord->SamplingRate;
	DecoderInstance->InputDescr.mFormatID		  = ConfigRecord->PSSignal > 0 ? kAudioFormatMPEG4AAC_HE_V2 : ConfigRecord->SBRSignal > 0 ? kAudioFormatMPEG4AAC_HE : kAudioFormatMPEG4AAC;
	DecoderInstance->InputDescr.mFormatFlags      = DecoderInstance->InputDescr.mFormatID == kAudioFormatMPEG4AAC ? kMPEG4Object_AAC_LC : kMPEG4Object_AAC_SBR;
	DecoderInstance->InputDescr.mFramesPerPacket  = DecoderInstance->InputDescr.mFormatID == kAudioFormatMPEG4AAC ? 1024 : 2048;
    // Decoding parametric stereo (PS) implies the output to go from 1 to 2 channels.
	DecoderInstance->InputDescr.mChannelsPerFrame = ConfigRecord->PSSignal > 0 ? 2 : NumChannelsForConfig[ConfigRecord->ChannelConfiguration];

	// Want LPCM output, use convenience method to set this up.
	memset(&DecoderInstance->OutputDescr, 0, sizeof(AudioStreamBasicDescription));
	FillOutASBDForLPCM(DecoderInstance->OutputDescr, DecoderInstance->InputDescr.mSampleRate, DecoderInstance->InputDescr.mChannelsPerFrame, 16, 16, false, false, false);
	// Try to create audio converter
	OSStatus res = AudioConverterNew(&DecoderInstance->InputDescr, &DecoderInstance->OutputDescr, &DecoderInstance->ConverterRef);
	check(res == 0);
	if (res)
	{
		InternalDecoderDestroy();
		PostError(res, "Failed to create decoder", ERRCODE_INTERNAL_APPLE_COULD_NOT_CREATE_AUDIO_DECODER);
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Destroys the audio decoder ("converter" in AudioToolBox terminology)
 */
void FAudioDecoderAAC::InternalDecoderDestroy()
{
	if (DecoderInstance)
	{
		OSStatus res = 0;
		if (DecoderInstance->ConverterRef)
		{
			res = AudioConverterReset(DecoderInstance->ConverterRef);
			check(res == 0);	// not really that important
			res = AudioConverterDispose(DecoderInstance->ConverterRef);
			check(res == 0);
			DecoderInstance->ConverterRef = nullptr;
		}
		delete DecoderInstance;
		DecoderInstance = nullptr;
	}
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
 * Callback from the AudioConverter to get input to be converted.
 *
 * @param InAudioConverter
 * @param InOutNumberDataPackets
 * @param InOutData
 * @param OutDataPacketDescription
 */
OSStatus FAudioDecoderAAC::FAudioConverterInstance::AudioToolbox_ComplexInputCallback(AudioConverterRef InAudioConverter, UInt32* InOutNumberDataPackets, AudioBufferList* InOutData, AudioStreamPacketDescription** OutDataPacketDescription)
{
	// Make sure we have input work data when being called
	if (WorkData.InputData)
	{
		if (InOutNumberDataPackets && *InOutNumberDataPackets)
		{
			*InOutNumberDataPackets   		     = 1;
			InOutData->mNumberBuffers 		     = 1;
			InOutData->mBuffers[0].mData  	     = WorkData.InputData;
			InOutData->mBuffers[0].mDataByteSize = WorkData.InputSize;

			if (OutDataPacketDescription)
			{
				check(*OutDataPacketDescription == nullptr);
				*OutDataPacketDescription = &PacketDescr;
				PacketDescr.mStartOffset 		    = 0;
				PacketDescr.mVariableFramesInPacket = 0;
				PacketDescr.mDataByteSize		    = WorkData.InputSize;
			}
			WorkData.InputConsumed = WorkData.InputSize;
			WorkData.InputData     = nullptr;
			WorkData.InputSize     = 0;
		}
		return 0;
	}
	else
	{
		// When we have no pending input access unit we can't return data.
		// We return some dummy value to indicate that we have no data at the moment.
		// Returning 0 would indicate an EOF and make the decoder stop working.
		*InOutNumberDataPackets = 0;
		return MAGIC_INSUFFICIENT_INPUT_MARKER;
	}
}


//-----------------------------------------------------------------------------
/**
 * Decodes an access unit
 *
 * @param AU
 */
bool FAudioDecoderAAC::Decode(TSharedPtrTS<FDecoderInput> AU)
{
	FTimeValue CurrentPTS;
	CurrentPTS = AU->AdjustedPTS;
	check(CurrentPTS.IsValid());
	if (!CurrentPTS.IsValid())
	{
		CurrentPTS = AU->AccessUnit->PTS;
	}

	void* CurrentOutputBufferAddr = PCMBuffer;
	int32 CurrentOutputBufferAvailSize = PCMBufferSize;
	int32 NumSamplesProduced = 0;
	uint32 nDataOffset = 0;
	// Loop until all data has been consumed.
	bool bAllInputConsumed = false;
	while(!bAllInputConsumed && !TerminateThreadSignal.IsSignaled() && !FlushDecoderSignal.IsSignaled())
	{
		// Need a new output buffer?
		if (CurrentOutputBuffer == nullptr)
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);
			UEMediaError bufResult = Renderer->AcquireBuffer(CurrentOutputBuffer, 0, BufferAcquireOptions);
			check(bufResult == UEMEDIA_ERROR_OK || bufResult == UEMEDIA_ERROR_INSUFFICIENT_DATA);
			if (bufResult != UEMEDIA_ERROR_OK && bufResult != UEMEDIA_ERROR_INSUFFICIENT_DATA)
			{
				PostError(0, "Failed to acquire sample buffer", ERRCODE_INTERNAL_APPLE_COULD_NOT_GET_SAMPLE_BUFFER, bufResult);
				return false;
			}
		}

		bool bHaveAvailSmpBlk = CurrentOutputBuffer != nullptr;

		// Check if the renderer can accept the output we will want to send to it.
		// If it can't right now we treat this as if we do not have an available output buffer.
		if (Renderer.IsValid() && !Renderer->CanReceiveOutputFrames(1))
		{
			bHaveAvailSmpBlk = false;
		}

		NotifyReadyBufferListener(bHaveAvailSmpBlk);
		if (bHaveAvailSmpBlk)
		{
			if (DecoderInstance && !AU->AccessUnit->bIsDummyData)
			{
				if (bInDummyDecodeMode)
				{
					bHaveDiscontinuity = true;
					bInDummyDecodeMode = false;
				}

				DecoderInstance->WorkData.Clear();
				DecoderInstance->WorkData.InputData = Electra::AdvancePointer(AU->AccessUnit->AUData, nDataOffset);
				DecoderInstance->WorkData.InputSize = AU->AccessUnit->AUSize - nDataOffset;

				uint32 NumberOfChannels   = DecoderInstance->OutputDescr.mChannelsPerFrame;
				uint32 SamplingRate       = DecoderInstance->InputDescr.mSampleRate;
				while(1)
				{
					AudioBuffer		OutputBuffer;
					AudioBufferList OutputBufferList;
					OutputBuffer.mNumberChannels    = NumberOfChannels;
					OutputBuffer.mDataByteSize	    = CurrentOutputBufferAvailSize;
					OutputBuffer.mData			    = CurrentOutputBufferAddr;
					OutputBufferList.mNumberBuffers = 1;
					OutputBufferList.mBuffers[0]    = OutputBuffer;

					// Get all possible output in LC size blocks. This may return fewer samples or we loop a number of times here.
					UInt32 InOutPacketSize = 1024;

					OSStatus res = AudioConverterFillComplexBuffer(DecoderInstance->ConverterRef, FAudioConverterInstance::_AudioToolbox_ComplexInputCallback, DecoderInstance, &InOutPacketSize, &OutputBufferList, nullptr);
					if (res != 0 && res != MAGIC_INSUFFICIENT_INPUT_MARKER)
					{
						PostError(res, "Failed to decode", ERRCODE_INTERNAL_APPLE_FAILED_TO_DECODE_AUDIO);
						return false;
					}

					if (InOutPacketSize)
					{
						SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACConvertOutput);
						CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACConvertOutput);
						NumSamplesProduced += InOutPacketSize;

						int32 OutputByteCount = InOutPacketSize * NumberOfChannels * sizeof(int16);
						check(OutputByteCount <= CurrentOutputBufferAvailSize);
						if (OutputByteCount <= CurrentOutputBufferAvailSize)
						{
							CurrentOutputBufferAddr = Electra::AdvancePointer(CurrentOutputBufferAddr, OutputByteCount);
							CurrentOutputBufferAvailSize -= OutputByteCount;
						}
						else
						{
							PostError(0, "Output buffer not large enough", ERRCODE_INTERNAL_APPLE_AUDIO_OUTPUT_BUFFER_TOO_SMALL);
							return false;
						}
					}
					else
					{
						// All output exhausted. This should happen only when all input was consumed as well.
						check(res == MAGIC_INSUFFICIENT_INPUT_MARKER);
						break;
					}

					if (res == MAGIC_INSUFFICIENT_INPUT_MARKER)
					{
						break;
					}

				}
				nDataOffset += DecoderInstance->WorkData.InputConsumed;
				bAllInputConsumed = nDataOffset >= AU->AccessUnit->AUSize;

				if (bAllInputConsumed && NumSamplesProduced)
				{
					SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACConvertOutput);
					CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACConvertOutput);

					if (!ChannelMapper.IsInitialized())
					{
						if (NumberOfChannels)
						{
							TArray<FAudioChannelMapper::FSourceLayout> Layout;
							const FAudioChannelMapper::EChannelPosition* ChannelPositions = AACDecoderChannelOutputMappingApple::Order[NumberOfChannels - 1];
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
								PostError(0, "Failed to initialize audio channel mapper", ERRCODE_INTERNAL_APPLE_UNSUPPORTED_NUMBER_OF_AUDIO_CHANNELS);
								return false;
							}
						}
						else
						{
							PostError(0, "Bad number (0) of decoded audio channels", ERRCODE_INTERNAL_APPLE_UNSUPPORTED_NUMBER_OF_AUDIO_CHANNELS);
							return false;
						}
					}


					int32 NumDecodedBytes = PCMBufferSize - CurrentOutputBufferAvailSize;
					int32 nBytesPerSample = NumberOfChannels * sizeof(int16);

					int32 ByteOffsetToFirstSample = 0;
					if (AU->StartOverlapDuration.GetAsHNS() || AU->EndOverlapDuration.GetAsHNS())
					{
						int32 SkipStartSampleNum = (int32) (AU->StartOverlapDuration.GetAsHNS() * SamplingRate / 10000000);
						int32 SkipEndSampleNum = (int32) (AU->EndOverlapDuration.GetAsHNS() * SamplingRate / 10000000);

						if (SkipStartSampleNum + SkipEndSampleNum < NumSamplesProduced)
						{
							ByteOffsetToFirstSample = SkipStartSampleNum * nBytesPerSample;
							NumSamplesProduced -= SkipStartSampleNum;
							NumSamplesProduced -= SkipEndSampleNum;
							NumDecodedBytes -= (SkipStartSampleNum + SkipEndSampleNum) * nBytesPerSample;
						}
						else
						{
							NumSamplesProduced = 0;
							NumDecodedBytes = 0;
						}
					}

					int32 NumDecodedSamplesPerChannel = NumDecodedBytes / (sizeof(int16) * NumberOfChannels);
					if (NumDecodedSamplesPerChannel)
					{
						int32 CurrentRenderOutputBufferSize = (int32)CurrentOutputBuffer->GetBufferProperties().GetValue("size").GetInt64();
						void* CurrentRenderOutputBufferAddress = CurrentOutputBuffer->GetBufferProperties().GetValue("address").GetPointer();
						ChannelMapper.MapChannels(CurrentRenderOutputBufferAddress, CurrentRenderOutputBufferSize, AdvancePointer(PCMBuffer, ByteOffsetToFirstSample), NumDecodedBytes, NumDecodedSamplesPerChannel);

						FTimeValue Duration;
						Duration.SetFromND(NumSamplesProduced, SamplingRate);

						OutputBufferSampleProperties.Clear();
						OutputBufferSampleProperties.Set("num_channels", FVariantValue((int64)ChannelMapper.GetNumTargetChannels()));
						OutputBufferSampleProperties.Set("byte_size", FVariantValue((int64)(ChannelMapper.GetNumTargetChannels() * NumDecodedSamplesPerChannel * sizeof(int16))));
						OutputBufferSampleProperties.Set("sample_rate", FVariantValue((int64) SamplingRate));
						OutputBufferSampleProperties.Set("duration", FVariantValue(Duration));
						OutputBufferSampleProperties.Set("pts", FVariantValue(CurrentPTS));
						OutputBufferSampleProperties.Set("discontinuity", FVariantValue((bool)bHaveDiscontinuity));

						Renderer->ReturnBuffer(CurrentOutputBuffer, true, OutputBufferSampleProperties);
						CurrentOutputBuffer = nullptr;
					}
					else
					{
						ReturnUnusedOutputBuffer();
					}
					bHaveDiscontinuity = false;
				}
			}
			else
			{
				SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACConvertOutput);
				CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACConvertOutput);
				// Check if we are in dummy decode mode already.
				if (!bInDummyDecodeMode)
				{
					bInDummyDecodeMode = true;
					bHaveDiscontinuity = true;
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
					NumChannels = ConfigRecord->PSSignal > 0 ? 2 : NumChannelsForConfig[ConfigRecord->ChannelConfiguration];
					SampleRate = ConfigRecord->ExtSamplingFrequency ? ConfigRecord->ExtSamplingFrequency : ConfigRecord->SamplingRate;
					SamplesPerBlock = ConfigRecord->SBRSignal > 0 ? 2048 : 1024;
				}

				// A partial sample block has fewer samples.
				if (AU->AdjustedDuration.IsValid())
				{
					SamplesPerBlock = AU->AdjustedDuration.GetAsHNS() * SampleRate / 10000000;
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
					OutputBufferSampleProperties.Set("pts", FVariantValue(CurrentPTS));
					OutputBufferSampleProperties.Set("discontinuity", FVariantValue((bool)bHaveDiscontinuity));

					Renderer->ReturnBuffer(CurrentOutputBuffer, true, OutputBufferSampleProperties);
					CurrentOutputBuffer = nullptr;
				}
				else
				{
					ReturnUnusedOutputBuffer();
				}
				bHaveDiscontinuity = false;
				bAllInputConsumed = true;
			}
		}
		else
		{
			// No available buffer. Sleep for a bit. Can't sleep on a signal since we have to check two: abort and flush
			// We sleep for 20ms since a sample block for LC AAC produces 1024 samples which amount to 21.3ms @48kHz (or more at lower sample rates).
			FMediaRunnable::SleepMilliseconds(20);
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Checks if the codec specific format has changed.
 *
 * @param AU
 *
 * @return
 */
bool FAudioDecoderAAC::IsDifferentFormat(TSharedPtrTS<FDecoderInput> AU)
{
	if (AU.IsValid() && AU->AccessUnit->AUCodecData.IsValid() && (!CurrentCodecData.IsValid() || CurrentCodecData->CodecSpecificData != AU->AccessUnit->AUCodecData->CodecSpecificData))
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
		AU->PTS = StartTime.GetAsHNS();		// The PTS we give the decoder no matter any adjustment.
		AU->EndPTS = EndTime.GetAsHNS();	// End PTS we need to check the PTS value returned by the decoder against.
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
 * Application has entered foreground.
 */
void FAudioDecoderAAC::HandleApplicationHasEnteredForeground()
{
	ApplicationRunningSignal.Signal();
}


//-----------------------------------------------------------------------------
/**
 * Application goes into background.
 */
void FAudioDecoderAAC::HandleApplicationWillEnterBackground()
{
	ApplicationSuspendConfirmedSignal.Reset();
	ApplicationRunningSignal.Reset();
}


//-----------------------------------------------------------------------------
/**
 * AAC audio decoder main threaded decode loop
 */
void FAudioDecoderAAC::WorkerThread()
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);

	ApplicationRunningSignal.Signal();
	ApplicationSuspendConfirmedSignal.Reset();

	TSharedPtrTS<FFGBGNotificationHandlers> FGBGHandlers = MakeSharedTS<FFGBGNotificationHandlers>();
	FGBGHandlers->WillEnterBackground = [this]() { HandleApplicationWillEnterBackground(); };
	FGBGHandlers->HasEnteredForeground = [this]() { HandleApplicationHasEnteredForeground(); };
	AddBGFGNotificationHandler(FGBGHandlers);

	TOptional<int64> SequenceIndex;
	bool bError = false;

	PCMBufferSize = sizeof(int16) * 8 * 2048 * 2;				// max. of 8 channels decoding HE-AAC with twice the room to assemble packets
	PCMBuffer = (int16*)FMemory::Malloc(PCMBufferSize, 32);
	CurrentOutputBuffer = nullptr;
	bInDummyDecodeMode = false;
	bHaveDiscontinuity = false;

	bError = !CreateDecodedSamplePool();
	check(!bError);

	while(!TerminateThreadSignal.IsSignaled())
	{
		// If in background, wait until we get activated again.
		if (!ApplicationRunningSignal.IsSignaled())
		{
			UE_LOG(LogElectraPlayer, Log, TEXT("FAudioDecoderAAC(%p): OnSuspending"), this);
			ApplicationSuspendConfirmedSignal.Signal();
			while(!ApplicationRunningSignal.WaitTimeout(100 * 1000) && !TerminateThreadSignal.IsSignaled())
			{
			}
			UE_LOG(LogElectraPlayer, Log, TEXT("FAudioDecoderAAC(%p): OnResuming"), this);
		}
	
		// Notify the buffer listener that we will now be needing an AU for our input buffer.
		if (!bError && InputBufferListener && NextAccessUnits.IsEmpty())
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
		bool bHaveData = NextAccessUnits.Wait(1000 * 10);
		if (bHaveData)
		{
			TSharedPtrTS<FDecoderInput> CurrentAccessUnit;
			bool bOk = NextAccessUnits.Dequeue(CurrentAccessUnit);
			MEDIA_UNUSED_VAR(bOk);
			check(bOk);

			PrepareAU(CurrentAccessUnit);
			if (!SequenceIndex.IsSet())
			{
				SequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
			}

			// Check if the format has changed such that we need to destroy and re-create the decoder.
			if (CurrentAccessUnit->AccessUnit->bTrackChangeDiscontinuity || IsDifferentFormat(CurrentAccessUnit) || (CurrentAccessUnit->AccessUnit->bIsDummyData && !bInDummyDecodeMode) || SequenceIndex.GetValue() != CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex())
			{
				SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
				CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);
				if (DecoderInstance)
				{
					InternalDecoderDestroy();
					bHaveDiscontinuity = true;
				}
				ReturnUnusedOutputBuffer();
				ConfigRecord.Reset();
				CurrentCodecData.Reset();
				ChannelMapper.Reset();
				InDecoderInput.Empty();
			}

			// Parse the CSD into a configuration record.
			if (!ConfigRecord.IsValid() && !bError)
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
						PostError(0, "Failed to parse AAC configuration record", ERRCODE_INTERNAL_APPLE_FAILED_TO_PARSE_CSD);
						bError = true;
					}
					else
					{
						// Check for unsupported configurations
						if (ConfigRecord->ChannelConfiguration != 1 &&		// Mono
							ConfigRecord->ChannelConfiguration != 2 &&		// Stereo
							ConfigRecord->ChannelConfiguration != 3 &&		// L+C+R
							ConfigRecord->ChannelConfiguration != 6 &&		// 5.1
							ConfigRecord->ChannelConfiguration != 12)		// 7.1
						{
							ConfigRecord.Reset();
							CurrentCodecData.Reset();
							PostError(0, "Unsupported AAC channel configuration", ERRCODE_INTERNAL_APPLE_UNSUPPORTED_NUMBER_OF_AUDIO_CHANNELS);
							bError = true;
						}
					}
				}
			}

			SequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
			// Is this audio packet to be dropped?
			if (!CurrentAccessUnit->AdjustedPTS.IsValid())
			{
				CurrentAccessUnit.Reset();
				continue;
			}

			// Need to create a decoder instance?
			if (DecoderInstance == nullptr && !bError)
			{
				if (!InternalDecoderCreate())
				{
					bError = true;
				}
			}

			// Decode if not in error, otherwise just spend some idle time.
			if (!bError)
			{
				if (!Decode(CurrentAccessUnit))
				{
					bError = true;
				}
			}
			else
			{
				// Pace ourselves in consuming any more data until we are being stopped.
				FMediaRunnable::SleepMicroseconds(CurrentAccessUnit->AccessUnit->Duration.GetAsMicroseconds());
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
			// No data. Is the buffer at EOD?
			if (NextAccessUnits.ReachedEOD())
			{
				InDecoderInput.Empty();
				NotifyReadyBufferListener(true);
				FMediaRunnable::SleepMilliseconds(20);
			}
		}

		// Flush?
		if (FlushDecoderSignal.IsSignaled())
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);
			ReturnUnusedOutputBuffer();
			NextAccessUnits.Empty();
			InDecoderInput.Empty();
			SequenceIndex.Reset();

			FlushDecoderSignal.Reset();
			DecoderFlushedSignal.Signal();
		}
	}

	ReturnUnusedOutputBuffer();
	// Close the decoder.
	InternalDecoderDestroy();
	DestroyDecodedSamplePool();

	// Flush any remaining input data.
	NextAccessUnits.Empty();
	InDecoderInput.Empty();

	CurrentCodecData.Reset();
	ConfigRecord.Reset();

	FMemory::Free(PCMBuffer);
	
	RemoveBGFGNotificationHandler(FGBGHandlers);
}


} // namespace Electra


#endif
