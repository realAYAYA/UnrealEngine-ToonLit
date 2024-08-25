// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraMediaVPxDecoder.h"
#include "VPxDecoderElectraModule.h"
#include "IElectraCodecRegistry.h"
#include "IElectraCodecFactory.h"
#include "Misc/ScopeLock.h"
#include "Templates/SharedPointer.h"
#include "Features/IModularFeatures.h"
#include "Features/IModularFeature.h"
#include "IElectraCodecFactoryModule.h"
#include "IElectraDecoderFeaturesAndOptions.h"
#include "IElectraDecoder.h"
#include "IElectraDecoderOutputVideo.h"
#include "IElectraDecoderResourceDelegate.h"
#include "ElectraDecodersUtils.h"
#include "Utils/Google/ElectraUtilsVPxVideo.h"
#include "Containers/Queue.h"
#include <Stats/Stats.h>

#include COMPILED_PLATFORM_HEADER(ElectraDecoderGPUBufferHelpers.h)

#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

/*********************************************************************************************************************/


/*********************************************************************************************************************/

DECLARE_CYCLE_STAT(TEXT("ElectraDecoder ConvertOutput"), STAT_ElectraDecoder_ConvertOutputVpx, STATGROUP_Media);

/*********************************************************************************************************************/

#define ERRCODE_INTERNAL_NO_ERROR							0
#define ERRCODE_INTERNAL_ALREADY_CLOSED						1
#define ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER			2
#define ERRCODE_INTERNAL_FAILED_TO_DECODE_INPUT				3
#define ERRCODE_INTERNAL_FAILED_TO_FLUSH_DECODER			4
#define ERRCODE_INTERNAL_FAILED_TO_CONVERT_OUTPUT_SAMPLE	5

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

class FVideoDecoderVPxElectra;


class FDecoderDefaultVideoOutputFormatVPxElectra : public IElectraDecoderDefaultVideoOutputFormat
{
public:
	virtual ~FDecoderDefaultVideoOutputFormatVPxElectra()
	{ }

};


class FVideoDecoderOutputVPxElectra : public IElectraDecoderVideoOutput, public IElectraDecoderVideoOutputImageBuffers
{
public:
	virtual ~FVideoDecoderOutputVPxElectra()
	{ }

	FTimespan GetPTS() const override
	{ return PTS; }
	uint64 GetUserValue() const override
	{ return UserValue; }

	int32 GetWidth() const override
	{ return Width - Crop.Left - Crop.Right; }
	int32 GetHeight() const override
	{ return Height - Crop.Top - Crop.Bottom; }
	int32 GetDecodedWidth() const override
	{ return DecodedWidth; }
	int32 GetDecodedHeight() const override
	{ return DecodedHeight; }
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
	{
		if (InTypeOfHandle == EElectraDecoderPlatformOutputHandleType::ImageBuffers)
		{
			return static_cast<IElectraDecoderVideoOutputImageBuffers*>(const_cast<FVideoDecoderOutputVPxElectra*>(this));
		}
		return nullptr;
	}
	IElectraDecoderVideoOutputTransferHandle* GetTransferHandle() const override
	{ return nullptr; }
	IElectraDecoderVideoOutput::EImageCopyResult CopyPlatformImage(IElectraDecoderVideoOutputCopyResources* InCopyResources) const override
	{ return IElectraDecoderVideoOutput::EImageCopyResult::NotSupported; }

	// Methods from IElectraDecoderVideoOutputImageBuffers
	uint32 GetCodec4CC() const override
	{ return Codec4CC; }
	int32 GetNumberOfBuffers() const override
	{
		return NumBuffers;
	}
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> GetBufferDataByIndex(int32 InBufferIndex) const override
	{
		if (InBufferIndex == 0)
		{
			return ColorBuffer;
		}
		return nullptr;
	}
	void* GetBufferTextureByIndex(int32 InBufferIndex) const override
	{
#if ELECTRA_MEDIAGPUBUFFER_DX12
		if (InBufferIndex == 0)
		{
			return GPUBuffer.Resource.GetReference();
		}
#endif
		return nullptr;
	}
	virtual bool GetBufferTextureSyncByIndex(int32 InBufferIndex, FElectraDecoderOutputSync& SyncObject) const override
	{
#if ELECTRA_MEDIAGPUBUFFER_DX12
		if (InBufferIndex == 0)
		{
			SyncObject = { GPUBuffer.Fence.GetReference(), GPUBuffer.FenceValue };
			return true;
		}
#endif
		return false;
	}
	EElectraDecoderPlatformPixelFormat GetBufferFormatByIndex(int32 InBufferIndex) const override
	{
		if (InBufferIndex == 0)
		{
			return ColorBufferFormat;
		}
		return EElectraDecoderPlatformPixelFormat::INVALID;
	}
	EElectraDecoderPlatformPixelEncoding GetBufferEncodingByIndex(int32 InBufferIndex) const override
	{
		if (InBufferIndex == 0)
		{
			return ColorBufferEncoding;
		}
		return EElectraDecoderPlatformPixelEncoding::Native;
	}
	int32 GetBufferPitchByIndex(int32 InBufferIndex) const override
	{
		if (InBufferIndex == 0)
		{
			return ColorPitch;
		}
		return 0;
	}

public:
	FTimespan PTS;
	uint64 UserValue = 0;

	FElectraVideoDecoderOutputCropValues Crop;
	int32 Width = 0;
	int32 Height = 0;
	int32 DecodedWidth = 0;
	int32 DecodedHeight = 0;
	int32 NumBits = 0;
	int32 AspectW = 1;
	int32 AspectH = 1;
	int32 FrameRateN = 0;
	int32 FrameRateD = 0;
	int32 PixelFormat = 0;
	TMap<FString, FVariant> ExtraValues;

	uint32 Codec4CC = 0;
	int32 NumBuffers = 0;
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ColorBuffer;
	EElectraDecoderPlatformPixelFormat ColorBufferFormat;
	EElectraDecoderPlatformPixelEncoding ColorBufferEncoding;
	int32 ColorPitch = 0;
#if ELECTRA_MEDIAGPUBUFFER_DX12
	FElectraMediaDecoderOutputBufferPool_DX12::FOutputData GPUBuffer;
#endif
};


class FVideoDecoderVPxElectra : public IElectraDecoder
{
public:
	static void GetConfigurationOptions(TMap<FString, FVariant>& OutOptions)
	{
		OutOptions.Emplace(IElectraDecoderFeature::MinimumNumberOfOutputFrames, FVariant((int32)5));
		OutOptions.Emplace(IElectraDecoderFeature::IsAdaptive, FVariant(false));
	}

	FVideoDecoderVPxElectra(const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate);

	virtual ~FVideoDecoderVPxElectra();

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

	void Suspend() override
	{ }
	void Resume() override
	{ }

private:
	struct FDecoderInput
	{
		~FDecoderInput()
		{
			FMemory::Free(InputDataCopy);
		}
		FInputAccessUnit AccessUnit;
		TMap<FString, FVariant> AdditionalOptions;
		void* InputDataCopy = nullptr;
		int32 InputDataSize = 0;
		int32 SuperFrameIndexPlusOne = 0;
	};

	enum class EDecodeState
	{
		Decoding,
		Draining
	};

	static constexpr uint32 Make4CC(const uint8 A, const uint8 B, const uint8 C, const uint8 D)
	{
		return (static_cast<uint32>(A) << 24) | (static_cast<uint32>(B) << 16) | (static_cast<uint32>(C) << 8) | static_cast<uint32>(D);
	}

	bool PostError(int32 ApiReturnValue, FString Message, int32 Code);

	bool InternalDecoderCreate(const TMap<FString, FVariant>& InAdditionalOptions);
	void InternalDecoderDestroy();

	bool DecomposeSuperFrame(const FInputAccessUnit& InInputAccessUnit, const TMap<FString, FVariant>& InAdditionalOptions);
	bool PrepareSingleFrame(const FInputAccessUnit& InInputAccessUnit, const TMap<FString, FVariant>& InAdditionalOptions);
	IElectraDecoder::EDecoderError DecodeNextPending();

	enum class EConvertResult
	{
		Success,
		Failure,
		GotEOS
	};
	EConvertResult ConvertDecoderOutput(vpx_image_t* InDecodedImage);

	bool ConvertDecodedImageToNV12orP010(FVideoDecoderOutputVPxElectra* InNewOutput, const vpx_image_t* InDecodedImage, void* PlatformDevice, int32 PlatformDeviceVersion) const;

	TWeakPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> ResourceDelegate;

	TQueue<TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe>, EQueueMode::Spsc> PendingDecoderInput;
	TArray<TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe>> InDecoderInput;
	TSharedPtr<FVideoDecoderOutputVPxElectra, ESPMode::ThreadSafe> CurrentOutput;
	IElectraDecoder::FError LastError;
	EDecodeState DecodeState = EDecodeState::Decoding;
	uint32 Codec4CC = 0;

	vpx_codec_ctx_t DecoderContext;
	vpx_codec_ctx_t* VideoDecoderHandle = nullptr;
	vpx_codec_iter_t VideoDecoderOutputIterator = nullptr;

	uint32 MaxWidth;
	uint32 MaxHeight;
	uint32 MaxOutputBuffers;
#if ELECTRA_MEDIAGPUBUFFER_DX12
	mutable TSharedPtr<FElectraMediaDecoderOutputBufferPool_DX12> D3D12ResourcePool;
#endif
};

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

class FVPxVideoDecoderElectraFactory : public IElectraCodecFactory, public IElectraCodecModularFeature, public TSharedFromThis<FVPxVideoDecoderElectraFactory, ESPMode::ThreadSafe>
{
public:
	virtual ~FVPxVideoDecoderElectraFactory()
	{ }

	void GetListOfFactories(TArray<TWeakPtr<IElectraCodecFactory, ESPMode::ThreadSafe>>& OutCodecFactories) override
	{
		OutCodecFactories.Add(AsShared());
	}

	int32 SupportsFormat(const FString& InCodecFormat, bool bInEncoder, const TMap<FString, FVariant>& InOptions) const override
	{
		if (bInEncoder)
		{
			return 0;
		}
		if (InCodecFormat.StartsWith(TEXT("vp08"), ESearchCase::IgnoreCase))
		{
			return 1;
		}
		else if (InCodecFormat.StartsWith(TEXT("vp09"), ESearchCase::IgnoreCase))
		{
			ElectraDecodersUtil::FMimeTypeVideoCodecInfo ci;
			if (ElectraDecodersUtil::ParseCodecVP9(ci, InCodecFormat, ElectraDecodersUtil::GetVariantValueUInt8Array(InOptions, TEXT("$vpcC_box"))))
			{
				if (ci.NumBitsLuma > 8)
				{
				#if defined(VPX_CODEC_CAP_HIGHBITDEPTH)
					return !!(vpx_codec_get_caps(vpx_codec_vp9_dx()) & VPX_CODEC_CAP_HIGHBITDEPTH);
				#else
					return 0;
				#endif
				}
				return 1;
			}
		}
		return 0;
	}

	void GetConfigurationOptions(TMap<FString, FVariant>& OutOptions) const override
	{
		FVideoDecoderVPxElectra::GetConfigurationOptions(OutOptions);
	}

	TSharedPtr<IElectraDecoder, ESPMode::ThreadSafe> CreateDecoderForFormat(const FString& InCodecFormat, const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate) override
	{
		return MakeShared<FVideoDecoderVPxElectra, ESPMode::ThreadSafe>(InOptions, InResourceDelegate);
	}

	static TSharedPtr<FVPxVideoDecoderElectraFactory, ESPMode::ThreadSafe> Self;
};
TSharedPtr<FVPxVideoDecoderElectraFactory, ESPMode::ThreadSafe> FVPxVideoDecoderElectraFactory::Self;

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

void FElectraMediaVPxDecoder::Startup()
{
	// Make sure the codec factory module has been loaded.
	FModuleManager::Get().LoadModule(TEXT("ElectraCodecFactory"));

	// Create an instance of the factory, which is also the modular feature.
	check(!FVPxVideoDecoderElectraFactory::Self.IsValid());
	FVPxVideoDecoderElectraFactory::Self = MakeShared<FVPxVideoDecoderElectraFactory, ESPMode::ThreadSafe>();
	// Register as modular feature.
	IModularFeatures::Get().RegisterModularFeature(IElectraCodecFactoryModule::GetModularFeatureName(), FVPxVideoDecoderElectraFactory::Self.Get());
}

void FElectraMediaVPxDecoder::Shutdown()
{
	IModularFeatures::Get().UnregisterModularFeature(IElectraCodecFactoryModule::GetModularFeatureName(), FVPxVideoDecoderElectraFactory::Self.Get());
	FVPxVideoDecoderElectraFactory::Self.Reset();
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

FVideoDecoderVPxElectra::FVideoDecoderVPxElectra(const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate)
{
	Codec4CC = (uint32)ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("codec_4cc"), 0);
	ResourceDelegate = InResourceDelegate;

	MaxWidth = (uint32)ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("max_width"), 1920);
	MaxHeight = (uint32)ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("max_height"), 1080);

	MaxOutputBuffers = (uint32)ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("max_output_buffers"), 5);
	MaxOutputBuffers += kElectraDecoderPipelineExtraFrames;
}

FVideoDecoderVPxElectra::~FVideoDecoderVPxElectra()
{
	// Close() must have been called already!
	check(LastError.Code == ERRCODE_INTERNAL_ALREADY_CLOSED);
	// We do it nonetheless...
	Close();
}

void FVideoDecoderVPxElectra::GetFeatures(TMap<FString, FVariant>& OutFeatures) const
{
	GetConfigurationOptions(OutFeatures);
}

IElectraDecoder::FError FVideoDecoderVPxElectra::GetError() const
{
	return LastError;
}

bool FVideoDecoderVPxElectra::PostError(int32 ApiReturnValue, FString Message, int32 Code)
{
	LastError.Code = Code;
	LastError.SdkCode = ApiReturnValue;
	LastError.Message = MoveTemp(Message);
	return false;
}

void FVideoDecoderVPxElectra::Close()
{
	ResetToCleanStart();
	// Set the error state that all subsequent calls will fail.
	PostError(0, TEXT("Already closed"), ERRCODE_INTERNAL_ALREADY_CLOSED);
}

IElectraDecoder::ECSDCompatibility FVideoDecoderVPxElectra::IsCompatibleWith(const TMap<FString, FVariant>& CSDAndAdditionalOptions)
{
	if (!VideoDecoderHandle)
	{
		return IElectraDecoder::ECSDCompatibility::Compatible;
	}
	return IElectraDecoder::ECSDCompatibility::Drain;
}

bool FVideoDecoderVPxElectra::ResetToCleanStart()
{
	InternalDecoderDestroy();

	PendingDecoderInput.Empty();
	InDecoderInput.Empty();
	CurrentOutput.Reset();
	DecodeState = EDecodeState::Decoding;

	return !LastError.IsSet();
}

TSharedPtr<IElectraDecoderDefaultOutputFormat, ESPMode::ThreadSafe> FVideoDecoderVPxElectra::GetDefaultOutputFormatFromCSD(const TMap<FString, FVariant>& CSDAndAdditionalOptions)
{
	return nullptr;
}

IElectraDecoder::EDecoderError FVideoDecoderVPxElectra::DecodeAccessUnit(const FInputAccessUnit& InInputAccessUnit, const TMap<FString, FVariant>& InAdditionalOptions)
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

	// If we still have pending input we do not want anything new right now.
	if (!PendingDecoderInput.IsEmpty())
	{
		return IElectraDecoder::EDecoderError::NoBuffer;
	}

	// If there is pending output it is very likely that decoding this access unit would also generate output.
	// Since that would result in loss of the pending output we return now.
	if (CurrentOutput.IsValid())
	{
		return IElectraDecoder::EDecoderError::NoBuffer;
	}

#if ELECTRA_MEDIAGPUBUFFER_DX12
	// If we will create a new resource pool or we have still buffers in an existing one, we can proceed, else we'd have no resources to output the data
	if (D3D12ResourcePool.IsValid() && !D3D12ResourcePool->BufferAvailable())
	{
		return IElectraDecoder::EDecoderError::NoBuffer;
	}
#endif

	// CSD only buffer is not handled at the moment.
	check((InInputAccessUnit.Flags & EElectraDecoderFlags::InitCSDOnly) == EElectraDecoderFlags::None);

	// Create decoder transform if necessary.
	if (!VideoDecoderHandle && !InternalDecoderCreate(InAdditionalOptions))
	{
		return IElectraDecoder::EDecoderError::Error;
	}

	// Decode data. This immediately produces a new output frame.
	if (InInputAccessUnit.Data && InInputAccessUnit.DataSize)
	{
		switch(Codec4CC)
		{
			case Make4CC('v','p','0','8'):
			{
		        if (!PrepareSingleFrame(InInputAccessUnit, InAdditionalOptions))
		        {
			        return IElectraDecoder::EDecoderError::Error;
		        }
		        return DecodeNextPending();
			}
			case Make4CC('v','p','0','9'):
			{
		        // Decompose superframe into separate frames.
		        if (!DecomposeSuperFrame(InInputAccessUnit, InAdditionalOptions))
		        {
			        return IElectraDecoder::EDecoderError::Error;
		        }
		        return DecodeNextPending();
			}
			default:
			{
				PostError(0, TEXT("Unsupported format"), ERRCODE_INTERNAL_FAILED_TO_DECODE_INPUT);
				return IElectraDecoder::EDecoderError::Error;
			}
		}
	}
	return IElectraDecoder::EDecoderError::None;
}

IElectraDecoder::EDecoderError FVideoDecoderVPxElectra::DecodeNextPending()
{
	TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe> In;
	if (!PendingDecoderInput.Dequeue(In))
	{
		return IElectraDecoder::EDecoderError::None;
	}
	switch(Codec4CC)
	{
		case Make4CC('v','p','0','8'):
		{
			ElectraDecodersUtil::VPxVideo::FVP8UncompressedHeader Header;
			if (!ElectraDecodersUtil::VPxVideo::ParseVP8UncompressedHeader(Header, In->InputDataCopy, In->InputDataSize))
			{
				PostError(0, TEXT("Failed to validate VP8 bitstream header"), ERRCODE_INTERNAL_FAILED_TO_DECODE_INPUT);
				return IElectraDecoder::EDecoderError::Error;
			}
			break;
		}
		case Make4CC('v','p','0','9'):
		{
			ElectraDecodersUtil::VPxVideo::FVP9UncompressedHeader Header;
			if (!ElectraDecodersUtil::VPxVideo::ParseVP9UncompressedHeader(Header, In->InputDataCopy, In->InputDataSize))
			{
				PostError(0, TEXT("Failed to validate VP9 bitstream header"), ERRCODE_INTERNAL_FAILED_TO_DECODE_INPUT);
				return IElectraDecoder::EDecoderError::Error;
			}
			break;
		}
		default:
		{
			return IElectraDecoder::EDecoderError::Error;
		}
	}

	// Invoke decoder
	vpx_codec_err_t Result = vpx_codec_decode(VideoDecoderHandle, reinterpret_cast<const uint8*>(In->InputDataCopy), (unsigned int) In->InputDataSize, In.Get(), 0);
	if (Result != VPX_CODEC_OK)
	{
		PostError(Result, TEXT("Failed to decode video decoder input"), ERRCODE_INTERNAL_FAILED_TO_DECODE_INPUT);
		return IElectraDecoder::EDecoderError::Error;
	}

	// Add to the list of inputs passed to the decoder.
	InDecoderInput.Emplace(MoveTemp(In));

	// Did we produce a new frame?
	VideoDecoderOutputIterator = nullptr;
	EConvertResult ConvResult = ConvertDecoderOutput(vpx_codec_get_frame(VideoDecoderHandle, &VideoDecoderOutputIterator));
	if (ConvResult == EConvertResult::Failure)
	{
		return IElectraDecoder::EDecoderError::Error;
	}
	return IElectraDecoder::EDecoderError::None;
}


bool FVideoDecoderVPxElectra::PrepareSingleFrame(const FInputAccessUnit& InInputAccessUnit, const TMap<FString, FVariant>& InAdditionalOptions)
{
	TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe> In(new FDecoderInput);
	In->SuperFrameIndexPlusOne = 0;
	In->AdditionalOptions = InAdditionalOptions;
	In->AccessUnit = InInputAccessUnit;
	In->InputDataSize = InInputAccessUnit.DataSize;
	In->InputDataCopy = FMemory::Malloc(InInputAccessUnit.DataSize);
	FMemory::Memcpy(In->InputDataCopy, InInputAccessUnit.Data, InInputAccessUnit.DataSize);
	// Zero the input pointer and size in the copy. That data is not owned by us and it's best not to have any
	// values here that would lead us to think that we do.
	In->AccessUnit.Data = nullptr;
	In->AccessUnit.DataSize = 0;
	PendingDecoderInput.Enqueue(MoveTemp(In));
	return true;
}

bool FVideoDecoderVPxElectra::DecomposeSuperFrame(const FInputAccessUnit& InInputAccessUnit, const TMap<FString, FVariant>& InAdditionalOptions)
{
	TArray<uint32> FrameSizes;
	if (!ElectraDecodersUtil::VPxVideo::GetVP9SuperframeSizes(FrameSizes, InInputAccessUnit.Data, InInputAccessUnit.DataSize))
	{
		return false;
	}
	// Create input frames.
	const uint8* Data = reinterpret_cast<const uint8*>(InInputAccessUnit.Data);
	for(int32 i=0; i<FrameSizes.Num(); ++i)
	{
		// Add to the list of inputs passed to the decoder.
		TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe> In(new FDecoderInput);
		In->SuperFrameIndexPlusOne = FrameSizes.Num()>1 ? i+1 : 0;
		In->AdditionalOptions = InAdditionalOptions;
		In->AccessUnit = InInputAccessUnit;
		In->InputDataSize = FrameSizes[i];
		In->InputDataCopy = FMemory::Malloc(FrameSizes[i]);
		FMemory::Memcpy(In->InputDataCopy, Data, FrameSizes[i]);
		Data += FrameSizes[i];
		// Zero the input pointer and size in the copy. That data is not owned by us and it's best not to have any
		// values here that would lead us to think that we do.
		In->AccessUnit.Data = nullptr;
		In->AccessUnit.DataSize = 0;
		PendingDecoderInput.Enqueue(MoveTemp(In));
	}
	return true;
}

IElectraDecoder::EDecoderError FVideoDecoderVPxElectra::SendEndOfData()
{
	// If already in error do nothing!
	if (LastError.IsSet())
	{
		return IElectraDecoder::EDecoderError::Error;
	}

	// Already draining?
	if (DecodeState == EDecodeState::Draining)
	{
		return IElectraDecoder::EDecoderError::EndOfData;
	}

	// If there is a transform send an end-of-stream and drain message.
	if (VideoDecoderHandle)
	{
		DecodeState = EDecodeState::Draining;
	}
	return IElectraDecoder::EDecoderError::None;
}

IElectraDecoder::EDecoderError FVideoDecoderVPxElectra::Flush()
{
	// If already in error do nothing!
	if (LastError.IsSet())
	{
		return IElectraDecoder::EDecoderError::Error;
	}
	if (VideoDecoderHandle)
	{
		InternalDecoderDestroy();
		DecodeState = EDecodeState::Decoding;
		PendingDecoderInput.Empty();
		InDecoderInput.Empty();
		CurrentOutput.Reset();
	}
	return IElectraDecoder::EDecoderError::None;
}

IElectraDecoder::EOutputStatus FVideoDecoderVPxElectra::HaveOutput()
{
	// If already in error do nothing!
	if (LastError.IsSet())
	{
		return IElectraDecoder::EOutputStatus::Error;
	}
	// Have output?
	if (CurrentOutput.IsValid())
	{
		return IElectraDecoder::EOutputStatus::Available;
	}

	// See if there is any additional pending output.
	if (VideoDecoderHandle && VideoDecoderOutputIterator)
	{
		EConvertResult ConvResult = ConvertDecoderOutput(vpx_codec_get_frame(VideoDecoderHandle, &VideoDecoderOutputIterator));
		if (ConvResult == EConvertResult::Failure)
		{
			return IElectraDecoder::EOutputStatus::Error;
		}
		else if (ConvResult == EConvertResult::Success && CurrentOutput.IsValid())
		{
			return IElectraDecoder::EOutputStatus::Available;
		}
	}

	// Decode any pending input first, even when we are to drain the decoder.
	if (VideoDecoderHandle && !PendingDecoderInput.IsEmpty())
	{
		switch(DecodeNextPending())
		{
			case IElectraDecoder::EDecoderError::NoBuffer:
			{
				return IElectraDecoder::EOutputStatus::TryAgainLater;
			}
			case IElectraDecoder::EDecoderError::None:
			{
				if (CurrentOutput.IsValid())
				{
					return IElectraDecoder::EOutputStatus::Available;
				}
				break;
			}
			case IElectraDecoder::EDecoderError::Error:
			{
				return IElectraDecoder::EOutputStatus::Error;
			}
			default:
			{
				break;
			}
		}
	}
	if (DecodeState == EDecodeState::Draining && VideoDecoderHandle)
	{
		vpx_codec_err_t Result = vpx_codec_decode(VideoDecoderHandle, nullptr, 0, nullptr, 0);

		EConvertResult ConvResult = ConvertDecoderOutput(vpx_codec_get_frame(VideoDecoderHandle, &VideoDecoderOutputIterator));
		if (ConvResult == EConvertResult::Failure)
		{
			return IElectraDecoder::EOutputStatus::Error;
		}
		else if (ConvResult == EConvertResult::GotEOS)
		{
			DecodeState = EDecodeState::Decoding;
			PendingDecoderInput.Empty();
			InDecoderInput.Empty();
			return IElectraDecoder::EOutputStatus::EndOfData;
		}
		else
		{
			return CurrentOutput.IsValid() ? IElectraDecoder::EOutputStatus::Available : IElectraDecoder::EOutputStatus::EndOfData;
		}
	}
	return IElectraDecoder::EOutputStatus::NeedInput;
}

TSharedPtr<IElectraDecoderOutput, ESPMode::ThreadSafe> FVideoDecoderVPxElectra::GetOutput()
{
	TSharedPtr<IElectraDecoderOutput, ESPMode::ThreadSafe> Out = CurrentOutput;
	CurrentOutput.Reset();
	return Out;
}

bool FVideoDecoderVPxElectra::InternalDecoderCreate(const TMap<FString, FVariant>& InAdditionalOptions)
{
	FMemory::Memzero(DecoderContext);
	vpx_codec_err_t Result = VPX_CODEC_INCAPABLE;
	switch(Codec4CC)
	{
		case Make4CC('v','p','0','8'):
		{
			vpx_codec_caps_t Capabilities = vpx_codec_get_caps(vpx_codec_vp8_dx());
			check((Capabilities & (VPX_CODEC_CAP_PUT_SLICE | VPX_CODEC_CAP_PUT_FRAME)) == 0);

			vpx_codec_flags_t Flags = 0;
			const int32 NumOfThreads = 1;
			const vpx_codec_dec_cfg_t CodecConfig = { NumOfThreads, 0, 0 };
			Result = vpx_codec_dec_init(&DecoderContext, vpx_codec_vp8_dx(), &CodecConfig, Flags);
			break;
		}
		case Make4CC('v','p','0','9'):
		{
			vpx_codec_caps_t Capabilities = vpx_codec_get_caps(vpx_codec_vp9_dx());
			check((Capabilities & (VPX_CODEC_CAP_PUT_SLICE | VPX_CODEC_CAP_PUT_FRAME)) == 0);
			//   VPX_CODEC_CAP_HIGHBITDEPTH 0x4
			//   VPX_CODEC_CAP_EXTERNAL_FRAME_BUFFER 0x400000
			vpx_codec_flags_t Flags = 0;
			const int32 NumOfThreads = 1;
			const vpx_codec_dec_cfg_t CodecConfig = { NumOfThreads, 0, 0 };
			Result = vpx_codec_dec_init(&DecoderContext, vpx_codec_vp9_dx(), &CodecConfig, Flags);
			break;
		}
	}
	if (Result != VPX_CODEC_OK)
	{
		return PostError(Result, TEXT("Failed to create decoder"), ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
	}
	VideoDecoderHandle = &DecoderContext;
	VideoDecoderOutputIterator = nullptr;
	return true;
}

void FVideoDecoderVPxElectra::InternalDecoderDestroy()
{
	if (VideoDecoderHandle)
	{
		/*vpx_codec_err_t Result =*/ vpx_codec_destroy(VideoDecoderHandle);
		VideoDecoderHandle = nullptr;
		FMemory::Memzero(DecoderContext);
		VideoDecoderOutputIterator = nullptr;
	}
}



FVideoDecoderVPxElectra::EConvertResult FVideoDecoderVPxElectra::ConvertDecoderOutput(vpx_image_t* InDecodedImage)
{
	if (!InDecodedImage)
	{
		VideoDecoderOutputIterator = nullptr;
		return EConvertResult::GotEOS;
	}

	SCOPE_CYCLE_COUNTER(STAT_ElectraDecoder_ConvertOutputVpx);

	// Find the input corresponding to this output.
	TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe> In;
	for(int32 nInDec=0; nInDec<InDecoderInput.Num(); ++nInDec)
	{
		if (InDecoderInput[nInDec].Get() == reinterpret_cast<FDecoderInput*>(InDecodedImage->user_priv))
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

	TSharedPtr<FVideoDecoderOutputVPxElectra, ESPMode::ThreadSafe> NewOutput = MakeShared<FVideoDecoderOutputVPxElectra>();
	NewOutput->PTS = In->AccessUnit.PTS;
	NewOutput->UserValue = In->AccessUnit.UserValue;
	
	NewOutput->Width = InDecodedImage->d_w;
	NewOutput->Height = InDecodedImage->d_h;
	NewOutput->NumBits = InDecodedImage->bit_depth;
	NewOutput->AspectW = 1;
	NewOutput->AspectH = 1;

	void* PlatformDevice = nullptr;
	int32 PlatformDeviceVersion = 0;
	bool bUseGPUBuffers = false;
#if ELECTRA_MEDIAGPUBUFFER_DX12
	if (auto PinnedResourceDelegate = ResourceDelegate.Pin())
	{
		PinnedResourceDelegate->GetD3DDevice(&PlatformDevice, &PlatformDeviceVersion);
		bUseGPUBuffers = (PlatformDevice && PlatformDeviceVersion >= 12000);
	}
#endif

	if (InDecodedImage->fmt == VPX_IMG_FMT_I420)
	{
		if (!ConvertDecodedImageToNV12orP010(NewOutput.Get(), InDecodedImage, PlatformDevice, PlatformDeviceVersion))
		{
			PostError(0, FString::Printf(TEXT("Failed to convert decoded image")), ERRCODE_INTERNAL_FAILED_TO_CONVERT_OUTPUT_SAMPLE);
			return EConvertResult::Failure;
		}
		NewOutput->NumBuffers = 1;
		NewOutput->ColorBufferFormat = EElectraDecoderPlatformPixelFormat::NV12;
		NewOutput->ColorBufferEncoding = EElectraDecoderPlatformPixelEncoding::Native;
		NewOutput->DecodedWidth = NewOutput->Width;
		NewOutput->DecodedHeight = bUseGPUBuffers ? NewOutput->Height : (NewOutput->Height * 3 / 2);
	}
	else if (InDecodedImage->fmt == VPX_IMG_FMT_I42016)
	{
		check(NewOutput->NumBits == 10);
		if (!ConvertDecodedImageToNV12orP010(NewOutput.Get(), InDecodedImage, PlatformDevice, PlatformDeviceVersion))
		{
			PostError(0, FString::Printf(TEXT("Failed to convert decoded image")), ERRCODE_INTERNAL_FAILED_TO_CONVERT_OUTPUT_SAMPLE);
			return EConvertResult::Failure;
		}
		NewOutput->NumBuffers = 1;
		NewOutput->ColorBufferFormat = EElectraDecoderPlatformPixelFormat::P010;
		NewOutput->ColorBufferEncoding = EElectraDecoderPlatformPixelEncoding::Native;
		NewOutput->DecodedWidth = NewOutput->Width;
		NewOutput->DecodedHeight = bUseGPUBuffers ? NewOutput->Height : (NewOutput->Height * 3 / 2);

		// VPx decoders return the 10-bit output in the lower bits, but the output pipe expects it in the upper bits. Post scale to compensate!
		NewOutput->ExtraValues.Emplace(TEXT("pix_datascale"), FVariant(64.0));
	}
	else
	{
		PostError(0, FString::Printf(TEXT("Unsupported decoded image format (%d)"), InDecodedImage->fmt), ERRCODE_INTERNAL_FAILED_TO_CONVERT_OUTPUT_SAMPLE);
		return EConvertResult::Failure;
	}

	switch(Codec4CC)
	{
		case Make4CC('v','p','0','8'):
		{
			NewOutput->ExtraValues.Emplace(TEXT("codec"), FVariant(TEXT("vp8")));
			break;
		}
		case Make4CC('v','p','0','9'):
		{
			NewOutput->ExtraValues.Emplace(TEXT("codec"), FVariant(TEXT("vp9")));
			break;
		}
	}
	NewOutput->ExtraValues.Emplace(TEXT("platform"), FVariant(TEXT("generic")));

	CurrentOutput = MoveTemp(NewOutput);
	return EConvertResult::Success;
}

bool FVideoDecoderVPxElectra::ConvertDecodedImageToNV12orP010(FVideoDecoderOutputVPxElectra* InNewOutput, const vpx_image_t* InDecodedImage, void* PlatformDevice, int32 PlatformDeviceVersion) const
{
	const bool bIsNV12 = (InDecodedImage->fmt == VPX_IMG_FMT_I420);

	const int32 w = InDecodedImage->d_w;
	const int32 h = InDecodedImage->d_h;
	const int32 aw = Align(w, 2);
	const int32 ah = Align(h, 2);

	const uint8* SrcY = (const uint8*)InDecodedImage->planes[0];
	const uint8* SrcU = (const uint8*)InDecodedImage->planes[1];
	const uint8* SrcV = (const uint8*)InDecodedImage->planes[2];
	const int32 PitchY = InDecodedImage->stride[0];
	const int32 PitchU = InDecodedImage->stride[1];
	const int32 PitchV = InDecodedImage->stride[2];
	if (!SrcY || !SrcU || !SrcV)
	{
		return false;
	}

	uint8* DstY;
	uint8* DstUV;
	uint32 DstPitch;

	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& OutNV12Buffer = InNewOutput->ColorBuffer;
#if ELECTRA_MEDIAGPUBUFFER_DX12
	if (!PlatformDevice || PlatformDeviceVersion < 12000)
#endif
	{
		uint32 Pitch = bIsNV12 ? aw : (aw * 2);
		int32 AllocSize = Pitch * (ah * 3 / 2);

#if ELECTRA_MEDIAGPUBUFFER_DX12
		InNewOutput->GPUBuffer.Resource = nullptr;
		InNewOutput->GPUBuffer.Fence = nullptr;
#endif

		OutNV12Buffer = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>();
		OutNV12Buffer->AddUninitialized(AllocSize);
		DstY = OutNV12Buffer->GetData();
		DstUV = DstY + Pitch * ah;
		DstPitch = Pitch;
	}
#if ELECTRA_MEDIAGPUBUFFER_DX12
	else
	{
		TRefCountPtr D3D12Device(static_cast<ID3D12Device*>(PlatformDevice));

		OutNV12Buffer.Reset();
		InNewOutput->GPUBuffer.Resource = nullptr;

		// Create the resource pool as needed...
		if (!D3D12ResourcePool)
		{
			D3D12ResourcePool = MakeShared<FElectraMediaDecoderOutputBufferPool_DX12, ESPMode::ThreadSafe>(D3D12Device, MaxOutputBuffers, MaxWidth, MaxHeight * 3 / 2, bIsNV12 ? 1 : 2);
		}

		// Request resource and fence...
		uint32 BufferPitch;
		D3D12ResourcePool->AllocateOutputDataAsBuffer(InNewOutput->GPUBuffer, BufferPitch);

		// Correct pitch ton reflect resource's setup
		InNewOutput->ColorPitch = BufferPitch;

		// Map the buffer so we can get a CPU address to the WC configured buffer
		HRESULT Res = InNewOutput->GPUBuffer.Resource->Map(0, nullptr, (void**)&DstY);
		check(SUCCEEDED(Res));
		DstUV = DstY + BufferPitch * ah;
		DstPitch = BufferPitch;
	}
#endif // ELECTRA_MEDIAGPUBUFFER_DX12

	InNewOutput->ColorPitch = DstPitch;

	if (bIsNV12)
	{
		for (int32 y = 0; y < h; ++y)
		{
			FMemory::Memcpy(DstY, SrcY, w);
			SrcY += PitchY;
			DstY += DstPitch;
		}
		for (int32 v = 0; v < h / 2; ++v)
		{
			uint8* DstUVLine = DstUV;
			for (int32 u = 0; u < w / 2; ++u)
			{
				*DstUVLine++ = SrcU[u];
				*DstUVLine++ = SrcV[u];
			}
			SrcU += PitchU;
			SrcV += PitchV;
			DstUV += DstPitch;
		}
	}
	else
	{
		// note: data is delivered in the lower 10-bits, but expected in the upper
		// -> instead of processing the data here, we provide a "data scale" attribute to be applied on conversion from YUV to RGB
		for (int32 y = 0; y < h; ++y)
		{
			FMemory::Memcpy(DstY, SrcY, int64(w) << 1);
			SrcY += PitchY;
			DstY += DstPitch;
		}
		for (int32 v = 0; v < h / 2; ++v)
		{
			uint16* DstUVLine = (uint16*)DstUV;
			const uint16* SrcU16 = (const uint16*)SrcU;
			const uint16* SrcV16 = (const uint16*)SrcV;
			for (int32 u = 0; u < w / 2; ++u)
			{
				*DstUVLine++ = SrcU16[u];
				*DstUVLine++ = SrcV16[u];
			}
			SrcU += PitchU;
			SrcV += PitchV;
			DstUV += DstPitch;
		}
	}

#if ELECTRA_MEDIAGPUBUFFER_DX12
	if (InNewOutput->GPUBuffer.Resource.IsValid())
	{
		InNewOutput->GPUBuffer.Resource->Unmap(0, nullptr);
		// To be compatible with implementations that might do the copy into the resource async, we also signal a fence
		// (strictly speaking we would not need to as this is all 100% synchronous and done before the GPU ever attempts to read from the resource)
		InNewOutput->GPUBuffer.Fence->Signal(InNewOutput->GPUBuffer.FenceValue);
	}
#endif

	return true;
}
