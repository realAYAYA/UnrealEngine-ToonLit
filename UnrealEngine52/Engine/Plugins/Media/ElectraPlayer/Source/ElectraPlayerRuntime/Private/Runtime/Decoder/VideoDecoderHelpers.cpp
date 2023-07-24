// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoDecoderHelpers.h"

#include "PlayerCore.h"
#include "Utilities/UtilsMPEGVideo.h"
#include "MediaVideoDecoderOutput.h"
#include "ParameterDictionary.h"

namespace Electra
{

namespace MPEG
{



void FHDRHelper::Reset()
{
	CurrentHDRInfo.Reset();
	ActiveMasteringDisplayColourVolume.Reset();
	ActiveContentLightLevelInfo.Reset();
	ActiveAlternativeTransferCharacteristics.Reset();
	CurrentAlternativeTransferCharacteristics = -1;
	bIsFirst = true;
}

void FHDRHelper::Update(int32 BitDepth, const FColorimetryHelper& InColorimetry, const TArray<FSEIMessage>& InGlobalPrefixSEIs, const TArray<FSEIMessage>& InLocalPrefixSEIs, bool bIsNewCLVS)
{
	auto UseSEI = [](TOptional<FSEIMessage>& InOutWhere, const TArray<FSEIMessage>& InSeis, FSEIMessage::EPayloadType InWhich) -> bool
	{
		bool bUpdated = false;
		for(auto &sei : InSeis)
		{
			if (sei.PayloadType == InWhich)
			{
				bUpdated = !InOutWhere.IsSet() || InOutWhere.GetValue().Message != sei.Message;
				if (bUpdated)
				{
					InOutWhere = sei;
				}
				break;
			}
		}
		return bUpdated;
	};

	bool bChanged = false;
	if (bIsFirst)
	{
		bChanged |= UseSEI(ActiveMasteringDisplayColourVolume, InGlobalPrefixSEIs, FSEIMessage::EPayloadType::PT_mastering_display_colour_volume);
		bChanged |= UseSEI(ActiveContentLightLevelInfo, InGlobalPrefixSEIs, FSEIMessage::EPayloadType::PT_content_light_level_info);
		bChanged |= UseSEI(ActiveAlternativeTransferCharacteristics, InGlobalPrefixSEIs, FSEIMessage::EPayloadType::PT_alternative_transfer_characteristics);
	}
	bChanged |= UseSEI(ActiveMasteringDisplayColourVolume, InLocalPrefixSEIs, FSEIMessage::EPayloadType::PT_mastering_display_colour_volume);
	bChanged |= UseSEI(ActiveContentLightLevelInfo, InLocalPrefixSEIs, FSEIMessage::EPayloadType::PT_content_light_level_info);
	bChanged |= UseSEI(ActiveAlternativeTransferCharacteristics, InLocalPrefixSEIs, FSEIMessage::EPayloadType::PT_alternative_transfer_characteristics);

	if (bChanged)
	{
		CurrentHDRInfo = MakeShareable(new FVideoDecoderHDRInformation);

		// Mastering Display Colour Volume
		if (ActiveMasteringDisplayColourVolume.IsSet())
		{
			FSEImastering_display_colour_volume seim;
			if (seim.ParseFromMessage(ActiveMasteringDisplayColourVolume.GetValue()))
			{
				const float ST2086_Chroma_Scale = 50000.0f;
				const float ST2086_Luma_Scale = 10000.0f;
				FVideoDecoderHDRMetadata_mastering_display_colour_volume mdcv;
				mdcv.display_primaries_x[0] = seim.display_primaries_x[2] / ST2086_Chroma_Scale;
				mdcv.display_primaries_y[0] = seim.display_primaries_y[2] / ST2086_Chroma_Scale;
				mdcv.display_primaries_x[1] = seim.display_primaries_x[0] / ST2086_Chroma_Scale;
				mdcv.display_primaries_y[1] = seim.display_primaries_y[0] / ST2086_Chroma_Scale;
				mdcv.display_primaries_x[2] = seim.display_primaries_x[1] / ST2086_Chroma_Scale;
				mdcv.display_primaries_y[2] = seim.display_primaries_y[1] / ST2086_Chroma_Scale;
				mdcv.white_point_x = seim.white_point_x / ST2086_Chroma_Scale;
				mdcv.white_point_y = seim.white_point_y / ST2086_Chroma_Scale;
				mdcv.max_display_mastering_luminance = seim.max_display_mastering_luminance / ST2086_Luma_Scale;
				mdcv.min_display_mastering_luminance = seim.min_display_mastering_luminance / ST2086_Luma_Scale;
				CurrentHDRInfo->SetMasteringDisplayColourVolume(mdcv);
			}
		}

		// Content Light Level Info
		if (ActiveContentLightLevelInfo.IsSet())
		{
			FSEIcontent_light_level_info seim;
			if (seim.ParseFromMessage(ActiveContentLightLevelInfo.GetValue()))
			{
				FVideoDecoderHDRMetadata_content_light_level_info ll;
				ll.max_content_light_level = seim.max_content_light_level;
				ll.max_pic_average_light_level = seim.max_pic_average_light_level;
				CurrentHDRInfo->SetContentLightLevelInfo(ll);
			}
		}

		// Alternative Transfer Characteristics
		if (ActiveAlternativeTransferCharacteristics.IsSet())
		{
			FSEIalternative_transfer_characteristics seim;
			if (seim.ParseFromMessage(ActiveAlternativeTransferCharacteristics.GetValue()))
			{
				CurrentAlternativeTransferCharacteristics = seim.preferred_transfer_characteristics;
			}
		}
	}

	// Set HDR type from colorimetry!
	IVideoDecoderHDRInformation::EType hdrType = IVideoDecoderHDRInformation::EType::Unknown;
	uint8 colour_primaries, transfer_characteristics, matrix_coeffs;
	InColorimetry.GetCurrentValues(colour_primaries, transfer_characteristics, matrix_coeffs);
	if (colour_primaries == 9 && matrix_coeffs == 9)
	{
		if (BitDepth == 10 && transfer_characteristics == 16)
		{
			hdrType = IVideoDecoderHDRInformation::EType::PQ10;
			if (CurrentHDRInfo.IsValid() && CurrentHDRInfo->GetMasteringDisplayColourVolume() && CurrentHDRInfo->GetContentLightLevelInfo())
			{
				hdrType = IVideoDecoderHDRInformation::EType::HDR10;
			}
		}
		else if (BitDepth >= 10 && (transfer_characteristics == 18 || (transfer_characteristics == 14 && CurrentAlternativeTransferCharacteristics == 18)))
		{
			hdrType = IVideoDecoderHDRInformation::EType::HLG10;
		}
	}
	if (CurrentHDRInfo.IsValid())
	{
		CurrentHDRInfo->SetHDRType(hdrType);
	}

	bIsFirst = false;
}

void FHDRHelper::UpdateParamDict(FParamDict& InOutDictionary)
{
	InOutDictionary.Set(TEXT("hdr_info"), FVariantValue(CurrentHDRInfo));
}


} // namespace MPEG
} // namespace Electra

