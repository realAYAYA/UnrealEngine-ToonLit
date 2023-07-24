// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCore.h"
#include "PlayerRuntimeGlobal.h"

#include "Stats/Stats.h"
#include "StreamAccessUnitBuffer.h"
#include "Decoder/VideoDecoderH265.h"
#include "Decoder/Android/DecoderOptionNames_Android.h"
#include "Renderer/RendererBase.h"
#include "Player/PlayerSessionServices.h"
#include "Utilities/StringHelpers.h"
#include "Utilities/UtilsMPEGVideo.h"
#include "DecoderErrors_Android.h"
#include "HAL/LowLevelMemTracker.h"
#include "Android/AndroidPlatformMisc.h"
#include "ElectraPlayerPrivate.h"
#include "Decoder/VideoDecoderHelpers.h"

#include "VideoDecoderH265_JavaWrapper_Android.h"
#include "MediaVideoDecoderOutputAndroid.h"
#include "Renderer/RendererVideo.h"
#include "ElectraVideoDecoder_Android.h"

#include "Android/AndroidPlatform.h"
#include "Android/AndroidJava.h"

#if ELECTRA_PLATFORM_HAS_H265_DECODER

DECLARE_CYCLE_STAT(TEXT("FVideoDecoderH265::Decode()"), STAT_ElectraPlayer_VideoH265Decode, STATGROUP_ElectraPlayer);
DECLARE_CYCLE_STAT(TEXT("FVideoDecoderH265::ConvertOutput()"), STAT_ElectraPlayer_VideoH265ConvertOutput, STATGROUP_ElectraPlayer);


namespace Electra
{

/**
 * H265 video decoder class implementation.
**/
class FVideoDecoderH265 : public IVideoDecoderH265, public FMediaThread
{
public:
	static bool Startup(const FParamDict& Options);
	static void Shutdown();

	static FParamDict& Android_Workarounds()
	{
		static FParamDict Workarounds;
		return Workarounds;
	}

	FVideoDecoderH265();
	virtual ~FVideoDecoderH265();

	virtual void SetPlayerSessionServices(IPlayerSessionServices* SessionServices) override;

	virtual void Open(const FInstanceConfiguration& InConfig) override;
	virtual void Close() override;
	virtual void DrainForCodecChange() override;

	virtual void SetMaximumDecodeCapability(int32 MaxTier, int32 MaxWidth, int32 MaxHeight, int32 MaxProfile, int32 MaxProfileLevel, const FParamDict& AdditionalOptions) override;

	virtual void SetAUInputBufferListener(IAccessUnitBufferListener* Listener) override;

	virtual void SetReadyBufferListener(IDecoderOutputBufferListener* Listener) override;

	virtual void SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer) override;

	virtual void SetResourceDelegate(const TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>& ResourceDelegate) override;

	virtual void AUdataPushAU(FAccessUnit* AccessUnit) override;
	virtual void AUdataPushEOD() override;
	virtual void AUdataClearEOD() override;
	virtual void AUdataFlushEverything() override;

	virtual void Android_UpdateSurface(const TSharedPtr<IOptionPointerValueContainer, ESPMode::ThreadSafe>& Surface) override;
	virtual void Android_SuspendOrResumeDecoder(bool bSuspend) override;

	static void ReleaseToSurface(uint32 NativeDecoderID, const FDecoderTimeStamp& Time);

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
		bool			bIsIDR = false;
		bool			bIsDiscardable = false;
		int64			PTS = 0;
		int64			EndPTS = 0;
		FTimeValue		AdjustedPTS;
		FTimeValue		AdjustedDuration;

		int32			TotalWidth = 0;
		int32			TotalHeight = 0;
		int32			CropLeft = 0;
		int32			CropRight = 0;
		int32			CropTop = 0;
		int32			CropBottom = 0;
		int32			AspectX = 0;
		int32			AspectY = 0;

		TArray<MPEG::FSEIMessage> CSDPrefixSEIMessages;
		TArray<MPEG::FSEIMessage> CSDSuffixSEIMessages;
		TArray<MPEG::FSEIMessage> PrefixSEIMessages;
		TArray<MPEG::FSEIMessage> SuffixSEIMessages;
		TArray<MPEG::FISO23008_2_seq_parameter_set_data> SPSs;
	};

	struct FDecoderFormatInfo
	{
		void Reset()
		{
			CurrentCodecData.Reset();
			CSD.Reset();
		}
		void Flushed()
		{
			CSD.Reset();
		}
		bool IsDifferentFrom(TSharedPtrTS<FDecoderInput> AU)
		{
			// If there is no current CSD set, set it initially.
			if (!CurrentCodecData.IsValid())
			{
				SetFrom(AU);
			}
			return AU->AccessUnit->AUCodecData.IsValid() && AU->AccessUnit->AUCodecData.Get() != CurrentCodecData.Get();
		}

		void SetFrom(TSharedPtrTS<FDecoderInput> AU)
		{
			if (AU.IsValid() && AU->AccessUnit && AU->AccessUnit->AUCodecData.IsValid())
			{
				CurrentCodecData = AU->AccessUnit->AUCodecData;
			}
		}

		void UpdateFromCSD(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU)
		{
			if (AU->AccessUnit->AUCodecData.IsValid() && AU->AccessUnit->AUCodecData.Get() != CSD.Get())
			{
				// Pointers are different. Is the content too?
				bool bDifferent = !CSD.IsValid() || (CSD.IsValid() && AU->AccessUnit->AUCodecData->CodecSpecificData != CSD->CodecSpecificData);
				if (bDifferent)
				{
					PrefixSEIMessages.Empty();
					SuffixSEIMessages.Empty();
					SPSs.Empty();
					// The CSD may contain SEI messages that apply to the stream as a whole.
					// We need to parse the CSD to get them, if there are any.
					TArray<MPEG::FNaluInfo>	NALUs;
					const uint8* pD = AU->AccessUnit->AUCodecData->CodecSpecificData.GetData();
					MPEG::ParseBitstreamForNALUs(NALUs, pD, AU->AccessUnit->AUCodecData->CodecSpecificData.Num());
					for(int32 i=0; i<NALUs.Num(); ++i)
					{
						const uint8* NALU = (const uint8*)Electra::AdvancePointer(pD, NALUs[i].Offset + NALUs[i].UnitLength);
						uint8 nut = *NALU >> 1;
						// Prefix or suffix NUT?
						if (nut == 39 || nut == 40)
						{
							MPEG::ExtractSEIMessages(nut == 39 ? PrefixSEIMessages : SuffixSEIMessages, Electra::AdvancePointer(NALU, 2), NALUs[i].Size - 2, MPEG::ESEIStreamType::H265, nut == 39);
						}
						// SPS nut?
						else if (nut == 33)
						{
							MPEG::FISO23008_2_seq_parameter_set_data sps;
							if (MPEG::ParseH265SPS(sps, NALU, NALUs[i].Size - 2))
							{
								SPSs.Emplace(MoveTemp(sps));
							}
						}
					}
				}
				CSD = AU->AccessUnit->AUCodecData;
			}
		}
		TSharedPtr<const FAccessUnit::CodecData, ESPMode::ThreadSafe> CSD;
		TArray<MPEG::FSEIMessage> PrefixSEIMessages;
		TArray<MPEG::FSEIMessage> SuffixSEIMessages;
		TArray<MPEG::FISO23008_2_seq_parameter_set_data> SPSs;

		TSharedPtr<const FAccessUnit::CodecData, ESPMode::ThreadSafe> CurrentCodecData;
	};

	struct FDecodedImage
	{
		TSharedPtrTS<FDecoderInput> SourceInfo;
		IAndroidJavaH265VideoDecoder::FOutputFormatInfo	OutputFormat;
		IAndroidJavaH265VideoDecoder::FOutputBufferInfo	OutputBufferInfo;
		bool bIsDummy = false;
	};

	struct FOutputBufferInfo
	{
		FOutputBufferInfo()
		{ }
		FOutputBufferInfo(const FDecoderTimeStamp& InTimeStamp, int32 InBufferIndex, int32 InValidCount)
			: Timestamp(InTimeStamp), BufferIndex(InBufferIndex), ValidCount(InValidCount)
		{ }

		FDecoderTimeStamp Timestamp;
		int32 BufferIndex = -1;
		int32 ValidCount = -1;
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

	enum class EDecoderState
	{
		IsFlushed,
		IsActive,
		NeedReconfig
	};

	bool InternalDecoderCreate();
	void InternalDecoderDestroy();
	void RecreateDecoderSession();
	void StartThread();
	void StopThread();
	void WorkerThread();
	void RenderThreadFN();

	bool CreateDecodedImagePool();
	void DestroyDecodedImagePool();

	void NotifyReadyBufferListener(bool bHaveOutput);
	bool AcquireOutputBuffer(IMediaRenderer::IBuffer*& RenderOutputBuffer);

	void GetAndPrepareInputAU();
	void PrepareAU(TSharedPtrTS<FDecoderInput> AU);

	bool CheckForFlush();

	bool PrepareDecoder();
	void UnprepareDecoder();
	EDecodeResult DrainDecoder();
	EDecodeResult Decode();
	EDecodeResult DecodeDummy();

	bool GetMatchingDecoderInput(TSharedPtrTS<FDecoderInput>& OutMatchingInput, int64 InPTSFromDecoder);
	void ProcessReadyOutputBuffersToSurface();
	EOutputResult GetOutput();
	EOutputResult ProcessOutput(const FDecodedImage& NextImage);

	void PostError(int32_t ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error = UEMEDIA_ERROR_OK);
	void LogMessage(IInfoLog::ELevel Level, const FString& Message);

	void HandleApplicationHasEnteredForeground();
	void HandleApplicationWillEnterBackground();


	FInstanceConfiguration															Config;

	bool																			bCfgForceSkipUntilIDROnSurfaceChange = false;
	bool																			bCfgReconfigureSurfaceOnWakeup = false;
	bool																			bCfgForceNewDecoderOnWakeup = false;

	FMediaEvent																		ApplicationRunningSignal;
	FMediaEvent																		ApplicationSuspendConfirmedSignal;
	int32																			ApplicationSuspendCount = 0;

	FMediaEvent																		TerminateThreadSignal;
	FMediaEvent																		FlushDecoderSignal;
	FMediaEvent																		DecoderFlushedSignal;
	bool																			bThreadStarted = false;
	bool																			bDrainForCodecChange = false;
	bool																			bError = false;
	bool																			bDone = false;
	bool																			bBlockedOnInput = false;

	IPlayerSessionServices*															PlayerSessionServices = nullptr;

	TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe>									Renderer;

	TWeakPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>					ResourceDelegate;

	uint32																			NativeDecoderID = 0;

	TAccessUnitQueue<TSharedPtrTS<FDecoderInput>>									NextAccessUnits;
	TAccessUnitQueue<TSharedPtrTS<FDecoderInput>>									ReplayAccessUnits;

	FMediaCriticalSection															ListenerMutex;
	IAccessUnitBufferListener*														InputBufferListener = nullptr;
	IDecoderOutputBufferListener*													ReadyBufferListener = nullptr;

	FDecoderFormatInfo																CurrentStreamFormatInfo;
	MPEG::FColorimetryHelper														Colorimetry;
	MPEG::FHDRHelper																HDR;

	TSharedPtrTS<IAndroidJavaH265VideoDecoder>										DecoderInstance;
	IAndroidJavaH265VideoDecoder::FDecoderInformation								DecoderInfo;
	TSharedPtrTS<FDecoderInput>														CurrentAccessUnit;
	bool																			bMustSendCSD = false;
	EDecodingState																	DecodingState = EDecodingState::Regular;
	EDecoderState																	CurrentDecoderState = EDecoderState::IsFlushed;
	int64																			LastPushedPresentationTimeUs = 0;
	TOptional<int64>																CurrentSequenceIndex;
	TArray<TSharedPtrTS<FDecoderInput>>												InDecoderInput;
	volatile bool																	bForceDecoderRefresh = false;
	bool																			bSkipUntilNextIDR = false;

	TArray<FOutputBufferInfo>														ReadyOutputBuffersToSurface;
	FCriticalSection																OutputSurfaceTargetCS;
	FDecoderTimeStamp																OutputSurfaceTargetPTS;
	int32																			MaxDecodeBufferSize = 0;
	bool																			bSurfaceIsView = false;

	static int32																	NextNativeDecoderID;
	static FCriticalSection															NativeDecoderMapCS;
	static TMap<uint32, FVideoDecoderH265*>											NativeDecoderMap;
};

int32									FVideoDecoderH265::NextNativeDecoderID = 0;
FCriticalSection						FVideoDecoderH265::NativeDecoderMapCS;
TMap<uint32, FVideoDecoderH265*>		FVideoDecoderH265::NativeDecoderMap;

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

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
#if ELECTRA_PLATFORM_ENABLE_HDR == 0
	if (InStreamParameter.Profile > 1 || (InStreamParameter.CompatibilityFlags & 0x60000000) == 0x20000000)
	{
		OutResult.DecoderSupportType = FStreamDecodeCapability::ESupported::NotSupported;
		return true;
	}
#endif
	return false;
}

IVideoDecoderH265::FInstanceConfiguration::FInstanceConfiguration()
	: MaxDecodedFrames(8)
{
}

IVideoDecoderH265* IVideoDecoderH265::Create()
{
	return new FVideoDecoderH265;
}

FParamDict& IVideoDecoderH265::Android_Workarounds()
{
	return FVideoDecoderH265::Android_Workarounds();
}


/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
//-----------------------------------------------------------------------------
/**
 * Decoder system startup
 *
 * @param InOptions
 *
 * @return
 */
bool FVideoDecoderH265::Startup(const FParamDict& InOptions)
{
	// Create a temporary instance of the decoder wrapper. This will initialize the Java class singletons for later use.
	TSharedPtrTS<IAndroidJavaH265VideoDecoder> Temp = IAndroidJavaH265VideoDecoder::Create(nullptr);
	if (Temp.IsValid())
	{
		const IAndroidJavaH265VideoDecoder::FDecoderInformation* DecInf = Temp->GetDecoderInformation();
		if (DecInf)
		{
			Android_Workarounds().SetOrUpdate(TEXT("setOutputSurface"), FVariantValue(DecInf->bCanUse_SetOutputSurface));
		}
	}
	return Temp.IsValid();
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
	: FMediaThread("ElectraPlayer::H265 decoder")
{
	NativeDecoderID = (uint32)FPlatformAtomics::InterlockedIncrement(&NextNativeDecoderID);
	FScopeLock Lock(&NativeDecoderMapCS);
	NativeDecoderMap.Add(NativeDecoderID, this);
}


//-----------------------------------------------------------------------------
/**
 * Destructor
 */
FVideoDecoderH265::~FVideoDecoderH265()
{
	Close();
	FScopeLock Lock(&NativeDecoderMapCS);
	NativeDecoderMap.Remove(NativeDecoderID);
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
 */
void FVideoDecoderH265::SetPlayerSessionServices(IPlayerSessionServices* InSessionServices)
{
	PlayerSessionServices = InSessionServices;
}


//-----------------------------------------------------------------------------
/**
 * Opens a decoder instance
 *
 * @param InConfig
 */
void FVideoDecoderH265::Open(const IVideoDecoderH265::FInstanceConfiguration& InConfig)
{
	Config = InConfig;
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
 * precedence and force a decoder capable of decoding it!
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
	// Not implemented
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


//-----------------------------------------------------------------------------
/**
 * Sets a resource delegate.
 */
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
void FVideoDecoderH265::PostError(int32_t ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error)
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
	bError = true;
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

	UEMediaError Error = Renderer->CreateBufferPool(poolOpts);
	check(Error == UEMEDIA_ERROR_OK);

	MaxDecodeBufferSize = (int32) Renderer->GetBufferPoolProperties().GetValue("max_buffers").GetInt64();

	if (Error != UEMEDIA_ERROR_OK)
	{
		PostError(0, "Failed to create image pool", ERRCODE_INTERNAL_ANDROID_COULD_NOT_CREATE_IMAGE_POOL, Error);
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


//-----------------------------------------------------------------------------
/**
 * Notifies the decoder that there may be further access units.
 */
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
 * Create a decoder instance.
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH265::InternalDecoderCreate()
{
	int32 Result;

	// Check if there is an existing decoder instance we can re purpose
	if (!DecoderInstance.IsValid())
	{
		DecoderInstance = IAndroidJavaH265VideoDecoder::Create(PlayerSessionServices);
		// Create
		Result = DecoderInstance->CreateDecoder();
		if (Result)
		{
			PostError(Result, "Failed to create decoder", ERRCODE_INTERNAL_ANDROID_COULD_NOT_CREATE_VIDEO_DECODER);
			return false;
		}
	}
	else
	{
		DecoderInstance->Flush();
		DecoderInstance->Stop();
	}

	// Configure
	IAndroidJavaH265VideoDecoder::FCreateParameters cp;
	cp.CodecData = CurrentAccessUnit->AccessUnit->AUCodecData;
	cp.MaxWidth = Config.MaxFrameWidth;
	cp.MaxHeight = Config.MaxFrameHeight;
	cp.MaxTier = Config.Tier;
	cp.MaxProfile = Config.Profile;
	cp.MaxProfileLevel = Config.Level;
	cp.MaxFrameRate = 60;
	cp.NativeDecoderID = NativeDecoderID;

	// See if we should decode directly to a externally provided surface or not...
	cp.VideoCodecSurface = nullptr;
	if (Config.AdditionalOptions.HaveKey("videoDecoder_Android_UseSurface"))
	{
		cp.bUseVideoCodecSurface = Config.AdditionalOptions.GetValue("videoDecoder_Android_UseSurface").GetBool();
		if (cp.bUseVideoCodecSurface && Config.AdditionalOptions.HaveKey("videoDecoder_Android_Surface"))
		{
			TSharedPtr<IOptionPointerValueContainer> Value = Config.AdditionalOptions.GetValue("videoDecoder_Android_Surface").GetSharedPointer<IOptionPointerValueContainer>();
			cp.VideoCodecSurface = AndroidJavaEnv::GetJavaEnv()->NewLocalRef(reinterpret_cast<jweak>(Value->GetPointer()));
			cp.bSurfaceIsView = true;
		}
	}
	if (!cp.bSurfaceIsView)
	{
		if (TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> PinnedResourceDelegate = ResourceDelegate.Pin())
		{
			cp.VideoCodecSurface = PinnedResourceDelegate->VideoDecoderResourceDelegate_GetCodecSurface();
		}
	}
	if (!cp.VideoCodecSurface)
	{
		PostError(0, "No surface to create decoder with", ERRCODE_INTERNAL_ANDROID_COULD_NOT_CREATE_VIDEO_DECODER);
		return false;
	}

	// Recall if we render to a view or off-screen here, too
	bSurfaceIsView = cp.bSurfaceIsView;

	Result = DecoderInstance->InitializeDecoder(cp);
	if (Result)
	{
		PostError(Result, "Failed to initialize decoder", ERRCODE_INTERNAL_ANDROID_COULD_NOT_CREATE_VIDEO_DECODER);
		return false;
	}
	// Get the decoder information.
	const IAndroidJavaH265VideoDecoder::FDecoderInformation* DecInf = DecoderInstance->GetDecoderInformation();
	check(DecInf);
	if (DecInf)
	{
		DecoderInfo = *DecInf;
		if (DecoderInfo.bIsAdaptive)
		{
			bMustSendCSD = true;
		}
	}

	// Start it.
	Result = DecoderInstance->Start();
	if (Result)
	{
		PostError(Result, "Failed to start decoder", ERRCODE_INTERNAL_ANDROID_COULD_NOT_START_DECODER);
		return false;
	}
	CurrentDecoderState = EDecoderState::IsFlushed;
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Destroys the current decoder instance.
 */
void FVideoDecoderH265::InternalDecoderDestroy()
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
 * Creates a new decoder and runs all AUs since the last IDR frame through
 * without producing decoded images.
 * Used to continue decoding after the application has been backgrounded
 * which results in a decoder session loss when resumed.
 * Since decoding cannot resume at arbitrary points in the stream everything
 * from the last IDR frame needs to be decoded again.
 * To speed this up AUs that have no dependencies are not added to the replay data.
 */
void FVideoDecoderH265::RecreateDecoderSession()
{
	if (DecoderInstance.IsValid())
	{
		// Try to change the output surface.
		if (bCfgForceNewDecoderOnWakeup == false && Android_Workarounds().GetValue(TEXT("setOutputSurface")).SafeGetBool(false))
		{
			jobject VideoCodecSurface = nullptr;
			if (Config.AdditionalOptions.GetValue(TEXT("videoDecoder_Android_UseSurface")).SafeGetBool(false) && Config.AdditionalOptions.HaveKey(TEXT("videoDecoder_Android_Surface")))
			{
				TSharedPtr<IOptionPointerValueContainer> Value = Config.AdditionalOptions.GetValue("videoDecoder_Android_Surface").GetSharedPointer<IOptionPointerValueContainer>();
				VideoCodecSurface = AndroidJavaEnv::GetJavaEnv()->NewLocalRef(reinterpret_cast<jweak>(Value->GetPointer()));
				bSurfaceIsView = true;
			}
			else
			{
				if (TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe> PinnedResourceDelegate = ResourceDelegate.Pin())
				{
					VideoCodecSurface = PinnedResourceDelegate->VideoDecoderResourceDelegate_GetCodecSurface();
				}
				bSurfaceIsView = false;
			}
			if (VideoCodecSurface)
			{
				int32 Result;
				Result = DecoderInstance->SetOutputSurface(VideoCodecSurface);
				if (Result == 0)
				{
					return;
				}
			}
		}
	}

	// Destroy the current decoder instance. We need to start over.
	bError = false;
	InternalDecoderDestroy();

	// Clear out whatever input and output residuals there might be.
	InDecoderInput.Empty();
	CurrentSequenceIndex.Reset();
	LastPushedPresentationTimeUs = 0;
	DecodingState = EDecodingState::Regular;
	OutputSurfaceTargetPTS.Time = -1.0;
	OutputSurfaceTargetPTS.SequenceIndex = 0;
	ReadyOutputBuffersToSurface.Empty();

	// When we are asked to skip until the next IDR or there is no replay data we are done.
	if (bCfgForceSkipUntilIDROnSurfaceChange || ReplayAccessUnits.IsEmpty())
	{
		// But we must ignore all data until the next IDR frame.
		bSkipUntilNextIDR = true;
		return;
	}

	TSharedPtrTS<FDecoderInput> PreviouslyActiveAU = MoveTemp(CurrentAccessUnit);
	TAccessUnitQueue<TSharedPtrTS<FDecoderInput>> ReplayedAUs;
	while(!ReplayAccessUnits.IsEmpty() && !TerminateThreadSignal.IsSignaled() && !FlushDecoderSignal.IsSignaled())
	{
		if (!CurrentAccessUnit.IsValid())
		{
			TSharedPtrTS<FDecoderInput> ReplayAU;
			ReplayAccessUnits.Dequeue(ReplayAU);
			ReplayedAUs.Enqueue(ReplayAU);
			// Make a copy of the replay AU as the current AU.
			CurrentAccessUnit = MakeSharedTS<FDecoderInput>();
			// Copy all members across.
			*CurrentAccessUnit = *ReplayAU;
			// Increase the actual AU's ref count since it is now shared.
			CurrentAccessUnit->AccessUnit->AddRef();
			// Set the adjusted PTS to invalid to prevent this from being rendered again.
			// This is why we had to make a copy of the replay AU so we can modify it!
			CurrentAccessUnit->AdjustedPTS.SetToInvalid();
		}

		// Need new decoder?
		if (!DecoderInstance.IsValid())
		{
			if (!InternalDecoderCreate())
			{
				bError = true;
				break;
			}
		}

		EOutputResult OutputResult = GetOutput();
		if (OutputResult == EOutputResult::Fail)
		{
			bError = true;
			break;
		}

		EDecodeResult DecodeResult = Decode();
		if (DecodeResult == EDecodeResult::Ok)
		{
			// Update the sequence index.
			CurrentSequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
			CurrentAccessUnit.Reset();
		}
		else if (DecodeResult == EDecodeResult::Fail)
		{
			bError = true;
			break;
		}
	}

	// Get any remaining unreplayed AUs over into the replayed AU queue.
	while(!ReplayAccessUnits.IsEmpty())
	{
		TSharedPtrTS<FDecoderInput> ReplayAU;
		ReplayAccessUnits.Dequeue(ReplayAU);
		ReplayedAUs.Enqueue(ReplayAU);
	}
	// Move all replayed AUs back into the replay queue in case we have to do this over again.
	while(!ReplayedAUs.IsEmpty())
	{
		TSharedPtrTS<FDecoderInput> ReplayAU;
		ReplayedAUs.Dequeue(ReplayAU);
		ReplayAccessUnits.Enqueue(ReplayAU);
	}

	// Reinstate what was the current AU before.
	CurrentAccessUnit = MoveTemp(PreviouslyActiveAU);

	// Get and discard as much output as possible
	while(!TerminateThreadSignal.IsSignaled() && !FlushDecoderSignal.IsSignaled())
	{
		if (GetOutput() != EOutputResult::Ok)
		{
			break;
		}
	}
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
void FVideoDecoderH265::PrepareAU(TSharedPtrTS<FDecoderInput> AU)
{
	if (!AU->bHasBeenPrepared)
	{
		AU->bHasBeenPrepared = true;

		if (!AU->AccessUnit->bIsDummyData)
		{
			// Take note of changes in codec specific data.
			CurrentStreamFormatInfo.UpdateFromCSD(AU);
			AU->CSDPrefixSEIMessages = CurrentStreamFormatInfo.PrefixSEIMessages;
			AU->CSDSuffixSEIMessages = CurrentStreamFormatInfo.SuffixSEIMessages;
			AU->SPSs = CurrentStreamFormatInfo.SPSs;

			const FStreamCodecInformation::FResolution& res = AU->AccessUnit->AUCodecData->ParsedInfo.GetResolution();
			const FStreamCodecInformation::FAspectRatio& ar = AU->AccessUnit->AUCodecData->ParsedInfo.GetAspectRatio();
			const FStreamCodecInformation::FCrop& crop = AU->AccessUnit->AUCodecData->ParsedInfo.GetCrop();
			AU->TotalWidth = res.Width + crop.Left + crop.Right;
			AU->TotalHeight = res.Height + crop.Top + crop.Bottom;
			AU->CropLeft = crop.Left;
			AU->CropRight = crop.Right;
			AU->CropTop = crop.Top;
			AU->CropBottom = crop.Bottom;
			AU->AspectX = ar.Width ? ar.Width : 1;
			AU->AspectY = ar.Height ? ar.Height : 1;

			// Process NALUs
			AU->bIsIDR = AU->AccessUnit->bIsSyncSample;
			AU->bIsDiscardable = false;
			// Replace the NALU lengths with the startcode.
			uint32* NALU = (uint32*)AU->AccessUnit->AUData;
			uint32* End  = (uint32*)Electra::AdvancePointer(NALU, AU->AccessUnit->AUSize);
			while(NALU < End)
			{
				uint32 naluLen = MEDIA_FROM_BIG_ENDIAN(*NALU);
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
				// Prefix or suffix NUT?
				else if (nut == 39 || nut == 40)
				{
					MPEG::ExtractSEIMessages(nut == 39 ? AU->PrefixSEIMessages : AU->SuffixSEIMessages, Electra::AdvancePointer(NALU, 6), naluLen-2, MPEG::ESEIStreamType::H265, nut == 39);
				}

				*NALU = MEDIA_TO_BIG_ENDIAN(0x00000001U);
				NALU = Electra::AdvancePointer(NALU, naluLen+4);
			}
		}

		// Does this AU fall (partially) outside the range for rendering?
		FTimeValue StartTime = AU->AccessUnit->PTS;
		FTimeValue EndTime = AU->AccessUnit->PTS + AU->AccessUnit->Duration;
		AU->PTS = StartTime.GetAsMicroseconds();		// The PTS we give the decoder no matter any adjustment.
		AU->EndPTS = EndTime.GetAsMicroseconds();		// End PTS we need to check the PTS value returned by the decoder against.
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
				if (AU->AccessUnit->DTS.IsValid() && AU->AccessUnit->DTS >= AU->AccessUnit->LatestPTS)
				{
					AU->bIsDiscardable = true;
				}
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


//-----------------------------------------------------------------------------
/**
 * Gets an input access unit and prepares it for use.
 */
void FVideoDecoderH265::GetAndPrepareInputAU()
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

	// Set the current sequence index if it is not set yet.
	if (!CurrentSequenceIndex.IsSet() && CurrentAccessUnit.IsValid())
	{
		CurrentSequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
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
bool FVideoDecoderH265::AcquireOutputBuffer(IMediaRenderer::IBuffer*& RenderOutputBuffer)
{
	RenderOutputBuffer = nullptr;
	FParamDict BufferAcquireOptions;
	while(!TerminateThreadSignal.IsSignaled() && !FlushDecoderSignal.IsSignaled())
	{
		UEMediaError bufResult = Renderer->AcquireBuffer(RenderOutputBuffer, 0, BufferAcquireOptions);
		check(bufResult == UEMEDIA_ERROR_OK || bufResult == UEMEDIA_ERROR_INSUFFICIENT_DATA);
		if (bufResult != UEMEDIA_ERROR_OK && bufResult != UEMEDIA_ERROR_INSUFFICIENT_DATA)
		{
			PostError(0, "Failed to acquire sample buffer", ERRCODE_INTERNAL_ANDROID_COULD_NOT_GET_OUTPUT_BUFFER, bufResult);
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

		ProcessReadyOutputBuffersToSurface();
	}
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Creates a dummy output image that is not to be displayed and has no image data.
 * Dummy access units are created when stream data is missing to ensure the data
 * pipeline does not run dry and exhibits no gaps in the timeline.
 *
 * @return
 */
FVideoDecoderH265::EDecodeResult FVideoDecoderH265::DecodeDummy()
{
	if (CurrentAccessUnit.IsValid() && CurrentAccessUnit->AdjustedPTS.IsValid())
	{
		FDecodedImage NextImage;
		NextImage.bIsDummy = true;
		NextImage.SourceInfo = CurrentAccessUnit;
		EOutputResult OutputResult = ProcessOutput(NextImage);
		if (OutputResult == EOutputResult::TryAgainLater)
		{
			return EDecodeResult::TryAgainLater;
		}
	}
	return EDecodeResult::Ok;
}



//-----------------------------------------------------------------------------
/**
 * Sends an access unit to the decoder for decoding.
 *
 * @return
 */
FVideoDecoderH265::EDecodeResult FVideoDecoderH265::Decode()
{
	// No input AU to decode?
	if (!CurrentAccessUnit.IsValid())
	{
		return EDecodeResult::Ok;
	}
	// We gotta have a decoder here.
	if (!DecoderInstance.IsValid())
	{
		return EDecodeResult::Fail;
	}

	int32 Result = -1;
	int32 InputBufferIndex = -1;

	// Send actual AU data now.
	InputBufferIndex = DecoderInstance->DequeueInputBuffer(0);
	if (InputBufferIndex >= 0)
	{
		CurrentDecoderState = EDecoderState::IsActive;
		if (bMustSendCSD)
		{
			int32 nb = CurrentAccessUnit->AccessUnit->AUCodecData->CodecSpecificData.Num() + CurrentAccessUnit->AccessUnit->AUSize;
			void* NewData = FMemory::Malloc(nb);
			FMemory::Memcpy(NewData, CurrentAccessUnit->AccessUnit->AUCodecData->CodecSpecificData.GetData(), CurrentAccessUnit->AccessUnit->AUCodecData->CodecSpecificData.Num());
			FMemory::Memcpy(Electra::AdvancePointer(NewData, CurrentAccessUnit->AccessUnit->AUCodecData->CodecSpecificData.Num()), CurrentAccessUnit->AccessUnit->AUData, CurrentAccessUnit->AccessUnit->AUSize);
			Result = DecoderInstance->QueueInputBuffer(InputBufferIndex, NewData, nb, CurrentAccessUnit->PTS);
			FMemory::Free(NewData);
			bMustSendCSD = false;
		}
		else
		{
			Result = DecoderInstance->QueueInputBuffer(InputBufferIndex, CurrentAccessUnit->AccessUnit->AUData, CurrentAccessUnit->AccessUnit->AUSize, CurrentAccessUnit->PTS);
		}
		check(Result == 0);
		if (Result == 0)
		{
			LastPushedPresentationTimeUs = CurrentAccessUnit->PTS;
			CurrentStreamFormatInfo.SetFrom(CurrentAccessUnit);

			InDecoderInput.Add(CurrentAccessUnit);
			InDecoderInput.Sort([](const TSharedPtrTS<FDecoderInput>& a, const TSharedPtrTS<FDecoderInput>& b) { return a->PTS < b->PTS; });

			return EDecodeResult::Ok;
		}
		else
		{
			PostError(Result, "Failed to submit decoder input buffer", ERRCODE_INTERNAL_ANDROID_FAILED_TO_DECODE_VIDEO);
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
	return EDecodeResult::Fail;
}


//-----------------------------------------------------------------------------
/**
 * Send an EOS to the decoder to drain it and get all pending output.
 *
 * @return
 */
FVideoDecoderH265::EDecodeResult FVideoDecoderH265::DrainDecoder()
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
		CurrentDecoderState = EDecoderState::IsActive;
		Result = DecoderInstance->QueueEOSInputBuffer(InputBufferIndex, LastPushedPresentationTimeUs);
		check(Result == 0);
		if (Result == 0)
		{
			return EDecodeResult::Ok;
		}
		else
		{
			PostError(Result, "Failed to submit decoder EOS input buffer", ERRCODE_INTERNAL_ANDROID_FAILED_TO_DECODE_VIDEO);
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
 * Prepares the decoder for decoding.
 * If we do not have one yet it will be created.
 * If we already have one that must be reconfigured we go through the same sequence
 * where creation will be skipped.
 */
bool FVideoDecoderH265::PrepareDecoder()
{
	// Need to create a decoder instance?
	if (!DecoderInstance.IsValid() || CurrentDecoderState == EDecoderState::NeedReconfig)
	{
		SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
		CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);
		if (!InternalDecoderCreate())
		{
			return false;
		}
	}
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Clears the decoder.
 * If possible a simple flush will be made, otherwise a reconfiguration will be requested.
 */
void FVideoDecoderH265::UnprepareDecoder()
{
	if (DecoderInstance.IsValid())
	{
		if (CurrentDecoderState != EDecoderState::IsFlushed && CurrentDecoderState != EDecoderState::NeedReconfig)
		{
			DecoderInstance->Flush();
			if (DecoderInfo.bIsAdaptive)
			{
				bMustSendCSD = true;
				CurrentDecoderState = EDecoderState::IsFlushed;
			}
			else
			{
				CurrentDecoderState = EDecoderState::NeedReconfig;
			}
		}
	}
	else
	{
		CurrentDecoderState = EDecoderState::IsFlushed;
	}
}


//-----------------------------------------------------------------------------
/**
 * Gets and removes the decoder input matching the presentation timestamp received from the decoder.
 * This needs to be the first element in the list.
 *
 * @return
 */
bool FVideoDecoderH265::GetMatchingDecoderInput(TSharedPtrTS<FDecoderInput>& OutMatchingInput, int64 InPTSFromDecoder)
{
	if (InDecoderInput.Num())
	{
/*
		for(int32 i=0; i<InDecoderInput.Num(); ++i)
		{
			if (InDecoderInput[i]->PTS == InPTSFromDecoder)
			{
				OutMatchingInput = InDecoderInput[i];
				InDecoderInput.RemoveAt(i);
				return true;
			}
		}
*/
		OutMatchingInput = InDecoderInput[0];
		if (OutMatchingInput->PTS != InPTSFromDecoder)
		{
			LogMessage(IInfoLog::ELevel::Info, FString::Printf(TEXT("Returned decoder PTS of %lld has no exact match. Returning %lld instead!"), (long long int)InPTSFromDecoder, (long long int)OutMatchingInput->PTS));
		}
		InDecoderInput.RemoveAt(0);
		return true;
	}
	LogMessage(IInfoLog::ELevel::Warning, FString::Printf(TEXT("No decoder input found for decoded image PTS %lld. List is empty!"), (long long int)InPTSFromDecoder));
	return false;
}


//-----------------------------------------------------------------------------
/**
 * Tries to get another completed output buffer from the decoder.
 *
 * @return
 */
FVideoDecoderH265::EOutputResult FVideoDecoderH265::GetOutput()
{
	// When there is no decoder yet there will be no output so all is well!
	if (!DecoderInstance.IsValid())
	{
		return EOutputResult::TryAgainLater;
	}

	int32 Result = -1;
	IAndroidJavaH265VideoDecoder::FOutputBufferInfo OutputBufferInfo;
	Result = DecoderInstance->DequeueOutputBuffer(OutputBufferInfo, 0);
	if (Result != 0)
	{
		PostError(Result, "Failed to get decoder output buffer", ERRCODE_INTERNAL_ANDROID_COULD_NOT_GET_OUTPUT_BUFFER);
		return EOutputResult::Fail;
	}

	if (OutputBufferInfo.BufferIndex >= 0)
	{
		// Received our EOS buffer back?
		if (OutputBufferInfo.bIsEOS)
		{
			// We have sent an empty buffer with only the EOS flag set, so we expect an empty buffer in return.
			check(OutputBufferInfo.Size == 0);
			// We still need to release the EOS buffer back to the decoder though.
			Result = DecoderInstance->ReleaseOutputBuffer(OutputBufferInfo.BufferIndex, OutputBufferInfo.ValidCount, false, -1);
			return EOutputResult::EOS;
		}
		// If a configuration buffer is returned to us we ignore it. This serves no purpose.
		if (OutputBufferInfo.bIsConfig)
		{
			Result = DecoderInstance->ReleaseOutputBuffer(OutputBufferInfo.BufferIndex, OutputBufferInfo.ValidCount, false, -1);
			return EOutputResult::Ok;
		}

		IAndroidJavaH265VideoDecoder::FOutputFormatInfo OutputFormatInfo;
		Result = DecoderInstance->GetOutputFormatInfo(OutputFormatInfo, OutputBufferInfo.BufferIndex);
		if (Result == 0)
		{
			// Check the output for a matching PTS.
			TSharedPtrTS<FDecoderInput> MatchingInput;
			bool bFound = GetMatchingDecoderInput(MatchingInput, OutputBufferInfo.PresentationTimestamp);
			if (!bFound)
			{
				// No pushed entries. Now what?
				PostError(0, "Could not find matching decoder output information", ERRCODE_INTERNAL_ANDROID_COULD_NOT_FIND_OUTPUT_INFO);
				return EOutputResult::Fail;
			}

			bool bRender = MatchingInput->AdjustedPTS.IsValid();
			if (bRender)
			{
				FDecodedImage NextImage;
				NextImage.bIsDummy = false;
				NextImage.SourceInfo = MatchingInput;
				NextImage.OutputFormat = OutputFormatInfo;
				NextImage.OutputBufferInfo = OutputBufferInfo;
				ProcessOutput(NextImage);

				// NOTE: We do not release the output buffer here as this would immediately overwrite the last frame in the surface not yet rendered.
				//	     Instead we leave returning the buffer and the subsequent update of the texture to the render task.
				//Result = DecoderInstance->ReleaseOutputBuffer(OutputBufferInfo.BufferIndex, OutputBufferInfo.ValidCount, false, -1);
			}
			else
			{
				// If the frame is not to be rendered we can short-circuit processing and just return the buffer to the decoder.
				Result = DecoderInstance->ReleaseOutputBuffer(OutputBufferInfo.BufferIndex, OutputBufferInfo.ValidCount, false, -1);
			}

			return EOutputResult::Ok;
		}
		else
		{
			PostError(Result, "Failed to get decoder output format", ERRCODE_INTERNAL_ANDROID_COULD_NOT_GET_OUTPUT_FORMAT);
			return EOutputResult::Fail;
		}
	}
	else if (OutputBufferInfo.BufferIndex == IAndroidJavaH265VideoDecoder::FOutputBufferInfo::EBufferIndexValues::MediaCodec_INFO_TRY_AGAIN_LATER)
	{
		return EOutputResult::TryAgainLater;
	}
	else if (OutputBufferInfo.BufferIndex == IAndroidJavaH265VideoDecoder::FOutputBufferInfo::EBufferIndexValues::MediaCodec_INFO_OUTPUT_FORMAT_CHANGED)
	{
		// We do not care about the global format change here. When we need the format we get it from the actual buffer then.
		// Instead let's try to get the following output right away.
		return GetOutput();
	}
	else if (OutputBufferInfo.BufferIndex == IAndroidJavaH265VideoDecoder::FOutputBufferInfo::EBufferIndexValues::MediaCodec_INFO_OUTPUT_BUFFERS_CHANGED)
	{
		// No-op as this is the Result of a deprecated API we are not using.
		// Let's try to get the following output right away.
		return GetOutput();
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
 * Processes the next decoded image to be output.
 *
 * @param NextImage
 *
 * @return
 */
FVideoDecoderH265::EOutputResult FVideoDecoderH265::ProcessOutput(const FDecodedImage& NextImage)
{
	// Get an output buffer from the renderer to pass the image to.
	IMediaRenderer::IBuffer* RenderOutputBuffer = nullptr;
	if (AcquireOutputBuffer(RenderOutputBuffer))
	{
		bool bHaveAvailSmpBlk = RenderOutputBuffer != nullptr;
		NotifyReadyBufferListener(bHaveAvailSmpBlk);
		if (RenderOutputBuffer)
		{
			// Set the bit depth and the colorimetry.
			uint8 colour_primaries = 2, transfer_characteristics = 2, matrix_coeffs = 2;
			uint8 video_full_range_flag = 0, video_format = 5;
			uint8 num_bits = 8;
			if (NextImage.SourceInfo.IsValid() && NextImage.SourceInfo->SPSs.Num())
			{
				check(NextImage.SourceInfo->SPSs[0].bit_depth_luma_minus8 == NextImage.SourceInfo->SPSs[0].bit_depth_chroma_minus8);
				num_bits = NextImage.SourceInfo->SPSs[0].bit_depth_luma_minus8 + 8;
				if (NextImage.SourceInfo->SPSs[0].colour_description_present_flag)
				{
					colour_primaries = NextImage.SourceInfo->SPSs[0].colour_primaries;
					transfer_characteristics = NextImage.SourceInfo->SPSs[0].transfer_characteristics;
					matrix_coeffs = NextImage.SourceInfo->SPSs[0].matrix_coeffs;
				}
				if (NextImage.SourceInfo->SPSs[0].video_signal_type_present_flag)
				{
					video_full_range_flag = NextImage.SourceInfo->SPSs[0].video_full_range_flag;
					video_format = NextImage.SourceInfo->SPSs[0].video_format;
				}
			}
			Colorimetry.Update(colour_primaries, transfer_characteristics, matrix_coeffs, video_full_range_flag, video_format);
			if (NextImage.SourceInfo.IsValid())
			{
				HDR.Update(num_bits, Colorimetry, NextImage.SourceInfo->CSDPrefixSEIMessages, NextImage.SourceInfo->PrefixSEIMessages, false);
			}

			TUniquePtr<FParamDict> OutputBufferSampleProperties = MakeUnique<FParamDict>();
			bool bRender = NextImage.SourceInfo->AdjustedPTS.IsValid();
			if (bRender)
			{
				OutputBufferSampleProperties->Set("pts", FVariantValue(NextImage.SourceInfo->AdjustedPTS));
				OutputBufferSampleProperties->Set("duration", FVariantValue(NextImage.SourceInfo->AdjustedDuration));
				if (!NextImage.bIsDummy)
				{
					int32 w = NextImage.OutputFormat.CropRight - NextImage.OutputFormat.CropLeft + 1;
					int32 h = NextImage.OutputFormat.CropBottom - NextImage.OutputFormat.CropTop + 1;

					int64 ax = NextImage.SourceInfo->AspectX;
					int64 ay = NextImage.SourceInfo->AspectY;
					if (ax == 0 || ay == 0)
					{
						ax = ay = 1;
					}

					OutputBufferSampleProperties->Set("width", FVariantValue((int64)w));
					OutputBufferSampleProperties->Set("height", FVariantValue((int64)h));
					OutputBufferSampleProperties->Set("crop_left", FVariantValue((int64)NextImage.OutputFormat.CropLeft));
					OutputBufferSampleProperties->Set("crop_right", FVariantValue((int64)NextImage.OutputFormat.Width - (NextImage.OutputFormat.CropLeft + w))); // convert into crop-offset from image border
					OutputBufferSampleProperties->Set("crop_top", FVariantValue((int64)NextImage.OutputFormat.CropTop));
					OutputBufferSampleProperties->Set("crop_bottom", FVariantValue((int64)NextImage.OutputFormat.Height - (NextImage.OutputFormat.CropTop + h))); // convert into crop-offset from image border
					OutputBufferSampleProperties->Set("aspect_ratio", FVariantValue((double)ax / (double)ay));
					OutputBufferSampleProperties->Set("aspect_w", FVariantValue(ax));
					OutputBufferSampleProperties->Set("aspect_h", FVariantValue(ay));
					OutputBufferSampleProperties->Set("fps_num", FVariantValue((int64)0));
					OutputBufferSampleProperties->Set("fps_denom", FVariantValue((int64)0));
					OutputBufferSampleProperties->Set("pixelfmt", FVariantValue((int64)((num_bits > 8)  ? EPixelFormat::PF_A2B10G10R10 : EPixelFormat::PF_B8G8R8A8)));
					OutputBufferSampleProperties->Set("bits_per", FVariantValue((int64)num_bits));
					Colorimetry.UpdateParamDict(*OutputBufferSampleProperties);
					HDR.UpdateParamDict(*OutputBufferSampleProperties);

					TSharedPtrTS<FElectraPlayerVideoDecoderOutputAndroid> DecoderOutput = RenderOutputBuffer->GetBufferProperties().GetValue("texture").GetSharedPointer<FElectraPlayerVideoDecoderOutputAndroid>();
					check(DecoderOutput);

					DecoderOutput->Initialize(bSurfaceIsView ? FVideoDecoderOutputAndroid::EOutputType::DirectToSurfaceAsView : FVideoDecoderOutputAndroid::EOutputType::DirectToSurfaceAsQueue, NextImage.OutputBufferInfo.BufferIndex, NextImage.OutputBufferInfo.ValidCount, ReleaseToSurface, NativeDecoderID, OutputBufferSampleProperties.Get());

					if (!bSurfaceIsView)
					{
						// Release the decoder output buffer, thereby enqueuing it on our output surface.
						// (we are issuing an RHI thread based update to our texture for each of these, so we should always have a 1:1 mapping - assuming we are fast enough
						//  to not make the surface drop a frame before we get to it)
						DecoderInstance->ReleaseOutputBuffer(NextImage.OutputBufferInfo.BufferIndex, NextImage.OutputBufferInfo.ValidCount, true, -1);
					}
					else
					{
						// We decode right into a Surface. Queue up output buffers until we are ready to show them
						ReadyOutputBuffersToSurface.Emplace(FOutputBufferInfo(FDecoderTimeStamp(NextImage.SourceInfo->AdjustedPTS.GetAsTimespan(), NextImage.SourceInfo->AdjustedPTS.GetSequenceIndex()), NextImage.OutputBufferInfo.BufferIndex, NextImage.OutputBufferInfo.ValidCount));
					}
					// Note: we are returning the buffer to the renderer before we are done getting data
					// (but: this will sync all up as the render command queues are all in order - and hence the async process will be done before MediaTextureResources sees this)
					Renderer->ReturnBuffer(RenderOutputBuffer, true, *OutputBufferSampleProperties);
					// The output buffer properties is now owned by the decoder output.
					OutputBufferSampleProperties.Release();
				}
				else
				{
					OutputBufferSampleProperties->Set("is_dummy", FVariantValue(true));
					Renderer->ReturnBuffer(RenderOutputBuffer, true, *OutputBufferSampleProperties);
				}
			}
			else
			{
				// When this image is not to be rendered and it is also not a dummy image we need to release the output buffer back to the decoder.
				if (!NextImage.bIsDummy)
				{
					DecoderInstance->ReleaseOutputBuffer(NextImage.OutputBufferInfo.BufferIndex, NextImage.OutputBufferInfo.ValidCount, false, -1);
				}
				Renderer->ReturnBuffer(RenderOutputBuffer, false, *OutputBufferSampleProperties);
			}
			return EOutputResult::Ok;
		}
		else
		{
			return EOutputResult::TryAgainLater;
		}
	}
	return EOutputResult::Ok;
}


//-----------------------------------------------------------------------------
/**
 * Handle presenting output to the Surface for display
 * (if in 'DirectToSurface' mode)
 *
 * We would love to do this in "rendering" code, bt that is impossible as that
 * would mandate a multi-threaded control of the decoder... which seems impossible
 * to get to be reliable.
 */
void FVideoDecoderH265::ProcessReadyOutputBuffersToSurface()
{
	if (!DecoderInstance.IsValid())
	{
		return;
	}

	uint32 I = 0;
	{
		FScopeLock Lock(&OutputSurfaceTargetCS);

		// No presentation PTS known?
		if (OutputSurfaceTargetPTS.Time < 0.0)
		{
			return;
		}

		// Do we have anything to output?
		uint32 Num = ReadyOutputBuffersToSurface.Num();
		if (Num == 0)
		{
			return;
		}

		// Look which frame is the newest we could show
		// (this is a SIMPLE version to select frames - COULD BE BETTER)
		for (; I < Num; ++I)
		{
			const FOutputBufferInfo& OI = ReadyOutputBuffersToSurface[I];
			if ((OI.Timestamp.Time > OutputSurfaceTargetPTS.Time && OI.Timestamp.SequenceIndex == OutputSurfaceTargetPTS.SequenceIndex) || OI.Timestamp.SequenceIndex > OutputSurfaceTargetPTS.SequenceIndex)
			{
				// Too new, this one must stay...
				break;
			}
		}
	}

	// Anything?
	if (I > 0)
	{
		// Yes. Remove all deemed too old without any display...
		--I;
		for (uint32 J = 0; J < I; ++J)
		{
			const FOutputBufferInfo& OI = ReadyOutputBuffersToSurface[J];
			DecoderInstance->ReleaseOutputBuffer(OI.BufferIndex, OI.ValidCount , false, -1);
		}
		// Display the one we selected
		const FOutputBufferInfo& OI = ReadyOutputBuffersToSurface[I];
		DecoderInstance->ReleaseOutputBuffer(OI.BufferIndex, OI.ValidCount, true, -1);

		// Remove what we processed...
		ReadyOutputBuffersToSurface.RemoveAt(0, I + 1);
	}
}


//-----------------------------------------------------------------------------
/**
*/
void FVideoDecoderH265::ReleaseToSurface(uint32 NativeDecoderID, const FDecoderTimeStamp& Time)
{
	FVideoDecoderH265** NativeDecoder = NativeDecoderMap.Find(NativeDecoderID);
	if (NativeDecoder)
	{
		FScopeLock Lock(&(*NativeDecoder)->OutputSurfaceTargetCS);
		(*NativeDecoder)->OutputSurfaceTargetPTS = Time;
	}
}


//-----------------------------------------------------------------------------
/**
*/
void FVideoDecoderH265::Android_UpdateSurface(const TSharedPtr<IOptionPointerValueContainer, ESPMode::ThreadSafe>& Surface)
{
	if (bSurfaceIsView)
	{
		//!!!! ARE WE 100% SURE ABOUT "CHAIN OF EVENTS"? CAN THIS NOT BE READ TOO EARLY? (esp. also: thread safety)
		// Update config that will be used to recreate decoder
		Config.AdditionalOptions.SetOrUpdate("videoDecoder_Android_Surface", FVariantValue(Surface));

		// Force the decoder to be refreshed. If possible by switching the output surface, otherwise by creating
		// a new instance and running the replay AUs through it.
		bForceDecoderRefresh = true;
	}
}

void FVideoDecoderH265::Android_SuspendOrResumeDecoder(bool bSuspend)
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
void FVideoDecoderH265::HandleApplicationHasEnteredForeground()
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
void FVideoDecoderH265::HandleApplicationWillEnterBackground()
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
bool FVideoDecoderH265::CheckForFlush()
{
	if (FlushDecoderSignal.IsSignaled())
	{
		SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
		CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);
			
		// Flush and stop the decoder.
		UnprepareDecoder();
			
		// Flush all pending input
		CurrentAccessUnit.Reset();
		NextAccessUnits.Empty();
		ReplayAccessUnits.Empty();
		InDecoderInput.Empty();
		CurrentSequenceIndex.Reset();
		LastPushedPresentationTimeUs = 0;
		DecodingState = EDecodingState::Regular;
			
		// Flush all pending output
		OutputSurfaceTargetPTS.Time = -1.0;
		OutputSurfaceTargetPTS.SequenceIndex = 0;
		ReadyOutputBuffersToSurface.Empty();

		CurrentStreamFormatInfo.Flushed();
		Colorimetry.Reset();
		HDR.Reset();

		FlushDecoderSignal.Reset();
		DecoderFlushedSignal.Signal();

		// Reset done state.
		bDone = false;
		bBlockedOnInput = false;
		return true;
	}
	return false;
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
	if (AddBGFGNotificationHandler(FGBGHandlers))
	{
		HandleApplicationWillEnterBackground();
	}

	// Get configuration values from player options.
	bCfgForceSkipUntilIDROnSurfaceChange = PlayerSessionServices->GetOptions().GetValue(Android::OptionKey_Decoder_SkipUntilIDROnSurfaceChange).SafeGetBool(false);
	bCfgReconfigureSurfaceOnWakeup = PlayerSessionServices->GetOptions().GetValue(Android::OptionKey_Decoder_ReconfigureSurfaceOnWakeup).SafeGetBool(true);
	bCfgForceNewDecoderOnWakeup = PlayerSessionServices->GetOptions().GetValue(Android::OptionKey_Decoder_ForceNewDecoderOnWakeup).SafeGetBool(false);

	bool bGotEOS = false;
	bool bGotLastSequenceAU = false;

	bDone = false;
	bBlockedOnInput = false;
	DecodingState = EDecodingState::Regular;
	CurrentDecoderState = EDecoderState::IsFlushed;
	bDrainForCodecChange = false;
	bForceDecoderRefresh = false;

	bError = false;
	bMustSendCSD = false;

	OutputSurfaceTargetPTS.Time = -1.0;
	OutputSurfaceTargetPTS.SequenceIndex = 0;

	// Create decoded image pool.
	if (!CreateDecodedImagePool())
	{
		bError = true;
	}

	CurrentStreamFormatInfo.Reset();
	Colorimetry.Reset();
	HDR.Reset();

	EDecodeResult DecodeResult;
	int64 TimeLast = MEDIAutcTime::CurrentMSec();
	while(!TerminateThreadSignal.IsSignaled())
	{
		// If in background, wait until we get activated again.
		if (!ApplicationRunningSignal.IsSignaled())
		{
			UE_LOG(LogElectraPlayer, Log, TEXT("FVideoDecoderH265(%p): OnSuspending"), this);
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
			UE_LOG(LogElectraPlayer, Log, TEXT("FVideoDecoderH265(%p): OnResuming"), this);
			if (bCfgReconfigureSurfaceOnWakeup)
			{
				bForceDecoderRefresh = true;
			}
			ApplicationSuspendConfirmedSignal.Reset();
		}

		// Is there a pending flush? If so, execute the flush and go back to the top to check if we must terminate now.
		if (CheckForFlush())
		{
			continue;
		}

		// Is a decoder refresh required? This is necessary if the decoder output surface has changed.
		if (bForceDecoderRefresh)
		{
			RecreateDecoderSession();
			bForceDecoderRefresh = false;
		}

		// Because of the different paths this decode loop can take there is a possibility that
		// it may go very fast and not wait for any resources.
		// To prevent this from becoming a tight loop we make sure to sleep at least some time
		// here to throttle down.
		int64 TimeNow = MEDIAutcTime::CurrentMSec();
		int64 elapsedMS = TimeNow - TimeLast;
		const int32 kTotalSleepTimeMsec = 5;
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

		// Try to pull output. Do this first to make room in the decoder for new input data.
		EOutputResult OutputResult;
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265ConvertOutput);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265ConvertOutput);
			ProcessReadyOutputBuffersToSurface();

			bool bIsBlocked = false;
			// See if the output queue can receive more...
			if (!Renderer->CanReceiveOutputFrames(1))
			{
				bIsBlocked = true;
				OutputResult = EOutputResult::TryAgainLater;
			}
			else
			{
				OutputResult = GetOutput();
				// If there is no output and the input is blocked we have probably allowed for more output frames than the decoder
				// has internally. Normally this should occur only while prerolling, so let's signal that we are stalled on output.
				if (OutputResult == EOutputResult::TryAgainLater && bBlockedOnInput)
				{
					bIsBlocked = true;
				}
			}
			if (bIsBlocked)
			{
				NotifyReadyBufferListener(false);
			}
		}

		// Are we currently draining the decoder?
		if (DecodingState == EDecodingState::Draining)
		{
			// Did we get the final output?
			if (OutputResult == EOutputResult::EOS)
			{
				SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
				CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);

				// Done draining, clear all remaining stray inputs/outputs, if any.
				bGotEOS = true;
				bBlockedOnInput = false;
				InDecoderInput.Empty();
				LastPushedPresentationTimeUs = 0;
				OutputSurfaceTargetPTS.Time = -1.0;
				OutputSurfaceTargetPTS.SequenceIndex = 0;
				ReadyOutputBuffersToSurface.Empty();
				UnprepareDecoder();
			}
			else if (OutputResult == EOutputResult::Fail)
			{
				bError = true;
			}
			// At EOS now?
			if (bGotEOS)
			{
				// Finished draining for codec change?
				if (bDrainForCodecChange)
				{
					// Leave outermost while() loop. We are done.
					break;
				}
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

		// Asked to drain for a change in codec?
		if (bDrainForCodecChange)
		{
			if (DecoderInstance.IsValid())
			{
				if (DecodingState != EDecodingState::Draining)
				{
					DecodeResult = DrainDecoder();
					if (DecodeResult == EDecodeResult::Ok)
					{
						bBlockedOnInput = false;
						DecodingState = EDecodingState::Draining;
						bGotEOS = false;
					}
					else if (DecodeResult == EDecodeResult::TryAgainLater)
					{
						// Could not enqueue the EOS buffer. We will try this again on the next iteration.
						bBlockedOnInput = true;
					}
					else
					{
						break;
					}
				}
			}
			else
			{
				break;
			}
		}
		else
		{
			// Regular or dummy decoding?
			if (DecodingState == EDecodingState::Regular)
			{
				// Check if this AU requires a new decoder
				if (CurrentAccessUnit.IsValid())
				{
					if (DecoderInstance.IsValid())
					{
						bool bHardChange = CurrentAccessUnit->AccessUnit->bTrackChangeDiscontinuity || bGotLastSequenceAU || CurrentAccessUnit->AccessUnit->bIsDummyData || CurrentSequenceIndex.GetValue() != CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
						bool bSoftChange = CurrentStreamFormatInfo.IsDifferentFrom(CurrentAccessUnit);
						if (bHardChange || bSoftChange)
						{
							// If we have a decoder we need to drain it now to get all pending output unless it is already flushed.
							bool bNeedsDraining = CurrentDecoderState != EDecoderState::IsFlushed && CurrentDecoderState != EDecoderState::NeedReconfig;
							// An adaptive decoder does not need to be flushed for only resolution changes.
							if (bSoftChange && !bHardChange && DecoderInfo.bIsAdaptive)
							{
								bNeedsDraining = false;
								bMustSendCSD = true;
							}
							if (bNeedsDraining)
							{
								DecodeResult = DrainDecoder();
								if (DecodeResult == EDecodeResult::Ok)
								{
									bBlockedOnInput = false;
									// EOS was sent. Now wait until draining is complete.
									DecodingState = EDecodingState::Draining;
									bGotEOS = false;
									continue;
								}
								else if (DecodeResult == EDecodeResult::TryAgainLater)
								{
									// Could not enqueue the EOS buffer. We will try this again on the next iteration.
									bBlockedOnInput = true;
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
							// With an already clean decoder, if we need to decode dummy data we can switch into dummy mode now.
							else if (CurrentAccessUnit->AccessUnit->bIsDummyData)
							{
								DecodingState = EDecodingState::Dummy;
								continue;
							}
						}
					}
					// With no decoder, if we need to decode dummy data we can switch into dummy mode now.
					else if (CurrentAccessUnit->AccessUnit->bIsDummyData)
					{
						DecodingState = EDecodingState::Dummy;
						continue;
					}

					// An IDR frame means we can start decoding there, so we can purge any accumulated replay AUs.
					if (CurrentAccessUnit->bIsIDR)
					{
						ReplayAccessUnits.Empty();
						bSkipUntilNextIDR = false;
					}

					// If this AU falls outside the range where it is to be rendered and it is also discardable
					// we do not need to concern ourselves with it at all.
					if (bSkipUntilNextIDR || (CurrentAccessUnit->bIsDiscardable && !CurrentAccessUnit->AdjustedPTS.IsValid()))
					{
						CurrentAccessUnit.Reset();
						continue;
					}

					// Prepare the decoder if necessary.
					if (!PrepareDecoder())
					{
						bError = true;
					}

					// Decode.
					if (!bError && DecoderInstance.IsValid())
					{
						SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
						CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);

						DecodeResult = Decode();
						if (DecodeResult == EDecodeResult::Ok)
						{
							// Update the sequence index.
							CurrentSequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
							bGotLastSequenceAU = CurrentAccessUnit->AccessUnit->bIsLastInPeriod;
							bBlockedOnInput = false;
							// Add to the replay buffer if it is not a discardable access unit.
							if (!CurrentAccessUnit->bIsDiscardable)
							{
								ReplayAccessUnits.Enqueue(CurrentAccessUnit);
							}
							// Done with this AU.
							CurrentAccessUnit.Reset();
						}
						else if (DecodeResult == EDecodeResult::TryAgainLater)
						{
							bBlockedOnInput = true;
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
					// Update the sequence index.
					CurrentSequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();

					// If this AU falls outside the range where it is to be rendered we do not need to concern ourselves with it at all.
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
						// DecodeDummy() went through most of the regular path, but has returned the output buffer immediately
						// and can thus always get a new one with no waiting. To avoid draining the player buffer by consuming
						// the dummy AUs at rapid pace we put ourselves to sleep for the duration the AU was supposed to last.
						FMediaRunnable::SleepMicroseconds(CurrentAccessUnit->AdjustedDuration.GetAsMicroseconds());
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
							bBlockedOnInput = false;
							DecodingState = EDecodingState::Draining;
							bGotEOS = false;
							continue;
						}
						else if (DecodeResult == EDecodeResult::TryAgainLater)
						{
							// Could not enqueue the EOS buffer. We will try this again on the next iteration.
							bBlockedOnInput = true;
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
	}

	InternalDecoderDestroy();
	DestroyDecodedImagePool();
	CurrentAccessUnit.Reset();
	NextAccessUnits.Empty();
	ReplayAccessUnits.Empty();
	InDecoderInput.Empty();
	ReadyOutputBuffersToSurface.Empty();

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

#endif
