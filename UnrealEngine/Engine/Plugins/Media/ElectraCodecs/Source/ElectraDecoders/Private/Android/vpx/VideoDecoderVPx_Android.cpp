// Copyright Epic Games, Inc. All Rights Reserved.

#include "vpx/VideoDecoderVPx_Android.h"
#include "DecoderErrors_Android.h"
#include "ElectraDecodersUtils.h"
#include "Utils/Google/ElectraUtilsVPxVideo.h"

#include "IElectraDecoderFeaturesAndOptions.h"
#include "IElectraDecoderOutputVideo.h"

#include "IElectraDecoderResourceDelegate.h"

#include "ElectraDecodersModule.h"

#include <atomic>
/*********************************************************************************************************************/

#include "vpx/VideoDecoderVPx_JavaWrapper_Android.h"

/*********************************************************************************************************************/
#define ENABLE_DETAILED_LOG 0
#if ENABLE_DETAILED_LOG
#define DETAILLOG UE_LOG
#else
#define DETAILLOG(CategoryName, Verbosity, Format, ...) while(0){}
#endif

#define DESTROY_DECODER_WHEN_FLUSHING 1

/*********************************************************************************************************************/

class FElectraVideoDecoderVPx_Android;


class FElectraDecoderDefaultVideoOutputFormatVPx_Android : public IElectraDecoderDefaultVideoOutputFormat
{
public:
	virtual ~FElectraDecoderDefaultVideoOutputFormatVPx_Android()
	{ }
};


class FElectraVideoDecoderOutputVPx_Android : public IElectraDecoderVideoOutput
{
public:
	virtual ~FElectraVideoDecoderOutputVPx_Android()
	{
		ReleaseOutputBuffer();
	}

	FTimespan GetPTS() const override
	{ return PTS; }
	uint64 GetUserValue() const override
	{ return UserValue; }

	int32 GetWidth() const override
	{ return Width - Crop.Left - Crop.Right; }
	int32 GetHeight() const override
	{ return Height - Crop.Top - Crop.Bottom; }
	int32 GetDecodedWidth() const override
	{ return Width; }
	int32 GetDecodedHeight() const override
	{ return Height; }
	FElectraVideoDecoderOutputCropValues GetCropValues() const override
	{ return Crop; }
	int32 GetAspectRatioW() const override
	{ return AspectW; }
	int32 GetAspectRatioH() const override
	{ return AspectH; }
	int32 GetFrameRateNumerator() const override
	{ return FrameRateN; }
	int32 GetFrameRateDenominator() const override
	{ return FrameRateD; }
	int32 GetNumberOfBits() const override
	{ return NumBits; }
	void GetExtraValues(TMap<FString, FVariant>& OutExtraValues) const override
	{ OutExtraValues = ExtraValues; }
	void* GetPlatformOutputHandle(EElectraDecoderPlatformOutputHandleType InTypeOfHandle) const override
	{ return nullptr; }
	IElectraDecoderVideoOutputTransferHandle* GetTransferHandle() const override
	{ return nullptr; }
	IElectraDecoderVideoOutput::EImageCopyResult CopyPlatformImage(IElectraDecoderVideoOutputCopyResources* InCopyResources) const override;

public:
	void ReleaseOutputBuffer();

	FTimespan PTS;
	uint64 UserValue = 0;

	FElectraVideoDecoderOutputCropValues Crop;
	int32 Width = 0;
	int32 Height = 0;
	int32 Pitch = 0;
	int32 NumBits = 0;
	int32 AspectW = 1;
	int32 AspectH = 1;
	int32 FrameRateN = 0;
	int32 FrameRateD = 0;
	int32 PixelFormat = 0;
	TMap<FString, FVariant> ExtraValues;

	TWeakPtr<FElectraVideoDecoderVPx_Android, ESPMode::ThreadSafe> OwningDecoder;
	mutable IElectraVPxVideoDecoderAndroidJava::FOutputBufferInfo OwningDecoderBufferInfo;
	mutable bool bBufferGotReferenced = false;
};



class FElectraVideoDecoderVPx_Android : public IElectraVideoDecoderVPx_Android
{
public:
	FElectraVideoDecoderVPx_Android(const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate);

	virtual ~FElectraVideoDecoderVPx_Android();

	IElectraDecoder::EType GetType() const override
	{ return IElectraDecoder::EType::Video; }

	void GetFeatures(TMap<FString, FVariant>& OutFeatures) const override;

	FError GetError() const override;

	void Close() override;
	IElectraDecoder::ECSDCompatibility IsCompatibleWith(const TMap<FString, FVariant>& CSDAndAdditionalOptions) override;
	bool ResetToCleanStart() override;

	TSharedPtr<IElectraDecoderDefaultOutputFormat, ESPMode::ThreadSafe> GetDefaultOutputFormatFromCSD(const TMap<FString, FVariant>& CSDAndAdditionalOptions) override;

	EDecoderError DecodeAccessUnit(const FInputAccessUnit& InInputAccessUnit, const TMap<FString, FVariant>& InAdditionalOptions) override;
	EDecoderError SendEndOfData() override;
	EDecoderError Flush() override;
	EOutputStatus HaveOutput() override;
	TSharedPtr<IElectraDecoderOutput, ESPMode::ThreadSafe> GetOutput() override;

	void Suspend() override;
	void Resume() override;

	void ReleaseOutputBuffer(const IElectraVPxVideoDecoderAndroidJava::FOutputBufferInfo* InBufferInfo, bool bInRender, int64 InReleaseAt);

private:
	class FBufferReleaseCallback : public IElectraDecoderResourceDelegateAndroid::IDecoderPlatformResourceAndroid::IBufferReleaseCallback
	{
	public:
		FBufferReleaseCallback(TWeakPtr<FElectraVideoDecoderVPx_Android, ESPMode::ThreadSafe> InOwningDecoder)
			: OwningDecoder(MoveTemp(InOwningDecoder))
		{ }
		virtual ~FBufferReleaseCallback() = default;
		void OnReleaseBuffer(int32 InBufferIndex, TOptional<int32> InBufferValidCount, TOptional<bool> InDoRender, TOptional<int64> InRenderAt) override
		{
			// Note: This could potentially be called from any thread but should be executed only from the decoder thread...
			check(!"not implemented yet");
		}
		TWeakPtr<FElectraVideoDecoderVPx_Android, ESPMode::ThreadSafe> OwningDecoder;
	};

	class FSurfaceRequestCallback : public IElectraDecoderResourceDelegateAndroid::IDecoderPlatformResourceAndroid::ISurfaceRequestCallback
	{
	public:
		virtual ~FSurfaceRequestCallback() = default;
		bool HasCompleted() const
		{ return bIsSet; }
		ESurfaceType GetType() const
		{ return SurfaceType; }
		jobject GetSurface() const
		{ return Surface; }
	private:
		void OnNewSurface(ESurfaceType InSurfaceType, void* InSurface)
		{
			SurfaceType = InSurfaceType;
			Surface = (jobject) InSurface;
			FPlatformMisc::MemoryBarrier();
			bIsSet = true;
		}
		ESurfaceType SurfaceType = ESurfaceType::Error;
		jobject Surface = nullptr;
		volatile bool bIsSet = false;
	};

	enum class ECodec
	{
		VP8,
		VP9
	};

	struct FInitialMaxValues
	{
		int32 Width = 0;
		int32 Height = 0;
		uint32 Profile = 0;
		uint32 Level = 0;
		bool IsSet() const
		{
			return Width && Height && Profile && Level;
		}
	};

	struct FDecoderInput
	{
		FInputAccessUnit AccessUnit;
		TMap<FString, FVariant> AdditionalOptions;
		ElectraDecodersUtil::VPxVideo::FVP8UncompressedHeader HdrVP8;
		ElectraDecodersUtil::VPxVideo::FVP9UncompressedHeader HdrVP9;
	};

	enum class EDecodeState
	{
		Decoding,
		Draining,
		CreatingDecoder
	};

	bool PostError(int32 ApiReturnValue, FString Message, int32 Code);

	bool InternalDecoderCreate();
	bool InternalHandleDecoderCreate();
	bool InternalFinishDecoderCreate();
	void InternalDecoderDestroy();
	bool InternalDecoderDrain();

	enum class EConvertResult
	{
		Success,
		Failure,
		GotEOS
	};
	EConvertResult ConvertDecoderOutput(const IElectraVPxVideoDecoderAndroidJava::FOutputBufferInfo& InInfo);
	bool HavePendingOutputIndex(int32 InIndex, bool bRemoveIfExists);

	IElectraDecoder::FError LastError;
	FInitialMaxValues InitialMaxValues;
	ECodec ActiveCodec = ECodec::VP9;

	TWeakPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> ResourceDelegate;
	IElectraDecoderResourceDelegateAndroid::IDecoderPlatformResourceAndroid* PlatformResource = nullptr;

	TSharedPtr<FElectraVideoDecoderOutputVPx_Android, ESPMode::ThreadSafe> CurrentOutput;
	TArray<TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe>> InDecoderInput;

	TSharedPtr<IElectraVPxVideoDecoderAndroidJava, ESPMode::ThreadSafe> DecoderInstance;
	IElectraVPxVideoDecoderAndroidJava::FDecoderInformation DecoderInfo;
	TArray<IElectraVPxVideoDecoderAndroidJava::FOutputBufferInfo> PendingDecoderOutputBuffers;
	EDecodeState DecodeState = EDecodeState::Decoding;
	int64 LastPushedPresentationTime = -1;
	bool bDidSendEOS = false;
	bool bNewDecoderRequired = false;
	volatile uint32 DecoderCreatedAtResumeCount = 0;
	std::atomic<uint32> CurrentResumeCount;
	bool bTriggerReplay = false;

	TSharedPtr<FBufferReleaseCallback, ESPMode::ThreadSafe> CurrentBufferReleaseCallback;
	TSharedPtr<FSurfaceRequestCallback, ESPMode::ThreadSafe> CurrentSurfaceRequest;
	uint32 NativeDecoderID = 0;

	static std::atomic<uint32> NextNativeDecoderID;
};
std::atomic<uint32>  FElectraVideoDecoderVPx_Android::NextNativeDecoderID { 0 };


namespace IElectraVideoDecoderVPx_Android_Platform
{
	static TArray<IElectraVideoDecoderVPx_Android::FSupportedConfiguration> DecoderConfigurations;
	static bool bDecoderConfigurationsDirty = true;
}

void IElectraVideoDecoderVPx_Android::PlatformGetSupportedConfigurations(TArray<FSupportedConfiguration>& OutSupportedConfigurations)
{
	if (IElectraVideoDecoderVPx_Android_Platform::bDecoderConfigurationsDirty)
	{
		IElectraVideoDecoderVPx_Android_Platform::DecoderConfigurations.Empty();

		// Perhaps these can be determined dynamically for each type of device?
		// What is really supported isn't quite clear so allow for UHD.

		// Main
		IElectraVideoDecoderVPx_Android_Platform::DecoderConfigurations.Emplace(IElectraVideoDecoderVPx_Android::FSupportedConfiguration(0, 1, 0, 153, 60, 4096, 2304, 0));
		IElectraVideoDecoderVPx_Android_Platform::DecoderConfigurations.Emplace(IElectraVideoDecoderVPx_Android::FSupportedConfiguration(0, 1, 0, 153, 0, 0, 0, (3840 / 8) * (2160 / 8) * 60));
		// Main10
		IElectraVideoDecoderVPx_Android_Platform::DecoderConfigurations.Emplace(IElectraVideoDecoderVPx_Android::FSupportedConfiguration(0, 2, 0, 153, 60, 4096, 2304, 0));
		IElectraVideoDecoderVPx_Android_Platform::DecoderConfigurations.Emplace(IElectraVideoDecoderVPx_Android::FSupportedConfiguration(0, 2, 0, 153, 0, 0, 0, (3840 / 8) * (2160 / 8) * 60));

		IElectraVideoDecoderVPx_Android_Platform::bDecoderConfigurationsDirty = false;
	}
	OutSupportedConfigurations = IElectraVideoDecoderVPx_Android_Platform::DecoderConfigurations;
}

void IElectraVideoDecoderVPx_Android::GetConfigurationOptions(TMap<FString, FVariant>& OutOptions)
{
	OutOptions.Emplace(IElectraDecoderFeature::MinimumNumberOfOutputFrames, FVariant((int32)8));
	OutOptions.Emplace(IElectraDecoderFeature::IsAdaptive, FVariant(false));
	OutOptions.Emplace(IElectraDecoderFeature::NeedReplayDataOnDecoderLoss, FVariant(true));
	OutOptions.Emplace(IElectraDecoderFeature::MustBeSuspendedInBackground, FVariant(true));
	OutOptions.Emplace(IElectraDecoderFeature::SupportsDroppingOutput, FVariant(true));
}

TSharedPtr<IElectraDecoder, ESPMode::ThreadSafe> IElectraVideoDecoderVPx_Android::Create(const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate)
{
	if (InResourceDelegate)
	{
		TSharedPtr<FElectraVideoDecoderVPx_Android, ESPMode::ThreadSafe> New = MakeShared<FElectraVideoDecoderVPx_Android>(InOptions, InResourceDelegate);
		return New;
	}
	return nullptr;
}


FElectraVideoDecoderVPx_Android::FElectraVideoDecoderVPx_Android(const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate)
{
	ResourceDelegate = InResourceDelegate;
	PlatformResource = reinterpret_cast<IElectraDecoderResourceDelegateAndroid::IDecoderPlatformResourceAndroid*>(ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("platformresource"), 0));

	uint32 Codec4CC = (uint32) ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("codec_4cc"), 0);
	ActiveCodec = Codec4CC == 'vp09' ? ECodec::VP9 : ECodec::VP8;

	NativeDecoderID = ++NextNativeDecoderID;
	DecoderCreatedAtResumeCount = 0;
	CurrentResumeCount = 0;

	// Get the maximum values if provided.
	InitialMaxValues.Width = (int32) Align(ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("max_width"), 0), 16);
	InitialMaxValues.Height = (int32) Align(ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("max_height"), 0), 16);
	const FVariant* Var = InOptions.Find(TEXT("max_codecprofile"));
	if (Var)
	{
		ElectraDecodersUtil::FMimeTypeVideoCodecInfo ci;
		if (ActiveCodec == ECodec::VP9)
		{
			if (Var->GetType() == EVariantTypes::String && ElectraDecodersUtil::ParseCodecVP9(ci, Var->GetValue<FString>(), ElectraDecodersUtil::GetVariantValueUInt8Array(InOptions, TEXT("$vpcC_box"))))
			{
				InitialMaxValues.Profile = (uint32) ci.Profile;
				InitialMaxValues.Level = (uint32) ci.Level;
			}
		}
		else
		{
			if (Var->GetType() == EVariantTypes::String && ElectraDecodersUtil::ParseCodecVP8(ci, Var->GetValue<FString>(), ElectraDecodersUtil::GetVariantValueUInt8Array(InOptions, TEXT("$vpcC_box"))))
			{
				InitialMaxValues.Profile = (uint32) ci.Profile;
				InitialMaxValues.Level = (uint32) ci.Level;
			}
		}
	}
}


FElectraVideoDecoderVPx_Android::~FElectraVideoDecoderVPx_Android()
{
	// Close() must have been called already!
	check(LastError.Code == ERRCODE_INTERNAL_ALREADY_CLOSED);
	// We do it nonetheless...
	Close();
}


void FElectraVideoDecoderVPx_Android::GetFeatures(TMap<FString, FVariant>& OutFeatures) const
{
	GetConfigurationOptions(OutFeatures);
	OutFeatures.Emplace(IElectraDecoderFeature::MustBeSuspendedInBackground, FVariant(true));
}


IElectraDecoder::FError FElectraVideoDecoderVPx_Android::GetError() const
{
	return LastError;
}


void FElectraVideoDecoderVPx_Android::Close()
{
	ResetToCleanStart();
	// Set the error state that all subsequent calls will fail.
	PostError(0, TEXT("Already closed"), ERRCODE_INTERNAL_ALREADY_CLOSED);
}


IElectraDecoder::ECSDCompatibility FElectraVideoDecoderVPx_Android::IsCompatibleWith(const TMap<FString, FVariant>& CSDAndAdditionalOptions)
{
	// When we have no decoder yet then we are compatible because we will be creating a decoder when needed.
	if (!DecoderInstance.IsValid())
	{
		return IElectraDecoder::ECSDCompatibility::Compatible;
	}
	return DecoderInfo.bIsAdaptive ? IElectraDecoder::ECSDCompatibility::Compatible : IElectraDecoder::ECSDCompatibility::DrainAndReset;
}

bool FElectraVideoDecoderVPx_Android::ResetToCleanStart()
{
	DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::ResetToCleanStart()"));
	InternalDecoderDestroy();

	CurrentOutput.Reset();
	InDecoderInput.Empty();
	PendingDecoderOutputBuffers.Empty();
	DecodeState = EDecodeState::Decoding;
	LastPushedPresentationTime = -1;
	bDidSendEOS = false;
	
	return !LastError.IsSet();
}


TSharedPtr<IElectraDecoderDefaultOutputFormat, ESPMode::ThreadSafe> FElectraVideoDecoderVPx_Android::GetDefaultOutputFormatFromCSD(const TMap<FString, FVariant>& CSDAndAdditionalOptions)
{
	return nullptr;
}


IElectraDecoder::EDecoderError FElectraVideoDecoderVPx_Android::DecodeAccessUnit(const FInputAccessUnit& InInputAccessUnit, const TMap<FString, FVariant>& InAdditionalOptions)
{
	// If already in error do nothing!
	if (LastError.IsSet())
	{
		return IElectraDecoder::EDecoderError::Error;
	}

	// Can not feed new input until draining has finished.
	if (DecodeState == EDecodeState::Draining)
	{
		return IElectraDecoder::EDecoderError::EndOfData;
	}

	// CSD only buffer is not handled at the moment.
	check((InInputAccessUnit.Flags & EElectraDecoderFlags::InitCSDOnly) == EElectraDecoderFlags::None);

	// If this is discardable and won't be output we do not need to handle it at all.
	if ((InInputAccessUnit.Flags & (EElectraDecoderFlags::DoNotOutput | EElectraDecoderFlags::IsDiscardable)) == (EElectraDecoderFlags::DoNotOutput | EElectraDecoderFlags::IsDiscardable))
	{
		return IElectraDecoder::EDecoderError::None;
	}

	// Still creating the decoder?
	if (DecodeState == EDecodeState::CreatingDecoder)
	{
		if (!InternalHandleDecoderCreate())
		{
			return IElectraDecoder::EDecoderError::Error;
		}
		else if (DecodeState == EDecodeState::CreatingDecoder)
		{
			return IElectraDecoder::EDecoderError::NoBuffer;
		}
	}

	// When using a SurfaceView we need to check if it did not suddenly change.
	bool bNeedReplayOnNewSurface = false;
	if (PlatformResource && CurrentSurfaceRequest.IsValid() && CurrentSurfaceRequest->HasCompleted() && CurrentSurfaceRequest->GetType() == FSurfaceRequestCallback::ESurfaceType::SurfaceView)
	{
		void* NewSurfaceView = nullptr;
		IElectraDecoderResourceDelegateAndroid::IDecoderPlatformResourceAndroid::ESurfaceChangeResult SurfaceChangeResult;
		SurfaceChangeResult = PlatformResource->VerifySurfaceView(NewSurfaceView, reinterpret_cast<void*>(CurrentSurfaceRequest->GetSurface()));
		if (SurfaceChangeResult == IElectraDecoderResourceDelegateAndroid::IDecoderPlatformResourceAndroid::ESurfaceChangeResult::NewSurface)
		{
			// This depends on whether or not the decoder supports setting a new surface. If it doesn't we need to replay on a new instance.
			check(!"Not currently handled");
			bNeedReplayOnNewSurface = true;
		}
		else if (SurfaceChangeResult == IElectraDecoderResourceDelegateAndroid::IDecoderPlatformResourceAndroid::ESurfaceChangeResult::Error)
		{
			PostError(0, TEXT("Failed to get the new output surface"), ERRCODE_INTERNAL_INVALID_OUTPUT_SURFACE);
			return IElectraDecoder::EDecoderError::Error;
		}
	}

	// Resuming after suspending will re-create a decoder by replaying previous data.
	// This may not be necessary on all devices, but is a safe catch-all.
	if (DecoderCreatedAtResumeCount != CurrentResumeCount || bNeedReplayOnNewSurface)
	{
		bTriggerReplay = true;
		CurrentOutput.Reset();
		InDecoderInput.Empty();
		PendingDecoderOutputBuffers.Empty();
		LastPushedPresentationTime = -1;
		bNewDecoderRequired = true;
	}

	// Create decoder transform if necessary.
	if (bNewDecoderRequired)
	{
		DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::DecodeAccessUnit() - require new decoder, destroying..."));
		InternalDecoderDestroy();
	}
	if (!DecoderInstance.IsValid())
	{
		InternalDecoderCreate();
		return InternalHandleDecoderCreate() ? IElectraDecoder::EDecoderError::NoBuffer : IElectraDecoder::EDecoderError::Error;
	}

	if (bTriggerReplay)
	{
		bTriggerReplay = false;
		return IElectraDecoder::EDecoderError::LostDecoder;
	}

	// Decode the data if given.
	if (InInputAccessUnit.Data && InInputAccessUnit.DataSize)
	{
		ElectraDecodersUtil::VPxVideo::FVP8UncompressedHeader HdrVP8;
		ElectraDecodersUtil::VPxVideo::FVP9UncompressedHeader HdrVP9;
		if (ActiveCodec == ECodec::VP9)
		{
			if (!ElectraDecodersUtil::VPxVideo::ParseVP9UncompressedHeader(HdrVP9, InInputAccessUnit.Data, InInputAccessUnit.DataSize))
			{
				PostError(0, TEXT("Failed to validate VP9 bitstream header"), ERRCODE_INTERNAL_FAILED_TO_DECODE_INPUT);
				return IElectraDecoder::EDecoderError::Error;
			}
		}
		else //if (ActiveCodec == ECodec::VP8)
		{
			if (!ElectraDecodersUtil::VPxVideo::ParseVP8UncompressedHeader(HdrVP8, InInputAccessUnit.Data, InInputAccessUnit.DataSize))
			{
				PostError(0, TEXT("Failed to validate VP8 bitstream header"), ERRCODE_INTERNAL_FAILED_TO_DECODE_INPUT);
				return IElectraDecoder::EDecoderError::Error;
			}
		}

		int32 InputBufferIndex = DecoderInstance->DequeueInputBuffer(0);
		if (InputBufferIndex >= 0)
		{
			// Android decoders need the PTS in microseconds
			int64 PTS = InInputAccessUnit.PTS.GetTicks() / 10;
			int32 Result = 0;
			// If this is a sync sample we prepend the CSD. While this is not necessary on a running stream we need to have the CSD
			// on the first frame and it is easier to prepend it to all IDR frames when seeking etc.
			if ((InInputAccessUnit.Flags & EElectraDecoderFlags::IsSyncSample) != EElectraDecoderFlags::None)
			{
				TArray<uint8> CSD = ElectraDecodersUtil::GetVariantValueUInt8Array(InAdditionalOptions, TEXT("csd"));
				Result = DecoderInstance->QueueInputBuffer(InputBufferIndex, CSD.GetData(), CSD.Num(), InInputAccessUnit.Data, InInputAccessUnit.DataSize, PTS);
			}
			else
			{
				Result = DecoderInstance->QueueInputBuffer(InputBufferIndex, nullptr, 0, InInputAccessUnit.Data, InInputAccessUnit.DataSize, PTS);
			}
			DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::DecodeAccessUnit() - queue input %lld, 0x%x"), (long long int)InInputAccessUnit.PTS.GetTicks(), InInputAccessUnit.Flags);
			if (Result == 0)
			{
				LastPushedPresentationTime = PTS;

				// Add to the list of inputs passed to the decoder.
				TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe> In(new FDecoderInput);
				In->AdditionalOptions = InAdditionalOptions;
				In->AccessUnit = InInputAccessUnit;
				In->HdrVP8 = HdrVP8;
				In->HdrVP9 = HdrVP9;
				// Zero the input pointer and size in the copy. That data is not owned by us and it's best not to have any
				// values here that would lead us to think that we do.
				In->AccessUnit.Data = nullptr;
				In->AccessUnit.DataSize = 0;
				InDecoderInput.Emplace(MoveTemp(In));
				InDecoderInput.Sort([](const TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe>& e1, const TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe>& e2)
				{
					return e1->AccessUnit.PTS < e2->AccessUnit.PTS;
				});

				return IElectraDecoder::EDecoderError::None;
			}
			else
			{
				PostError(Result, TEXT("Failed to decode video decoder input"), ERRCODE_INTERNAL_FAILED_TO_DECODE_INPUT);
				return IElectraDecoder::EDecoderError::Error;
			}
		}
		else if (InputBufferIndex == -1)
		{
			// No available input buffer. Try later.
			return IElectraDecoder::EDecoderError::NoBuffer;
		}
		else
		{
			PostError(InputBufferIndex, TEXT("Failed to get a decoder input buffer"), ERRCODE_INTERNAL_DID_NOT_GET_INPUT_BUFFER);
			return IElectraDecoder::EDecoderError::Error;
		}
	}
	return IElectraDecoder::EDecoderError::None;
}


IElectraDecoder::EDecoderError FElectraVideoDecoderVPx_Android::SendEndOfData()
{
	DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::SendEndOfData()"));
	// If already in error do nothing!
	if (LastError.IsSet())
	{
		return IElectraDecoder::EDecoderError::Error;
	}

	// Already draining?
	if (DecodeState == EDecodeState::Draining)
	{
		DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::SendEndOfData() - already sent"));
		return IElectraDecoder::EDecoderError::EndOfData;
	}
	// If there is a transform send an end-of-stream and drain message.
	if (DecoderInstance.IsValid())
	{
		if (!InternalDecoderDrain())
		{
			return IElectraDecoder::EDecoderError::Error;
		}
		DecodeState = EDecodeState::Draining;
	}
	return IElectraDecoder::EDecoderError::None;
}


IElectraDecoder::EDecoderError FElectraVideoDecoderVPx_Android::Flush()
{
	DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::Flush()"));
	// If already in error do nothing!
	if (LastError.IsSet())
	{
		return IElectraDecoder::EDecoderError::Error;
	}

#ifdef DESTROY_DECODER_WHEN_FLUSHING
	ResetToCleanStart();

#else
	if (DecoderInstance.IsValid())
	{
		DecoderInstance->Flush();
	}
	CurrentOutput.Reset();
	InDecoderInput.Empty();
	PendingDecoderOutputBuffers.Empty();
	LastPushedPresentationTime = -1;
	bDidSendEOS = false;
	DecodeState = EDecodeState::Decoding;
#endif
	return IElectraDecoder::EDecoderError::None;
}


IElectraDecoder::EOutputStatus FElectraVideoDecoderVPx_Android::HaveOutput()
{
	// If already in error do nothing!
	if (LastError.IsSet())
	{
		return IElectraDecoder::EOutputStatus::Error;
	}

	if (CurrentOutput.IsValid())
	{
		DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::HaveOutput() -> Have"));
		return IElectraDecoder::EOutputStatus::Available;
	}

	if (bNewDecoderRequired || !DecoderInstance.IsValid())
	{
		DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::HaveOutput() no / need new decoder"));
		return IElectraDecoder::EOutputStatus::NeedInput;
	}

	// Are we to drain the decoder?
	// We need an available input buffer to do this so we put it here where pulling output is sure to free
	// up an input buffer eventually.
	if (DecodeState == EDecodeState::Draining && !bDidSendEOS)
	{
		int32 Result = -1;
		int32 InputBufferIndex = DecoderInstance->DequeueInputBuffer(0);
		DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::HaveOutput() Sending EOS"));
		if (InputBufferIndex >= 0)
		{
			Result = DecoderInstance->QueueEOSInputBuffer(InputBufferIndex, LastPushedPresentationTime);
			if (Result == 0)
			{
				bDidSendEOS = true;
			}
			else
			{
				PostError(Result, TEXT("Failed to submit decoder EOS input buffer"), ERRCODE_INTERNAL_FAILED_TO_HANDLE_OUTPUT);
				return IElectraDecoder::EOutputStatus::Error;
			}
		}
		else if (InputBufferIndex == -1)
		{
			// No available input buffer. Try later.
		}
		else
		{
			PostError(InputBufferIndex, TEXT("Failed to get a decoder input buffer for EOS"), ERRCODE_INTERNAL_FAILED_TO_HANDLE_OUTPUT);
			return IElectraDecoder::EOutputStatus::Error;
		}
	}

	// Check if there is available output from the decoder.
	while(1)
	{
		IElectraVPxVideoDecoderAndroidJava::FOutputBufferInfo OutputBufferInfo;
		int32 Result = DecoderInstance->DequeueOutputBuffer(OutputBufferInfo, 0);
		if (Result != 0)
		{
			PostError(Result, TEXT("Failed to get decoder output buffer"), ERRCODE_INTERNAL_DID_NOT_GET_OUTPUT_BUFFER);
			return IElectraDecoder::EOutputStatus::Error;
		}

		// Valid output buffer?
		if (OutputBufferInfo.BufferIndex >= 0)
		{
			// Is this a pure EOS buffer with no decoded data?
			if (OutputBufferInfo.bIsEOS && OutputBufferInfo.Size == 0)
			{
				DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::HaveOutput() got EOS, %d still in?"), InDecoderInput.Num());
				// We have to release that buffer even if it's empty.
				Result = DecoderInstance->ReleaseOutputBuffer(OutputBufferInfo.BufferIndex, OutputBufferInfo.ValidCount, false, -1);

				// Did we ask for an EOS?
				if (DecodeState == EDecodeState::Draining)
				{
					DecodeState = EDecodeState::Decoding;
					bDidSendEOS = false;
					InDecoderInput.Empty();
					// The decoder will not be accepting new input after an EOS until it gets flushed.
					// Since it will be drained mostly because of a format change we rather create a new one.
					bNewDecoderRequired = true;
					return IElectraDecoder::EOutputStatus::EndOfData;
				}
				// On some Android versions there could be such empty buffers without us having requested an EOS.
				else
				{
					DETAILLOG(LogElectraDecoders, Log, TEXT("  -> UNEXPECTED, continuing!"));
					continue;
				}
			}
			// If a configuration buffer is returned to us we ignore it. This serves no purpose.
			if (OutputBufferInfo.bIsConfig)
			{
				Result = DecoderInstance->ReleaseOutputBuffer(OutputBufferInfo.BufferIndex, OutputBufferInfo.ValidCount, false, -1);
				continue;
			}

			EConvertResult CnvRes = ConvertDecoderOutput(OutputBufferInfo);
			if (CnvRes == EConvertResult::Success)
			{
				// Was there the EOS flag set and did we request it?
				if (OutputBufferInfo.bIsEOS && DecodeState == EDecodeState::Draining)
				{
					// This should not happen. But assuming that it does then return EOS instead.
					DecodeState = EDecodeState::Decoding;
					bDidSendEOS = false;
					InDecoderInput.Empty();
					CurrentOutput.Reset();
					bNewDecoderRequired = true;
					return IElectraDecoder::EOutputStatus::EndOfData;
				}
				return IElectraDecoder::EOutputStatus::Available;
			}
			else
			{
				return IElectraDecoder::EOutputStatus::Error;
			}
		}
		else if (OutputBufferInfo.BufferIndex == IElectraVPxVideoDecoderAndroidJava::FOutputBufferInfo::EBufferIndexValues::MediaCodec_INFO_TRY_AGAIN_LATER)
		{
			if (DecodeState == EDecodeState::Draining)
			{
				DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::HaveOutput() - DRAINING - TRY AGAIN LATER!?!"));
				return IElectraDecoder::EOutputStatus::TryAgainLater;
			}
			DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::HaveOutput() - TRY AGAIN LATER!?!"));
			return IElectraDecoder::EOutputStatus::NeedInput;
		}
		else if (OutputBufferInfo.BufferIndex == IElectraVPxVideoDecoderAndroidJava::FOutputBufferInfo::EBufferIndexValues::MediaCodec_INFO_OUTPUT_FORMAT_CHANGED)
		{
			// Not needed.
		}
		else if (OutputBufferInfo.BufferIndex == IElectraVPxVideoDecoderAndroidJava::FOutputBufferInfo::EBufferIndexValues::MediaCodec_INFO_OUTPUT_BUFFERS_CHANGED)
		{
			// No-op as this is the Result of a deprecated API we are not using.
		}
		else
		{
			PostError(Result, TEXT("Unhandled output buffer index value"), ERRCODE_INTERNAL_UNEXPECTED_PROBLEM);
			return IElectraDecoder::EOutputStatus::Error;
		}
	}
	check(!"should never get here!");
	return IElectraDecoder::EOutputStatus::Error;
}


TSharedPtr<IElectraDecoderOutput, ESPMode::ThreadSafe> FElectraVideoDecoderVPx_Android::GetOutput()
{
	TSharedPtr<IElectraDecoderOutput, ESPMode::ThreadSafe> Out = CurrentOutput;
	CurrentOutput.Reset();
	return Out;
}


void FElectraVideoDecoderVPx_Android::Suspend()
{
	DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::Suspend()"));
}


void FElectraVideoDecoderVPx_Android::Resume()
{
	DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::Resume()"));
	// Increment the resume counter to re-create the decoder.
	++CurrentResumeCount;
}


bool FElectraVideoDecoderVPx_Android::PostError(int32 ApiReturnValue, FString Message, int32 Code)
{
	LastError.Code = Code;
	LastError.SdkCode = ApiReturnValue;
	LastError.Message = MoveTemp(Message);
	return false;
}


bool FElectraVideoDecoderVPx_Android::InternalDecoderCreate()
{
	bDidSendEOS = false;
	bNewDecoderRequired = false;

	CurrentSurfaceRequest.Reset();
	CurrentBufferReleaseCallback.Reset();

	DecodeState = EDecodeState::CreatingDecoder;
	return true;
}


bool FElectraVideoDecoderVPx_Android::InternalHandleDecoderCreate()
{
	if (CurrentSurfaceRequest.IsValid())
	{
		// Request complete?
		if (!CurrentSurfaceRequest->HasCompleted())
		{
			// Not yet. Wait until it has.
			return true;
		}
		switch(CurrentSurfaceRequest->GetType())
		{
			//case FSurfaceRequestCallback::ESurfaceType::Error:
			default:
			{
				return PostError(0, TEXT("Failed to create decoder. Failed to obtain a surface to decode onto."), ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
			}
			case FSurfaceRequestCallback::ESurfaceType::NoSurface:
			{
				return PostError(0, TEXT("Failed to create decoder. Decoding into CPU buffer is not supported."), ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
			}
			case FSurfaceRequestCallback::ESurfaceType::Surface:
			{
				return InternalFinishDecoderCreate();
			}
			case FSurfaceRequestCallback::ESurfaceType::SurfaceView:
			{
			// TODO: When we are to use an externally provided surface we need to check with every decode call if it has changed.
			//       If so we need to update the decoder, or if this fails re-create and replay it.
				check(!"implement this!");
				return PostError(0, TEXT("Failed to create decoder. Surface view is not currently supported."), ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
			}
		}
	}
	else
	{
		// We need a surface, for which we need to have a platform resource instance to get it.
		if (!PlatformResource)
		{
			return PostError(0, TEXT("Failed to create decoder. No platform resource handle is set."), ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
		}
		// Create a request to get a surface and request one.
		CurrentSurfaceRequest = MakeShared<FSurfaceRequestCallback, ESPMode::ThreadSafe>();
		PlatformResource->RequestSurface(CurrentSurfaceRequest);
		return true;
	}
	return true;
}

bool FElectraVideoDecoderVPx_Android::InternalFinishDecoderCreate()
{
	if (CurrentSurfaceRequest.IsValid())
	{
		DecoderInstance = IElectraVPxVideoDecoderAndroidJava::Create();

		// Remember at which resume count we created the decoder.
		DecoderCreatedAtResumeCount = CurrentResumeCount;

		IElectraVPxVideoDecoderAndroidJava::FCreateParameters cp;
		cp.CodecData = nullptr;
		cp.MaxWidth = InitialMaxValues.Width ? InitialMaxValues.Width : 640;
		cp.MaxHeight = InitialMaxValues.Height ? InitialMaxValues.Height : 480;
		cp.MaxProfile = InitialMaxValues.Profile;
		cp.MaxFrameRate = 60;
		cp.NativeDecoderID = NativeDecoderID;
		cp.VideoCodecSurface = CurrentSurfaceRequest->GetSurface();
		int32 Result = DecoderInstance->CreateDecoder(ActiveCodec == ECodec::VP9);
		if (Result)
		{
			return PostError(Result, TEXT("Failed to create decoder"), ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
		}
		Result = DecoderInstance->InitializeDecoder(cp);
		if (Result)
		{
			return PostError(Result, TEXT("Failed to initialize decoder"), ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
		}

		const IElectraVPxVideoDecoderAndroidJava::FDecoderInformation* DecInf = DecoderInstance->GetDecoderInformation();
		if (!DecInf)
		{
			return PostError(0, TEXT("Failed to initialize decoder, could not get decoder information"), ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
		}
		DecoderInfo = *DecInf;

		Result = DecoderInstance->Start();
		if (Result)
		{
			return PostError(Result, TEXT("Failed to start decoder"), ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
		}
		DecodeState = EDecodeState::Decoding;

		// Create a new buffer release interface.
		if (PlatformResource)
		{
			CurrentBufferReleaseCallback = MakeShared<FBufferReleaseCallback, ESPMode::ThreadSafe>(StaticCastWeakPtr<FElectraVideoDecoderVPx_Android>(AsWeak()));
			PlatformResource->SetBufferReleaseCallback(CurrentBufferReleaseCallback);
		}
		return true;
	}
	else
	{
		return PostError(0, TEXT("Failed to start decoder, surface interface disappeared."), ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
	}
}

bool FElectraVideoDecoderVPx_Android::InternalDecoderDrain()
{
	bDidSendEOS = false;
	return true;
}


void FElectraVideoDecoderVPx_Android::InternalDecoderDestroy()
{
	DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::InternalDecoderDestroy()"));
	if (DecoderInstance.IsValid())
	{
		CurrentBufferReleaseCallback.Reset();
		CurrentSurfaceRequest.Reset();
		DecoderInstance->Flush();
		DecoderInstance->Stop();
		DecoderInstance->ReleaseDecoder();
		DecoderInstance.Reset();
	}
}


void FElectraVideoDecoderVPx_Android::ReleaseOutputBuffer(const IElectraVPxVideoDecoderAndroidJava::FOutputBufferInfo* InBufferInfo, bool bInRender, int64 InReleaseAt)
{
	if (InBufferInfo && DecoderInstance.IsValid() && HavePendingOutputIndex(InBufferInfo->BufferIndex, true))
	{
		int32 Result = DecoderInstance->ReleaseOutputBuffer(InBufferInfo->BufferIndex, InBufferInfo->ValidCount, bInRender, InReleaseAt);
		if (Result)
		{
			PostError(Result, TEXT("Failed to release decoder output buffer"), ERRCODE_INTERNAL_FAILED_TO_CONVERT_OUTPUT_SAMPLE);
		}
	}
}

bool FElectraVideoDecoderVPx_Android::HavePendingOutputIndex(int32 InIndex, bool bRemoveIfExists)
{
	for(int32 i=0; i<PendingDecoderOutputBuffers.Num(); ++i)
	{
		if (PendingDecoderOutputBuffers[i].BufferIndex == InIndex)
		{
			if (bRemoveIfExists)
			{
				PendingDecoderOutputBuffers.RemoveAtSwap(i);
			}
			return true;
		}
	}
	return false;
}



FElectraVideoDecoderVPx_Android::EConvertResult FElectraVideoDecoderVPx_Android::ConvertDecoderOutput(const IElectraVPxVideoDecoderAndroidJava::FOutputBufferInfo& InInfo)
{
	DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::ConvertDecoderOutput()"));
	if (!InDecoderInput.Num())
	{
		PostError(0, TEXT("There is no pending decoder input for the decoded output!"), ERRCODE_INTERNAL_FAILED_TO_CONVERT_OUTPUT_SAMPLE);
		return EConvertResult::Failure;
	}

	// Find the input corresponding to this output.
	TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe> In;
	for(int32 nInDec=0; nInDec<InDecoderInput.Num(); ++nInDec)
	{
		// Decoder timestamps are in microseconds, so we need to compare as that.
		if (InDecoderInput[nInDec]->AccessUnit.PTS / 10 == InInfo.PresentationTimestamp)
		{
			In = InDecoderInput[nInDec];
			InDecoderInput.RemoveAt(nInDec);
			break;
		}
	}
	if (!In.IsValid())
	{
		PostError(0, TEXT("There is no matching decoder input for the decoded output!"), ERRCODE_INTERNAL_FAILED_TO_CONVERT_OUTPUT_SAMPLE);
		return EConvertResult::Failure;
	}

	// If this is a replay frame or a frame that will not be output, we do not need to set it up and merely release the buffer immediately.
	if ((In->AccessUnit.Flags & (EElectraDecoderFlags::IsReplaySample | EElectraDecoderFlags::DoNotOutput)) != EElectraDecoderFlags::None)
	{
		DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::ConvertDecoderOutput() - Discard replay output %lld, 0x%x"), (long long int)In->AccessUnit.PTS.GetTicks(), In->AccessUnit.Flags);
		if (DecoderInstance.IsValid())
		{
			DecoderInstance->ReleaseOutputBuffer(InInfo.BufferIndex, InInfo.ValidCount, false, -1);
		}
		return EConvertResult::Success;
	}
	DETAILLOG(LogElectraDecoders, Log, TEXT("VideoDecoderVPx::ConvertDecoderOutput() - Convert output %lld, 0x%x"), (long long int)In->AccessUnit.PTS.GetTicks(), In->AccessUnit.Flags);

	IElectraVPxVideoDecoderAndroidJava::FOutputFormatInfo Info;
	if (!DecoderInstance.IsValid() || DecoderInstance->GetOutputFormatInfo(Info, InInfo.BufferIndex) != 0)
	{
		PostError(0, TEXT("Could not get decoded buffer format info!"), ERRCODE_INTERNAL_FAILED_TO_CONVERT_OUTPUT_SAMPLE);
		return EConvertResult::Failure;
	}

	TSharedPtr<FElectraVideoDecoderOutputVPx_Android, ESPMode::ThreadSafe> NewOutput = MakeShared<FElectraVideoDecoderOutputVPx_Android>();
	NewOutput->PTS = In->AccessUnit.PTS;
	NewOutput->UserValue = In->AccessUnit.UserValue;
	NewOutput->Width = Info.Width;
	NewOutput->Height = Info.Height;
	NewOutput->Pitch = Info.Stride;
	NewOutput->NumBits = ActiveCodec == ECodec::VP8 ? 8 : In->HdrVP9.GetBitDepth();
	NewOutput->PixelFormat = 0;
	NewOutput->Crop.Left = Info.CropLeft;
	NewOutput->Crop.Right = Info.Width - Info.CropRight - 1;
	NewOutput->Crop.Top = Info.CropTop;
	NewOutput->Crop.Bottom = Info.Height - Info.CropBottom - 1;
	NewOutput->AspectW = 1;
	NewOutput->AspectH = 1;
	NewOutput->OwningDecoder = StaticCastWeakPtr<FElectraVideoDecoderVPx_Android>(AsWeak());
	NewOutput->OwningDecoderBufferInfo = InInfo;

	NewOutput->ExtraValues.Emplace(TEXT("platform"), FVariant(TEXT("android")));
	NewOutput->ExtraValues.Emplace(TEXT("codec"), FVariant(ActiveCodec == ECodec::VP9 ? TEXT("vp9") : TEXT("vp8")));

	PendingDecoderOutputBuffers.Emplace(InInfo);
	CurrentOutput = MoveTemp(NewOutput);
	return EConvertResult::Success;
}


void FElectraVideoDecoderOutputVPx_Android::ReleaseOutputBuffer()
{
	if (OwningDecoderBufferInfo.BufferIndex >= 0)
	{
		TSharedPtr<FElectraVideoDecoderVPx_Android, ESPMode::ThreadSafe> Decoder = OwningDecoder.Pin();
		if (Decoder.IsValid())
		{
			Decoder->ReleaseOutputBuffer(&OwningDecoderBufferInfo, bBufferGotReferenced, -1);
		}
		OwningDecoderBufferInfo.BufferIndex = -1;
	}
}

IElectraDecoderVideoOutput::EImageCopyResult FElectraVideoDecoderOutputVPx_Android::CopyPlatformImage(IElectraDecoderVideoOutputCopyResources* InCopyResources) const
{ 
	TSharedPtr<FElectraVideoDecoderVPx_Android, ESPMode::ThreadSafe> Decoder = OwningDecoder.Pin();
	if (Decoder.IsValid())
	{
		bBufferGotReferenced = true;

		InCopyResources->SetBufferIndex(OwningDecoderBufferInfo.BufferIndex);
		InCopyResources->SetValidCount(OwningDecoderBufferInfo.ValidCount);

		if (InCopyResources->ShouldReleaseBufferImmediately())
		{
			Decoder->ReleaseOutputBuffer(&OwningDecoderBufferInfo, bBufferGotReferenced, -1);
			OwningDecoderBufferInfo.BufferIndex = -1;
		}

		return IElectraDecoderVideoOutput::EImageCopyResult::Succeeded; 
	}
	return IElectraDecoderVideoOutput::EImageCopyResult::Failed; 
}
