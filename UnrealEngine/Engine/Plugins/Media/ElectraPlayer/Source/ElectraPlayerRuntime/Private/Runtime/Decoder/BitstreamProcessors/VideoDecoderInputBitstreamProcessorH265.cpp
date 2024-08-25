// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoDecoderInputBitstreamProcessorH265.h"

#include "Utilities/Utilities.h"
#include "Utils/MPEG/ElectraUtilsMPEGVideo.h"
#include "StreamAccessUnitBuffer.h"
#include "Decoder/VideoDecoderHelpers.h"

#include "ElectraDecodersUtils.h"
#include "IElectraDecoderFeaturesAndOptions.h"
#include "IElectraDecoderOutputVideo.h"

namespace Electra
{


class FVideoDecoderInputBitstreamProcessorH265 : public IVideoDecoderInputBitstreamProcessor
{
public:
	FVideoDecoderInputBitstreamProcessorH265(const TMap<FString, FVariant>& InDecoderConfigOptions);
	virtual ~FVideoDecoderInputBitstreamProcessorH265() = default;
	void Clear() override;
	EProcessResult ProcessAccessUnitForDecoding(FBitstreamInfo& OutBSI, FAccessUnit* InAccessUnit) override;
	void SetPropertiesOnOutput(TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, FParamDict* InOutProperties, const FBitstreamInfo& InFromBSI) override;

private:
	class FCodecSpecificMessages : public ICodecSpecificMessages
	{
	public:
		virtual ~FCodecSpecificMessages() = default;
		TArray<ElectraDecodersUtil::MPEG::FISO23008_2_seq_parameter_set_data> SPSs;
		TArray<ElectraDecodersUtil::MPEG::FSEIMessage> CSDPrefixSEIMessages;
		TArray<ElectraDecodersUtil::MPEG::FSEIMessage> CSDSuffixSEIMessages;
		TArray<ElectraDecodersUtil::MPEG::FSEIMessage> PrefixSEIMessages;
		TArray<ElectraDecodersUtil::MPEG::FSEIMessage> SuffixSEIMessages;
	};

	bool bReplaceLengthWithStartcode = true;

	TSharedPtr<const FAccessUnit::CodecData, ESPMode::ThreadSafe> PreviousCodecData;
	TSharedPtr<const FAccessUnit::CodecData, ESPMode::ThreadSafe> CurrentCodecData;
	TArray<ElectraDecodersUtil::MPEG::FISO23008_2_seq_parameter_set_data> SPSs;
	TArray<ElectraDecodersUtil::MPEG::FSEIMessage> PrefixSEIMessages;
	TArray<ElectraDecodersUtil::MPEG::FSEIMessage> SuffixSEIMessages;

	MPEG::FColorimetryHelper Colorimetry;
	MPEG::FHDRHelper HDR;
};





TSharedPtr<IVideoDecoderInputBitstreamProcessor, ESPMode::ThreadSafe> IVideoDecoderInputBitstreamProcessorH265::Create(const FString& InCodec, const TMap<FString, FVariant>& InDecoderConfigOptions)
{
	check(InCodec.StartsWith(TEXT("hvc1")) || InCodec.StartsWith(TEXT("hev1")));
	return MakeShared<FVideoDecoderInputBitstreamProcessorH265, ESPMode::ThreadSafe>(InDecoderConfigOptions);
}


FVideoDecoderInputBitstreamProcessorH265::FVideoDecoderInputBitstreamProcessorH265(const TMap<FString, FVariant>& InDecoderConfigOptions)
{
	int32 S2L = (int32)ElectraDecodersUtil::GetVariantValueSafeI64(InDecoderConfigOptions, IElectraDecoderFeature::StartcodeToLength, -1);
	check(S2L == -1 || S2L == 0);

	bReplaceLengthWithStartcode = S2L == -1;
}

void FVideoDecoderInputBitstreamProcessorH265::Clear()
{
	PreviousCodecData.Reset();
	CurrentCodecData.Reset();
	SPSs.Empty();
	PrefixSEIMessages.Empty();
	SuffixSEIMessages.Empty();
	Colorimetry.Reset();
	HDR.Reset();
}

IVideoDecoderInputBitstreamProcessor::EProcessResult FVideoDecoderInputBitstreamProcessorH265::ProcessAccessUnitForDecoding(FBitstreamInfo& OutBSI, FAccessUnit* InOutAccessUnit)
{
	IVideoDecoderInputBitstreamProcessor::EProcessResult Result = IVideoDecoderInputBitstreamProcessor::EProcessResult::None;
	if (!InOutAccessUnit)
	{
		return Result;
	}
	check(InOutAccessUnit->ESType == EStreamType::Video);

	TSharedPtr<FCodecSpecificMessages> Msgs = StaticCastSharedPtr<FCodecSpecificMessages>(OutBSI.CodecSpecificMessages);
	if (!Msgs.IsValid())
	{
		Msgs = MakeShared<FCodecSpecificMessages>();
		OutBSI.CodecSpecificMessages = Msgs;
	}

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
			PrefixSEIMessages.Empty();
			SuffixSEIMessages.Empty();
			// The CSD may contain SEI messages that apply to the stream as a whole.
			// We need to parse the CSD to get them, if there are any.
			TArray<ElectraDecodersUtil::MPEG::FNaluInfo>	NALUs;
			const uint8* pD = InOutAccessUnit->AUCodecData->CodecSpecificData.GetData();
			ElectraDecodersUtil::MPEG::ParseBitstreamForNALUs(NALUs, pD, InOutAccessUnit->AUCodecData->CodecSpecificData.Num());
			for(int32 i=0; i<NALUs.Num(); ++i)
			{
				const uint8* NALU = (const uint8*)Electra::AdvancePointer(pD, NALUs[i].Offset + NALUs[i].UnitLength);
				uint8 nut = *NALU >> 1;
				// Prefix or suffix NUT?
				if (nut == 39 || nut == 40)
				{
					ElectraDecodersUtil::MPEG::ExtractSEIMessages(nut == 39 ? PrefixSEIMessages : SuffixSEIMessages, Electra::AdvancePointer(NALU, 2), NALUs[i].Size - 2, ElectraDecodersUtil::MPEG::ESEIStreamType::H265, nut == 39);
				}
				// SPS nut?
				else if (nut == 33)
				{
					ElectraDecodersUtil::MPEG::FISO23008_2_seq_parameter_set_data sps;
					if (ElectraDecodersUtil::MPEG::ParseH265SPS(sps, NALU, NALUs[i].Size))
					{
						SPSs.Emplace(MoveTemp(sps));
					}
				}
			}

			Result = IVideoDecoderInputBitstreamProcessor::EProcessResult::CSDChanged;
		}
		PreviousCodecData = CurrentCodecData.IsValid() ? CurrentCodecData : InOutAccessUnit->AUCodecData;
		CurrentCodecData = InOutAccessUnit->AUCodecData;
	}

	// NOTE: In a second phase we should probably scan the input access unit for inband VPS/SPS/PPS if the codec is hev1.

	// Set the messages from the CSD on the access unit.
	Msgs->SPSs = SPSs;
	Msgs->CSDPrefixSEIMessages = PrefixSEIMessages;
	Msgs->CSDSuffixSEIMessages = SuffixSEIMessages;

	// Now go over the NALUs in the access unit and see what is there.
	OutBSI.bIsDiscardable = false;
	OutBSI.bIsSyncFrame = InOutAccessUnit->bIsSyncSample;
	uint32* NALU = (uint32*)InOutAccessUnit->AUData;
	uint32* LastNALU = (uint32*)Electra::AdvancePointer(NALU, InOutAccessUnit->AUSize);
	while(NALU < LastNALU)
	{
		uint32 naluLen = MEDIA_FROM_BIG_ENDIAN(*NALU);

		uint8 nut = *(const uint8 *)(NALU + 1);
		nut >>= 1;

		// IDR, CRA or BLA frame?
		if (nut >= 16 && nut <= 21)
		{
			OutBSI.bIsSyncFrame = true;
		}
		// One of TRAIL_N, TSA_N, STSA_N, RADL_N, RASL_N, RSV_VCL_N10, RSV_VCL_N12 or RSV_VCL_N14 ?
		else if (nut <= 14 && (nut & 1) == 0)
		{
			OutBSI.bIsDiscardable = true;
		}
		// Prefix or suffix NUT?
		else if (nut == 39 || nut == 40)
		{
			ElectraDecodersUtil::MPEG::ExtractSEIMessages(nut == 39 ? Msgs->PrefixSEIMessages : Msgs->SuffixSEIMessages, Electra::AdvancePointer(NALU, 6), naluLen-2, ElectraDecodersUtil::MPEG::ESEIStreamType::H265, nut == 39);
		}

		if (bReplaceLengthWithStartcode)
		{
			*NALU = MEDIA_TO_BIG_ENDIAN(0x00000001U);
		}
		NALU = Electra::AdvancePointer(NALU, naluLen + 4);
	}

	return Result;
}


void FVideoDecoderInputBitstreamProcessorH265::SetPropertiesOnOutput(TSharedPtr<IElectraDecoderVideoOutput, ESPMode::ThreadSafe> InDecoderOutput, FParamDict* InOutProperties, const FBitstreamInfo& InFromBSI)
{
	check(InFromBSI.CodecSpecificMessages.IsValid());
	if (!InOutProperties || !InFromBSI.CodecSpecificMessages.IsValid())
	{
		return;
	}

	TSharedPtr<FCodecSpecificMessages> Msg = StaticCastSharedPtr<FCodecSpecificMessages>(InFromBSI.CodecSpecificMessages);
	uint8 num_bits = 8;
	// We only process the first SPS.
	if (Msg->SPSs.Num() > 0)
	{
		// Set the bit depth and the colorimetry.
		uint8 colour_primaries=2, transfer_characteristics=2, matrix_coeffs=2;
		uint8 video_full_range_flag=0, video_format=5;
		num_bits = Msg->SPSs[0].bit_depth_luma_minus8 + 8;
		if (Msg->SPSs[0].colour_description_present_flag)
		{
			colour_primaries = Msg->SPSs[0].colour_primaries;
			transfer_characteristics = Msg->SPSs[0].transfer_characteristics;
			matrix_coeffs = Msg->SPSs[0].matrix_coeffs;
		}
		if (Msg->SPSs[0].video_signal_type_present_flag)
		{
			video_full_range_flag = Msg->SPSs[0].video_full_range_flag;
			video_format = Msg->SPSs[0].video_format;
		}

		Colorimetry.Update(colour_primaries, transfer_characteristics, matrix_coeffs, video_full_range_flag, video_format);
		Colorimetry.UpdateParamDict(*InOutProperties);
	}

	// Extract HDR metadata from SEI messages
	HDR.Update(num_bits, Colorimetry, Msg->CSDPrefixSEIMessages, Msg->PrefixSEIMessages, false);
	HDR.UpdateParamDict(*InOutProperties);
}


} // namespace Electra
