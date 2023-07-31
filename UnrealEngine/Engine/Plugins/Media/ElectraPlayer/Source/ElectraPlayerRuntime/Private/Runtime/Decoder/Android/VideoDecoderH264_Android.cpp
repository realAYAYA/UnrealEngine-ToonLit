// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCore.h"
#include "PlayerRuntimeGlobal.h"

#include "StreamAccessUnitBuffer.h"
#include "Decoder/VideoDecoderH264.h"
#include "Decoder/Android/DecoderOptionNames_Android.h"
#include "Renderer/RendererBase.h"
#include "Player/PlayerSessionServices.h"
#include "Utilities/StringHelpers.h"
#include "Utilities/UtilsMPEGVideo.h"
#include "DecoderErrors_Android.h"
#include "HAL/LowLevelMemTracker.h"
#include "Android/AndroidPlatformMisc.h"
#include "ElectraPlayerPrivate.h"

#include "VideoDecoderH264_JavaWrapper_Android.h"
#include "MediaVideoDecoderOutputAndroid.h"
#include "Renderer/RendererVideo.h"
#include "ElectraVideoDecoder_Android.h"

#include "Android/AndroidPlatform.h"
#include "Android/AndroidJava.h"


DECLARE_CYCLE_STAT(TEXT("FVideoDecoderH264::Decode()"), STAT_ElectraPlayer_VideoH264Decode, STATGROUP_ElectraPlayer);
DECLARE_CYCLE_STAT(TEXT("FVideoDecoderH264::ConvertOutput()"), STAT_ElectraPlayer_VideoH264ConvertOutput, STATGROUP_ElectraPlayer);


namespace Electra
{

/**
 * H264 video decoder class implementation.
**/
class FVideoDecoderH264 : public IVideoDecoderH264, public FMediaThread
{
public:
	static bool Startup(const IVideoDecoderH264::FSystemConfiguration& InConfig);
	static void Shutdown();

	static FParamDict& Android_Workarounds()
	{
		static FParamDict Workarounds;
		return Workarounds;
	}

	FVideoDecoderH264();
	virtual ~FVideoDecoderH264();

	virtual void SetPlayerSessionServices(IPlayerSessionServices* SessionServices) override;

	virtual void Open(const FInstanceConfiguration& InConfig) override;
	virtual void Close() override;
	virtual void DrainForCodecChange() override;

	virtual void SetMaximumDecodeCapability(int32 MaxWidth, int32 MaxHeight, int32 MaxProfile, int32 MaxProfileLevel, const FParamDict& AdditionalOptions) override;

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
	};

	struct FDecoderFormatInfo
	{
		void Reset()
		{
			CurrentCodecData.Reset();
		}
		bool IsDifferentFrom(TSharedPtrTS<FDecoderInput> AU);
		void SetFrom(TSharedPtrTS<FDecoderInput> AU);

		TSharedPtrTS<const FAccessUnit::CodecData> CurrentCodecData;
	};

	struct FDecodedImage
	{
		TSharedPtrTS<FDecoderInput> SourceInfo;
		IAndroidJavaH264VideoDecoder::FOutputFormatInfo	OutputFormat;
		IAndroidJavaH264VideoDecoder::FOutputBufferInfo	OutputBufferInfo;
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
	TSharedPtrTS<IAndroidJavaH264VideoDecoder>										DecoderInstance;
	IAndroidJavaH264VideoDecoder::FDecoderInformation								DecoderInfo;
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
	static TMap<uint32, FVideoDecoderH264*>											NativeDecoderMap;

public:
	static FSystemConfiguration														SystemConfig;
};

IVideoDecoderH264::FSystemConfiguration	FVideoDecoderH264::SystemConfig;

int32									FVideoDecoderH264::NextNativeDecoderID = 0;
FCriticalSection						FVideoDecoderH264::NativeDecoderMapCS;
TMap<uint32, FVideoDecoderH264*>		FVideoDecoderH264::NativeDecoderMap;

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/

bool IVideoDecoderH264::Startup(const IVideoDecoderH264::FSystemConfiguration& InConfig)
{
	return FVideoDecoderH264::Startup(InConfig);
}

void IVideoDecoderH264::Shutdown()
{
	FVideoDecoderH264::Shutdown();
}

bool IVideoDecoderH264::GetStreamDecodeCapability(FStreamDecodeCapability& OutResult, const FStreamDecodeCapability& InStreamParameter)
{
	return false;
}

IVideoDecoderH264::FSystemConfiguration::FSystemConfiguration()
{
	ThreadConfig.Decoder.Priority = TPri_Normal;
	ThreadConfig.Decoder.StackSize = 64 << 10;
	ThreadConfig.Decoder.CoreAffinity = -1;
}

IVideoDecoderH264::FInstanceConfiguration::FInstanceConfiguration()
	: ThreadConfig(FVideoDecoderH264::SystemConfig.ThreadConfig)
	, MaxDecodedFrames(8)
{
}

IVideoDecoderH264* IVideoDecoderH264::Create()
{
	return new FVideoDecoderH264;
}

FParamDict& IVideoDecoderH264::Android_Workarounds()
{
	return FVideoDecoderH264::Android_Workarounds();
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
bool FVideoDecoderH264::Startup(const IVideoDecoderH264::FSystemConfiguration& InConfig)
{
	SystemConfig = InConfig;
	// Create a temporary instance of the decoder wrapper. This will initialize the Java class singletons for later use.
	TSharedPtrTS<IAndroidJavaH264VideoDecoder> Temp = IAndroidJavaH264VideoDecoder::Create(nullptr);
	if (Temp.IsValid())
	{
		const IAndroidJavaH264VideoDecoder::FDecoderInformation* DecInf = Temp->GetDecoderInformation();
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
void FVideoDecoderH264::Shutdown()
{
}


//-----------------------------------------------------------------------------
/**
 * Constructor
 */
FVideoDecoderH264::FVideoDecoderH264()
	: FMediaThread("ElectraPlayer::H264 decoder")
{
	NativeDecoderID = (uint32)FPlatformAtomics::InterlockedIncrement(&NextNativeDecoderID);
	FScopeLock Lock(&NativeDecoderMapCS);
	NativeDecoderMap.Add(NativeDecoderID, this);
}


//-----------------------------------------------------------------------------
/**
 * Destructor
 */
FVideoDecoderH264::~FVideoDecoderH264()
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
void FVideoDecoderH264::SetAUInputBufferListener(IAccessUnitBufferListener* InListener)
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
void FVideoDecoderH264::SetReadyBufferListener(IDecoderOutputBufferListener* InListener)
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
void FVideoDecoderH264::SetPlayerSessionServices(IPlayerSessionServices* InSessionServices)
{
	PlayerSessionServices = InSessionServices;
}


//-----------------------------------------------------------------------------
/**
 * Opens a decoder instance
 *
 * @param InConfig
 */
void FVideoDecoderH264::Open(const IVideoDecoderH264::FInstanceConfiguration& InConfig)
{
	Config = InConfig;
	StartThread();
}


//-----------------------------------------------------------------------------
/**
 * Closes the decoder instance.
 */
void FVideoDecoderH264::Close()
{
	StopThread();
}


//-----------------------------------------------------------------------------
/**
 * Drains the decoder of all enqueued input and ends it, after which the decoder must send an FDecoderMessage to the player
 * to signal completion.
 */
void FVideoDecoderH264::DrainForCodecChange()
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
 * @param MaxWidth
 * @param MaxHeight
 * @param MaxProfile
 * @param MaxProfileLevel
 * @param AdditionalOptions
 */
void FVideoDecoderH264::SetMaximumDecodeCapability(int32 MaxWidth, int32 MaxHeight, int32 MaxProfile, int32 MaxProfileLevel, const FParamDict& AdditionalOptions)
{
	// Not implemented
}


//-----------------------------------------------------------------------------
/**
 * Sets a new renderer.
 *
 * @param InRenderer
 */
void FVideoDecoderH264::SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer)
{
	Renderer = InRenderer;
}


//-----------------------------------------------------------------------------
/**
 * Sets a resource delegate.
 */
void FVideoDecoderH264::SetResourceDelegate(const TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>& InResourceDelegate)
{
	ResourceDelegate = InResourceDelegate;
}


//-----------------------------------------------------------------------------
/**
 * Creates and runs the decoder thread.
 */
void FVideoDecoderH264::StartThread()
{
	ThreadSetPriority(Config.ThreadConfig.Decoder.Priority);
	ThreadSetCoreAffinity(Config.ThreadConfig.Decoder.CoreAffinity);
	ThreadSetStackSize(Config.ThreadConfig.Decoder.StackSize);
	ThreadStart(FMediaRunnable::FStartDelegate::CreateRaw(this, &FVideoDecoderH264::WorkerThread));
	bThreadStarted = true;
}


//-----------------------------------------------------------------------------
/**
 * Stops the decoder thread.
 */
void FVideoDecoderH264::StopThread()
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
void FVideoDecoderH264::PostError(int32_t ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error)
{
	check(PlayerSessionServices);
	if (PlayerSessionServices)
	{
		FErrorDetail err;
		err.SetError(Error != UEMEDIA_ERROR_OK ? Error : UEMEDIA_ERROR_DETAIL);
		err.SetFacility(Facility::EFacility::H264Decoder);
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
void FVideoDecoderH264::LogMessage(IInfoLog::ELevel Level, const FString& Message)
{
	if (PlayerSessionServices)
	{
		PlayerSessionServices->PostLog(Facility::EFacility::H264Decoder, Level, Message);
	}
}


//-----------------------------------------------------------------------------
/**
 * Create a pool of decoded images for the decoder.
 *
 * @return
 */
bool FVideoDecoderH264::CreateDecodedImagePool()
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
void FVideoDecoderH264::DestroyDecodedImagePool()
{
	Renderer->ReleaseBufferPool();
}


//-----------------------------------------------------------------------------
/**
 * Called to receive a new input access unit for decoding.
 *
 * @param InAccessUnit
 */
void FVideoDecoderH264::AUdataPushAU(FAccessUnit* InAccessUnit)
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
void FVideoDecoderH264::AUdataPushEOD()
{
	NextAccessUnits.SetEOD();
}


//-----------------------------------------------------------------------------
/**
 * Notifies the decoder that there may be further access units.
 */
void FVideoDecoderH264::AUdataClearEOD()
{
	NextAccessUnits.ClearEOD();
}


//-----------------------------------------------------------------------------
/**
 * Flushes the decoder and clears the input access unit buffer.
 */
void FVideoDecoderH264::AUdataFlushEverything()
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
bool FVideoDecoderH264::InternalDecoderCreate()
{
	int32 Result;

	// Check if there is an existing decoder instance we can re purpose
	if (!DecoderInstance.IsValid())
	{
		DecoderInstance = IAndroidJavaH264VideoDecoder::Create(PlayerSessionServices);
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
	IAndroidJavaH264VideoDecoder::FCreateParameters cp;
	cp.CodecData = CurrentAccessUnit->AccessUnit->AUCodecData;
	cp.MaxWidth = Config.MaxFrameWidth;
	cp.MaxHeight = Config.MaxFrameHeight;
	cp.MaxProfile = Config.ProfileIdc;
	cp.MaxProfileLevel = Config.LevelIdc;
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
	const IAndroidJavaH264VideoDecoder::FDecoderInformation* DecInf = DecoderInstance->GetDecoderInformation();
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
void FVideoDecoderH264::InternalDecoderDestroy()
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
void FVideoDecoderH264::RecreateDecoderSession()
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
void FVideoDecoderH264::NotifyReadyBufferListener(bool bHaveOutput)
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
void FVideoDecoderH264::PrepareAU(TSharedPtrTS<FDecoderInput> AU)
{
	if (!AU->bHasBeenPrepared)
	{
		AU->bHasBeenPrepared = true;

		if (!AU->AccessUnit->bIsDummyData)
		{
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
			AU->bIsDiscardable = !AU->bIsIDR;
			// Replace the NALU lengths with the startcode.
			uint32* NALU = (uint32 *)AU->AccessUnit->AUData;
			uint32* End  = (uint32 *)Electra::AdvancePointer(NALU, AU->AccessUnit->AUSize);
			while(NALU < End)
			{
				// Check the nal_ref_idc in the NAL unit for dependencies.
				uint8 nal = *(const uint8 *)(NALU + 1);
				check((nal & 0x80) == 0);
				if ((nal >> 5) != 0)
				{
					AU->bIsDiscardable = false;
				}
				// IDR frame?
				if ((nal & 0x1f) == 5)
				{
					AU->bIsIDR = true;
				}

				// SEI message(s)?
				if ((nal & 0x1f) == 6)
				{
					// TODO: we might need to set aside any SEI messages carrying 608 or 708 caption data.
				}

				uint32 naluLen = MEDIA_FROM_BIG_ENDIAN(*NALU) + 4;
				*NALU = MEDIA_TO_BIG_ENDIAN(0x00000001U);
				NALU = Electra::AdvancePointer(NALU, naluLen);
			}

			// Note #1: SPS and PPS may need to be removed for some decoders and only sent in a dedicated CSD buffer.
			//          We are not expecting inband SPS/PPS at the moment.
			
			// Note #2: There might be decoder implementations that do not like NALUs other than slice NALUs, or at
			//          least none preceding any SPS/PPS (if they are there and not removed), possible even AUD NALUs.
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


//-----------------------------------------------------------------------------
/**
 * Gets an input access unit and prepares it for use.
 */
void FVideoDecoderH264::GetAndPrepareInputAU()
{
	if (bDrainForCodecChange)
	{
		return;
	}
	
	// Need new input?
	if (!CurrentAccessUnit.IsValid() && InputBufferListener && NextAccessUnits.IsEmpty())
	{
		{
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH264Decode);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH264Decode);

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
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH264Decode);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH264Decode);

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
bool FVideoDecoderH264::AcquireOutputBuffer(IMediaRenderer::IBuffer*& RenderOutputBuffer)
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
FVideoDecoderH264::EDecodeResult FVideoDecoderH264::DecodeDummy()
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
 * Checks if the codec specific data has changed.
 *
 * @return false if the format is still the same, true if it has changed.
 */
bool FVideoDecoderH264::FDecoderFormatInfo::IsDifferentFrom(TSharedPtrTS<FDecoderInput> AU)
{
	// If there is no current CSD set, set it initially.
	if (!CurrentCodecData.IsValid())
	{
		SetFrom(AU);
	}
	return AU->AccessUnit->AUCodecData.IsValid() && AU->AccessUnit->AUCodecData.Get() != CurrentCodecData.Get();
}


//-----------------------------------------------------------------------------
/**
 * Sets the last used codec specific data.
 */
void FVideoDecoderH264::FDecoderFormatInfo::SetFrom(TSharedPtrTS<FDecoderInput> AU)
{
	if (AU.IsValid() && AU->AccessUnit && AU->AccessUnit->AUCodecData.IsValid())
	{
		CurrentCodecData = AU->AccessUnit->AUCodecData;
	}
}


//-----------------------------------------------------------------------------
/**
 * Sends an access unit to the decoder for decoding.
 *
 * @return
 */
FVideoDecoderH264::EDecodeResult FVideoDecoderH264::Decode()
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

#if 0
	// Do we need to send the CSD first?
	if (bMustSendCSD)
	{
		InputBufferIndex = DecoderInstance->DequeueInputBuffer(0);
		if (InputBufferIndex >= 0)
		{
			CurrentDecoderState = EDecoderState::IsActive;
			Result = DecoderInstance->QueueCSDInputBuffer(InputBufferIndex, CurrentAccessUnit->AccessUnit->AUCodecData->CodecSpecificData.GetData(), CurrentAccessUnit->AccessUnit->AUCodecData->CodecSpecificData.Num(), CurrentAccessUnit->PTS);
			check(Result == 0);
			if (Result == 0)
			{
				CurrentStreamFormatInfo.SetFrom(CurrentAccessUnit);
				bMustSendCSD = false;
			}
			else
			{
				PostError(Result, "Failed to submit decoder CSD input buffer", ERRCODE_INTERNAL_ANDROID_FAILED_TO_DECODE_VIDEO);
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
			PostError(InputBufferIndex, "Failed to get a decoder input buffer for CSD", ERRCODE_INTERNAL_ANDROID_COULD_NOT_GET_INPUT_BUFFER);
			return EDecodeResult::Fail;
		}
	}
#endif

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
FVideoDecoderH264::EDecodeResult FVideoDecoderH264::DrainDecoder()
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
bool FVideoDecoderH264::PrepareDecoder()
{
	// Need to create a decoder instance?
	if (!DecoderInstance.IsValid() || CurrentDecoderState == EDecoderState::NeedReconfig)
	{
		SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH264Decode);
		CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH264Decode);
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
void FVideoDecoderH264::UnprepareDecoder()
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
bool FVideoDecoderH264::GetMatchingDecoderInput(TSharedPtrTS<FDecoderInput>& OutMatchingInput, int64 InPTSFromDecoder)
{
	if (InDecoderInput.Num())
	{
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
FVideoDecoderH264::EOutputResult FVideoDecoderH264::GetOutput()
{
	// When there is no decoder yet there will be no output so all is well!
	if (!DecoderInstance.IsValid())
	{
		return EOutputResult::TryAgainLater;
	}

	int32 Result = -1;
	IAndroidJavaH264VideoDecoder::FOutputBufferInfo OutputBufferInfo;
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

		IAndroidJavaH264VideoDecoder::FOutputFormatInfo OutputFormatInfo;
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
	else if (OutputBufferInfo.BufferIndex == IAndroidJavaH264VideoDecoder::FOutputBufferInfo::EBufferIndexValues::MediaCodec_INFO_TRY_AGAIN_LATER)
	{
		return EOutputResult::TryAgainLater;
	}
	else if (OutputBufferInfo.BufferIndex == IAndroidJavaH264VideoDecoder::FOutputBufferInfo::EBufferIndexValues::MediaCodec_INFO_OUTPUT_FORMAT_CHANGED)
	{
		// We do not care about the global format change here. When we need the format we get it from the actual buffer then.
		// Instead let's try to get the following output right away.
		return GetOutput();
	}
	else if (OutputBufferInfo.BufferIndex == IAndroidJavaH264VideoDecoder::FOutputBufferInfo::EBufferIndexValues::MediaCodec_INFO_OUTPUT_BUFFERS_CHANGED)
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
FVideoDecoderH264::EOutputResult FVideoDecoderH264::ProcessOutput(const FDecodedImage& NextImage)
{
	// Get an output buffer from the renderer to pass the image to.
	IMediaRenderer::IBuffer* RenderOutputBuffer = nullptr;
	if (AcquireOutputBuffer(RenderOutputBuffer))
	{
		bool bHaveAvailSmpBlk = RenderOutputBuffer != nullptr;
		NotifyReadyBufferListener(bHaveAvailSmpBlk);
		if (RenderOutputBuffer)
		{
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
					OutputBufferSampleProperties->Set("pixelfmt", FVariantValue((int64)EPixelFormat::PF_B8G8R8A8));

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
void FVideoDecoderH264::ProcessReadyOutputBuffersToSurface()
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
void FVideoDecoderH264::ReleaseToSurface(uint32 NativeDecoderID, const FDecoderTimeStamp& Time)
{
	FVideoDecoderH264** NativeDecoder = NativeDecoderMap.Find(NativeDecoderID);
	if (NativeDecoder)
	{
		FScopeLock Lock(&(*NativeDecoder)->OutputSurfaceTargetCS);
		(*NativeDecoder)->OutputSurfaceTargetPTS = Time;
	}
}


//-----------------------------------------------------------------------------
/**
*/
void FVideoDecoderH264::Android_UpdateSurface(const TSharedPtr<IOptionPointerValueContainer, ESPMode::ThreadSafe>& Surface)
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

void FVideoDecoderH264::Android_SuspendOrResumeDecoder(bool bSuspend)
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
void FVideoDecoderH264::HandleApplicationHasEnteredForeground()
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
void FVideoDecoderH264::HandleApplicationWillEnterBackground()
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
bool FVideoDecoderH264::CheckForFlush()
{
	if (FlushDecoderSignal.IsSignaled())
	{
		SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH264Decode);
		CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH264Decode);
			
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
 * H264 video decoder main threaded decode loop
 */
void FVideoDecoderH264::WorkerThread()
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

	EDecodeResult DecodeResult;
	int64 TimeLast = MEDIAutcTime::CurrentMSec();
	while(!TerminateThreadSignal.IsSignaled())
	{
		// If in background, wait until we get activated again.
		if (!ApplicationRunningSignal.IsSignaled())
		{
			UE_LOG(LogElectraPlayer, Log, TEXT("FVideoDecoderH264(%p): OnSuspending"), this);
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
			UE_LOG(LogElectraPlayer, Log, TEXT("FVideoDecoderH264(%p): OnResuming"), this);
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
			SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH264ConvertOutput);
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH264ConvertOutput);
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
				SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH264Decode);
				CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH264Decode);

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
						SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH264Decode);
						CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH264Decode);

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
		PlayerSessionServices->SendMessageToPlayer(FDecoderMessage::Create(FDecoderMessage::EReason::DrainingFinished, this, EStreamType::Video, FStreamCodecInformation::ECodec::H264));
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
