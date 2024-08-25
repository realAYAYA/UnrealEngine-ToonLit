// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoDecoderInputBitstreamProcessorH264.h"

#include "Utilities/Utilities.h"
#include "Utils/MPEG/ElectraUtilsMPEGVideo.h"
#include "StreamAccessUnitBuffer.h"
#include "Decoder/VideoDecoderHelpers.h"

#include "ElectraDecodersUtils.h"
#include "IElectraDecoderFeaturesAndOptions.h"
#include "IElectraDecoderOutputVideo.h"

namespace Electra
{


class FVideoDecoderInputBitstreamProcessorH264 : public IVideoDecoderInputBitstreamProcessor
{
public:
	FVideoDecoderInputBitstreamProcessorH264(const TMap<FString, FVariant>& InDecoderConfigOptions);
	virtual ~FVideoDecoderInputBitstreamProcessorH264() = default;
	void Clear() override;
	EProcessResult ProcessAccessUnitForDecoding(FBitstreamInfo& OutBSI, FAccessUnit* InAccessUnit) override;
	void SetPropertiesOnOutput(TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, FParamDict* InOutProperties, const FBitstreamInfo& InFromBSI) override;

private:
	class FCodecSpecificMessages : public ICodecSpecificMessages
	{
	public:
		virtual ~FCodecSpecificMessages() = default;
		TArray<ElectraDecodersUtil::MPEG::FISO14496_10_seq_parameter_set_data> SPSs;
	};

	bool bReplaceLengthWithStartcode = true;

	TSharedPtr<const FAccessUnit::CodecData, ESPMode::ThreadSafe> PreviousCodecData;
	TSharedPtr<const FAccessUnit::CodecData, ESPMode::ThreadSafe> CurrentCodecData;
	TArray<ElectraDecodersUtil::MPEG::FISO14496_10_seq_parameter_set_data> SPSs;

	MPEG::FColorimetryHelper Colorimetry;
};





TSharedPtr<IVideoDecoderInputBitstreamProcessor, ESPMode::ThreadSafe> IVideoDecoderInputBitstreamProcessorH264::Create(const FString& InCodec, const TMap<FString, FVariant>& InDecoderConfigOptions)
{
	check(InCodec.StartsWith(TEXT("avc")));
	return MakeShared<FVideoDecoderInputBitstreamProcessorH264, ESPMode::ThreadSafe>(InDecoderConfigOptions);
}


FVideoDecoderInputBitstreamProcessorH264::FVideoDecoderInputBitstreamProcessorH264(const TMap<FString, FVariant>& InDecoderConfigOptions)
{
	int32 S2L = (int32)ElectraDecodersUtil::GetVariantValueSafeI64(InDecoderConfigOptions, IElectraDecoderFeature::StartcodeToLength, -1);
	check(S2L == -1 || S2L == 0);

	bReplaceLengthWithStartcode = S2L == -1;
}

void FVideoDecoderInputBitstreamProcessorH264::Clear()
{
	PreviousCodecData.Reset();
	CurrentCodecData.Reset();
	SPSs.Empty();
	Colorimetry.Reset();
}

IVideoDecoderInputBitstreamProcessor::EProcessResult FVideoDecoderInputBitstreamProcessorH264::ProcessAccessUnitForDecoding(FBitstreamInfo& OutBSI, FAccessUnit* InOutAccessUnit)
{
	IVideoDecoderInputBitstreamProcessor::EProcessResult Result = IVideoDecoderInputBitstreamProcessor::EProcessResult::None;
	if (!InOutAccessUnit)
	{
		return Result;
	}
	check(InOutAccessUnit->ESType == EStreamType::Video);

	//
	// Extract sequence parameter sets from the codec specific data.
	//
	if (InOutAccessUnit->AUCodecData.IsValid() && InOutAccessUnit->AUCodecData.Get() != CurrentCodecData.Get())
	{
		// Pointers are different. Is the content too?
		bool bDifferent = !CurrentCodecData.IsValid() || (CurrentCodecData.IsValid() && InOutAccessUnit->AUCodecData->CodecSpecificData != CurrentCodecData->CodecSpecificData);
		if (bDifferent)
		{
			SPSs.Empty();
			TArray<ElectraDecodersUtil::MPEG::FNaluInfo> NALUs;
			const uint8* pD = InOutAccessUnit->AUCodecData->CodecSpecificData.GetData();
			ElectraDecodersUtil::MPEG::ParseBitstreamForNALUs(NALUs, pD, InOutAccessUnit->AUCodecData->CodecSpecificData.Num());
			for(int32 i=0; i<NALUs.Num(); ++i)
			{
				const uint8* NALU = (const uint8*)Electra::AdvancePointer(pD, NALUs[i].Offset + NALUs[i].UnitLength);
				uint8 nal_unit_type = *NALU & 0x1f;
				// SPS?
				if (nal_unit_type == 7)
				{
					ElectraDecodersUtil::MPEG::FISO14496_10_seq_parameter_set_data sps;
					if (ElectraDecodersUtil::MPEG::ParseH264SPS(sps, NALU, NALUs[i].Size))
					{
						SPSs.Emplace(MoveTemp(sps));
					}
				}
			}

			// Create the message structure if it does not exist yet.
			if (!OutBSI.CodecSpecificMessages.IsValid())
			{
				OutBSI.CodecSpecificMessages = MakeShared<FCodecSpecificMessages>();
			}
			// Set the new SPS in the message structure.
			StaticCastSharedPtr<FCodecSpecificMessages>(OutBSI.CodecSpecificMessages)->SPSs = SPSs;

			Result = IVideoDecoderInputBitstreamProcessor::EProcessResult::CSDChanged;
		}
		PreviousCodecData = CurrentCodecData.IsValid() ? CurrentCodecData : InOutAccessUnit->AUCodecData;
		CurrentCodecData = InOutAccessUnit->AUCodecData;
	}

	// NOTE: In a second phase we should probably scan the input access unit for inband SPS if the codec is avc3.


	// Now go over the NALUs in the access unit and see what is there.
	OutBSI.bIsDiscardable = true;
	OutBSI.bIsSyncFrame = InOutAccessUnit->bIsSyncSample;
	uint32* NALU = (uint32*)InOutAccessUnit->AUData;
	uint32* LastNALU = (uint32*)Electra::AdvancePointer(NALU, InOutAccessUnit->AUSize);
	while(NALU < LastNALU)
	{
		// Check the nal_ref_idc in the NAL unit for dependencies.
		uint8 nal = *(const uint8*)(NALU + 1);
		check((nal & 0x80) == 0);
		if ((nal >> 5) != 0)
		{
			OutBSI.bIsDiscardable = false;
		}
		// IDR frame?
		if ((nal & 0x1f) == 5)
		{
			OutBSI.bIsSyncFrame = true;
		}
		// SEI message(s)?
		if ((nal & 0x1f) == 6)
		{
			// NOTE: We could get closed captions here.
		}

		uint32 naluLen = MEDIA_FROM_BIG_ENDIAN(*NALU) + 4;
		if (bReplaceLengthWithStartcode)
		{
			*NALU = MEDIA_TO_BIG_ENDIAN(0x00000001U);
		}
		NALU = Electra::AdvancePointer(NALU, naluLen);
	}

	return Result;
}


void FVideoDecoderInputBitstreamProcessorH264::SetPropertiesOnOutput(TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, FParamDict* InOutProperties, const FBitstreamInfo& InFromBSI)
{
	if (!InOutProperties)
	{
		return;
	}

	TSharedPtr<FCodecSpecificMessages> Msg = StaticCastSharedPtr<FCodecSpecificMessages>(InFromBSI.CodecSpecificMessages);

	const TArray<ElectraDecodersUtil::MPEG::FISO14496_10_seq_parameter_set_data>& SPSList = Msg.IsValid() ? Msg->SPSs : SPSs;

	// We only interact with the first SPS.
	if (SPSList.Num() > 0)
	{
		// Set the bit depth and the colorimetry.
		uint8 colour_primaries=2, transfer_characteristics=2, matrix_coeffs=2;
		uint8 video_full_range_flag=0, video_format=5;
		if (SPSList[0].colour_description_present_flag)
		{
			colour_primaries = SPSList[0].colour_primaries;
			transfer_characteristics = SPSList[0].transfer_characteristics;
			matrix_coeffs = SPSList[0].matrix_coefficients;
		}
		if (SPSList[0].video_signal_type_present_flag)
		{
			video_full_range_flag = SPSList[0].video_full_range_flag;
			video_format = SPSList[0].video_format;
		}

		Colorimetry.Update(colour_primaries, transfer_characteristics, matrix_coeffs, video_full_range_flag, video_format);
		Colorimetry.UpdateParamDict(*InOutProperties);
	}
}


} // namespace Electra
