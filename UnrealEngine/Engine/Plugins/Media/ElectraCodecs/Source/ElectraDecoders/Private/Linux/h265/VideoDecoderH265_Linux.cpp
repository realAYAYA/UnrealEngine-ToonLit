// Copyright Epic Games, Inc. All Rights Reserved.

#include "h265/VideoDecoderH265_Linux.h"
#include "DecoderErrors_Linux.h"
#include "ElectraDecodersUtils.h"
#include "Utils/MPEG/ElectraUtilsMPEGVideo.h"

#include "IElectraDecoderFeaturesAndOptions.h"
#include "IElectraDecoderOutputVideo.h"

#include "IElectraDecoderResourceDelegate.h"
#include "ElectraDecodersModule.h"

/*********************************************************************************************************************/

#include "libav_Decoder_Common.h"
#include "libav_Decoder_H265.h"

/*********************************************************************************************************************/

class FElectraVideoDecoderH265_Linux;


class FElectraDecoderDefaultVideoOutputFormatH265_Linux : public IElectraDecoderDefaultVideoOutputFormat
{
public:
	virtual ~FElectraDecoderDefaultVideoOutputFormatH265_Linux()
	{ }

};


class FElectraVideoDecoderOutputH265_Linux : public IElectraDecoderVideoOutput
{
public:
	virtual ~FElectraVideoDecoderOutputH265_Linux()
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
	{
		if (InTypeOfHandle == EElectraDecoderPlatformOutputHandleType::LibavDecoderDecodedImage)
		{
			return DecodedImage.Get();
		}
		return nullptr;
	}
	IElectraDecoderVideoOutputTransferHandle* GetTransferHandle() const override
	{ return nullptr; }
	IElectraDecoderVideoOutput::EImageCopyResult CopyPlatformImage(IElectraDecoderVideoOutputCopyResources* InCopyResources) const override
	{ return IElectraDecoderVideoOutput::EImageCopyResult::NotSupported; }

public:
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

	TSharedPtr<ILibavDecoderDecodedImage, ESPMode::ThreadSafe> DecodedImage;
};



class FElectraVideoDecoderH265_Linux : public IElectraVideoDecoderH265_Linux, public ILibavDecoderVideoResourceAllocator
{
public:
	FElectraVideoDecoderH265_Linux(const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate);

	virtual ~FElectraVideoDecoderH265_Linux();

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

private:
	// Add methods from ILibavDecoderVideoResourceAllocator here
	// ...

	struct FDecoderInput
	{
		FInputAccessUnit AccessUnit;
		TMap<FString, FVariant> AdditionalOptions;
		TSharedPtr<ElectraDecodersUtil::MPEG::FISO23008_2_seq_parameter_set_data, ESPMode::ThreadSafe> SPS;
		bool bDropOutput = false;
	};

	enum class EDecodeState
	{
		Decoding,
		Draining
	};

	bool PostError(int32_t ApiReturnValue, FString Message, int32 Code);

	TSharedPtr<ElectraDecodersUtil::MPEG::FHEVCDecoderConfigurationRecord, ESPMode::ThreadSafe> GetDCRFromOptions(const TMap<FString, FVariant>& InOptions);
	TSharedPtr<ElectraDecodersUtil::MPEG::FISO23008_2_seq_parameter_set_data, ESPMode::ThreadSafe> GetSPSFromOptions(const TMap<FString, FVariant>& InOptions);

	bool InternalDecoderCreate(const TMap<FString, FVariant>& InAdditionalOptions);
	void InternalDecoderDestroy();

	enum class EConvertResult
	{
		Success,
		Failure
	};
	EConvertResult ConvertDecoderOutput(const ILibavDecoderVideoCommon::FOutputInfo& OutInfo);

	TMap<FString, FVariant> InitialCreationOptions;
	TWeakPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> ResourceDelegate;

	IElectraDecoder::FError LastError;

	TArray<TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe>> InDecoderInput;
	TSharedPtr<FElectraVideoDecoderOutputH265_Linux, ESPMode::ThreadSafe> CurrentOutput;
	TSharedPtr<ElectraDecodersUtil::MPEG::FISO23008_2_seq_parameter_set_data, ESPMode::ThreadSafe> CurrentSPS;

	TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> DecoderInstance;
	EDecodeState DecodeState = EDecodeState::Decoding;
	bool bNewDecoderRequired = false;
};


namespace IElectraVideoDecoderH265_Linux_Platform
{
	static TArray<IElectraVideoDecoderH265_Linux::FSupportedConfiguration> DecoderConfigurations;
	static bool bDecoderConfigurationsDirty = true;
}


void IElectraVideoDecoderH265_Linux::PlatformGetSupportedConfigurations(TArray<FSupportedConfiguration>& OutSupportedConfigurations)
{
	if (IElectraVideoDecoderH265_Linux_Platform::bDecoderConfigurationsDirty)
	{
		IElectraVideoDecoderH265_Linux_Platform::DecoderConfigurations.Empty();

		// Perhaps these can be determined dynamically?

		// Main
		IElectraVideoDecoderH265_Linux_Platform::DecoderConfigurations.Emplace(IElectraVideoDecoderH265_Linux::FSupportedConfiguration(0, 1, 0, 153, 60, 4096, 2304, 0));
		IElectraVideoDecoderH265_Linux_Platform::DecoderConfigurations.Emplace(IElectraVideoDecoderH265_Linux::FSupportedConfiguration(0, 1, 0, 153, 0, 0, 0, (3840 / 8) * (2160 / 8) * 60));
		// Main10
		IElectraVideoDecoderH265_Linux_Platform::DecoderConfigurations.Emplace(IElectraVideoDecoderH265_Linux::FSupportedConfiguration(0, 2, 0, 153, 60, 4096, 2304, 0));
		IElectraVideoDecoderH265_Linux_Platform::DecoderConfigurations.Emplace(IElectraVideoDecoderH265_Linux::FSupportedConfiguration(0, 2, 0, 153, 0, 0, 0, (3840 / 8) * (2160 / 8) * 60));

		IElectraVideoDecoderH265_Linux_Platform::bDecoderConfigurationsDirty = false;
	}
	OutSupportedConfigurations = IElectraVideoDecoderH265_Linux_Platform::DecoderConfigurations;
}

void IElectraVideoDecoderH265_Linux::GetConfigurationOptions(TMap<FString, FVariant>& OutOptions)
{
	OutOptions.Emplace(IElectraDecoderFeature::MinimumNumberOfOutputFrames, FVariant((int32)8));
	// The decoder is adaptive. There is no need to call IsCompatibleWith() on stream switches.
	OutOptions.Emplace(IElectraDecoderFeature::IsAdaptive, FVariant(true));
	OutOptions.Emplace(IElectraDecoderFeature::SupportsDroppingOutput, FVariant(true));
}

TSharedPtr<IElectraDecoder, ESPMode::ThreadSafe> IElectraVideoDecoderH265_Linux::Create(const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate)
{
	if (InResourceDelegate.IsValid())
	{
		TSharedPtr<FElectraVideoDecoderH265_Linux, ESPMode::ThreadSafe> New = MakeShared<FElectraVideoDecoderH265_Linux>(InOptions, InResourceDelegate);
		return New;
	}
	return nullptr;
}

FElectraVideoDecoderH265_Linux::FElectraVideoDecoderH265_Linux(const TMap<FString, FVariant>& InOptions, TSharedPtr<IElectraDecoderResourceDelegate, ESPMode::ThreadSafe> InResourceDelegate)
{
	InitialCreationOptions = InOptions;
	ResourceDelegate = InResourceDelegate;
}

FElectraVideoDecoderH265_Linux::~FElectraVideoDecoderH265_Linux()
{
	// Close() must have been called already!
	check(LastError.Code == ERRCODE_INTERNAL_ALREADY_CLOSED);
	// We do it nonetheless...
	Close();
}

TSharedPtr<ElectraDecodersUtil::MPEG::FHEVCDecoderConfigurationRecord, ESPMode::ThreadSafe> FElectraVideoDecoderH265_Linux::GetDCRFromOptions(const TMap<FString, FVariant>& InOptions)
{
	TArray<uint8> SidebandData = ElectraDecodersUtil::GetVariantValueUInt8Array(InOptions, TEXT("dcr"));
	if (SidebandData.Num())
	{
		TSharedPtr<ElectraDecodersUtil::MPEG::FHEVCDecoderConfigurationRecord, ESPMode::ThreadSafe> NewConfigRecord;
		NewConfigRecord = MakeShared<ElectraDecodersUtil::MPEG::FHEVCDecoderConfigurationRecord, ESPMode::ThreadSafe>();
		NewConfigRecord->SetRawData(SidebandData.GetData(), SidebandData.Num());
		if (NewConfigRecord->Parse())
		{
			return NewConfigRecord;
		}
		else
		{
			LastError.Code = ERRCODE_INTERNAL_FAILED_TO_PARSE_CSD;
			LastError.Message = TEXT("Failed to parse codec specific data");
		}
	}
	return nullptr;
}

TSharedPtr<ElectraDecodersUtil::MPEG::FISO23008_2_seq_parameter_set_data, ESPMode::ThreadSafe> FElectraVideoDecoderH265_Linux::GetSPSFromOptions(const TMap<FString, FVariant>& InOptions)
{
	TArray<uint8> SidebandData = ElectraDecodersUtil::GetVariantValueUInt8Array(InOptions, TEXT("csd"));
	if (SidebandData.Num())
	{
		TArray<ElectraDecodersUtil::MPEG::FNaluInfo> NALUs;
		ElectraDecodersUtil::MPEG::ParseBitstreamForNALUs(NALUs, SidebandData.GetData(), SidebandData.Num());
		for(int32 i=0; i<NALUs.Num(); ++i)
		{
			uint8 nut = NALUs[i].Type >> 1;
			if (nut == 33)
			{
				TSharedPtr<ElectraDecodersUtil::MPEG::FISO23008_2_seq_parameter_set_data, ESPMode::ThreadSafe> NewSPS = MakeShared<ElectraDecodersUtil::MPEG::FISO23008_2_seq_parameter_set_data, ESPMode::ThreadSafe>();
				if (ElectraDecodersUtil::MPEG::ParseH265SPS(*NewSPS, ElectraDecodersUtil::AdvancePointer(SidebandData.GetData(), NALUs[i].Offset + NALUs[i].UnitLength), NALUs[i].Size))
				{
					return NewSPS;
				}
				else
				{
					break;
				}
			}
		}
		PostError(0, TEXT("Failed to parse codec specific data"), ERRCODE_INTERNAL_FAILED_TO_PARSE_CSD);
	}
	return nullptr;
}

void FElectraVideoDecoderH265_Linux::GetFeatures(TMap<FString, FVariant>& OutFeatures) const
{
	GetConfigurationOptions(OutFeatures);
}

IElectraDecoder::FError FElectraVideoDecoderH265_Linux::GetError() const
{
	return LastError;
}

void FElectraVideoDecoderH265_Linux::Close()
{
	ResetToCleanStart();
	// Set the error state that all subsequent calls will fail.
	PostError(0, TEXT("Already closed"), ERRCODE_INTERNAL_ALREADY_CLOSED);
}

bool FElectraVideoDecoderH265_Linux::ResetToCleanStart()
{
	InternalDecoderDestroy();

	InDecoderInput.Empty();
	CurrentOutput.Reset();
	CurrentSPS.Reset();
	bNewDecoderRequired = false;
	DecodeState = EDecodeState::Decoding;
	return !LastError.IsSet();
}

TSharedPtr<IElectraDecoderDefaultOutputFormat, ESPMode::ThreadSafe> FElectraVideoDecoderH265_Linux::GetDefaultOutputFormatFromCSD(const TMap<FString, FVariant>& CSDAndAdditionalOptions)
{
	return nullptr;
}

IElectraDecoder::ECSDCompatibility FElectraVideoDecoderH265_Linux::IsCompatibleWith(const TMap<FString, FVariant>& CSDAndAdditionalOptions)
{
	// NOTE: Since we set IElectraDecoderFeature::IsAdaptive to be true this method here will not be invoked on stream switches.

	// When we have no decoder yet then we are compatible because we will be creating a decoder when needed.
	if (!DecoderInstance.IsValid())
	{
		return IElectraDecoder::ECSDCompatibility::Compatible;
	}

	TSharedPtr<ElectraDecodersUtil::MPEG::FISO23008_2_seq_parameter_set_data, ESPMode::ThreadSafe> NewSPS = GetSPSFromOptions(CSDAndAdditionalOptions);
	if (!NewSPS.IsValid())
	{
		UE_LOG(LogElectraDecoders, Warning, TEXT("No CSD provided to IsCompatibleWith(), returning to drain and reset."));
		return IElectraDecoder::ECSDCompatibility::DrainAndReset;
	}

	// The decoder is adaptive, so it is compatible.
	return IElectraDecoder::ECSDCompatibility::Compatible;
}

IElectraDecoder::EDecoderError FElectraVideoDecoderH265_Linux::DecodeAccessUnit(const FInputAccessUnit& InInputAccessUnit, const TMap<FString, FVariant>& InAdditionalOptions)
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

	// If there is pending output it is very likely that decoding this access unit would also generate output.
	// Since that would result in loss of the pending output we return now.
	if (CurrentOutput.IsValid())
	{
		return IElectraDecoder::EDecoderError::NoBuffer;
	}

	// If a new decoder is needed, destroy the current one.
	if (bNewDecoderRequired)
	{
		InternalDecoderDestroy();
	}

	// Create decoder transform if necessary.
	if (!DecoderInstance.IsValid() && !InternalDecoderCreate(InAdditionalOptions))
	{
		return IElectraDecoder::EDecoderError::Error;
	}

	// Decode the data if given.
	if (InInputAccessUnit.Data && InInputAccessUnit.DataSize)
	{
		uint8* Temp = nullptr;
		ILibavDecoderVideoCommon::FInputAU DecAU;
		// If this is a sync sample we prepend the CSD. While this is not necessary on a running stream we need to have the CSD
		// on the first frame and it is easier to prepend it to all IDR frames when seeking etc.
		DecAU.DataSize = InInputAccessUnit.DataSize;
		if ((InInputAccessUnit.Flags & EElectraDecoderFlags::IsSyncSample) != EElectraDecoderFlags::None)
		{
			CurrentSPS = GetSPSFromOptions(InAdditionalOptions);
			if (!CurrentSPS.IsValid())
			{
				PostError(0, TEXT("Failed to parse video SPS"), ERRCODE_INTERNAL_FAILED_TO_DECODE_INPUT);
				return IElectraDecoder::EDecoderError::Error;
			}

			TArray<uint8> CSD = ElectraDecodersUtil::GetVariantValueUInt8Array(InAdditionalOptions, TEXT("csd"));
			Temp = (uint8*)FMemory::Malloc(DecAU.DataSize + CSD.Num());
			FMemory::Memcpy(Temp, CSD.GetData(), CSD.Num());
			FMemory::Memcpy(Temp + CSD.Num(), InInputAccessUnit.Data, InInputAccessUnit.DataSize);
			DecAU.DataSize += CSD.Num();
			DecAU.Data = Temp;
		}
		else
		{
			DecAU.Data = InInputAccessUnit.Data;
		}

		DecAU.DTS = InInputAccessUnit.DTS.GetTicks();
		DecAU.UserValue = DecAU.PTS = InInputAccessUnit.PTS.GetTicks();
		DecAU.Duration = InInputAccessUnit.Duration.GetTicks();
		DecAU.Flags = 0;
		ILibavDecoder::EDecoderError Result = DecoderInstance->DecodeAccessUnit(DecAU);
		FMemory::Free(Temp);
		if (Result == ILibavDecoder::EDecoderError::None)
		{
			// Add to the list of inputs passed to the decoder.
			TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe> In(new FDecoderInput);
			In->AdditionalOptions = InAdditionalOptions;
			In->AccessUnit = InInputAccessUnit;
			In->bDropOutput = (InInputAccessUnit.Flags & EElectraDecoderFlags::DoNotOutput) == EElectraDecoderFlags::DoNotOutput;
			In->SPS = CurrentSPS;
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
			PostError(DecoderInstance->GetLastLibraryError(), TEXT("Failed to decode video frame"), ERRCODE_INTERNAL_FAILED_TO_DECODE_INPUT);
			return IElectraDecoder::EDecoderError::Error;
		}
	}
	return IElectraDecoder::EDecoderError::None;
}

IElectraDecoder::EDecoderError FElectraVideoDecoderH265_Linux::SendEndOfData()
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
	if (DecoderInstance.IsValid())
	{
		DecoderInstance->SendEndOfData();
		DecodeState = EDecodeState::Draining;
	}
	return IElectraDecoder::EDecoderError::None;
}

IElectraDecoder::EDecoderError FElectraVideoDecoderH265_Linux::Flush()
{
	// If already in error do nothing!
	if (LastError.IsSet())
	{
		return IElectraDecoder::EDecoderError::Error;
	}
	if (DecoderInstance.IsValid())
	{
		InternalDecoderDestroy();
	}
	InDecoderInput.Empty();
	CurrentOutput.Reset();
	CurrentSPS.Reset();
	bNewDecoderRequired = false;
	DecodeState = EDecodeState::Decoding;
	return IElectraDecoder::EDecoderError::None;
}

IElectraDecoder::EOutputStatus FElectraVideoDecoderH265_Linux::HaveOutput()
{
	// If already in error do nothing!
	if (LastError.IsSet())
	{
		return IElectraDecoder::EOutputStatus::Error;
	}

	if (CurrentOutput.IsValid())
	{
		return IElectraDecoder::EOutputStatus::Available;
	}

	if (bNewDecoderRequired || !DecoderInstance.IsValid())
	{
		return IElectraDecoder::EOutputStatus::NeedInput;
	}

	ILibavDecoderVideoCommon::FOutputInfo Out;
	switch(DecoderInstance->HaveOutput(Out))
	{
		case ILibavDecoder::EOutputStatus::Available:
		{
			return ConvertDecoderOutput(Out) == EConvertResult::Success ? IElectraDecoder::EOutputStatus::Available : IElectraDecoder::EOutputStatus::Error;
		}
		case ILibavDecoder::EOutputStatus::EndOfData:
		{
			DecodeState = EDecodeState::Decoding;
			InDecoderInput.Empty();
			bNewDecoderRequired = true;
			return IElectraDecoder::EOutputStatus::EndOfData;
		}
		default:
		case ILibavDecoder::EOutputStatus::NeedInput:
		{
			return IElectraDecoder::EOutputStatus::NeedInput;
		}
	}
	return IElectraDecoder::EOutputStatus::NeedInput;
}

TSharedPtr<IElectraDecoderOutput, ESPMode::ThreadSafe> FElectraVideoDecoderH265_Linux::GetOutput()
{
	TSharedPtr<IElectraDecoderOutput, ESPMode::ThreadSafe> Out = CurrentOutput;
	CurrentOutput.Reset();
	return Out;
}

void FElectraVideoDecoderH265_Linux::Suspend()
{
}

void FElectraVideoDecoderH265_Linux::Resume()
{
}

bool FElectraVideoDecoderH265_Linux::PostError(int32_t ApiReturnValue, FString Message, int32 Code)
{
	LastError.Code = Code;
	LastError.SdkCode = ApiReturnValue;
	LastError.Message = MoveTemp(Message);
	return false;
}

bool FElectraVideoDecoderH265_Linux::InternalDecoderCreate(const TMap<FString, FVariant>& InAdditionalOptions)
{
	CurrentSPS.Reset();
	CurrentOutput.Reset();
	bNewDecoderRequired = false;

	if (!ILibavDecoderH265::IsAvailable())
	{
		return PostError(0, "libavcodec does not support this video format", ERRCODE_INTERNAL_DECODER_NOT_SUPPORTED);
	}
	TMap<FString, FVariant> Options;
	Options.Add(TEXT("hw_priority"), FVariant(FString("cuda;vdpau;vaapi")));
	DecoderInstance = ILibavDecoderH265::Create(this, Options);
	int32 ErrorCode = 0;
	if (DecoderInstance.IsValid() && ((ErrorCode = DecoderInstance->GetLastLibraryError())==0))
	{
		return true;
	}
	InternalDecoderDestroy();
	PostError(ErrorCode, "libavcodec failed to open video decoder", ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
	return false;
}

void FElectraVideoDecoderH265_Linux::InternalDecoderDestroy()
{
	if (DecoderInstance.IsValid())
	{
		DecoderInstance->Reset();
		DecoderInstance.Reset();
	}
}

FElectraVideoDecoderH265_Linux::EConvertResult FElectraVideoDecoderH265_Linux::ConvertDecoderOutput(const ILibavDecoderVideoCommon::FOutputInfo& OutInfo)
{
	if (!InDecoderInput.Num())
	{
		PostError(0, TEXT("There is no pending decoder input for the decoded output!"), ERRCODE_INTERNAL_FAILED_TO_CONVERT_OUTPUT_SAMPLE);
		return EConvertResult::Failure;
	}

	// Find the input corresponding to this output.
	TSharedPtr<FDecoderInput, ESPMode::NotThreadSafe> In;
	for(int32 nInDec=0; nInDec<InDecoderInput.Num(); ++nInDec)
	{
		if (InDecoderInput[nInDec]->AccessUnit.PTS.GetTicks() == OutInfo.UserValue)
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
	if (!In->SPS.IsValid())
	{
		PostError(0, TEXT("There is no SPS associated with the decoded output!"), ERRCODE_INTERNAL_FAILED_TO_CONVERT_OUTPUT_SAMPLE);
		return EConvertResult::Failure;
	}

	TSharedPtr<FElectraVideoDecoderOutputH265_Linux, ESPMode::ThreadSafe> NewOutput = MakeShared<FElectraVideoDecoderOutputH265_Linux>();
	NewOutput->PTS = In->AccessUnit.PTS;
	NewOutput->UserValue = In->AccessUnit.UserValue;
	NewOutput->DecodedImage = DecoderInstance->GetOutput();

	NewOutput->Width = In->SPS->GetWidth();
	NewOutput->Height = In->SPS->GetHeight();
	NewOutput->Pitch = NewOutput->Width;
	NewOutput->NumBits = 8 + In->SPS->bit_depth_luma_minus8;
	NewOutput->PixelFormat = 0;
	In->SPS->GetCrop(NewOutput->Crop.Left, NewOutput->Crop.Right, NewOutput->Crop.Top, NewOutput->Crop.Bottom);
	In->SPS->GetAspect(NewOutput->AspectW, NewOutput->AspectH);
	if (NewOutput->AspectW == 0 || NewOutput->AspectH == 0)
	{
		NewOutput->AspectW = 1;
		NewOutput->AspectH = 1;
	}
	if (In->SPS->vui_parameters_present_flag && In->SPS->vui_timing_info_present_flag && In->SPS->vui_time_scale)
	{
		NewOutput->FrameRateN = In->SPS->vui_time_scale;
		NewOutput->FrameRateD = In->SPS->vui_num_units_in_tick;
	}

	NewOutput->ExtraValues.Emplace(TEXT("platform"), FVariant(TEXT("linux")));
	NewOutput->ExtraValues.Emplace(TEXT("decoder"), FVariant(TEXT("libavcodec")));
	NewOutput->ExtraValues.Emplace(TEXT("codec"), FVariant(TEXT("hevc")));

	// Take this output on if it is not to be dropped.
	// We need to have pulled it out of libav before deciding this though.
	if (!In->bDropOutput)
	{
		CurrentOutput = MoveTemp(NewOutput);
	}
	return EConvertResult::Success;
}
