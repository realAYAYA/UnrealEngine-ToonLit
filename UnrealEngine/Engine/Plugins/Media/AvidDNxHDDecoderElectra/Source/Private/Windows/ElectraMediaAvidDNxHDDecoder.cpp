// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraMediaAvidDNxHDDecoder.h"
#include "AvidDNxHDDecoderElectraModule.h"
#include "IElectraCodecRegistry.h"
#include "IElectraCodecFactory.h"
#include "Misc/ScopeLock.h"
#include "Templates/SharedPointer.h"
#include "Features/IModularFeatures.h"
#include "Features/IModularFeature.h"
#include "Modules/ModuleManager.h"
#include "HAL/Platform.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "IElectraCodecFactoryModule.h"
#include "IElectraDecoderFeaturesAndOptions.h"
#include "IElectraDecoder.h"
#include "IElectraDecoderOutputVideo.h"
#include "IElectraDecoderResourceDelegate.h"
#include "ElectraDecodersUtils.h"
#include COMPILED_PLATFORM_HEADER(ElectraDecoderGPUBufferHelpers.h)

#include "AvidDNxCodec.h"
//#include "dnx_uncompressed_sdk.h"

#define ERRCODE_INTERNAL_NO_ERROR							0
#define ERRCODE_INTERNAL_ALREADY_CLOSED						1
#define ERRCODE_INTERNAL_FAILED_TO_DECODE_INPUT				2

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

class FVideoDecoderAvidDNxHDElectra;


class FDecoderDefaultVideoOutputFormatAvidDNxHDElectra : public IElectraDecoderDefaultVideoOutputFormat
{
public:
	virtual ~FDecoderDefaultVideoOutputFormatAvidDNxHDElectra()
	{ }

};


class FVideoDecoderOutputAvidDNxHDElectra : public IElectraDecoderVideoOutput, public IElectraDecoderVideoOutputImageBuffers
{
public:
	virtual ~FVideoDecoderOutputAvidDNxHDElectra()
	{
	}

	FTimespan GetPTS() const override
	{
		return PTS;
	}
	uint64 GetUserValue() const override
	{
		return UserValue;
	}

	int32 GetWidth() const override
	{
		return ImageWidth;
	}
	int32 GetHeight() const override
	{
		return ImageHeight;
	}
	int32 GetDecodedWidth() const override
	{
		return Width;
	}
	int32 GetDecodedHeight() const override
	{
		return Height;
	}
	FElectraVideoDecoderOutputCropValues GetCropValues() const override
	{
		return Crop;
	}
	int32 GetAspectRatioW() const override
	{
		return AspectW;
	}
	int32 GetAspectRatioH() const override
	{
		return AspectH;
	}
	int32 GetFrameRateNumerator() const override
	{
		return FrameRateN;
	}
	int32 GetFrameRateDenominator() const override
	{
		return FrameRateD;
	}
	int32 GetNumberOfBits() const override
	{
		return NumBits;
	}
	void GetExtraValues(TMap<FString, FVariant>& OutExtraValues) const override
	{
		OutExtraValues = ExtraValues;
	}
	void* GetPlatformOutputHandle(EElectraDecoderPlatformOutputHandleType InTypeOfHandle) const override
	{
		if (InTypeOfHandle == EElectraDecoderPlatformOutputHandleType::ImageBuffers)
		{
			return static_cast<IElectraDecoderVideoOutputImageBuffers*>(const_cast<FVideoDecoderOutputAvidDNxHDElectra*>(this));
		}
		return nullptr;
	}
	IElectraDecoderVideoOutputTransferHandle* GetTransferHandle() const override
	{
		return nullptr;
	}
	IElectraDecoderVideoOutput::EImageCopyResult CopyPlatformImage(IElectraDecoderVideoOutputCopyResources* InCopyResources) const override
	{
		return IElectraDecoderVideoOutput::EImageCopyResult::NotSupported;
	}

	// Methods from IElectraDecoderVideoOutputImageBuffers
	uint32 GetCodec4CC() const override
	{
		return Codec4CC;
	}
	int32 GetNumberOfBuffers() const override
	{
		return 1;
	}
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> GetBufferDataByIndex(int32 InBufferIndex) const override
	{
		if (InBufferIndex == 0)
		{
			return Buffer;
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
			SyncObject = { GPUBuffer.Fence.GetReference(), GPUBuffer.FenceValue, nullptr, GPUBuffer_TaskSync };
			return true;
		}
#endif
	return false;
	}
	EElectraDecoderPlatformPixelFormat GetBufferFormatByIndex(int32 InBufferIndex) const override
	{
		if (InBufferIndex == 0)
		{
			return BufferFormat;
		}
		return EElectraDecoderPlatformPixelFormat::INVALID;
	}
	EElectraDecoderPlatformPixelEncoding GetBufferEncodingByIndex(int32 InBufferIndex) const override
	{
		if (InBufferIndex == 0)
		{
			return BufferEncoding;
		}
		return EElectraDecoderPlatformPixelEncoding::Native;
	}
	int32 GetBufferPitchByIndex(int32 InBufferIndex) const override
	{
		if (InBufferIndex == 0)
		{
			return Pitch;
		}
		return 0;
	}

public:
	FTimespan PTS;
	uint64 UserValue = 0;

	FElectraVideoDecoderOutputCropValues Crop;
	int32 ImageWidth = 0;
	int32 ImageHeight = 0;
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

	uint32 Codec4CC = 0;
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> Buffer;
	EElectraDecoderPlatformPixelFormat BufferFormat = EElectraDecoderPlatformPixelFormat::INVALID;
	EElectraDecoderPlatformPixelEncoding BufferEncoding = EElectraDecoderPlatformPixelEncoding::Native;
#if ELECTRA_MEDIAGPUBUFFER_DX12
	FElectraMediaDecoderOutputBufferPool_DX12::FOutputData GPUBuffer;
	TSharedPtr<IElectraDecoderResourceDelegateBase::IAsyncConsecutiveTaskSync> GPUBuffer_TaskSync;
#endif
};



class FVideoDecoderAvidDNxHDElectra : public IElectraDecoder
{
public:
	static void GetConfigurationOptions(TMap<FString, FVariant>& OutOptions)
	{
		OutOptions.Emplace(IElectraDecoderFeature::MinimumNumberOfOutputFrames, FVariant((int32)5));
		OutOptions.Emplace(IElectraDecoderFeature::IsAdaptive, FVariant(true));
	}

	FVideoDecoderAvidDNxHDElectra(const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate);

	virtual ~FVideoDecoderAvidDNxHDElectra();

	IElectraDecoder::EType GetType() const override
	{
		return IElectraDecoder::EType::Video;
	}

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
	static constexpr uint32 Make4CC(const uint8 A, const uint8 B, const uint8 C, const uint8 D)
	{
		return (static_cast<uint32>(A) << 24) | (static_cast<uint32>(B) << 16) | (static_cast<uint32>(C) << 8) | static_cast<uint32>(D);
	}

	bool PostError(int32 ApiReturnValue, FString Message, int32 Code);

	int32 DisplayWidth = 0;
	int32 DisplayHeight = 0;
	int32 DecodedWidth = 0;
	int32 DecodedHeight = 0;
	int32 AspectW = 0;
	int32 AspectH = 0;
	uint32 Codec4CC = 0;

	struct FDecoderHandle
	{
		int32 PrepareForDecoding(const void* InInput, uint32 InInputSize, int32 InNumDecodeThreads, bool bIgnoreAlpha);
		int32 GetNumOutputBits() const
		{ return NumOutputBits; }
		int32 Decode(uint8* OutBufferData, uint32 OutBufferSize, const void* InInput, uint32 InInputSize);
		void Reset();
		bool SetupDecompressStruct();
		static int32 GetNumberOfOutputColorBitsPerPixel(DNX_ComponentType_t InOutputComponentType);
		DNX_Decoder Handle = nullptr;
		DNX_CompressedParams_t CurrentCompressedParams;
		DNX_UncompressedParams_t CurrentUncompressedParams;
		DNX_SignalStandard_t CurrentSignalStandard = DNX_SignalStandard_t::DNX_SS_INVALID;
		DNX_DecodeOperationParams_t DecodeOperationParams;
		int32 NumOutputBits = 0;
		bool bHasAlpha = false;
	};
	int32 DesiredNumberOfDecoderThreads = 0;
	FDecoderHandle Decoder;

	IElectraDecoder::FError LastError;

	TSharedPtr<FVideoDecoderOutputAvidDNxHDElectra, ESPMode::ThreadSafe> CurrentOutput;
	bool bFlushPending = false;

	TWeakPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> ResourceDelegate;

	uint32 MaxOutputBuffers;
#if ELECTRA_MEDIAGPUBUFFER_DX12
	mutable TSharedPtr<FElectraMediaDecoderOutputBufferPool_DX12> D3D12ResourcePool;
	mutable TSharedPtr<IElectraDecoderResourceDelegateBase::IAsyncConsecutiveTaskSync> TaskSync;
#endif
};


/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

class FAvidDNxHDVideoDecoderElectraFactory : public IElectraCodecFactory, public IElectraCodecModularFeature, public TSharedFromThis<FAvidDNxHDVideoDecoderElectraFactory, ESPMode::ThreadSafe>
{
public:
	virtual ~FAvidDNxHDVideoDecoderElectraFactory()
	{ }

	void GetListOfFactories(TArray<TWeakPtr<IElectraCodecFactory, ESPMode::ThreadSafe>>& OutCodecFactories) override
	{
		OutCodecFactories.Add(AsShared());
	}

	int32 SupportsFormat(const FString& InCodecFormat, bool bInEncoder, const TMap<FString, FVariant>& InOptions) const override
	{
		return bInEncoder || !Permitted4CCs.Contains(InCodecFormat) ? 0 : 1;
	}

	void GetConfigurationOptions(TMap<FString, FVariant>& OutOptions) const override
	{
		FVideoDecoderAvidDNxHDElectra::GetConfigurationOptions(OutOptions);
	}

	TSharedPtr<IElectraDecoder, ESPMode::ThreadSafe> CreateDecoderForFormat(const FString& InCodecFormat, const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate) override
	{
		return MakeShared<FVideoDecoderAvidDNxHDElectra, ESPMode::ThreadSafe>(InOptions, InResourceDelegate);
	}

	static TSharedPtr<FAvidDNxHDVideoDecoderElectraFactory, ESPMode::ThreadSafe> Self;
	static TArray<FString> Permitted4CCs;
};
TSharedPtr<FAvidDNxHDVideoDecoderElectraFactory, ESPMode::ThreadSafe> FAvidDNxHDVideoDecoderElectraFactory::Self;
TArray<FString> FAvidDNxHDVideoDecoderElectraFactory::Permitted4CCs = { TEXT("AVdh") };

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

namespace FElectraMediaAvidDNxHDDecoder_Windows
{
	static void* LibHandleDNx = nullptr;
	static void* LibHandleDNxUncompressed = nullptr;
	static bool bWasRegistered = false;
}

void FElectraMediaAvidDNxHDDecoder::Startup()
{
	// Make sure the codec factory module has been loaded.
	FModuleManager::Get().LoadModule(TEXT("ElectraCodecFactory"));

	// Load the DLLs
	FElectraMediaAvidDNxHDDecoder_Windows::LibHandleDNx = FPlatformProcess::GetDllHandle(TEXT("DNxHR.dll"));
	if (!FElectraMediaAvidDNxHDDecoder_Windows::LibHandleDNx)
	{
		UE_LOG(LogAvidDNxHDElectraDecoder, Error, TEXT("Failed to load required library. Plug-in will not be functional."));
		return;
	}
/*
	FElectraMediaAvidDNxHDDecoder_Windows::LibHandleDNxUncompressed = FPlatformProcess::GetDllHandle(TEXT("DNxUncompressedSDK.dll"));
	if (!FElectraMediaAvidDNxHDDecoder_Windows::LibHandleDNxUncompressed)
	{
		UE_LOG(LogAvidDNxHDElectraDecoder, Error, TEXT("Failed to load required library. Plug-in will not be functional."));
		return;
	}
*/
	int Result = DNX_Initialize();
	if (Result != DNX_NO_ERROR)
	{
		UE_LOG(LogAvidDNxHDElectraDecoder, Error, TEXT("DNX_Initialize() returned error %d. Plug-in will not be functional."), Result);
		return;
	}

	// Create an instance of the factory, which is also the modular feature.
	check(!FAvidDNxHDVideoDecoderElectraFactory::Self.IsValid());
	FAvidDNxHDVideoDecoderElectraFactory::Self = MakeShared<FAvidDNxHDVideoDecoderElectraFactory, ESPMode::ThreadSafe>();
	// Register as modular feature.
	IModularFeatures::Get().RegisterModularFeature(IElectraCodecFactoryModule::GetModularFeatureName(), FAvidDNxHDVideoDecoderElectraFactory::Self.Get());
	FElectraMediaAvidDNxHDDecoder_Windows::bWasRegistered = true;
}

void FElectraMediaAvidDNxHDDecoder::Shutdown()
{
	if (FElectraMediaAvidDNxHDDecoder_Windows::bWasRegistered)
	{
		DNX_Finalize();
		FElectraMediaAvidDNxHDDecoder_Windows::bWasRegistered = false;
		IModularFeatures::Get().UnregisterModularFeature(IElectraCodecFactoryModule::GetModularFeatureName(), FAvidDNxHDVideoDecoderElectraFactory::Self.Get());
		FAvidDNxHDVideoDecoderElectraFactory::Self.Reset();
	}
	// Unload the DLLs
	if (FElectraMediaAvidDNxHDDecoder_Windows::LibHandleDNx)
	{
		FPlatformProcess::FreeDllHandle(FElectraMediaAvidDNxHDDecoder_Windows::LibHandleDNx);
		FElectraMediaAvidDNxHDDecoder_Windows::LibHandleDNx = nullptr;
	}
	if (FElectraMediaAvidDNxHDDecoder_Windows::LibHandleDNxUncompressed)
	{
		FPlatformProcess::FreeDllHandle(FElectraMediaAvidDNxHDDecoder_Windows::LibHandleDNxUncompressed);
		FElectraMediaAvidDNxHDDecoder_Windows::LibHandleDNxUncompressed = nullptr;
	}
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

FVideoDecoderAvidDNxHDElectra::FVideoDecoderAvidDNxHDElectra(const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate)
{
	ResourceDelegate = InResourceDelegate;

	DisplayWidth = (int32)ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("width"), 0);
	DisplayHeight = (int32)ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("height"), 0);

	DecodedWidth = DisplayWidth;
	DecodedHeight = DisplayHeight;
	AspectW = (int32)ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("aspect_w"), 0);
	AspectH = (int32)ElectraDecodersUtil::GetVariantValueSafeI64(InOptions, TEXT("aspect_h"), 0);
	Codec4CC = (uint32)ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("codec_4cc"), 0);

	// FIXME: How many exactly?
	DesiredNumberOfDecoderThreads = 8;

	MaxOutputBuffers = (uint32)ElectraDecodersUtil::GetVariantValueSafeU64(InOptions, TEXT("max_output_buffers"), 5);
	MaxOutputBuffers += kElectraDecoderPipelineExtraFrames;
}

FVideoDecoderAvidDNxHDElectra::~FVideoDecoderAvidDNxHDElectra()
{
	// Close() must have been called already!
	check(LastError.Code == ERRCODE_INTERNAL_ALREADY_CLOSED);
	// We do it nonetheless...
	Close();
}

void FVideoDecoderAvidDNxHDElectra::GetFeatures(TMap<FString, FVariant>& OutFeatures) const
{
	GetConfigurationOptions(OutFeatures);
}

IElectraDecoder::FError FVideoDecoderAvidDNxHDElectra::GetError() const
{
	return LastError;
}

bool FVideoDecoderAvidDNxHDElectra::PostError(int32 ApiReturnValue, FString Message, int32 Code)
{
	LastError.Code = Code;
	LastError.SdkCode = ApiReturnValue;
	LastError.Message = MoveTemp(Message);
#if 0
	void DNX_GetErrorString(int errorCode, char* errorStrPtr /*Pointer to a buffer which receives an error description string. Buffer should be at least 60 characters long.*/);
#endif
	return false;
}

void FVideoDecoderAvidDNxHDElectra::Close()
{
	ResetToCleanStart();
	// Set the error state that all subsequent calls will fail.
	PostError(0, TEXT("Already closed"), ERRCODE_INTERNAL_ALREADY_CLOSED);
}

IElectraDecoder::ECSDCompatibility FVideoDecoderAvidDNxHDElectra::IsCompatibleWith(const TMap<FString, FVariant>& CSDAndAdditionalOptions)
{
	return IElectraDecoder::ECSDCompatibility::Compatible;
}

bool FVideoDecoderAvidDNxHDElectra::ResetToCleanStart()
{
	bFlushPending = false;
	CurrentOutput.Reset();
	Decoder.Reset();
	return true;
}

TSharedPtr<IElectraDecoderDefaultOutputFormat, ESPMode::ThreadSafe> FVideoDecoderAvidDNxHDElectra::GetDefaultOutputFormatFromCSD(const TMap<FString, FVariant>& CSDAndAdditionalOptions)
{
	return nullptr;
}

static void CopyData(TRefCountPtr<ID3D12Resource> BufferResource, uint32 BufferPitch, const uint8* TempBuffer, uint32 TempBufferPitch, uint32 Height)
{
	uint8* ResourceAddr = nullptr;
	HRESULT Res = BufferResource->Map(0, nullptr, (void**)&ResourceAddr);
	check(SUCCEEDED(Res));

	FElectraMediaDecoderOutputBufferPool_DX12::CopyWithPitchAdjust(ResourceAddr, BufferPitch, TempBuffer, TempBufferPitch, Height);

	// We decoded into the resource: Unmap the resource memory and signal that it's usable by the GPU
	BufferResource->Unmap(0, nullptr);
}

IElectraDecoder::EDecoderError FVideoDecoderAvidDNxHDElectra::DecodeAccessUnit(const FInputAccessUnit& InInputAccessUnit, const TMap<FString, FVariant>& InAdditionalOptions)
{
	// If already in error do nothing!
	if (LastError.IsSet())
	{
		return IElectraDecoder::EDecoderError::Error;
	}

	// Can not feed new input until draining has finished.
	if (bFlushPending)
	{
		return IElectraDecoder::EDecoderError::EndOfData;
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

	// Decode data. This immediately produces a new output frame.
	if (InInputAccessUnit.Data && InInputAccessUnit.DataSize)
	{
		int32 Result;
		Result = Decoder.PrepareForDecoding(InInputAccessUnit.Data, (unsigned int)InInputAccessUnit.DataSize, DesiredNumberOfDecoderThreads, false);
		if (Result)
		{
			PostError(Result, TEXT("PrepareForDecoding() failed"), ERRCODE_INTERNAL_FAILED_TO_DECODE_INPUT);
			return IElectraDecoder::EDecoderError::Error;
		}

		void* PlatformDevice = nullptr;
		int32 PlatformDeviceVersion = 0;
		bool bUseGPUBuffers = false;
		auto PinnedResourceDelegate = ResourceDelegate.Pin();
#if ELECTRA_MEDIAGPUBUFFER_DX12
		if (PinnedResourceDelegate.IsValid())
		{
			PinnedResourceDelegate->GetD3DDevice(&PlatformDevice, &PlatformDeviceVersion);
			bUseGPUBuffers = (PlatformDevice && PlatformDeviceVersion >= 12000);
		}
#endif

		TSharedPtr<FVideoDecoderOutputAvidDNxHDElectra, ESPMode::ThreadSafe> NewOutput = MakeShared<FVideoDecoderOutputAvidDNxHDElectra>();
		NewOutput->PTS = InInputAccessUnit.PTS;
		NewOutput->UserValue = InInputAccessUnit.UserValue;

		NewOutput->Width = DecodedWidth;
		NewOutput->Height = DecodedHeight;
		NewOutput->Pitch = NewOutput->Width;
		NewOutput->Crop.Right = DecodedWidth - DisplayWidth;
		NewOutput->Crop.Bottom = DecodedHeight - DisplayHeight;
		if (AspectW && AspectH)
		{
			NewOutput->AspectW = AspectW;
			NewOutput->AspectH = AspectH;
		}
		NewOutput->Codec4CC = Codec4CC;
		NewOutput->NumBits = Decoder.GetNumOutputBits();

		NewOutput->ImageWidth = DecodedWidth - NewOutput->Crop.Left - NewOutput->Crop.Right;
		NewOutput->ImageHeight = DecodedHeight - NewOutput->Crop.Top - NewOutput->Crop.Bottom;

		NewOutput->PixelFormat = (int32) Decoder.CurrentUncompressedParams.compType;

		bool bIsPlanar = false;
		switch (Decoder.CurrentUncompressedParams.compType)
		{
			case	DNX_CT_UCHAR:
			{
				switch (Decoder.CurrentUncompressedParams.compOrder)
				{
					case	DNX_CCO_YCbYCr_NoA:
					case	DNX_CCO_CbYCrY_NoA:
					{
						NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::B8G8R8A8;
						NewOutput->BufferEncoding = (Decoder.CurrentUncompressedParams.compOrder  == DNX_CCO_CbYCrY_NoA) ?  EElectraDecoderPlatformPixelEncoding::CbY0CrY1 : EElectraDecoderPlatformPixelEncoding::Y0CbY1Cr;
						NewOutput->Width /= 2;
						NewOutput->Pitch *= 2;
						break;
					}
					case	DNX_CCO_ARGB_Interleaved:
					{
						NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::A8R8G8B8;
						NewOutput->BufferEncoding = EElectraDecoderPlatformPixelEncoding::Native;
						NewOutput->Pitch *= 4;
						break;
					}
					case	DNX_CCO_RGBA_Interleaved:
					{
						NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::R8G8B8A8;
						NewOutput->BufferEncoding = EElectraDecoderPlatformPixelEncoding::Native;
						NewOutput->Pitch *= 4;
						break;
					}
					case	DNX_CCO_CbYCrA_Interleaved:
					{
						NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::B8G8R8A8;
						NewOutput->BufferEncoding = EElectraDecoderPlatformPixelEncoding::YCbCr_Alpha;
						NewOutput->Pitch *= 4;
						break;
					}
					case	DNX_CCO_YCbCr_Planar:
					{
						NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::NV12;
						NewOutput->BufferEncoding = EElectraDecoderPlatformPixelEncoding::Native;
						if (!bUseGPUBuffers)
						{
							NewOutput->Height = (NewOutput->Height * 3) / 2;
						}
						bIsPlanar = true;
						break;
					}
					default:
					{
						NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::INVALID;
						NewOutput->BufferEncoding = EElectraDecoderPlatformPixelEncoding::Native;
					}
				}
				break;
			}
			case	DNX_CT_SHORT:
			case	DNX_CT_USHORT_10_6:
			case	DNX_CT_USHORT_12_4:
			{
				switch (Decoder.CurrentUncompressedParams.compOrder)
				{
					case	DNX_CCO_YCbYCr_NoA:
					case	DNX_CCO_CbYCrY_NoA:
					{
						NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::A16B16G16R16;
						NewOutput->BufferEncoding = (Decoder.CurrentUncompressedParams.compOrder == DNX_CCO_CbYCrY_NoA) ? EElectraDecoderPlatformPixelEncoding::CbY0CrY1 : EElectraDecoderPlatformPixelEncoding::Y0CbY1Cr;
						NewOutput->Width /= 2;
						NewOutput->Pitch *= 4;
						break;
					}
					case	DNX_CCO_RGBA_Interleaved:
					{
						NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::R16G16B16A16;
						NewOutput->BufferEncoding = EElectraDecoderPlatformPixelEncoding::Native;
						NewOutput->Pitch *= 8;
						break;
					}
					case	DNX_CCO_ABGR_Interleaved:
					{
						NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::A16B16G16R16;
						NewOutput->BufferEncoding = EElectraDecoderPlatformPixelEncoding::Native;
						NewOutput->Pitch *= 8;
						break;
					}
					case	DNX_CCO_CbYCrA_Interleaved:
					{
						NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::A16B16G16R16;
						NewOutput->BufferEncoding = EElectraDecoderPlatformPixelEncoding::YCbCr_Alpha;
						NewOutput->Pitch *= 8;
						break;
					}
					case	DNX_CCO_YCbCr_Planar:
					{
						NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::P010;
						NewOutput->BufferEncoding = EElectraDecoderPlatformPixelEncoding::Native;
						if (!bUseGPUBuffers)
						{
							NewOutput->Height = (NewOutput->Height * 3) / 2;
						}
						bIsPlanar = true;
						break;
					}
					default:
					{
						NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::INVALID;
						NewOutput->BufferEncoding = EElectraDecoderPlatformPixelEncoding::Native;
					}
				}
				break;
			}
			case	DNX_CT_V210:
			{
				NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::A2B10G10R10;
				NewOutput->BufferEncoding = EElectraDecoderPlatformPixelEncoding::CbY0CrY1;
				break;
			}
			default:
			{
				NewOutput->BufferFormat = EElectraDecoderPlatformPixelFormat::INVALID;
				NewOutput->BufferEncoding = EElectraDecoderPlatformPixelEncoding::Native;
			}
		}

		NewOutput->ExtraValues.Emplace(TEXT("codec"), FVariant(TEXT("avid")));
		NewOutput->ExtraValues.Emplace(TEXT("codec_4cc"), FVariant(Codec4CC));

		NewOutput->ExtraValues.Emplace(TEXT("avid_color_volume"), FVariant((int32) Decoder.CurrentUncompressedParams.colorVolume));

		uint32 BPP = NewOutput->Pitch / NewOutput->Width;

		uint8* OutBufferData;
		uint32 OutBufferSize;
		TArray<uint8> TempBuffer;

#if ELECTRA_MEDIAGPUBUFFER_DX12
		uint32 ResourcePitch = 0;
		if (bUseGPUBuffers)
		{
			TRefCountPtr D3D12Device(static_cast<ID3D12Device*>(PlatformDevice));

			uint32 AllocHeight = bIsPlanar ? (NewOutput->Height * 3 / 2) : NewOutput->Height;

			// Create the resource pool as needed...
			if (!D3D12ResourcePool || !D3D12ResourcePool->IsCompatibleAsBuffer(MaxOutputBuffers, NewOutput->Width, AllocHeight, BPP))
			{
				// Note: if we reconfigure the pool, we will end up with >1 heaps until all older users are destroyed - so streams with lots of changes will be quite resource hungry
				D3D12ResourcePool = MakeShared<FElectraMediaDecoderOutputBufferPool_DX12, ESPMode::ThreadSafe>(D3D12Device, MaxOutputBuffers, NewOutput->Width, AllocHeight, BPP);
			}

			// Create a tasksync instance so we can have our own async jobs run in consecutive order
			if (!TaskSync.IsValid())
			{
				TaskSync = PinnedResourceDelegate->CreateAsyncConsecutiveTaskSync();
			}

			// Request resource and fence...
			D3D12ResourcePool->AllocateOutputDataAsBuffer(NewOutput->GPUBuffer, ResourcePitch);

			NewOutput->Pitch = ResourcePitch;

			uint32 OutputPitch = BPP * NewOutput->Width;

			// Pitch compatible with DX12 resource?
			if (OutputPitch != ResourcePitch)
			{
				// No, decode to temp buffer...
				TempBuffer.AddUninitialized(OutputPitch * AllocHeight);

				OutBufferData = TempBuffer.GetData();
				OutBufferSize = TempBuffer.Num();

				check(OutBufferSize <= AllocHeight * ResourcePitch);
			}
			else
			{
				// Yes, decode directly into the DX12 resource...
				HRESULT Res = NewOutput->GPUBuffer.Resource->Map(0, nullptr, (void**)&OutBufferData);
				check(SUCCEEDED(Res));
				OutBufferSize = ResourcePitch * AllocHeight;
			}
		}
		else
#endif
		{
			NewOutput->Buffer = MakeShared<TArray<uint8>, ESPMode::ThreadSafe>();
			NewOutput->Buffer->AddUninitialized(BPP * NewOutput->Width * NewOutput->Height);

			OutBufferData = NewOutput->Buffer->GetData();
			OutBufferSize = NewOutput->Buffer->Num();
		}

		Result = Decoder.Decode(OutBufferData, OutBufferSize, InInputAccessUnit.Data, (unsigned int)InInputAccessUnit.DataSize);

#if ELECTRA_MEDIAGPUBUFFER_DX12
		if (bUseGPUBuffers)
		{
			if (Result == 0)
			{
				// If we have a temp buffer, we need to copy the data first...
				if (!TempBuffer.IsEmpty())
				{
					if (PinnedResourceDelegate->RunCodeAsync([BufferResource = NewOutput->GPUBuffer.Resource, Width = NewOutput->Width, Height = NewOutput->Height, ResourcePitch, BPP, TempBuffer, BufferFence = NewOutput->GPUBuffer.Fence, BufferFenceValue = NewOutput->GPUBuffer.FenceValue]()
						{
							CopyData(BufferResource, ResourcePitch, TempBuffer.GetData(), Width * BPP, Height);
							BufferFence->Signal(BufferFenceValue);
						}, TaskSync.Get()))
					{
						NewOutput->GPUBuffer_TaskSync = TaskSync;
					}
					else
					{
						// Async copy failed, do it synchronously...
						CopyData(NewOutput->GPUBuffer.Resource, ResourcePitch, TempBuffer.GetData(), NewOutput->Width * BPP, NewOutput->Height);
						NewOutput->GPUBuffer.Fence->Signal(NewOutput->GPUBuffer.FenceValue);
					}
				}
				else
				{
					// We decoded into the resource: Unmap the resource memory and signal that it's usable by the GPU
					NewOutput->GPUBuffer.Resource->Unmap(0, nullptr);
					NewOutput->GPUBuffer.Fence->Signal(NewOutput->GPUBuffer.FenceValue);
				}
			}
		}
#endif

		if (Result)
		{
			PostError(Result, TEXT("DNX_DecodeFrame() failed"), ERRCODE_INTERNAL_FAILED_TO_DECODE_INPUT);
			return IElectraDecoder::EDecoderError::Error;
		}

		CurrentOutput = MoveTemp(NewOutput);
	}
	return IElectraDecoder::EDecoderError::None;
}

IElectraDecoder::EDecoderError FVideoDecoderAvidDNxHDElectra::SendEndOfData()
{
	// If already in error do nothing!
	if (LastError.IsSet())
	{
		return IElectraDecoder::EDecoderError::Error;
	}
	// Already draining?
	if (bFlushPending)
	{
		return IElectraDecoder::EDecoderError::EndOfData;
	}
	bFlushPending = true;
	return IElectraDecoder::EDecoderError::None;
}

IElectraDecoder::EDecoderError FVideoDecoderAvidDNxHDElectra::Flush()
{
	// If already in error do nothing!
	if (LastError.IsSet())
	{
		return IElectraDecoder::EDecoderError::Error;
	}
	ResetToCleanStart();
	return IElectraDecoder::EDecoderError::None;
}

IElectraDecoder::EOutputStatus FVideoDecoderAvidDNxHDElectra::HaveOutput()
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
	// Pending flush?
	if (bFlushPending)
	{
		bFlushPending = false;
		return IElectraDecoder::EOutputStatus::EndOfData;
	}
	return IElectraDecoder::EOutputStatus::NeedInput;
}

TSharedPtr<IElectraDecoderOutput, ESPMode::ThreadSafe> FVideoDecoderAvidDNxHDElectra::GetOutput()
{
	TSharedPtr<IElectraDecoderOutput, ESPMode::ThreadSafe> Out = CurrentOutput;
	CurrentOutput.Reset();
	return Out;
}




int32 FVideoDecoderAvidDNxHDElectra::FDecoderHandle::PrepareForDecoding(const void* InInput, uint32 InInputSize, int32 InNumDecodeThreads, bool bIgnoreAlpha)
{
	DNX_CompressedParams_t CompressedParams;
	DNX_SignalStandard_t SignalStandard = DNX_SignalStandard_t::DNX_SS_INVALID;
	FMemory::Memzero(CompressedParams);
	CompressedParams.structSize = sizeof(CompressedParams);
	int Result = DNX_GetInfoFromCompressedFrame(InInput, InInputSize, &CompressedParams, &SignalStandard);
	if (Result != DNX_NO_ERROR)
	{
		UE_LOG(LogAvidDNxHDElectraDecoder, Log, TEXT("DNX_GetInfoFromCompressedFrame() failed with %d"), Result);
		return Result;
	}
	// If we have a decoder see if the format changed and reconfigure the decoder if so.
	if (Handle)
	{
		bool bChanged = SignalStandard != CurrentSignalStandard || FMemory::Memcmp(&CompressedParams, &CurrentCompressedParams, sizeof(CompressedParams)) != 0;
		if (bChanged)
		{
			CurrentCompressedParams = CompressedParams;
			CurrentSignalStandard = SignalStandard;
			bHasAlpha = !!CurrentCompressedParams.alphaPresence;
			DecodeOperationParams.decodeAlpha = bIgnoreAlpha ? 0 : (bHasAlpha ? 1 : 0);
			if (!SetupDecompressStruct())
			{
				return DNX_INVALID_UNCOMPINFO_ERROR;
			}
			Result = DNX_ConfigureDecoder(&CurrentCompressedParams, &CurrentUncompressedParams, &DecodeOperationParams, Handle);
			if (Result == DNX_NO_ERROR)
			{
				NumOutputBits = GetNumberOfOutputColorBitsPerPixel(CurrentUncompressedParams.compType);
			}
			else
			{
				DNX_DeleteDecoder(Handle);
				Handle = nullptr;
				NumOutputBits = 0;
				bHasAlpha = false;
			}
		}
	}
	// If we do not have a decoder, or the above reconfiguration failed, create a new one.
	if (!Handle)
	{
		CurrentCompressedParams = CompressedParams;
		CurrentSignalStandard = SignalStandard;
		bHasAlpha = !!CurrentCompressedParams.alphaPresence;

		FMemory::Memzero(DecodeOperationParams);
		DecodeOperationParams.structSize = sizeof(DecodeOperationParams);
		DecodeOperationParams.numThreads = InNumDecodeThreads;
		DecodeOperationParams.decodeResolution = DNX_DecodeResolution_t::DNX_DR_Full;
		DecodeOperationParams.verifyCRC = 0;
		DecodeOperationParams.decodeAlpha = bIgnoreAlpha ? 0 : (bHasAlpha ? 1 : 0);

		if (!SetupDecompressStruct())
		{
			return DNX_INVALID_UNCOMPINFO_ERROR;
		}
		Result = DNX_CreateDecoder(&CurrentCompressedParams, &CurrentUncompressedParams, &DecodeOperationParams, &Handle);
		if (Result != DNX_NO_ERROR)
		{
			UE_LOG(LogAvidDNxHDElectraDecoder, Log, TEXT("DNX_CreateDecoder() failed with %d"), Result);
			return Result;
		}

		NumOutputBits = GetNumberOfOutputColorBitsPerPixel(CurrentUncompressedParams.compType);
	}

	return Result;
}

bool FVideoDecoderAvidDNxHDElectra::FDecoderHandle::SetupDecompressStruct()
{
	FMemory::Memzero(CurrentUncompressedParams);
	CurrentUncompressedParams.structSize = sizeof(CurrentUncompressedParams);
	CurrentUncompressedParams.colorVolume = CurrentCompressedParams.colorVolume;
	CurrentUncompressedParams.colorFormat = CurrentCompressedParams.colorFormat;
	CurrentUncompressedParams.fieldOrder = DNX_BufferFieldOrder_t::DNX_BFO_Progressive;
	CurrentUncompressedParams.rgt = DNX_RasterGeometryType_t::DNX_RGT_Display;

	if (CurrentCompressedParams.colorFormat == DNX_ColorFormat_t::DNX_CF_YCbCr)
	{
		switch(CurrentCompressedParams.depth)
		{
			case 8:
			{
				CurrentUncompressedParams.compType = DNX_ComponentType_t::DNX_CT_UCHAR;
				// FIXME: What output format would the alpha channel require us to use?
				if (DecodeOperationParams.decodeAlpha)
				{
					UE_LOG(LogAvidDNxHDElectraDecoder, Log, TEXT("Compressed alpha with YCbYCr is not currently handled"));
					return false;
				}
				CurrentUncompressedParams.compOrder = DecodeOperationParams.decodeAlpha ? DNX_ColorComponentOrder_t::DNX_CCO_CbYCrY_A : DNX_ColorComponentOrder_t::DNX_CCO_YCbYCr_NoA;
				break;
			}
			default:
			{
				UE_LOG(LogAvidDNxHDElectraDecoder, Log, TEXT("Compressed %d bit depth is not currently handled"), CurrentCompressedParams.depth);
				return false;
			}
		}
	}
	else
	{
		UE_LOG(LogAvidDNxHDElectraDecoder, Log, TEXT("Compressed RGB format is not currently handled"));
		return false;
	}

	/*if (CurrentUncompressedParams.compType == DNX_ComponentType_t::DNX_CT_SHORT_2_14)
	{
		CurrentUncompressedParams.blackPoint = DNX_DEFAULT_SHORT_2_14_BLACK_POINT;
		CurrentUncompressedParams.whitePoint = DNX_DEFAULT_SHORT_2_14_WHITE_POINT;
		CurrentUncompressedParams.chromaExcursion = DNX_DEFAULT_CHROMA_SHORT_2_14_EXCURSION;
	}*/
	return true;
}

int32 FVideoDecoderAvidDNxHDElectra::FDecoderHandle::Decode(uint8* OutBufferData, uint32 OutBufferSize, const void* InInput, uint32 InInputSize)
{
	if (!Handle)
	{
		return DNX_NOT_INITIALIZED_ERROR;
	}
	int32 Result = DNX_DecodeFrame(Handle, InInput, OutBufferData, InInputSize, (unsigned int)OutBufferSize);
	return Result;
}


void FVideoDecoderAvidDNxHDElectra::FDecoderHandle::Reset()
{
	if (Handle)
	{
		DNX_DeleteDecoder(Handle);
		Handle = nullptr;
	}
	NumOutputBits = 0;
}

int32 FVideoDecoderAvidDNxHDElectra::FDecoderHandle::GetNumberOfOutputColorBitsPerPixel(DNX_ComponentType_t InOutputComponentType)
{
	switch (InOutputComponentType)
	{
		default:
		case DNX_ComponentType_t::DNX_CT_UCHAR:			// 8 bit
			return 8;
		case DNX_ComponentType_t::DNX_CT_USHORT_10_6:	// 10 bit
		case DNX_ComponentType_t::DNX_CT_10Bit_2_8:		// 10 bit in 2_8 format. Byte ordering is fixed. This is to be used with 10-bit 4:2:2 YCbCr components.
		case DNX_ComponentType_t::DNX_CT_V210:			// Apple's V210 
			return 10;
		case DNX_ComponentType_t::DNX_CT_SHORT_2_14:	// Fixed point 
		case DNX_ComponentType_t::DNX_CT_SHORT:			// 16 bit. Premultiplied by 257. Byte ordering is machine dependent.
			return 16;
		case DNX_ComponentType_t::DNX_CT_USHORT_12_4:	// 12 bit
			return 12;
	}
}

