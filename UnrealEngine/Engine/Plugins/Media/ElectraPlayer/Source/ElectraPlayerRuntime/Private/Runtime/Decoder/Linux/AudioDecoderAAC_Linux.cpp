// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

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
#include "DecoderErrors_Linux.h"
#include "HAL/LowLevelMemTracker.h"
#include "ElectraPlayerPrivate.h"

#include "libav_Decoder_Common.h"
#include "libav_Decoder_AAC.h"

/***************************************************************************************************************************************************/

DECLARE_CYCLE_STAT(TEXT("FAudioDecoderAACLinuxLibavcodec::Decode()"), STAT_ElectraPlayer_AudioAACDecode, STATGROUP_ElectraPlayer);
DECLARE_CYCLE_STAT(TEXT("FAudioDecoderAACLinuxLibavcodec::ConvertOutput()"), STAT_ElectraPlayer_AudioAACConvertOutput, STATGROUP_ElectraPlayer);


namespace Electra
{

namespace AACDecoderChannelOutputMappingLibavcodec
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


class FAudioDecoderAACLinuxLibavcodec : public IAudioDecoderAAC, public FMediaThread
{
public:
	FAudioDecoderAACLinuxLibavcodec();
	~FAudioDecoderAACLinuxLibavcodec();

	void SetPlayerSessionServices(IPlayerSessionServices* SessionServices) override;
	void Open(const FInstanceConfiguration& Config) override;
	void Close() override;
	void SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer) override;
	void AUdataPushAU(FAccessUnit* AccessUnit) override;
	void AUdataPushEOD() override;
	void AUdataClearEOD() override;
	void AUdataFlushEverything() override;
	void SetAUInputBufferListener(IAccessUnitBufferListener* Listener) override;
	void SetReadyBufferListener(IDecoderOutputBufferListener* Listener) override;
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

	bool InternalDecoderCreate();
	void InternalDecoderDestroy();

	bool CreateDecodedSamplePool();
	void DestroyDecodedSamplePool();

	void ReturnUnusedOutputBuffer();

	void NotifyReadyBufferListener(bool bHaveOutput);

	void PrepareAU(TSharedPtrTS<FDecoderInput> AU);
	bool Decode(TSharedPtrTS<FDecoderInput> AU);
	bool IsDifferentFormat(TSharedPtrTS<FDecoderInput> AU);

	void PostError(int32 ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error = UEMEDIA_ERROR_OK);
	void LogMessage(IInfoLog::ELevel Level, const FString& Message);

private:
	FInstanceConfiguration													Config;

	TAccessUnitQueue<TSharedPtrTS<FDecoderInput>>							NextAccessUnits;

	FMediaEvent																TerminateThreadSignal;
	FMediaEvent																FlushDecoderSignal;
	FMediaEvent																DecoderFlushedSignal;
	bool																	bThreadStarted = false;

	FMediaEvent																ApplicationRunningSignal;
	FMediaEvent																ApplicationSuspendConfirmedSignal;

	TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe>							Renderer;
	int32																	MaxDecodeBufferSize = 0;

	FCriticalSection														ListenerMutex;
	IAccessUnitBufferListener*												InputBufferListener = nullptr;
	IDecoderOutputBufferListener*											ReadyBufferListener = nullptr;

	IPlayerSessionServices* 												SessionServices = nullptr;

	TSharedPtr<MPEG::FAACDecoderConfigurationRecord, ESPMode::ThreadSafe>	ConfigRecord;
	TSharedPtrTS<FAccessUnit::CodecData>									CurrentCodecData;
	bool																	bInDummyDecodeMode = false;
	bool																	bHaveDiscontinuity = false;

	TSharedPtr<ILibavDecoderAAC, ESPMode::ThreadSafe>						DecoderInstance;

	TArray<TSharedPtrTS<FDecoderInput>>										InDecoderInput;

	IMediaRenderer::IBuffer*												CurrentOutputBuffer = nullptr;
	FParamDict																BufferAcquireOptions;
	FParamDict																OutputBufferSampleProperties;
	int32																	PCMBufferSize = 0;
	int16*																	PCMBuffer = nullptr;
	FAudioChannelMapper														ChannelMapper;
	static const uint8														NumChannelsForConfig[16];

};
const uint8 FAudioDecoderAACLinuxLibavcodec::NumChannelsForConfig[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 7, 8, 0, 8, 0 };

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

IAudioDecoderAAC::FSystemConfiguration::FSystemConfiguration()
{ }

IAudioDecoderAAC::FInstanceConfiguration::FInstanceConfiguration()
{ }

bool IAudioDecoderAAC::Startup(const IAudioDecoderAAC::FSystemConfiguration& InConfig)
{
	ILibavDecoder::Startup();
	return ILibavDecoder::IsLibAvAvailable();
}

void IAudioDecoderAAC::Shutdown()
{
	ILibavDecoder::Shutdown();
}

bool IAudioDecoderAAC::CanDecodeStream(const FStreamCodecInformation& InCodecInfo)
{
	bool bSupported = ILibavDecoderAAC::IsAvailable();
	if (bSupported)
	{
		bSupported = InCodecInfo.GetChannelConfiguration() == 1 ||		// Mono
					 InCodecInfo.GetChannelConfiguration() == 2 ||		// Stereo
					 InCodecInfo.GetChannelConfiguration() == 3 ||		// L/C/R
					 InCodecInfo.GetChannelConfiguration() == 6;		// 5.1
	}
	return bSupported;
}

IAudioDecoderAAC* IAudioDecoderAAC::Create()
{
	return new FAudioDecoderAACLinuxLibavcodec;
}

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

FAudioDecoderAACLinuxLibavcodec::FAudioDecoderAACLinuxLibavcodec()
	: FMediaThread("ElectraPlayer::AAC decoder")
{
}

FAudioDecoderAACLinuxLibavcodec::~FAudioDecoderAACLinuxLibavcodec()
{
	Close();
}

void FAudioDecoderAACLinuxLibavcodec::SetPlayerSessionServices(IPlayerSessionServices* InSessionServices)
{
	SessionServices = InSessionServices;
}

void FAudioDecoderAACLinuxLibavcodec::Open(const FInstanceConfiguration& InConfig)
{
	Config = InConfig;
	StartThread();
}

void FAudioDecoderAACLinuxLibavcodec::Close()
{
	StopThread();
}

void FAudioDecoderAACLinuxLibavcodec::SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer)
{
	Renderer = InRenderer;
}

void FAudioDecoderAACLinuxLibavcodec::AUdataPushAU(FAccessUnit* InAccessUnit)
{
	InAccessUnit->AddRef();

	TSharedPtrTS<FDecoderInput> NextAU = MakeSharedTS<FDecoderInput>();
	NextAU->AccessUnit = InAccessUnit;
	NextAccessUnits.Enqueue(MoveTemp(NextAU));
}

void FAudioDecoderAACLinuxLibavcodec::AUdataPushEOD()
{
	NextAccessUnits.SetEOD();
}

void FAudioDecoderAACLinuxLibavcodec::AUdataClearEOD()
{
	NextAccessUnits.ClearEOD();
}

void FAudioDecoderAACLinuxLibavcodec::AUdataFlushEverything()
{
	FlushDecoderSignal.Signal();
	DecoderFlushedSignal.WaitAndReset();
}

void FAudioDecoderAACLinuxLibavcodec::SetAUInputBufferListener(IAccessUnitBufferListener* InListener)
{
	FScopeLock lock(&ListenerMutex);
	InputBufferListener = InListener;
}

void FAudioDecoderAACLinuxLibavcodec::SetReadyBufferListener(IDecoderOutputBufferListener* InListener)
{
	FScopeLock lock(&ListenerMutex);
	ReadyBufferListener = InListener;
}

void FAudioDecoderAACLinuxLibavcodec::StartThread()
{
	ThreadSetPriority(Config.ThreadConfig.Decoder.Priority);
	ThreadSetStackSize(Config.ThreadConfig.Decoder.StackSize);
	ThreadSetCoreAffinity(Config.ThreadConfig.Decoder.CoreAffinity);
	ThreadStart(FMediaRunnable::FStartDelegate::CreateRaw(this, &FAudioDecoderAACLinuxLibavcodec::WorkerThread));
	bThreadStarted = true;
}

void FAudioDecoderAACLinuxLibavcodec::StopThread()
{
	if (bThreadStarted)
	{
		TerminateThreadSignal.Signal();
		ThreadWaitDone();
		bThreadStarted = false;
	}
}

bool FAudioDecoderAACLinuxLibavcodec::CreateDecodedSamplePool()
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
		PostError(0, "Failed to create sample pool", ERRCODE_INTERNAL_LINUX_COULD_NOT_CREATE_SAMPLE_POOL, Error);
	}

	MaxDecodeBufferSize = (int32) Renderer->GetBufferPoolProperties().GetValue("max_buffers").GetInt64();

	return Error == UEMEDIA_ERROR_OK;
}

void FAudioDecoderAACLinuxLibavcodec::DestroyDecodedSamplePool()
{
	Renderer->ReleaseBufferPool();
}

void FAudioDecoderAACLinuxLibavcodec::NotifyReadyBufferListener(bool bHaveOutput)
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

void FAudioDecoderAACLinuxLibavcodec::PostError(int32 ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error)
{
	check(SessionServices);
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

void FAudioDecoderAACLinuxLibavcodec::LogMessage(IInfoLog::ELevel Level, const FString& Message)
{
	if (SessionServices)
	{
		SessionServices->PostLog(Facility::EFacility::AACDecoder, Level, Message);
	}
}

bool FAudioDecoderAACLinuxLibavcodec::InternalDecoderCreate()
{
	InternalDecoderDestroy();

	if (!ConfigRecord.IsValid())
	{
		PostError(0, "No CSD to create audio decoder with", ERRCODE_INTERNAL_LINUX_COULD_NOT_CREATE_AUDIO_DECODER);
		return false;
	}

	if (ILibavDecoderAAC::IsAvailable())
	{
		DecoderInstance = ILibavDecoderAAC::Create(ConfigRecord->GetCodecSpecificData());
		if (!DecoderInstance.IsValid() || DecoderInstance->GetLastLibraryError())
		{
			InternalDecoderDestroy();
			PostError(-2, "libavcodec failed to open audio decoder", ERRCODE_INTERNAL_LINUX_COULD_NOT_CREATE_AUDIO_DECODER);
			return false;
		}
		return true;
	}
	PostError(-1, "libavcodec does not support this audio format", ERRCODE_INTERNAL_LINUX_AUDIO_DECODER_NOT_SUPPORTED);
	return false;
}

void FAudioDecoderAACLinuxLibavcodec::InternalDecoderDestroy()
{
	DecoderInstance.Reset();
}

void FAudioDecoderAACLinuxLibavcodec::ReturnUnusedOutputBuffer()
{
	if (CurrentOutputBuffer)
	{
		OutputBufferSampleProperties.Clear();
		Renderer->ReturnBuffer(CurrentOutputBuffer, false, OutputBufferSampleProperties);
		CurrentOutputBuffer = nullptr;
	}
}

bool FAudioDecoderAACLinuxLibavcodec::Decode(TSharedPtrTS<FDecoderInput> AU)
{
	FTimeValue CurrentPTS;
	CurrentPTS = AU->AdjustedPTS;
	check(CurrentPTS.IsValid());
	if (!CurrentPTS.IsValid())
	{
		CurrentPTS = AU->AccessUnit->PTS;
	}

	bool bDone = false;
	while(!bDone && !TerminateThreadSignal.IsSignaled() && !FlushDecoderSignal.IsSignaled())
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
				PostError(0, "Failed to acquire sample buffer", ERRCODE_INTERNAL_LINUX_COULD_NOT_GET_SAMPLE_BUFFER, bufResult);
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

				ILibavDecoderAAC::FInputAU DecAU;
				DecAU.Data = AU->AccessUnit->AUData;
				DecAU.DataSize = (int32) AU->AccessUnit->AUSize;
				DecAU.DTS = AU->PTS;
				DecAU.PTS = AU->PTS;
				DecAU.Duration = AU->EndPTS - AU->PTS;
				DecAU.UserValue = AU->PTS;
				ILibavDecoder::EDecoderError DecErr = DecoderInstance->DecodeAccessUnit(DecAU);
				if (DecErr != ILibavDecoder::EDecoderError::None)
				{
					PostError((int32)DecErr, "Failed to decode access unit", ERRCODE_INTERNAL_LINUX_FAILED_TO_DECODE_AUDIO);
					return false;
				}
				ILibavDecoderAAC::FOutputInfo Out;
				while(DecoderInstance->HaveOutput(Out) == ILibavDecoder::EOutputStatus::Available)
				{
					SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACConvertOutput);
					CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACConvertOutput);

					if (!DecoderInstance->GetOutputAsS16(PCMBuffer, PCMBufferSize))
					{
						PostError(0, "Could not get decoded output due to decoded format being unsupported", ERRCODE_INTERNAL_LINUX_UNSUPPORTED_DECODER_OUTPUT_FORMAT);
						return false;
					}
					int32 SamplingRate = Out.SampleRate;
					int32 NumSamplesProduced = Out.NumSamples;
					int32 NumberOfChannels = Out.NumChannels;
					int32 nBytesPerSample = NumberOfChannels * sizeof(int16);
					int32 NumDecodedBytes = NumSamplesProduced * nBytesPerSample;
					if (!ChannelMapper.IsInitialized())
					{
						if (NumberOfChannels)
						{
							TArray<FAudioChannelMapper::FSourceLayout> Layout;
							const FAudioChannelMapper::EChannelPosition* ChannelPositions = AACDecoderChannelOutputMappingLibavcodec::Order[NumberOfChannels - 1];
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
								PostError(0, "Failed to initialize audio channel mapper", ERRCODE_INTERNAL_LINUX_UNSUPPORTED_NUMBER_OF_AUDIO_CHANNELS);
								return false;
							}
						}
						else
						{
							PostError(0, "Bad number (0) of decoded audio channels", ERRCODE_INTERNAL_LINUX_UNSUPPORTED_NUMBER_OF_AUDIO_CHANNELS);
							return false;
						}
					}

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
				bDone = true;
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
				bDone = true;
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

bool FAudioDecoderAACLinuxLibavcodec::IsDifferentFormat(TSharedPtrTS<FDecoderInput> AU)
{
	if (AU.IsValid() && AU->AccessUnit->AUCodecData.IsValid() && (!CurrentCodecData.IsValid() || CurrentCodecData->CodecSpecificData != AU->AccessUnit->AUCodecData->CodecSpecificData))
	{
		return true;
	}
	return false;
}

void FAudioDecoderAACLinuxLibavcodec::PrepareAU(TSharedPtrTS<FDecoderInput> AU)
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

void FAudioDecoderAACLinuxLibavcodec::WorkerThread()
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);

	ApplicationRunningSignal.Signal();
	ApplicationSuspendConfirmedSignal.Reset();

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
			UE_LOG(LogElectraPlayer, Log, TEXT("FAudioDecoderAACLinuxLibavcodec(%p): OnSuspending"), this);
			ApplicationSuspendConfirmedSignal.Signal();
			while(!ApplicationRunningSignal.WaitTimeout(100 * 1000) && !TerminateThreadSignal.IsSignaled())
			{
			}
			UE_LOG(LogElectraPlayer, Log, TEXT("FAudioDecoderAACLinuxLibavcodec(%p): OnResuming"), this);
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
						PostError(0, "Failed to parse AAC configuration record", ERRCODE_INTERNAL_LINUX_FAILED_TO_PARSE_CSD);
						bError = true;
					}
					else
					{
						// Check for unsupported configurations
						if (ConfigRecord->ChannelConfiguration != 1 &&		// Mono
							ConfigRecord->ChannelConfiguration != 2 &&		// Stereo
							ConfigRecord->ChannelConfiguration != 3 &&		// L+C+R
							ConfigRecord->ChannelConfiguration != 6)		// 5.1
						{
							ConfigRecord.Reset();
							CurrentCodecData.Reset();
							PostError(0, "Unsupported AAC channel configuration", ERRCODE_INTERNAL_LINUX_UNSUPPORTED_NUMBER_OF_AUDIO_CHANNELS);
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
}

} // namespace Electra
