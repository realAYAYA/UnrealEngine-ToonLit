// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef ELECTRA_ENABLE_MFDECODER

#include "PlayerCore.h"
#include "PlayerRuntimeGlobal.h"
#include "ElectraPlayerPrivate.h"
#include "ElectraPlayerPrivate_Platform.h"

#include "StreamAccessUnitBuffer.h"
#include "Decoder/AudioDecoderAAC.h"
#include "Renderer/RendererBase.h"
#include "Player/PlayerSessionServices.h"
#include "Utilities/Utilities.h"
#include "Utilities/UtilsMPEG.h"
#include "Utilities/UtilsMPEGAudio.h"
#include "Utilities/StringHelpers.h"
#include "Utilities/AudioChannelMapper.h"

#include "DecoderErrors_DX.h"

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
//#include "wmcodecdsp.h" // for MEDIASUBTYPE_RAW_AAC1, but requires linking of additional dll for the constant.
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
	static const GUID MFTmsAACDecoder_Audio = { 0x32d186a7, 0x218f, 0x4c75, { 0x88, 0x76, 0xdd, 0x77, 0x27, 0x3a, 0x89, 0x99 } };
	static const GUID MEDIASUBTYPE_RAW_AAC1_Audio = { 0x000000FF, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
}


/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

#define VERIFY_HR(FNcall, Msg, What)	\
res = FNcall;							\
if (FAILED(res))						\
{										\
	PostError(res, Msg, What);			\
	return false;						\
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

DECLARE_CYCLE_STAT(TEXT("FAudioDecoderAAC::Decode()"), STAT_ElectraPlayer_AudioAACDecode, STATGROUP_ElectraPlayer);
DECLARE_CYCLE_STAT(TEXT("FAudioDecoderAAC::ConvertOutput()"), STAT_ElectraPlayer_AudioAACConvertOutput, STATGROUP_ElectraPlayer);


#define AACDEC_PCM_SAMPLE_SIZE  2048
#define AACDEC_MAX_CHANNELS		8


namespace Electra
{
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

		virtual void SetPlayerSessionServices(IPlayerSessionServices* PlayerSessionServices) override;

		virtual void Open(const FInstanceConfiguration& InConfig) override;
		virtual void Close() override;

		virtual void SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer) override;

		virtual void SetAUInputBufferListener(IAccessUnitBufferListener* Listener) override;

		virtual void SetReadyBufferListener(IDecoderOutputBufferListener* Listener) override;

		virtual void AUdataPushAU(FAccessUnit* AccessUnit) override;
		virtual void AUdataPushEOD() override;
		virtual void AUdataClearEOD() override;
		virtual void AUdataFlushEverything() override;

	private:
		struct FDecoderOutputBuffer
		{
			FDecoderOutputBuffer()
			{
				FMemory::Memzero(mOutputStreamInfo);
				FMemory::Memzero(mOutputBuffer);
			}
			~FDecoderOutputBuffer()
			{
				if (mOutputBuffer.pSample)
					mOutputBuffer.pSample->Release();
			}
			TRefCountPtr<IMFSample> DetachOutputSample()
			{
				TRefCountPtr<IMFSample> pOutputSample;
				if (mOutputBuffer.pSample)
				{
					// mOutputBuffer.pSample already holds a reference, don't need to addref here.
					pOutputSample = TRefCountPtr<IMFSample>(mOutputBuffer.pSample, false);
					mOutputBuffer.pSample = nullptr;
				}
				return(pOutputSample);
			}
			void PrepareForProcess()
			{
				mOutputBuffer.dwStatus = 0;
				mOutputBuffer.dwStreamID = 0;
				mOutputBuffer.pEvents = nullptr;
			}
			MFT_OUTPUT_STREAM_INFO	mOutputStreamInfo;
			MFT_OUTPUT_DATA_BUFFER	mOutputBuffer;
		};

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
		bool SetDecoderOutputType();

		bool CreateDecodedSamplePool();
		void DestroyDecodedSamplePool();

		void PrepareAU(TSharedPtrTS<FDecoderInput> InAccessUnit);
		void ReturnUnusedFrame();

		void NotifyReadyBufferListener(bool bHaveOutput);

		void PostError(HRESULT ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error = UEMEDIA_ERROR_OK);
		void LogMessage(IInfoLog::ELevel Level, const FString& Message);

		bool CreateDecoderOutputBuffer();
		bool Decode(TSharedPtrTS<FDecoderInput> AU, bool bFlushOnly);
		bool FindAndUpdateDecoderInput(TSharedPtrTS<FDecoderInput>& OutMatchingInput, int64 InPTSFromDecoder);

		bool IsDifferentFormat(TSharedPtrTS<FDecoderInput> InAccessUnit);

		void HandleApplicationHasEnteredForeground();
		void HandleApplicationWillEnterBackground();

		FInstanceConfiguration													Config;

		TAccessUnitQueue<TSharedPtrTS<FDecoderInput>>							NextAccessUnits;

		FMediaEvent																ApplicationRunningSignal;
		FMediaEvent																ApplicationSuspendConfirmedSignal;

		FMediaEvent																TerminateThreadEvent;
		FMediaEvent																FlushDecoderEvent;
		FMediaEvent																DecoderFlushedEvent;
		bool																	bThreadStarted;

		TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe>							Renderer;
		int32																	MaxDecodeBufferSize;

		FMediaCriticalSection													ListenerCriticalSection;
		IAccessUnitBufferListener*												InputBufferListener;
		IDecoderOutputBufferListener*											ReadyBufferListener;

		IPlayerSessionServices*													PlayerSessionServices;

		TSharedPtr<MPEG::FAACDecoderConfigurationRecord, ESPMode::ThreadSafe>	ConfigRecord;
		TSharedPtrTS<FAccessUnit::CodecData>									CurrentCodecData;
		bool																	bHaveDiscontinuity;

		TRefCountPtr<IMFTransform>												DecoderTransform;
		TRefCountPtr<IMFMediaType>												CurrentOutputMediaType;
		MFT_INPUT_STREAM_INFO													DecoderInputStreamInfo;
		MFT_OUTPUT_STREAM_INFO													DecoderOutputStreamInfo;

		TArray<TSharedPtrTS<FDecoderInput>>										InDecoderInput;

		TUniquePtr<FDecoderOutputBuffer>										CurrentDecoderOutputBuffer;
		IMediaRenderer::IBuffer*												CurrentRenderOutputBuffer;
		FParamDict																BufferAcquireOptions;
		FParamDict																OutputBufferSampleProperties;

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
	 * @param config
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
	 */
	bool FAudioDecoderAAC::CanDecodeStream(const FStreamCodecInformation& InCodecInfo)
	{
		return InCodecInfo.GetChannelConfiguration() <= 6;
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
		, PlayerSessionServices(nullptr)
		, CurrentRenderOutputBuffer(nullptr)
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


	void FAudioDecoderAAC::SetPlayerSessionServices(IPlayerSessionServices* InSessionServices)
	{
		PlayerSessionServices = InSessionServices;
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
		uint32 frameSize = sizeof(int16) * AACDEC_MAX_CHANNELS * AACDEC_PCM_SAMPLE_SIZE;
		poolOpts.Set("max_buffer_size", FVariantValue((int64)frameSize));
		poolOpts.Set("num_buffers", FVariantValue((int64)8));
		poolOpts.Set("samples_per_block", FVariantValue((int64)AACDEC_PCM_SAMPLE_SIZE));
		poolOpts.Set("max_channels", FVariantValue((int64)AACDEC_MAX_CHANNELS));

		UEMediaError Error = Renderer->CreateBufferPool(poolOpts);
		check(Error == UEMEDIA_ERROR_OK);

		if (Error != UEMEDIA_ERROR_OK)
		{
			PostError(S_OK, "Failed to create sample pool", ERRCODE_INTERNAL_COULD_NOT_CREATE_SAMPLE_POOL, Error);
		}

		MaxDecodeBufferSize = (int32)Renderer->GetBufferPoolProperties().GetValue("max_buffers").GetInt64();

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
	 * @param config
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
	 *
	 * @param bKeepDecoder
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
	 * @param Listener
	 */
	void FAudioDecoderAAC::SetAUInputBufferListener(IAccessUnitBufferListener* Listener)
	{
		FMediaCriticalSection::ScopedLock lock(ListenerCriticalSection);
		InputBufferListener = Listener;
	}

	void FAudioDecoderAAC::SetReadyBufferListener(IDecoderOutputBufferListener* Listener)
	{
		FMediaCriticalSection::ScopedLock lock(ListenerCriticalSection);
		ReadyBufferListener = Listener;
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
			TerminateThreadEvent.Signal();
			ThreadWaitDone();
			bThreadStarted = false;
		}
	}




	void FAudioDecoderAAC::AUdataPushAU(FAccessUnit* InAccessUnit)
	{
		InAccessUnit->AddRef();

		TSharedPtrTS<FDecoderInput> NextAU = MakeSharedTS<FDecoderInput>();
		NextAU->AccessUnit = InAccessUnit;
		NextAccessUnits.Enqueue(MoveTemp(NextAU));
	}

	void FAudioDecoderAAC::AUdataPushEOD()
	{
		NextAccessUnits.SetEOD();
	}

	void FAudioDecoderAAC::AUdataClearEOD()
	{
		NextAccessUnits.ClearEOD();
	}

	void FAudioDecoderAAC::AUdataFlushEverything()
	{
		FlushDecoderEvent.Signal();
		DecoderFlushedEvent.WaitAndReset();
	}


	//-----------------------------------------------------------------------------
	/**
	 * Notify optional decode-ready listener that we will now be producing output data.
	 *
	 * @param bHaveOutput
	 */
	void FAudioDecoderAAC::NotifyReadyBufferListener(bool bHaveOutput)
	{
		if (ReadyBufferListener)
		{
			IDecoderOutputBufferListener::FDecodeReadyStats stats;
			stats.MaxDecodedElementsReady = MaxDecodeBufferSize;
			stats.NumElementsInDecoder = CurrentRenderOutputBuffer ? 1 : 0;
			stats.bOutputStalled = !bHaveOutput;
			stats.bEODreached = NextAccessUnits.ReachedEOD() && stats.NumDecodedElementsReady == 0 && stats.NumElementsInDecoder == 0;
			ListenerCriticalSection.Lock();
			if (ReadyBufferListener)
			{
				ReadyBufferListener->DecoderOutputReady(stats);
			}
			ListenerCriticalSection.Unlock();
		}
	}



	void FAudioDecoderAAC::PostError(HRESULT ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error)
	{
		check(PlayerSessionServices);
		if (PlayerSessionServices)
		{
			FErrorDetail err;
			err.SetError(Error != UEMEDIA_ERROR_OK ? Error : UEMEDIA_ERROR_DETAIL);
			err.SetFacility(Facility::EFacility::AACDecoder);
			err.SetCode(Code);
			err.SetMessage(Message);

			if (ApiReturnValue != S_OK)
			{
				err.SetPlatformMessage(FString::Printf(TEXT("%s (0x%08lx)"), *GetComErrorDescription(ApiReturnValue), ApiReturnValue));
			}
			PlayerSessionServices->PostError(err);
		}
	}

	void FAudioDecoderAAC::LogMessage(IInfoLog::ELevel Level, const FString& Message)
	{
		if (PlayerSessionServices)
		{
			PlayerSessionServices->PostLog(Facility::EFacility::AACDecoder, Level, Message);
		}
	}



	bool FAudioDecoderAAC::InternalDecoderCreate()
	{
		TRefCountPtr<IMFTransform>	Decoder;
		TRefCountPtr<IMFMediaType>	MediaType;
		HRESULT					res;

		ChannelMapper.Reset();
		if (!ConfigRecord.IsValid())
		{
			PostError(0, "No CSD to create audio decoder with", ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
			return false;
		}

		// Create decoder transform
		VERIFY_HR(CoCreateInstance(MFTmsAACDecoder_Audio, nullptr, CLSCTX_INPROC_SERVER, IID_IMFTransform, reinterpret_cast<void**>(&Decoder)), "CoCreateInstance failed", ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);

		// Create input media type
		VERIFY_HR(MFCreateMediaType(MediaType.GetInitReference()), "MFCreateMediaType failed", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUT_MEDIA_TYPE);
		VERIFY_HR(MediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio), "Failed to set input media type for audio", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_AUDIO_MAJOR_TYPE);
		UINT32 PayloadType = 0;	// 0=raw, 1=adts, 2=adif, 3=latm
		VERIFY_HR(MediaType->SetGUID(MF_MT_SUBTYPE, MEDIASUBTYPE_RAW_AAC1_Audio), "Failed to set input media audio type to RAW AAC", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_AUDIO_AAC_SUBTYPE);
		VERIFY_HR(MediaType->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, PayloadType), "Failed to set input media audio payload type", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_AUDIO_AAC_SUBTYPE);
		VERIFY_HR(MediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, ConfigRecord->SamplingRate), FString::Printf(TEXT("Failed to set input audio sampling rate to %u"), ConfigRecord->SamplingRate), ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_AUDIO_SAMPLING_RATE);
		// ConfigRecord->ChannelConfiguration is in the range 0-15.
		check(ConfigRecord->ChannelConfiguration < 16);
		if (NumChannelsForConfig[ConfigRecord->ChannelConfiguration])
		{
			VERIFY_HR(MediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, NumChannelsForConfig[ConfigRecord->ChannelConfiguration]), FString::Printf(TEXT("Failed to set input audio number of channels for configuration %u"), ConfigRecord->ChannelConfiguration), ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_AUDIO_CHANNEL_COUNT);
		}
		VERIFY_HR(MediaType->SetBlob(MF_MT_USER_DATA, ConfigRecord->GetCodecSpecificData().GetData(), ConfigRecord->GetCodecSpecificData().Num()), "Failed to set input audio CSD", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_AUDIO_CSD);
		// Set input media type with decoder
		VERIFY_HR(Decoder->SetInputType(0, MediaType, 0), "Failed to set audio decoder input type", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_TYPE);
		DecoderTransform = Decoder;

		// Set decoder output type to PCM
		if (!SetDecoderOutputType())
		{
			DecoderTransform = nullptr;
			return false;
		}
		// Get input and output stream information from decoder
		VERIFY_HR(DecoderTransform->GetInputStreamInfo(0, &DecoderInputStreamInfo), "Failed to get audio decoder input stream information", ERRCODE_INTERNAL_COULD_NOT_GET_INPUT_STREAM_FORMAT_INFO);
		VERIFY_HR(DecoderTransform->GetOutputStreamInfo(0, &DecoderOutputStreamInfo), "Failed to get audio decoder output stream information", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_STREAM_FORMAT_INFO);

		// Start the decoder transform
		VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0), "Failed to set audio decoder stream begin", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_BEGIN);
		VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0), "Failed to start audio decoder", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_START);

		return true;
	}


	bool FAudioDecoderAAC::SetDecoderOutputType()
	{
		TRefCountPtr<IMFMediaType>	MediaType;
		HRESULT						res;
		uint32						TypeIndex = 0;
		while(SUCCEEDED(DecoderTransform->GetOutputAvailableType(0, TypeIndex++, MediaType.GetInitReference())))
		{
			GUID Subtype;
			res = MediaType->GetGUID(MF_MT_SUBTYPE, &Subtype);
			if (SUCCEEDED(res) && Subtype == MFAudioFormat_PCM)
			{
				VERIFY_HR(DecoderTransform->SetOutputType(0, MediaType, 0), "Failed to set audio decoder output type", ERRCODE_INTERNAL_COULD_NOT_SET_OUTPUT_TYPE);
				CurrentOutputMediaType = MediaType;
				return true;
			}
		}
		PostError(S_OK, "Failed to set audio decoder output type to PCM", ERRCODE_INTERNAL_COULD_NOT_SET_OUTPUT_TYPE_TO_PCM);
		return false;
	}



	void FAudioDecoderAAC::ReturnUnusedFrame()
	{
		if (CurrentRenderOutputBuffer)
		{
			OutputBufferSampleProperties.Clear();
			Renderer->ReturnBuffer(CurrentRenderOutputBuffer, false, OutputBufferSampleProperties);
			CurrentRenderOutputBuffer = nullptr;
		}
	}



	bool FAudioDecoderAAC::CreateDecoderOutputBuffer()
	{
		HRESULT									res;
		TUniquePtr<FDecoderOutputBuffer>		NewDecoderOutputBuffer(new FDecoderOutputBuffer);

		VERIFY_HR(DecoderTransform->GetOutputStreamInfo(0, &NewDecoderOutputBuffer->mOutputStreamInfo), "Failed to get audio decoder output stream information", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_STREAM_FORMAT_INFO);
		// Do we need to provide the sample output buffer or does the decoder create it for us?
		if ((NewDecoderOutputBuffer->mOutputStreamInfo.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) == 0)
		{
			// We have to provide the output sample buffer.
			TRefCountPtr<IMFSample>		OutputSample;
			TRefCountPtr<IMFMediaBuffer>	OutputBuffer;
			VERIFY_HR(MFCreateSample(OutputSample.GetInitReference()), "Failed to create output sample for audio decoder", ERRCODE_INTERNAL_COULD_NOT_CREATE_OUTPUT_SAMPLE);
			if (NewDecoderOutputBuffer->mOutputStreamInfo.cbAlignment > 0)
			{
				VERIFY_HR(MFCreateAlignedMemoryBuffer(NewDecoderOutputBuffer->mOutputStreamInfo.cbSize, NewDecoderOutputBuffer->mOutputStreamInfo.cbAlignment, OutputBuffer.GetInitReference()), "Failed to create aligned output buffer for audio decoder", ERRCODE_INTERNAL_COULD_NOT_CREATE_ALIGNED_OUTPUTBUFFER);
			}
			else
			{
				VERIFY_HR(MFCreateMemoryBuffer(NewDecoderOutputBuffer->mOutputStreamInfo.cbSize, OutputBuffer.GetInitReference()), "Failed to create output buffer for audio decoder", ERRCODE_INTERNAL_COULD_NOT_CREATE_OUTPUTBUFFER);
			}
			VERIFY_HR(OutputSample->AddBuffer(OutputBuffer.GetReference()), "Failed to add sample buffer to output sample for audio decoder", ERRCODE_INTERNAL_COULD_NOT_ADD_OUTPUT_BUFFER_TO_SAMPLE);
			(NewDecoderOutputBuffer->mOutputBuffer.pSample = OutputSample.GetReference())->AddRef();
			OutputSample = nullptr;
		}
		CurrentDecoderOutputBuffer = MoveTemp(NewDecoderOutputBuffer);
		return true;
	}

	bool FAudioDecoderAAC::Decode(TSharedPtrTS<FDecoderInput> AU, bool bFlushOnly)
	{
		HRESULT					res;
		TRefCountPtr<IMFSample>	InputSample;
		bool					bFlush = !AU.IsValid();

		if (bFlush)
		{
			VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0), "Failed to set audio decoder end of stream notification", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_ENDOFSTREAM);
			if (bFlushOnly)
			{
				VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0), "Failed to issue audio decoder flush command", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_FLUSHCOMMAND);
			}
			else
			{
				VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0), "Failed to issue audio decoder drain command", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_DRAINCOMMAND);
			}
		}
		else
		{
			// Create the input sample.
			TRefCountPtr<IMFMediaBuffer>	InputSampleBuffer;
			BYTE*							pbNewBuffer = nullptr;
			DWORD							dwMaxBufferSize = 0;
			DWORD							dwSize = 0;
			LONGLONG						llSampleTime = 0;

			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);

			VERIFY_HR(MFCreateSample(InputSample.GetInitReference()), "Failed to create audio decoder input sample", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUT_SAMPLE);
			VERIFY_HR(MFCreateMemoryBuffer((DWORD)AU->AccessUnit->AUSize, InputSampleBuffer.GetInitReference()), "Failed to create audio decoder input sample memory buffer", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUTBUFFER);
			VERIFY_HR(InputSample->AddBuffer(InputSampleBuffer.GetReference()), "Failed to set audio decoder input buffer with sample", ERRCODE_INTERNAL_COULD_NOT_ADD_INPUT_BUFFER_TO_SAMPLE);
			VERIFY_HR(InputSampleBuffer->Lock(&pbNewBuffer, &dwMaxBufferSize, &dwSize), "Failed to lock audio decoder input sample buffer", ERRCODE_INTERNAL_COULD_NOT_LOCK_INPUT_BUFFER);
			FMemory::Memcpy(pbNewBuffer, AU->AccessUnit->AUData, AU->AccessUnit->AUSize);
			VERIFY_HR(InputSampleBuffer->Unlock(), "Failed to unlock audio decoder input sample buffer", ERRCODE_INTERNAL_COULD_NOT_UNLOCK_INPUT_BUFFER);
			VERIFY_HR(InputSampleBuffer->SetCurrentLength((DWORD)AU->AccessUnit->AUSize), "Failed to set audio decoder input sample buffer length", ERRCODE_INTERNAL_COULD_NOT_SET_BUFFER_CURRENT_LENGTH);
			// Set sample attributes
			llSampleTime = AU->PTS;
			VERIFY_HR(InputSample->SetSampleTime(llSampleTime), "Failed to set audio decoder input sample decode time", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_SAMPLE_TIME);
			llSampleTime = AU->AccessUnit->Duration.GetAsHNS();
			VERIFY_HR(InputSample->SetSampleDuration(llSampleTime), "Failed to set audio decode input sample duration", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_SAMPLE_DURATION);
		}

		while(!TerminateThreadEvent.IsSignaled())
		{
			if (FlushDecoderEvent.IsSignaled() && !bFlush)
			{
				break;
			}
			if (!CurrentDecoderOutputBuffer.IsValid())
			{
				SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
				CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);

				if (!CreateDecoderOutputBuffer())
				{
					return false;
				}
			}

			DWORD dwStatus = 0;
			CurrentDecoderOutputBuffer->PrepareForProcess();

			{
				SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
				CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);
				res = DecoderTransform->ProcessOutput(0, 1, &CurrentDecoderOutputBuffer->mOutputBuffer, &dwStatus);
			}

			if (res == MF_E_TRANSFORM_NEED_MORE_INPUT)
			{
				// Flushing / draining?
				if (bFlush)
				{
					// Yes. This means we have received all pending output and are done now.
					SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
					CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);
					if (!bFlushOnly)
					{
						// After a drain issue a flush.
						VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0), "Failed to issue audio decoder flush command", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_FLUSHCOMMAND);
					}
					// And start over.
					VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0), "Failed to set audio decoder stream begin", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_BEGIN);
					VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0), "Failed to start audio decoder", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_START);
					CurrentDecoderOutputBuffer.Reset();
					InDecoderInput.Empty();
					return true;
				}
				else if (InputSample.IsValid())
				{
					VERIFY_HR(DecoderTransform->ProcessInput(0, InputSample.GetReference(), 0), "Failed to process audio decoder input", ERRCODE_INTERNAL_COULD_NOT_PROCESS_INPUT);
					// Used this sample. Have no further input data for now, but continue processing to produce output if possible.
					InputSample = nullptr;

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
				SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
				CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);
				// Update output type.
				if (!SetDecoderOutputType())
				{
					return false;
				}
				// For the sake of argument lets get rid of the output buffer (might be too small)
				CurrentDecoderOutputBuffer.Reset();
			}
			else if (SUCCEEDED(res))
			{
				TRefCountPtr<IMFSample> DecodedOutputSample = CurrentDecoderOutputBuffer->DetachOutputSample();
				CurrentDecoderOutputBuffer.Reset();

				TRefCountPtr<IMFMediaBuffer>	DecodedLinearOutputBuffer;
				DWORD							dwBufferLen;
				DWORD							dwMaxBufferLen;
				BYTE*							pDecompressedData = nullptr;
				LONGLONG						llTimeStamp = 0;
				WAVEFORMATEX					OutputWaveFormat;
				WAVEFORMATEX*					OutputWaveFormatPtr = nullptr;
				UINT32							WaveFormatSize = 0;
				DWORD							ChannelMask = 0;

				VERIFY_HR(DecodedOutputSample->GetSampleTime(&llTimeStamp), "Failed to get audio decoder output sample timestamp", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_SAMPLE_TIME);
				VERIFY_HR(DecodedOutputSample->ConvertToContiguousBuffer(DecodedLinearOutputBuffer.GetInitReference()), "Failed to convert audio decoder output sample to contiguous buffer", ERRCODE_INTERNAL_COULD_NOT_MAKE_CONTIGUOUS_OUTPUT_BUFFER);
				VERIFY_HR(DecodedLinearOutputBuffer->GetCurrentLength(&dwBufferLen), "Failed to get audio decoder output buffer current length", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_BUFFER_LENGTH);
				VERIFY_HR(DecodedLinearOutputBuffer->Lock(&pDecompressedData, &dwMaxBufferLen, &dwBufferLen), "Failed to lock audio decoder output buffer", ERRCODE_INTERNAL_COULD_NOT_LOCK_OUTPUT_BUFFER)
				VERIFY_HR(MFCreateWaveFormatExFromMFMediaType(CurrentOutputMediaType.GetReference(), &OutputWaveFormatPtr, &WaveFormatSize, MFWaveFormatExConvertFlag_Normal), "Failed to create audio decoder output buffer format info", ERRCODE_INTERNAL_COULD_NOT_CREATE_OUTPUT_BUFFER_FORMAT_INFO);
				FMemory::Memcpy(&OutputWaveFormat, OutputWaveFormatPtr, sizeof(OutputWaveFormat));
				if (OutputWaveFormatPtr->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
				{
					ChannelMask = ((WAVEFORMATEXTENSIBLE*)OutputWaveFormatPtr)->dwChannelMask;
				}
				CoTaskMemFree(OutputWaveFormatPtr);
				OutputWaveFormatPtr = nullptr;

				FTimeValue pts;
				pts.SetFromHNS((int64)llTimeStamp);

				// Attempt to locate the information we saved when we sent the data for decoding.
				TSharedPtrTS<FDecoderInput> MatchingInput;
				FindAndUpdateDecoderInput(MatchingInput, (int64) llTimeStamp);


				int32 nBytesPerSample = sizeof(int16) * OutputWaveFormat.nChannels;
				int32 nSamplesProduced = dwBufferLen / nBytesPerSample;
				const int16* pPCMSamples = (const int16*)pDecompressedData;
				//int32 nSamplesProduced = dwBufferLen / (sizeof(float) * OutputWaveFormat.nChannels);
				//const float *pPCMSamples = (const float *)pDecompressedData;

				if (!ChannelMapper.IsInitialized())
				{
					bool bMapperOk = false;
					if (ChannelMask == 0)
					{
						check(ConfigRecord.IsValid());
						if (ConfigRecord->ChannelConfiguration == OutputWaveFormat.nChannels)
						{
							bMapperOk = ChannelMapper.Initialize(sizeof(int16), ConfigRecord->ChannelConfiguration);
						}
						else if (ConfigRecord->PSSignal == 1)
						{
							// Parametric stereo outputs as stereo
							bMapperOk = ChannelMapper.Initialize(sizeof(int16), OutputWaveFormat.nChannels);
						}
						else
						{
							// For up to 6 channels the AAC channel configuration value lines up with the actual number
							// of decoded channels albeit with a different layout. In absence of any better knowledge we
							// just take the number of channels for the configuration value and hope the channels line up.
							bMapperOk = ChannelMapper.Initialize(sizeof(int16), OutputWaveFormat.nChannels);
						}
					}
					else
					{
						// The channel positions from MS are oddly named with "back left" and "side left" in that "back left" is
						// what is traditionally "left surround" in 5.1 and "left surround side" in 7.1 with the additional 2 surround
						// channels called "left surround rear".
						// As long as the AAC decoder only decodes 6 channels and the two surrounds have the channel mask set as
						// "SPEAKER_BACK_LEFT" and "SPEAKER_BACK_RIGHT" there's no ambiguity.
						static FAudioChannelMapper::EChannelPosition ChannelPositions[] = {
							FAudioChannelMapper::EChannelPosition::L,		// SPEAKER_FRONT_LEFT
							FAudioChannelMapper::EChannelPosition::R,		// SPEAKER_FRONT_RIGHT
							FAudioChannelMapper::EChannelPosition::C,		// SPEAKER_FRONT_CENTER
							FAudioChannelMapper::EChannelPosition::LFE,		// SPEAKER_LOW_FREQUENCY
							FAudioChannelMapper::EChannelPosition::Ls,		// SPEAKER_BACK_LEFT
							FAudioChannelMapper::EChannelPosition::Rs,		// SPEAKER_BACK_RIGHT
							FAudioChannelMapper::EChannelPosition::Lc,		// SPEAKER_FRONT_LEFT_OF_CENTER
							FAudioChannelMapper::EChannelPosition::Rc,		// SPEAKER_FRONT_RIGHT_OF_CENTER
							FAudioChannelMapper::EChannelPosition::Cs,		// SPEAKER_BACK_CENTER
							FAudioChannelMapper::EChannelPosition::Lss,		// SPEAKER_SIDE_LEFT
							FAudioChannelMapper::EChannelPosition::Rss,		// SPEAKER_SIDE_RIGHT
							FAudioChannelMapper::EChannelPosition::Ts,		// SPEAKER_TOP_CENTER
							FAudioChannelMapper::EChannelPosition::Lv,		// SPEAKER_TOP_FRONT_LEFT
							FAudioChannelMapper::EChannelPosition::Cv,		// SPEAKER_TOP_FRONT_CENTER
							FAudioChannelMapper::EChannelPosition::Rv,		// SPEAKER_TOP_FRONT_RIGHT
							FAudioChannelMapper::EChannelPosition::Lvs,		// SPEAKER_TOP_BACK_LEFT
							FAudioChannelMapper::EChannelPosition::Cvr,		// SPEAKER_TOP_BACK_CENTER
							FAudioChannelMapper::EChannelPosition::Rvs		// SPEAKER_TOP_BACK_RIGHT
						};
						TArray<FAudioChannelMapper::FSourceLayout> Layout;
						for(int32 i=0; i<UE_ARRAY_COUNT(ChannelPositions); ++i)
						{
							if ((ChannelMask & (1 << i)) != 0)
							{
								FAudioChannelMapper::FSourceLayout lo;
								lo.ChannelPosition = ChannelPositions[i];
								Layout.Emplace(MoveTemp(lo));
							}
						}
						bMapperOk = ChannelMapper.Initialize(sizeof(int16), Layout);
					}
					if (!bMapperOk)
					{
						PostError(S_OK, "Failed to set up the output channel mapper", ERRCODE_INTERNAL_FAILED_TO_MAP_OUTPUT_CHANNELS, UEMEDIA_ERROR_INTERNAL);
						return false;
					}
				}

				// Get an sample block from the pool.
				while(!TerminateThreadEvent.IsSignaled())
				{
					if (FlushDecoderEvent.IsSignaled() && !bFlush)
					{
						break;
					}

					if (CurrentRenderOutputBuffer == nullptr)
					{
						SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
						CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);

						UEMediaError bufResult = Renderer->AcquireBuffer(CurrentRenderOutputBuffer, 0, BufferAcquireOptions);
						check(bufResult == UEMEDIA_ERROR_OK || bufResult == UEMEDIA_ERROR_INSUFFICIENT_DATA);
						if (bufResult != UEMEDIA_ERROR_OK && bufResult != UEMEDIA_ERROR_INSUFFICIENT_DATA)
						{
							PostError(S_OK, "Failed to acquire sample buffer", ERRCODE_INTERNAL_COULD_NOT_GET_SAMPLE_BUFFER, bufResult);
							return false;
						}
					}

					bool bHaveAvailSmpBlk = CurrentRenderOutputBuffer != nullptr;
					// Check if the renderer can accept the output we will want to send to it.
					// If it can't right now we treat this as if we do not have an available output buffer.
					if (Renderer.IsValid() && !Renderer->CanReceiveOutputFrames(1))
					{
						bHaveAvailSmpBlk = false;
					}

					NotifyReadyBufferListener(bHaveAvailSmpBlk);
					if (bHaveAvailSmpBlk)
					{
						SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACConvertOutput);
						CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACConvertOutput);

						DWORD CurrentRenderOutputBufferSize = (DWORD)CurrentRenderOutputBuffer->GetBufferProperties().GetValue("size").GetInt64();
						check(dwBufferLen <= CurrentRenderOutputBufferSize);
						if (dwBufferLen <= CurrentRenderOutputBufferSize)
						{
							int32 ByteOffsetToFirstSample = 0;
							if (MatchingInput.IsValid())
							{
								if (MatchingInput->StartOverlapDuration.GetAsHNS() || MatchingInput->EndOverlapDuration.GetAsHNS())
								{
									int32 SkipStartSampleNum = (int32) (MatchingInput->StartOverlapDuration.GetAsHNS() * OutputWaveFormat.nSamplesPerSec / 10000000);
									int32 SkipEndSampleNum = (int32) (MatchingInput->EndOverlapDuration.GetAsHNS() * OutputWaveFormat.nSamplesPerSec / 10000000);

									if (SkipStartSampleNum + SkipEndSampleNum < nSamplesProduced)
									{
										ByteOffsetToFirstSample = SkipStartSampleNum * nBytesPerSample;
										nSamplesProduced -= SkipStartSampleNum;
										nSamplesProduced -= SkipEndSampleNum;
										dwBufferLen -= (SkipStartSampleNum + SkipEndSampleNum) * nBytesPerSample;
									}
									else
									{
										nSamplesProduced = 0;
										dwBufferLen = 0;
									}
								}
								pts = MatchingInput->AdjustedPTS;
							}

							if (nSamplesProduced)
							{
								void* CurrentRenderOutputBufferAddress = CurrentRenderOutputBuffer->GetBufferProperties().GetValue("address").GetPointer();
								ChannelMapper.MapChannels(CurrentRenderOutputBufferAddress, CurrentRenderOutputBufferSize, AdvancePointer(pPCMSamples, ByteOffsetToFirstSample), dwBufferLen, nSamplesProduced);

								FTimeValue OutputSampleDuration;
								OutputSampleDuration.SetFromND(nSamplesProduced, OutputWaveFormat.nSamplesPerSec);

								OutputBufferSampleProperties.Clear();
								OutputBufferSampleProperties.Set("num_channels", FVariantValue((int64)ChannelMapper.GetNumTargetChannels()));
								OutputBufferSampleProperties.Set("sample_rate", FVariantValue((int64)OutputWaveFormat.nSamplesPerSec));
								OutputBufferSampleProperties.Set("byte_size", FVariantValue((int64)(ChannelMapper.GetNumTargetChannels() * nSamplesProduced * sizeof(int16))));
								OutputBufferSampleProperties.Set("duration", FVariantValue(OutputSampleDuration));
								OutputBufferSampleProperties.Set("pts", FVariantValue(pts));
								OutputBufferSampleProperties.Set("discontinuity", FVariantValue((bool)bHaveDiscontinuity));

								Renderer->ReturnBuffer(CurrentRenderOutputBuffer, true, OutputBufferSampleProperties);
								CurrentRenderOutputBuffer = nullptr;
							}
							else
							{
								ReturnUnusedFrame();
							}
							bHaveDiscontinuity = false;

							break;
						}
						else
						{
							PostError(S_OK, "Audio renderer buffer too small to receive decoded audio samples", ERRCODE_INTERNAL_RENDER_BUFFER_TOO_SMALL);
							return false;
						}
					}
					else
					{
						// No available buffer. Sleep for a bit. Can't sleep on a signal since we have to check two: abort and flush
						// We sleep for 20ms since a sample block for LC AAC produces 1024 samples which amount to 21.3ms @48kHz (or more at lower sample rates).
						FMediaRunnable::SleepMilliseconds(20);
					}
				}
				DecodedLinearOutputBuffer->Unlock();
			}
			else
			{
				// Error!
				VERIFY_HR(res, "Failed to process audio decoder output", ERRCODE_INTERNAL_COULD_NOT_PROCESS_OUTPUT);
				return false;
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

	bool FAudioDecoderAAC::FindAndUpdateDecoderInput(TSharedPtrTS<FDecoderInput>& OutMatchingInput, int64 InPTSFromDecoder)
	{
		if (InDecoderInput.Num())
		{
			OutMatchingInput = InDecoderInput[0];
			InDecoderInput.RemoveAt(0);
			return true;
		}
		LogMessage(IInfoLog::ELevel::Warning, FString::Printf(TEXT("No decoder input found for decoded audio PTS %lld. List is empty!"), (long long int)InPTSFromDecoder));
		return false;
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

		bool bDone = false;
		bool bError = false;
		bool bInDummyDecodeMode = false;
		TOptional<int64> SequenceIndex;

		bHaveDiscontinuity = false;
		CurrentRenderOutputBuffer = nullptr;

		DecoderInputStreamInfo = {};
		DecoderOutputStreamInfo = {};

		bError = !CreateDecodedSamplePool();
		check(!bError);

		while(!TerminateThreadEvent.IsSignaled())
		{
			// If in background, wait until we get activated again.
			if (!ApplicationRunningSignal.IsSignaled())
			{
				ApplicationSuspendConfirmedSignal.Signal();
				while(!ApplicationRunningSignal.WaitTimeout(100 * 1000) && !TerminateThreadEvent.IsSignaled())
				{
				}
				continue;
			}

			// Ask the buffer listener for an AU.
			if (!bError && InputBufferListener && NextAccessUnits.IsEmpty())
			{
				SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
				CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);

				IAccessUnitBufferListener::FBufferStats	stats;
				stats.bEODSignaled = NextAccessUnits.GetEOD();
				stats.bEODReached = NextAccessUnits.ReachedEOD();
				ListenerCriticalSection.Lock();
				if (InputBufferListener)
				{
					InputBufferListener->DecoderInputNeeded(stats);
				}
				ListenerCriticalSection.Unlock();
			}
			// Wait for data to arrive. If no data for 10ms check if we're supposed to abort.
			bool bHaveData = NextAccessUnits.Wait(1000 * 10);
			if (bHaveData)
			{
				bDone = false;
				TSharedPtrTS<FDecoderInput> CurrentAccessUnit;
				bool bOk = NextAccessUnits.Dequeue(CurrentAccessUnit);
				MEDIA_UNUSED_VAR(bOk);
				check(bOk);

				PrepareAU(CurrentAccessUnit);
				if (!SequenceIndex.IsSet())
				{
					SequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
				}

				// Check if the format has changed such that we need to flush and re-create the decoder.
				if (CurrentAccessUnit->AccessUnit->bTrackChangeDiscontinuity || IsDifferentFormat(CurrentAccessUnit) || (CurrentAccessUnit->AccessUnit->bIsDummyData && !bInDummyDecodeMode) || SequenceIndex.GetValue() != CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex())
				{
					if (DecoderTransform.IsValid())
					{
						Decode(nullptr, true);
						bHaveDiscontinuity = true;
					}
					ReturnUnusedFrame();
					DecoderTransform = nullptr;
					CurrentOutputMediaType = nullptr;
					ConfigRecord = nullptr;
					CurrentCodecData = nullptr;
					ChannelMapper.Reset();
					InDecoderInput.Empty();
				}

				// Parse the CSD into a configuration record.
				if (!ConfigRecord.IsValid() && !bError)
				{
					if (CurrentAccessUnit->AccessUnit->AUCodecData.IsValid())
					{
						CurrentCodecData = CurrentAccessUnit->AccessUnit->AUCodecData;
						ConfigRecord = MakeShared<MPEG::FAACDecoderConfigurationRecord, ESPMode::ThreadSafe>();
						if (!ConfigRecord->ParseFrom(CurrentCodecData->CodecSpecificData.GetData(), CurrentCodecData->CodecSpecificData.Num()))
						{
							ConfigRecord.Reset();
							CurrentCodecData.Reset();
							PostError(S_OK, "Failed to parse AAC configuration record", ERRCODE_INTERNAL_FAILED_TO_PARSE_CSD);
							bError = true;
						}
					}
				}

				// Is this audio packet to be dropped?
				if (!CurrentAccessUnit->AdjustedPTS.IsValid())
				{
					CurrentAccessUnit.Reset();
					continue;
				}

				// Need to create a decoder instance?
				if (!DecoderTransform.IsValid() && !bError)
				{
					SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
					CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);

					// Can't create a decoder based on dummy data.
					if (!CurrentAccessUnit->AccessUnit->bIsDummyData)
					{
						if (InternalDecoderCreate())
						{
							// Ok
						}
						else
						{
							bError = true;
						}
					}
				}

				if (!CurrentAccessUnit->AccessUnit->bIsDummyData)
				{
					if (bInDummyDecodeMode)
					{
						bInDummyDecodeMode = false;
						bHaveDiscontinuity = true;
					}
					SequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
					if (!bError && !Decode(CurrentAccessUnit, false))
					{
						bError = true;
					}
				}
				else
				{
					if (!bInDummyDecodeMode)
					{
						bInDummyDecodeMode = true;
						bHaveDiscontinuity = true;
					}

					while(!bError && !TerminateThreadEvent.IsSignaled() && !FlushDecoderEvent.IsSignaled())
					{
						// Need a new output buffer?
						if (CurrentRenderOutputBuffer == nullptr)
						{
							SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACDecode);
							CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACDecode);

							UEMediaError bufResult = Renderer->AcquireBuffer(CurrentRenderOutputBuffer, 0, BufferAcquireOptions);
							check(bufResult == UEMEDIA_ERROR_OK || bufResult == UEMEDIA_ERROR_INSUFFICIENT_DATA);
							if (bufResult != UEMEDIA_ERROR_OK && bufResult != UEMEDIA_ERROR_INSUFFICIENT_DATA)
							{
								PostError(S_OK, "Failed to acquire sample buffer", ERRCODE_INTERNAL_COULD_NOT_GET_SAMPLE_BUFFER, bufResult);
								bError = true;
							}
						}
						bool bHaveAvailSmpBlk = CurrentRenderOutputBuffer != nullptr;
						NotifyReadyBufferListener(bHaveAvailSmpBlk);
						if (bHaveAvailSmpBlk)
						{
							break;
						}
						else
						{
							FMediaRunnable::SleepMilliseconds(20);
						}
					}

					if (CurrentRenderOutputBuffer != nullptr)
					{
						SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_AudioAACConvertOutput);
						CSV_SCOPED_TIMING_STAT(ElectraPlayer, AudioAACConvertOutput);

						// Clear to silence
						SIZE_T CurrentRenderOutputBufferSize = (SIZE_T)CurrentRenderOutputBuffer->GetBufferProperties().GetValue("size").GetInt64();
						void* CurrentRenderOutputBufferAddress = CurrentRenderOutputBuffer->GetBufferProperties().GetValue("address").GetPointer();
						FMemory::Memzero(CurrentRenderOutputBufferAddress, CurrentRenderOutputBufferSize);

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
							Duration.SetFromND(SamplesPerBlock, (uint32)SampleRate);

							OutputBufferSampleProperties.Clear();
							OutputBufferSampleProperties.Set("num_channels", FVariantValue(NumChannels));
							OutputBufferSampleProperties.Set("sample_rate", FVariantValue(SampleRate));
							OutputBufferSampleProperties.Set("byte_size", FVariantValue((int64)(NumChannels * sizeof(int16) * SamplesPerBlock)));
							OutputBufferSampleProperties.Set("duration", FVariantValue(Duration));
							OutputBufferSampleProperties.Set("pts", FVariantValue(CurrentAccessUnit->AdjustedPTS));
							OutputBufferSampleProperties.Set("eod", FVariantValue((bool)false));
							OutputBufferSampleProperties.Set("discontinuity", FVariantValue((bool)bHaveDiscontinuity));

							Renderer->ReturnBuffer(CurrentRenderOutputBuffer, true, OutputBufferSampleProperties);
							CurrentRenderOutputBuffer = nullptr;
						}
						else
						{
							ReturnUnusedFrame();
						}
						bHaveDiscontinuity = false;
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
				// No data. Is the buffer at EOD?
				if (NextAccessUnits.ReachedEOD())
				{
					if (!bDone && !bError && DecoderTransform.IsValid())
					{
						bError = !Decode(nullptr, false);
						InDecoderInput.Empty();
					}
					NotifyReadyBufferListener(true);
					bDone = true;
					FMediaRunnable::SleepMilliseconds(20);
				}
			}

			// Flush?
			if (FlushDecoderEvent.IsSignaled())
			{
				if (DecoderTransform.IsValid())
				{
					Decode(nullptr, true);
				}

				ReturnUnusedFrame();
				NextAccessUnits.Empty();
				InDecoderInput.Empty();
				SequenceIndex.Reset();

				FlushDecoderEvent.Reset();
				DecoderFlushedEvent.Signal();
				bDone = false;
			}
		}

		ReturnUnusedFrame();
		// Close the decoder.
		DecoderTransform = nullptr;
		CurrentOutputMediaType = nullptr;

		DestroyDecodedSamplePool();

		CurrentCodecData.Reset();
		ConfigRecord.Reset();

		// Flush any remaining input data.
		NextAccessUnits.Empty();
		InDecoderInput.Empty();

		RemoveBGFGNotificationHandler(FGBGHandlers);
	}


} // namespace Electra


#endif

