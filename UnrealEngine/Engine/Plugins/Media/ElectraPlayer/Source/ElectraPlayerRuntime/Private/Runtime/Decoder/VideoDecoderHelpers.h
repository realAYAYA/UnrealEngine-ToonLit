// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "Utils/MPEG/ElectraUtilsMPEGVideo.h"
#include "MediaVideoDecoderOutput.h"
#include "ParameterDictionary.h"

namespace Electra
{

namespace MPEG
{

class FColorimetryHelper
{
public:
	void Reset();
	void Update(uint8 colour_primaries, uint8 transfer_characteristics, uint8 matrix_coeffs, uint8 video_full_range_flag, uint8 video_format);
	void Update(const TArray<uint8>& InFromCOLRBox);
	void UpdateParamDict(FParamDict& InOutDictionary);
	bool GetCurrentValues(uint8& colour_primaries, uint8& transfer_characteristics, uint8& matrix_coeffs) const;

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
	void Update(int32 BitDepth, const FColorimetryHelper& InColorimetry, const TArray<ElectraDecodersUtil::MPEG::FSEIMessage>& InGlobalPrefixSEIs, const TArray<ElectraDecodersUtil::MPEG::FSEIMessage>& InLocalPrefixSEIs, bool bIsNewCLVS);
	void UpdateFromMPEGBoxes(int32 BitDepth, const FColorimetryHelper& InColorimetry, const TArray<uint8>& InMDCVBox, const TArray<uint8>& InCLLIBox);
	void Update(int32 BitDepth, const FColorimetryHelper& InColorimetry, const TOptional<FVideoDecoderHDRMetadata_mastering_display_colour_volume>& InMDCV, const TOptional<FVideoDecoderHDRMetadata_content_light_level_info>& InCLLI);
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
	void SetHDRType(int32 BitDepth, const FColorimetryHelper& InColorimetry);
	TSharedPtr<FVideoDecoderHDRInformation, ESPMode::ThreadSafe> CurrentHDRInfo;
	TOptional<ElectraDecodersUtil::MPEG::FSEIMessage> ActiveMasteringDisplayColourVolume;
	TOptional<ElectraDecodersUtil::MPEG::FSEIMessage> ActiveContentLightLevelInfo;
	TOptional<ElectraDecodersUtil::MPEG::FSEIMessage> ActiveAlternativeTransferCharacteristics;
	bool bIsFirst = true;
	int32 CurrentAlternativeTransferCharacteristics = -1;
};



} // namespace MPEG
} // namespace Electra

