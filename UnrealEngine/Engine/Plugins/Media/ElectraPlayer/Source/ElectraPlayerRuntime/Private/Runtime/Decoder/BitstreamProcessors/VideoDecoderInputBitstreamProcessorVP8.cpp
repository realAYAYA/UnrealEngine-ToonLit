// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoDecoderInputBitstreamProcessorVP8.h"

#include "Utilities/Utilities.h"
#include "Utils/Google/ElectraUtilsVPxVideo.h"
#include "StreamAccessUnitBuffer.h"
#include "Decoder/VideoDecoderHelpers.h"

#include "ElectraDecodersUtils.h"
#include "IElectraDecoderFeaturesAndOptions.h"
#include "IElectraDecoderOutputVideo.h"

namespace Electra
{


class FVideoDecoderInputBitstreamProcessorVP8 : public IVideoDecoderInputBitstreamProcessor
{
public:
	FVideoDecoderInputBitstreamProcessorVP8(const TMap<FString, FVariant>& InDecoderConfigOptions);
	virtual ~FVideoDecoderInputBitstreamProcessorVP8() = default;
	void Clear() override;
	EProcessResult ProcessAccessUnitForDecoding(FBitstreamInfo& OutBSI, FAccessUnit* InAccessUnit) override;
	void SetPropertiesOnOutput(TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, FParamDict* InOutProperties, const FBitstreamInfo& InFromBSI) override;

private:

	class FCodecSpecificMessages : public ICodecSpecificMessages
	{
	public:
		virtual ~FCodecSpecificMessages() = default;

		FStreamCodecInformation::FCodecVideoColorInfo ColorInfo;
		TArray<uint8> ITUt35;
	};

	MPEG::FColorimetryHelper Colorimetry;
	MPEG::FHDRHelper HDR;
};


TSharedPtr<IVideoDecoderInputBitstreamProcessor, ESPMode::ThreadSafe> IVideoDecoderInputBitstreamProcessorVP8::Create(const FString& InCodec, const TMap<FString, FVariant>& InDecoderConfigOptions)
{
	check(InCodec.StartsWith(TEXT("vp08")));
	return MakeShared<FVideoDecoderInputBitstreamProcessorVP8, ESPMode::ThreadSafe>(InDecoderConfigOptions);
}


FVideoDecoderInputBitstreamProcessorVP8::FVideoDecoderInputBitstreamProcessorVP8(const TMap<FString, FVariant>& InDecoderConfigOptions)
{
}

void FVideoDecoderInputBitstreamProcessorVP8::Clear()
{
	Colorimetry.Reset();
	HDR.Reset();
}

IVideoDecoderInputBitstreamProcessor::EProcessResult FVideoDecoderInputBitstreamProcessorVP8::ProcessAccessUnitForDecoding(FBitstreamInfo& OutBSI, FAccessUnit* InOutAccessUnit)
{
	IVideoDecoderInputBitstreamProcessor::EProcessResult Result = IVideoDecoderInputBitstreamProcessor::EProcessResult::None;
	if (!InOutAccessUnit)
	{
		return Result;
	}
	check(InOutAccessUnit->ESType == EStreamType::Video);

	if (InOutAccessUnit->AUCodecData.IsValid())
	{
		TSharedPtr<FCodecSpecificMessages> ColorInfo = StaticCastSharedPtr<FCodecSpecificMessages>(OutBSI.CodecSpecificMessages);
		if (!ColorInfo.IsValid())
		{
			ColorInfo = MakeShared<FCodecSpecificMessages>();
			OutBSI.CodecSpecificMessages = ColorInfo;
		}
		ColorInfo->ColorInfo = InOutAccessUnit->AUCodecData->ParsedInfo.GetCodecVideoColorInfo();
		if (InOutAccessUnit->DynamicSidebandData.IsValid())
		{
			const TArray<uint8>* itu35 = InOutAccessUnit->DynamicSidebandData->Find(Utils::Make4CC('i','t','3','5'));
			if (itu35)
			{
				ColorInfo->ITUt35 = *itu35;
			}
		}
	}

	// Parse the bitstream header. The sync sample indicator is not always correctly set by the packager.
	ElectraDecodersUtil::VPxVideo::FVP8UncompressedHeader Header;
	if (ElectraDecodersUtil::VPxVideo::ParseVP8UncompressedHeader(Header, InOutAccessUnit->AUData, InOutAccessUnit->AUSize))
	{
		OutBSI.bIsSyncFrame = Header.IsKeyframe();
	}
	else
	{
		OutBSI.bIsSyncFrame = InOutAccessUnit->bIsSyncSample;
	}
	OutBSI.bIsDiscardable = false;

	return Result;
}


void FVideoDecoderInputBitstreamProcessorVP8::SetPropertiesOnOutput(TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, FParamDict* InOutProperties, const FBitstreamInfo& InFromBSI)
{
	check(InFromBSI.CodecSpecificMessages.IsValid());
	if (!InOutProperties || !InFromBSI.CodecSpecificMessages.IsValid())
	{
		return;
	}
	TSharedPtr<FCodecSpecificMessages> ColorInfo = StaticCastSharedPtr<FCodecSpecificMessages>(InFromBSI.CodecSpecificMessages);
	Colorimetry.Update((uint8)ColorInfo->ColorInfo.ColourPrimaries.Get(2),
					   (uint8)ColorInfo->ColorInfo.TransferCharacteristics.Get(2),
					   (uint8)ColorInfo->ColorInfo.MatrixCoefficients.Get(2),
					   (uint8)ColorInfo->ColorInfo.VideoFullRangeFlag.Get(0),
					   (uint8)ColorInfo->ColorInfo.VideoFormat.Get(5));
	Colorimetry.UpdateParamDict(*InOutProperties);
	HDR.Update(InDecoderOutput->GetNumberOfBits(), Colorimetry, ColorInfo->ColorInfo.MDCV, ColorInfo->ColorInfo.CLLI);
	HDR.UpdateParamDict(*InOutProperties);
}


} // namespace Electra
