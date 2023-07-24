// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/Platform.h"

#if PLATFORM_MAC || PLATFORM_IOS || PLATFORM_TVOS

#include "PlayerCore.h"
#include "PlayerRuntimeGlobal.h"

#include "Stats/Stats.h"
#include "StreamAccessUnitBuffer.h"
#include "Decoder/VideoDecoderH265.h"
#include "Renderer/RendererBase.h"
#include "Player/PlayerSessionServices.h"
#include "Utilities/StringHelpers.h"
#include "Utilities/UtilsMPEGVideo.h"
#include "DecoderErrors_Apple.h"
#include "HAL/LowLevelMemTracker.h"
#include "ElectraPlayerPrivate.h"
#include "MediaVideoDecoderOutputApple.h"
#include "Renderer/RendererVideo.h"
#include "Decoder/VideoDecoderHelpers.h"

#if ELECTRA_PLATFORM_HAS_H265_DECODER

#include "ElectraVideoDecoder_Apple.h"

#include <VideoToolbox/VideoToolbox.h>

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

private:
	enum EDecodeResult
	{
		Ok,
		Fail,
		SessionLost
	};

	enum
	{
		NumImagesHoldBackForPTSOrdering = 5,	// Number of frames held back to ensure proper PTS-ordering of decoder output
		MaxImagesHoldBackForPTSOrdering = 5		// Maximum number of frames to be held in the buffer before we stall the decoder
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

		TArray<MPEG::FSEIMessage> CSDPrefixSEIMessages;
		TArray<MPEG::FSEIMessage> CSDSuffixSEIMessages;
		TArray<MPEG::FSEIMessage> PrefixSEIMessages;
		TArray<MPEG::FSEIMessage> SuffixSEIMessages;
		TArray<MPEG::FISO23008_2_seq_parameter_set_data> SPSs;
	};

	struct FDecoderConfigExt
	{
		int32 NumBits = 8;
	};

	struct FDecoderFormatInfo
	{
		void Reset()
		{
			CurrentCodecData.Reset();
			CSD.Reset();
			PrefixSEIMessages.Empty();
			SuffixSEIMessages.Empty();
			SPSs.Empty();
		}

		bool UpdateCurrentIfChanged(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU)
		{
			if (AU->AccessUnit->AUCodecData.IsValid() && AU->AccessUnit->AUCodecData.Get() != CurrentCodecData.Get())
			{
				// Pointers are different. Is the content too?
				bool bDifferent = !CurrentCodecData.IsValid() || (CurrentCodecData.IsValid() && AU->AccessUnit->AUCodecData->CodecSpecificData != CurrentCodecData->CodecSpecificData);
				if (bDifferent)
				{
					CurrentCodecData = AU->AccessUnit->AUCodecData;
				}
				return bDifferent;
			}
			return false;
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

		TSharedPtr<const FAccessUnit::CodecData, ESPMode::ThreadSafe> CurrentCodecData;

		TSharedPtr<const FAccessUnit::CodecData, ESPMode::ThreadSafe> CSD;
		TArray<MPEG::FSEIMessage> PrefixSEIMessages;
		TArray<MPEG::FSEIMessage> SuffixSEIMessages;
		TArray<MPEG::FISO23008_2_seq_parameter_set_data> SPSs;
	};

	struct FDecoderHandle
	{
		FDecoderHandle()
			: FormatDescription(nullptr)
			, DecompressionSession(nullptr)
		{
		}
		~FDecoderHandle()
		{
			Close();
		}

		void Close()
		{
			if (DecompressionSession)
			{
				// Note: Do not finish delayed frames here. We don't want them and have to destroy the session immediately!
				//VTDecompressionSessionFinishDelayedFrames(DecompressionSession);
				VTDecompressionSessionWaitForAsynchronousFrames(DecompressionSession);
				VTDecompressionSessionInvalidate(DecompressionSession);
				CFRelease(DecompressionSession);
				DecompressionSession = nullptr;
			}
			if (FormatDescription)
			{
				CFRelease(FormatDescription);
				FormatDescription = nullptr;
			}
		}

		bool IsCompatibleWith(CMFormatDescriptionRef NewFormatDescription)
		{
			if (DecompressionSession)
			{
				Boolean bIsCompatible = VTDecompressionSessionCanAcceptFormatDescription(DecompressionSession, NewFormatDescription);
				return bIsCompatible;
			}
			return false;
		}
		CMFormatDescriptionRef		FormatDescription;
		VTDecompressionSessionRef	DecompressionSession;
	};

	struct FDecodedImage
	{
		FDecodedImage()
			: ImageBufferRef(nullptr)
		{
		}

		FDecodedImage(const FDecodedImage& rhs)
			: ImageBufferRef(nullptr)
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
			return(SourceInfo->SequenceIndex < rhs. SourceInfo->SequenceIndex || (SourceInfo->SequenceIndex == rhs. SourceInfo->SequenceIndex && SourceInfo->PTS < rhs.SourceInfo->PTS));
		}

		void SetImageBufferRef(CVImageBufferRef InImageBufferRef)
		{
			if (ImageBufferRef)
			{
				CFRelease(ImageBufferRef);
				ImageBufferRef = nullptr;
			}
			if (InImageBufferRef)
			{
				CFRetain(InImageBufferRef);
				ImageBufferRef = InImageBufferRef;
			}
		}
		CVImageBufferRef GetImageBufferRef()
		{
			return ImageBufferRef;
		}

		CVImageBufferRef ReleaseImageBufferRef()
		{
			CVImageBufferRef ref = ImageBufferRef;
			ImageBufferRef = nullptr;
			return ref;
		}

		~FDecodedImage()
		{
			if (ImageBufferRef)
			{
				CFRelease(ImageBufferRef);
			}
		}
		TSharedPtr<FDecoderInput, ESPMode::ThreadSafe>	SourceInfo;
	private:
		void InternalCopy(const FDecodedImage& rhs)
		{
			SourceInfo = rhs.SourceInfo;
			SetImageBufferRef(rhs.ImageBufferRef);
		}

		CVImageBufferRef			ImageBufferRef;
	};

	bool InternalDecoderCreate(CMFormatDescriptionRef InputFormatDescription, const FDecoderConfigExt& InputFormatExt);
	void InternalDecoderDestroy();
	void RecreateDecoderSession();
	void StartThread();
	void StopThread();
	void WorkerThread();

	void HandleApplicationHasEnteredForeground();
	void HandleApplicationWillEnterBackground();

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

	EDecodeResult Decode(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU, bool bRecreatingSession);
	bool DecodeDummy(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU);

	void PostError(int32_t ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error = UEMEDIA_ERROR_OK);
	void LogMessage(IInfoLog::ELevel Level, const FString& Message);


	void DecodeCallback(void* pSrcRef, OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration);
	static void _DecodeCallback(void* pUser, void* pSrcRef, OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration)
	{
		static_cast<FVideoDecoderH265*>(pUser)->DecodeCallback(pSrcRef, status, infoFlags, imageBuffer, presentationTimeStamp, presentationDuration);
	}

	bool CreateFormatDescription(CMFormatDescriptionRef& OutFormatDescription, FDecoderConfigExt& OutFormatExt, TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU);


	FInstanceConfiguration												Config;

	FMediaEvent															TerminateThreadSignal;
	FMediaEvent															FlushDecoderSignal;
	FMediaEvent															DecoderFlushedSignal;
	bool																bThreadStarted = false;

	FMediaEvent															ApplicationRunningSignal;
	FMediaEvent															ApplicationSuspendConfirmedSignal;

	IPlayerSessionServices*												PlayerSessionServices = nullptr;

	TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe>						Renderer;

    TWeakPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>		ResourceDelegate;

	TAccessUnitQueue<TSharedPtr<FDecoderInput, ESPMode::ThreadSafe>>	NextAccessUnits;
	TAccessUnitQueue<TSharedPtr<FDecoderInput, ESPMode::ThreadSafe>>	ReplayAccessUnits;
	TSharedPtr<FDecoderInput, ESPMode::ThreadSafe>						CurrentAccessUnit;
	bool																bDrainForCodecChange = false;

	FMediaCriticalSection												ListenerMutex;
	IAccessUnitBufferListener*											InputBufferListener = nullptr;
	IDecoderOutputBufferListener*										ReadyBufferListener = nullptr;

	FDecoderFormatInfo													CurrentStreamFormatInfo;
	FDecoderHandle*														DecoderHandle = nullptr;
	FMediaCriticalSection												InDecoderInputMutex;
	TArray<TSharedPtr<FDecoderInput, ESPMode::ThreadSafe>>				InDecoderInput;
	MPEG::FColorimetryHelper											Colorimetry;
	MPEG::FHDRHelper													HDR;

	int32																MaxDecodeBufferSize = 0;
	bool																bError = false;

	FMediaCriticalSection												ReadyImageMutex;
	TArray<FDecodedImage>												ReadyImages;
};

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
	: FMediaThread("ElectraPlayer::H265 decoder")
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
 * @param Listener
 */
void FVideoDecoderH265::SetAUInputBufferListener(IAccessUnitBufferListener* Listener)
{
	FMediaCriticalSection::ScopedLock lock(ListenerMutex);
	InputBufferListener = Listener;
}


//-----------------------------------------------------------------------------
/**
 * Sets a buffer-ready listener.
 *
 * @param Listener
 */
void FVideoDecoderH265::SetReadyBufferListener(IDecoderOutputBufferListener* Listener)
{
	FMediaCriticalSection::ScopedLock lock(ListenerMutex);
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
void FVideoDecoderH265::SetPlayerSessionServices(IPlayerSessionServices* InSessionServices)
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
void FVideoDecoderH265::Open(const FInstanceConfiguration& InConfig)
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
		PostError(0, "Failed to create image pool", ERRCODE_INTERNAL_APPLE_COULD_NOT_CREATE_IMAGE_POOL, Error);
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
 * @param AccessUnit
 */
void FVideoDecoderH265::AUdataPushAU(FAccessUnit* InAccessUnit)
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
 * @param InputFormatDescription
 * @param InputFormatExt
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH265::InternalDecoderCreate(CMFormatDescriptionRef InputFormatDescription, const FDecoderConfigExt& InputFormatExt)
{
	check(DecoderHandle == nullptr);
	check(InputFormatDescription != nullptr);
	DecoderHandle = new FDecoderHandle;
	CFRetain(InputFormatDescription);
	DecoderHandle->FormatDescription = InputFormatDescription;

	VTDecompressionOutputCallbackRecord CallbackRecord;
	CallbackRecord.decompressionOutputCallback = _DecodeCallback;
	CallbackRecord.decompressionOutputRefCon   = this;

	// Output image format configuration
	CFMutableDictionaryRef OutputImageFormat = CFDictionaryCreateMutable(nullptr, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	// Allow for an array of output formats, so the decoder can deliver SDR and HDR content in suitable buffers
	const int NumFmts = 4;
	int pxfmt[NumFmts] = {
		kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
		kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
		kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange,
		kCVPixelFormatType_420YpCbCr10BiPlanarFullRange
		};
	CFNumberRef PixelFormats[NumFmts] = {
		CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pxfmt[0]),
		CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pxfmt[1]),
		CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pxfmt[2]),
		CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pxfmt[3])
		};
	CFArrayRef PixFmtArray = CFArrayCreate(kCFAllocatorDefault, (const void**)PixelFormats, NumFmts, &kCFTypeArrayCallBacks);

	CFDictionarySetValue(OutputImageFormat, kCVPixelBufferPixelFormatTypeKey, PixFmtArray);

	// Choice of: kCVPixelBufferOpenGLCompatibilityKey (all)  kCVPixelBufferOpenGLESCompatibilityKey (iOS only)   kCVPixelBufferMetalCompatibilityKey (all)
	CFDictionarySetValue(OutputImageFormat, kCVPixelBufferMetalCompatibilityKey, kCFBooleanTrue);
#if PLATFORM_IOS || PLATFORM_TVOS
	CFDictionarySetValue(OutputImageFormat, kCVPixelBufferOpenGLESCompatibilityKey, kCFBooleanFalse);
#endif

	CFRelease(PixFmtArray);
	for (int Idx = 0; Idx < NumFmts; ++Idx)
	{
		CFRelease(PixelFormats[Idx]);
	}

	// Session configuration
	CFMutableDictionaryRef SessionConfiguration = CFDictionaryCreateMutable(nullptr, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	// Ask for hardware decoding
	//	CFDictionarySetValue(SessionConfiguration, kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder, kCFBooleanTrue);

	OSStatus res = VTDecompressionSessionCreate(kCFAllocatorDefault, InputFormatDescription, SessionConfiguration, OutputImageFormat, &CallbackRecord, &DecoderHandle->DecompressionSession);
	CFRelease(SessionConfiguration);
	CFRelease(OutputImageFormat);
	if (res != 0)
	{
		PostError(res, "Failed to create video decoder", ERRCODE_INTERNAL_APPLE_COULD_NOT_CREATE_VIDEO_DECODER);
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
	if (DecoderHandle)
	{
		DecoderHandle->Close();
		delete DecoderHandle;
		DecoderHandle = nullptr;
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
	// Destroy existing decoder. It is of no use any longer.
	InternalDecoderDestroy();
	// Likewise anything that was still pending we also no longer need.
	ClearInDecoderInfos();
	// Do we have replay data?
	if (!ReplayAccessUnits.IsEmpty())
	{
		TAccessUnitQueue<TSharedPtr<FDecoderInput, ESPMode::ThreadSafe>> ReprocessedAUs;

		TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU;
		bool bDone = false;
		bool bFirst = true;
		while(!bError && !bDone)
		{
			if (!ReplayAccessUnits.IsEmpty() && ReplayAccessUnits.Dequeue(AU))
			{
				ReprocessedAUs.Enqueue(AU);
				// Create the format description from the first replay AU.
				if (bFirst)
				{
					bFirst = false;
					CMFormatDescriptionRef NewFormatDescr = nullptr;
					FDecoderConfigExt FormatExt;
					if (CreateFormatDescription(NewFormatDescr, FormatExt, AU))
					{
						if (!InternalDecoderCreate(NewFormatDescr, FormatExt))
						{
							bError = true;
						}
						CFRelease(NewFormatDescr);
					}
					else
					{
						bError = true;
					}
				}
				// Decode
				EDecodeResult DecRes = Decode(AU, true);
				// On failure or yet another loss of decoder session, leave...
				if (DecRes != EDecodeResult::Ok)
				{
					bDone = true;
				}
			}
			else
			{
				bDone = true;
			}
		}
		// Even in case of an error we need to get all replay AUs into our processed FIFO and
		// from there back into the replay buffer. We may need them again and they need to
		// stay in the original order.
		while(!ReplayAccessUnits.IsEmpty())
		{
			TSharedPtrTS<FDecoderInput> ReplayAU;
			ReplayAccessUnits.Dequeue(ReplayAU);
			ReprocessedAUs.Enqueue(ReplayAU);
		}
		while(!ReprocessedAUs.IsEmpty())
		{
			TSharedPtrTS<FDecoderInput> ReplayAU;
			ReprocessedAUs.Dequeue(ReplayAU);
			ReplayAccessUnits.Enqueue(ReplayAU);
		}
	// Flush the decoder to get it idle and discard any accumulated source infos.
	FlushDecoder();
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
void FVideoDecoderH265::PrepareAU(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU)
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
			// Take note of changes in codec specific data.
			CurrentStreamFormatInfo.UpdateFromCSD(AU);
			AU->CSDPrefixSEIMessages = CurrentStreamFormatInfo.PrefixSEIMessages;
            AU->CSDSuffixSEIMessages = CurrentStreamFormatInfo.SuffixSEIMessages;
			AU->SPSs = CurrentStreamFormatInfo.SPSs;

			// Process NALUs
			AU->bIsDiscardable = false;
			AU->bIsIDR = AU->AccessUnit->bIsSyncSample;
			uint32* NALU = (uint32 *)AU->AccessUnit->AUData;
			uint32* End  = (uint32 *)Electra::AdvancePointer(NALU, AU->AccessUnit->AUSize);
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

				NALU = Electra::AdvancePointer(NALU, naluLen+4);
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

					// Get the NALUs from the CSD.
					TArray<MPEG::FNaluInfo>	NALUs;
					MPEG::ParseBitstreamForNALUs(NALUs, AU->AccessUnit->AUCodecData->CodecSpecificData.GetData(), AU->AccessUnit->AUCodecData->CodecSpecificData.Num());
					// Replace the startcodes in the CSD with length values
					for(int32 i=0; i<NALUs.Num(); ++i)
					{
						uint8* NALU = (uint8*)Electra::AdvancePointer(pD, NALUs[i].Offset);
						*(uint32*)NALU = MEDIA_TO_BIG_ENDIAN((uint32)NALUs[i].Size);
					}
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
			PostError(0, "Failed to acquire sample buffer", ERRCODE_INTERNAL_APPLE_COULD_NOT_GET_OUTPUT_BUFFER, bufResult);
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
bool FVideoDecoderH265::FlushDecoder()
{
	if (DecoderHandle && DecoderHandle->DecompressionSession)
	{
		// This call implies VTDecompressionSessionFinishDelayedFrames();
		VTDecompressionSessionWaitForAsynchronousFrames(DecoderHandle->DecompressionSession);
		// Push out all images we got from the decoder from our internal PTS sorting facility
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
void FVideoDecoderH265::ClearInDecoderInfos()
{
	FMediaCriticalSection::ScopedLock lock(InDecoderInputMutex);
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
bool FVideoDecoderH265::DecodeDummy(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU)
{
	if (AU.IsValid() && AU->AdjustedPTS.IsValid())
	{
		FDecodedImage NextImage;
		NextImage.SourceInfo = AU;

		ReadyImageMutex.Lock();
		ReadyImages.Add(NextImage);
		ReadyImages.Sort();
		ReadyImageMutex.Unlock();
	}
    return true;
}

//-----------------------------------------------------------------------------
/**
 * Flushes all images not passed to the renderer from our list.
 */
void FVideoDecoderH265::FlushPendingImages()
{
	// Clear out the map. This implicitly drops all image refcounts.
	ReadyImageMutex.Lock();
	ReadyImages.Empty();
	ReadyImageMutex.Unlock();
}

//-----------------------------------------------------------------------------
/**
 * Creates a source format description to be passed into the video decoder from
 * the codec specific data (CSD) attached to the input access unit.
 *
 * @param OutFormatDescription
 * @param AU
 *
 * @return true if successful, false otherwise.
 */
bool FVideoDecoderH265::CreateFormatDescription(CMFormatDescriptionRef& OutFormatDescription, FDecoderConfigExt& OutFormatExt, TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU)
{
	if (AU.IsValid() && AU->AccessUnit && AU->AccessUnit->AUCodecData.IsValid() && AU->AccessUnit->AUCodecData->CodecSpecificData.Num())
	{
		if (AU->SPSs.Num())
		{
			OutFormatExt.NumBits = AU->SPSs[0].bit_depth_luma_minus8 + 8;
		}

		// Get the NALUs from the CSD.
		TArray<MPEG::FNaluInfo>	NALUs;
		MPEG::ParseBitstreamForNALUs(NALUs, AU->AccessUnit->AUCodecData->CodecSpecificData.GetData(), AU->AccessUnit->AUCodecData->CodecSpecificData.Num());
		if (NALUs.Num())
		{
			int32 NumRecords = NALUs.Num();
			if (NumRecords)
			{
				uint8_t const* * DataPointers = new uint8_t const* [NumRecords];
				SIZE_T*          DataSizes    = new SIZE_T [NumRecords];
				CFDictionaryRef NoExtras = nullptr;
				for(int32 i=0; i<NumRecords; ++i)
				{
					DataPointers[i] = Electra::AdvancePointer(AU->AccessUnit->AUCodecData->CodecSpecificData.GetData(), NALUs[i].Offset + NALUs[i].UnitLength);
					DataSizes[i]    = NALUs[i].Size;
				}
				OSStatus res = CMVideoFormatDescriptionCreateFromHEVCParameterSets(kCFAllocatorDefault, NumRecords, DataPointers, DataSizes, 4, NoExtras, &OutFormatDescription);
				delete [] DataPointers;
				delete [] DataSizes;
				if (res == 0)
				{
					return true;
				}
				else
				{
					if (OutFormatDescription)
					{
						CFRelease(OutFormatDescription);
						OutFormatDescription = nullptr;
					}
					PostError(res, "Failed to create video format description from CSD", ERRCODE_INTERNAL_APPLE_BAD_VIDEO_CSD);
					return false;
				}
			}
		}
		PostError(0, "Failed to create video format description from CSD", ERRCODE_INTERNAL_APPLE_BAD_VIDEO_CSD);
		return false;
	}
	PostError(0, "Cannot create video format description from empty CSD", ERRCODE_INTERNAL_APPLE_NO_VIDEO_CSD);
	return false;
}


//-----------------------------------------------------------------------------
/**
 * Callback from the video decoder when a new decoded image is ready.
 *
 * @param pSrcRef
 * @param status
 * @param infoFlags
 * @param imageBuffer
 * @param presentationTimeStamp
 * @param presentationDuration
 */
void FVideoDecoderH265::DecodeCallback(void* pSrcRef, OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration)
{
	// Remove the source info even if there ultimately was a decode error or if the frame was dropped.
	TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> MatchingInput;
	InDecoderInputMutex.Lock();
	int32 NumCurrentDecodeInputs = InDecoderInput.Num();
	for(int32 i=0; i<NumCurrentDecodeInputs; ++i)
	{
		if (InDecoderInput[i].Get() == pSrcRef)
		{
			MatchingInput = InDecoderInput[i];
			InDecoderInput.RemoveSingle(MatchingInput);
			break;
		}
	}
	InDecoderInputMutex.Unlock();

	if (!MatchingInput.IsValid())
	{
		LogMessage(IInfoLog::ELevel::Error, FString::Printf(TEXT("FVideoDecoderH265::DecodeCallback(): No source info found for decoded srcref %p in %d pending infos (OSStatus %d, infoFlags %d)"), pSrcRef, NumCurrentDecodeInputs, (int32)status, (int32)infoFlags));
	}

	if (status == 0)
	{
		if (imageBuffer != nullptr && (infoFlags & kVTDecodeInfo_FrameDropped) == 0 && MatchingInput.IsValid())
		{
			FDecodedImage NextImage;
			NextImage.SourceInfo = MatchingInput;
			NextImage.SetImageBufferRef(imageBuffer);

			// Recall decoded frame for later processing
			// (we do all processing of output on the decoder thread)
 			ReadyImageMutex.Lock();
			ReadyImages.Add(NextImage);
			ReadyImages.Sort();
			ReadyImageMutex.Unlock();
		}
	}
	else
	{
		bError = true;
		PostError(status, "Failed to decode video", ERRCODE_INTERNAL_APPLE_FAILED_TO_DECODE_VIDEO);
	}
}


//-----------------------------------------------------------------------------
/**
 * Sends an access unit to the decoder for decoding.
 *
 * @param AU
 * @param bRecreatingSession
 *
 * @return
 */
FVideoDecoderH265::EDecodeResult FVideoDecoderH265::Decode(TSharedPtr<FDecoderInput, ESPMode::ThreadSafe> AU, bool bRecreatingSession)
{
	if (!DecoderHandle || !DecoderHandle->DecompressionSession)
	{
		return EDecodeResult::Fail;
	}

	// Create a memory block for the access unit. We pass it the memory we already have and ensure that by setting the block allocator
	// to kCFAllocatorNull no one will attempt to deallocate the memory!
	CMBlockBufferRef AUDataBlock = nullptr;
	SIZE_T AUDataSize = AU->AccessUnit->AUSize;
	OSStatus res = CMBlockBufferCreateWithMemoryBlock(nullptr, AU->AccessUnit->AUData, AUDataSize, kCFAllocatorNull, nullptr, 0, AUDataSize, 0, &AUDataBlock);
	if (res)
	{
		PostError(res, "Failed to create video data block buffer", ERRCODE_INTERNAL_APPLE_FAILED_TO_CREATE_BLOCK_BUFFER);
		return EDecodeResult::Fail;
	}

	// Set up the timing info with DTS, PTS and duration.
	CMSampleTimingInfo TimingInfo;
	const int64_t HNS_PER_S = 10000000;
	TimingInfo.decodeTimeStamp = CMTimeMake(AU->AccessUnit->DTS.GetAsHNS(), HNS_PER_S);
	TimingInfo.presentationTimeStamp = CMTimeMake(AU->AccessUnit->PTS.GetAsHNS(), HNS_PER_S);
	TimingInfo.duration = CMTimeMake(AU->AccessUnit->Duration.GetAsHNS(), HNS_PER_S);

	CMSampleBufferRef SampleBufferRef = nullptr;
	res = CMSampleBufferCreate(kCFAllocatorDefault, AUDataBlock, true, nullptr, nullptr, DecoderHandle->FormatDescription, 1, 1, &TimingInfo, 1, &AUDataSize, &SampleBufferRef);
	// The buffer is now held by the sample, so we release our ref count.
	CFRelease(AUDataBlock);
	if (res)	// see CMSampleBuffer for kCMSampleBufferError_AllocationFailed and such.
	{
		PostError(res, "Failed to create video sample buffer", ERRCODE_INTERNAL_APPLE_FAILED_TO_CREATE_SAMPLE_BUFFER);
		return EDecodeResult::Fail;
	}

	InDecoderInputMutex.Lock();
	InDecoderInput.Add(AU);
	InDecoderInputMutex.Unlock();

	// Decode
/*
	kVTDecodeFrame_EnableAsynchronousDecompression = 1<<0,
	kVTDecodeFrame_DoNotOutputFrame = 1<<1,
	kVTDecodeFrame_1xRealTimePlayback = 1<<2,
	kVTDecodeFrame_EnableTemporalProcessing = 1<<3,
*/
// TODO: set the proper flags. This may require a larger intermediate output queue. Needs experimenting.
	VTDecodeFrameFlags DecodeFlags = kVTDecodeFrame_EnableAsynchronousDecompression | kVTDecodeFrame_EnableTemporalProcessing;
	if (bRecreatingSession || !AU->AdjustedPTS.IsValid())
	{
		DecodeFlags |= kVTDecodeFrame_DoNotOutputFrame;
	}
	VTDecodeInfoFlags  InfoFlags = 0;
/*
	kVTDecodeInfo_Asynchronous = 1UL << 0,
	kVTDecodeInfo_FrameDropped = 1UL << 1,
	kVTDecodeInfo_ImageBufferModifiable = 1UL << 2,
*/
// FIXME: This will require access to the decoder that should be granted by some arbitrator.
//        Especially for iOS/padOS/tvos where the app can be backgrounded and the decoder potentially becoming inaccessible
	res = VTDecompressionSessionDecodeFrame(DecoderHandle->DecompressionSession, SampleBufferRef, DecodeFlags, AU.Get(), &InfoFlags);
	CFRelease(SampleBufferRef);
	if (res == 0)
	{
		// Ok.
		return EDecodeResult::Ok;
	}
	else
	{
		InDecoderInputMutex.Lock();
		InDecoderInput.RemoveSingle(AU);
		InDecoderInputMutex.Unlock();
		if (res == kVTInvalidSessionErr)
		{
			// Lost the decoder session due to being backgrounded and returning to the foreground.
			return EDecodeResult::SessionLost;
		}
		PostError(res, "Failed to decode video frame", ERRCODE_INTERNAL_APPLE_FAILED_TO_DECODE_VIDEO);
		return EDecodeResult::Fail;
	}
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

	TOptional<int64> SequenceIndex;
	bool bDone  = false;
    bool bGotLastSequenceAU = false;
	bool bInDummyDecodeMode = false;

	bError = false;

	CurrentStreamFormatInfo.Reset();
	Colorimetry.Reset();
	HDR.Reset();

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
			UE_LOG(LogElectraPlayer, Log, TEXT("FVideoDecoderH265(%p): OnSuspending"), this);
			ApplicationSuspendConfirmedSignal.Signal();
			while(!ApplicationRunningSignal.WaitTimeout(100 * 1000) && !TerminateThreadSignal.IsSignaled())
			{
			}
			UE_LOG(LogElectraPlayer, Log, TEXT("FVideoDecoderH265(%p): OnResuming"), this);
		}

		if (!bDrainForCodecChange)
		{
			GetAndPrepareInputAU();

			bool bHaveData = CurrentAccessUnit.IsValid();

			// Only send new data to the decoder if we know we got enough room (to avoid accumulating too many frames in our internal PTS-sort queue)
			bool bTooManyImagesWaiting = false;
			if (bHaveData)
			{
				// When there is data, even and especially after a previous EOD, we are no longer done and idling.
				bDone = false;

				ReadyImageMutex.Lock();
				bTooManyImagesWaiting = (ReadyImages.Num() > MaxImagesHoldBackForPTSOrdering);
				ReadyImageMutex.Unlock();
				if (bTooManyImagesWaiting)
				{
					// Signal blockage and wait for it to clear up before going back to normal decoder work...
					NotifyReadyBufferListener(false);

					while(!TerminateThreadSignal.IsSignaled() && !FlushDecoderSignal.IsSignaled())
					{
						ProcessOutput();

						ReadyImageMutex.Lock();
						bTooManyImagesWaiting = (ReadyImages.Num() > MaxImagesHoldBackForPTSOrdering);
						ReadyImageMutex.Unlock();
						if (!bTooManyImagesWaiting)
						{
							break;
						}

						FMediaRunnable::SleepMilliseconds(10);
					}
				}
			}

			if (bHaveData && !bTooManyImagesWaiting)
			{
				if (!CurrentAccessUnit->AccessUnit->bIsDummyData)
				{
					bInDummyDecodeMode = false;

					// An IDR frame means we can start decoding there, so we can purge any accumulated replay AUs.
					if (CurrentAccessUnit->bIsIDR)
					{
						ReplayAccessUnits.Empty();
					}

					bool bStreamFormatChanged = CurrentStreamFormatInfo.UpdateCurrentIfChanged(CurrentAccessUnit) || bGotLastSequenceAU;
					bool bNeedNewDecoder = false;

					if (!SequenceIndex.IsSet())
					{
						SequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
					}
					bNeedNewDecoder |= SequenceIndex.GetValue() != CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();
					SequenceIndex = CurrentAccessUnit->AccessUnit->PTS.GetSequenceIndex();

					bGotLastSequenceAU = CurrentAccessUnit->AccessUnit->bIsLastInPeriod;
					if (bStreamFormatChanged || !DecoderHandle)
					{
						SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265Decode);
						CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265Decode);

						CMFormatDescriptionRef NewFormatDescr = nullptr;
						FDecoderConfigExt FormatExt;
						if (CreateFormatDescription(NewFormatDescr, FormatExt, CurrentAccessUnit))
						{
							bNeedNewDecoder |= DecoderHandle == nullptr || !DecoderHandle->IsCompatibleWith(NewFormatDescr);
							if (bNeedNewDecoder)
							{
								FlushDecoder();
								InternalDecoderDestroy();
								if (!InternalDecoderCreate(NewFormatDescr, FormatExt))
								{
									bError = true;
								}
							}
							CFRelease(NewFormatDescr);
						}
						else
						{
							bError = true;
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
					if (!bError && DecoderHandle && DecoderHandle->DecompressionSession)
					{
						EDecodeResult DecRes = Decode(CurrentAccessUnit, false);

						// Process any output we might have pending
						ProcessOutput();

						// Decode ok?
						if (DecRes == EDecodeResult::Ok)
						{
							// Yes, add to the replay buffer if it is not a discardable access unit.
							if (!CurrentAccessUnit->bIsDiscardable)
							{
								ReplayAccessUnits.Enqueue(CurrentAccessUnit);
							}
						CurrentAccessUnit.Reset();
						}
						// Lost the decoder session?
						else if (DecRes == EDecodeResult::SessionLost)
						{
							// Did not produce output, release semaphore again.
							RecreateDecoderSession();
							// Retry this AU on the new decoder session.
							continue;
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
			ReplayAccessUnits.Empty();
			SequenceIndex.Reset();
			CurrentAccessUnit.Reset();

			CurrentStreamFormatInfo.Reset();
			Colorimetry.Reset();
			HDR.Reset();

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
	ReplayAccessUnits.Empty();
	CurrentAccessUnit.Reset();

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

//-----------------------------------------------------------------------------
/**
 */
void FVideoDecoderH265::ProcessOutput(bool bFlush)
{
	SCOPE_CYCLE_COUNTER(STAT_ElectraPlayer_VideoH265ConvertOutput);
	CSV_SCOPED_TIMING_STAT(ElectraPlayer, VideoH265ConvertOutput);

	// Pull any frames we can get from in-PTS-order array...
	while(1)
	{
		ReadyImageMutex.Lock();
		if (ReadyImages.Num() < (bFlush ? 1 : NumImagesHoldBackForPTSOrdering))
		{
			ReadyImageMutex.Unlock();
			break;
		}

		if (!Renderer->CanReceiveOutputFrames(1))
		{
			NotifyReadyBufferListener(false);
			ReadyImageMutex.Unlock();
			break;
		}

		FDecodedImage NextImage(ReadyImages[0]);
		ReadyImages.RemoveAt(0);

		ReadyImageMutex.Unlock();

		// Get an output buffer from the renderer to pass the image to.
		IMediaRenderer::IBuffer* RenderOutputBuffer = nullptr;
		if (AcquireOutputBuffer(RenderOutputBuffer))
		{
			if (RenderOutputBuffer)
			{
                // Note that the ImageBuffer reference below might be null if this is a dummy frame!
                CVImageBufferRef ImageBufferRef = NextImage.ReleaseImageBufferRef();

				FParamDict* OutputBufferSampleProperties = new FParamDict();
				if (ImageBufferRef)
				{
					// Start with a safe 1:1 aspect ratio assumption.
					long ax = NextImage.SourceInfo->AspectX;
					long ay = NextImage.SourceInfo->AspectY;
					// If there is aspect ratio information on the image itself and it's valid, use that instead.
					NSDictionary* Dict = (NSDictionary*)CVBufferCopyAttachments(ImageBufferRef, kCVAttachmentMode_ShouldPropagate);
					if (Dict)
					{
						NSDictionary* AspectDict = (NSDictionary*)Dict[(__bridge NSString*)kCVImageBufferPixelAspectRatioKey];
						if (AspectDict)
						{
							NSNumber* hs = (NSNumber*)AspectDict[(__bridge NSString*)kCVImageBufferPixelAspectRatioHorizontalSpacingKey];
							NSNumber* vs = (NSNumber*)AspectDict[(__bridge NSString*)kCVImageBufferPixelAspectRatioVerticalSpacingKey];
							if (hs && vs)
							{
								long parx = [hs longValue];
								long pary = [vs longValue];
								if (parx && pary)
								{
									ax = parx;
									ay = pary;
								}
							}
						}
						CFRelease(Dict);
					}
					double PixelAspectRatio = (double)ax / (double)ay;

					OutputBufferSampleProperties->Set("width", FVariantValue((int64) NextImage.SourceInfo->Width));
					OutputBufferSampleProperties->Set("height", FVariantValue((int64) NextImage.SourceInfo->Height));
					OutputBufferSampleProperties->Set("crop_left", FVariantValue((int64) 0));
					OutputBufferSampleProperties->Set("crop_right", FVariantValue((int64) 0));
					OutputBufferSampleProperties->Set("crop_top", FVariantValue((int64) 0));
					OutputBufferSampleProperties->Set("crop_bottom", FVariantValue((int64) 0));
					OutputBufferSampleProperties->Set("aspect_ratio", FVariantValue((double) PixelAspectRatio));
					OutputBufferSampleProperties->Set("aspect_w", FVariantValue((int64) ax));
					OutputBufferSampleProperties->Set("aspect_h", FVariantValue((int64) ay));
					OutputBufferSampleProperties->Set("fps_num", FVariantValue((int64) 0 ));
					OutputBufferSampleProperties->Set("fps_denom", FVariantValue((int64) 0 ));
					OutputBufferSampleProperties->Set("pixelfmt", FVariantValue((int64)EPixelFormat::PF_B8G8R8A8));

					// Set the bit depth and the colorimetry.
					uint8 colour_primaries=2, transfer_characteristics=2, matrix_coeffs=2;
					uint8 video_full_range_flag=0, video_format=5;
					uint8 num_bits = 8;
					if (NextImage.SourceInfo->SPSs.Num())
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
				/// see above // OutputBufferSampleProperties->Set("pixelfmt", num_bits==8 ? FVariantValue((int64)EPixelFormat::PF_B8G8R8A8) : FVariantValue((int64)EPixelFormat::PF_PLATFORM_HDR_1));
					OutputBufferSampleProperties->Set("bits_per", FVariantValue((int64)num_bits));
					Colorimetry.Update(colour_primaries, transfer_characteristics, matrix_coeffs, video_full_range_flag, video_format);
					Colorimetry.UpdateParamDict(*OutputBufferSampleProperties);
					// Extract HDR metadata from SEI messages
					HDR.Update(num_bits, Colorimetry, NextImage.SourceInfo->CSDPrefixSEIMessages, NextImage.SourceInfo->PrefixSEIMessages, false);
					HDR.UpdateParamDict(*OutputBufferSampleProperties);
				}
				else
				{
					OutputBufferSampleProperties->Set("is_dummy", FVariantValue(true));
				}
				OutputBufferSampleProperties->Set("pts", FVariantValue(NextImage.SourceInfo->AdjustedPTS));
				OutputBufferSampleProperties->Set("duration", FVariantValue(NextImage.SourceInfo->AdjustedDuration));

				bool bRender = NextImage.SourceInfo->AdjustedPTS.IsValid();

				TSharedPtr<FElectraPlayerVideoDecoderOutputApple, ESPMode::ThreadSafe> DecoderOutput = RenderOutputBuffer->GetBufferProperties().GetValue("texture").GetSharedPointer<FElectraPlayerVideoDecoderOutputApple>();

				DecoderOutput->Initialize(ImageBufferRef, OutputBufferSampleProperties);
                if (ImageBufferRef)
                {
                    CFRelease(ImageBufferRef);
                }

				// Return the buffer to the renderer.
				Renderer->ReturnBuffer(RenderOutputBuffer, bRender, *OutputBufferSampleProperties);
			}
		}
	}
}

} // namespace Electra

// -----------------------------------------------------------------------------------------------------------------------------

#endif // ELECTRA_PLATFORM_HAS_H265_DECODER

#endif

