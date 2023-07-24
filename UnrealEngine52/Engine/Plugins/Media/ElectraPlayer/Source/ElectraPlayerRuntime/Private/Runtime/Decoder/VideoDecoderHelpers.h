// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "Utilities/UtilsMPEGVideo.h"
#include "MediaVideoDecoderOutput.h"
#include "ParameterDictionary.h"

namespace Electra
{

namespace MPEG
{

class FColorimetryHelper
{
public:
	void Reset()
	{
		CurrentColorimetry.Reset();
	}
	void Update(uint8 colour_primaries, uint8 transfer_characteristics, uint8 matrix_coeffs, uint8 video_full_range_flag, uint8 video_format)
	{
		if (CurrentColorimetry.IsValid() &&
			CurrentColorimetry->Colorimetry.ColourPrimaries == colour_primaries &&
			CurrentColorimetry->Colorimetry.MatrixCoefficients == matrix_coeffs &&
			CurrentColorimetry->Colorimetry.TransferCharacteristics == transfer_characteristics &&
			CurrentColorimetry->Colorimetry.VideoFullRangeFlag == video_full_range_flag &&
			CurrentColorimetry->Colorimetry.VideoFormat == video_format)
		{
			return;
		}
		CurrentColorimetry = MakeShareable(new FVideoDecoderColorimetry(colour_primaries, transfer_characteristics, matrix_coeffs, video_full_range_flag, video_format));
	}

	void UpdateParamDict(FParamDict& InOutDictionary)
	{
		InOutDictionary.Set(TEXT("colorimetry"), FVariantValue(CurrentColorimetry));
	}

	bool GetCurrentValues(uint8& colour_primaries, uint8& transfer_characteristics, uint8& matrix_coeffs) const
	{
		if (CurrentColorimetry.IsValid() && CurrentColorimetry->GetMPEGDefinition())
		{
			colour_primaries = CurrentColorimetry->GetMPEGDefinition()->ColourPrimaries;
			transfer_characteristics = CurrentColorimetry->GetMPEGDefinition()->TransferCharacteristics;
			matrix_coeffs = CurrentColorimetry->GetMPEGDefinition()->MatrixCoefficients;
			return true;
		}
		else
		{
			colour_primaries = transfer_characteristics = matrix_coeffs = 2;
			return false;
		}
	}

private:
	class FVideoDecoderColorimetry : public IVideoDecoderColorimetry
	{
	public:
		FVideoDecoderColorimetry(uint8 colour_primaries, uint8 transfer_characteristics, uint8 matrix_coeffs, uint8 video_full_range_flag, uint8 video_format)
		{
			Colorimetry.ColourPrimaries = colour_primaries;
			Colorimetry.TransferCharacteristics = transfer_characteristics;
			Colorimetry.MatrixCoefficients = matrix_coeffs;
			Colorimetry.VideoFullRangeFlag = video_full_range_flag;
			Colorimetry.VideoFormat = video_format;
		}
		virtual ~FVideoDecoderColorimetry() = default;
		IVideoDecoderColorimetry::FMPEGDefinition const* GetMPEGDefinition() const override
		{ return &Colorimetry; }
		IVideoDecoderColorimetry::FMPEGDefinition Colorimetry;
	};

	TSharedPtr<FVideoDecoderColorimetry, ESPMode::ThreadSafe> CurrentColorimetry;
};



class FHDRHelper
{
public:
	void Reset();
	void Update(int32 BitDepth, const FColorimetryHelper& InColorimetry, const TArray<FSEIMessage>& InGlobalPrefixSEIs, const TArray<FSEIMessage>& InLocalPrefixSEIs, bool bIsNewCLVS);
	void UpdateParamDict(FParamDict& InOutDictionary);
private:
	class FVideoDecoderHDRInformation : public IVideoDecoderHDRInformation
	{
	public:
		virtual ~FVideoDecoderHDRInformation() = default;
		IVideoDecoderHDRInformation::EType GetHDRType() const override
		{ return HDRType; }
		const FVideoDecoderHDRMetadata_mastering_display_colour_volume* GetMasteringDisplayColourVolume() const override
		{ return MasteringDisplayColourVolume.GetPtrOrNull(); }
		const FVideoDecoderHDRMetadata_content_light_level_info* GetContentLightLevelInfo() const override
		{ return ContentLightLevelInfo.GetPtrOrNull(); }
		void SetHDRType(IVideoDecoderHDRInformation::EType InHDRType)
		{ HDRType = InHDRType; }
		void SetMasteringDisplayColourVolume(const FVideoDecoderHDRMetadata_mastering_display_colour_volume& In)
		{ MasteringDisplayColourVolume = In; }
		void SetContentLightLevelInfo(const FVideoDecoderHDRMetadata_content_light_level_info& In)
		{ ContentLightLevelInfo = In; }
	private:
		IVideoDecoderHDRInformation::EType HDRType = IVideoDecoderHDRInformation::EType::Unknown;
		TOptional<FVideoDecoderHDRMetadata_mastering_display_colour_volume> MasteringDisplayColourVolume;
		TOptional<FVideoDecoderHDRMetadata_content_light_level_info> ContentLightLevelInfo;
	};
	TSharedPtr<FVideoDecoderHDRInformation, ESPMode::ThreadSafe> CurrentHDRInfo;
	TOptional<FSEIMessage> ActiveMasteringDisplayColourVolume;
	TOptional<FSEIMessage> ActiveContentLightLevelInfo;
	TOptional<FSEIMessage> ActiveAlternativeTransferCharacteristics;
	bool bIsFirst = true;
	int32 CurrentAlternativeTransferCharacteristics = -1;
};



} // namespace MPEG
} // namespace Electra

