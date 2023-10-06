// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Containers/Array.h>

namespace ElectraDecodersUtil
{
	namespace VPxVideo
	{
		struct ELECTRADECODERS_API FVP9UncompressedHeader
		{
			uint8 frame_marker = 0;
			uint8 profile_low_bit = 0;
			uint8 profile_high_bit = 0;
			uint8 show_existing_frame = 0;
			uint8 frame_to_show_map_idx = 0;
			uint8 frame_type = 0;
			uint8 show_frame = 0;
			uint8 error_resilient_mode = 0;

			uint8 intra_only = 0;
			uint8 reset_frame_context = 0;

			uint8 ten_or_twelve_bit = 0;
			uint8 color_space = 0;
			uint8 color_range = 0;
			uint8 subsampling_x = 0;
			uint8 subsampling_y = 0;


			enum EColorSpace
			{
				CS_Unknown = 0,
				CS_BT_601 = 1,		// Rec. ITU-R BT.601-7
				CS_BT_709 = 2,		// Rec. ITU-R BT.709-6
				CS_SMPTE_170 = 3,	// SMPTE-170
				CS_SMPTE_240 = 4,	// SMPTE-240
				CS_BT_2020 = 5,		// Rec. ITU-R BT.2020-2
				CS_Reserved = 6,
				CS_RGB = 7			// sRGB (IEC 61966-2-1)
			};

			enum EColorRange
			{
				/*
					For BitDepth equals 8:
						Y is between 16 and 235 inclusive.
						U and V are between 16 and 240 inclusive.
					For BitDepth equals 10:
						Y is between 64 and 940 inclusive.
						U and V are between 64 and 960 inclusive.
					For BitDepth equals 12:
						Y is between 256 and 3760.
						U and V are between 256 and 3840 inclusive.
				*/
				CR_StudioSwing = 0,

				// No restriction on Y, U, V values.
				CR_FullSwing = 1
			};

			enum ESubSampling
			{
				SS_YUV_444 = 0,
				SS_YUV_440 = 1,
				SS_YUV_422 = 2,
				SS_YUV_420 = 3
			};

			bool IsKeyframe() const
			{ return frame_type == 0; }
			int32 GetProfile() const
			{ return (profile_high_bit << 1) + profile_low_bit; }
			int32 GetBitDepth() const
			{ return GetProfile() >= 2 ? (ten_or_twelve_bit ? 12 : 10) : 8; }
			EColorSpace GetColorSpace() const
			{ return (EColorSpace) color_space; }
			EColorRange GetColorRange() const
			{ return(EColorRange) color_range; }
			ESubSampling GetSubSampling() const
			{ return (ESubSampling)((subsampling_y << 1) + subsampling_x); }
		};

		bool ELECTRADECODERS_API GetVP9SuperframeSizes(TArray<uint32>& OutSizes, const void* Data, int64 Size);
		bool ELECTRADECODERS_API ParseVP9UncompressedHeader(FVP9UncompressedHeader& OutHeader, const void* Data, int64 Size);






		struct ELECTRADECODERS_API FVP8UncompressedHeader
		{
			uint8 key_frame = 0;
			uint8 version = 0;
			uint8 is_experimental = 0;
			uint8 show_frame = 0;
			uint16 horizontal_size_code = 0;
			uint16 vertical_size_code = 0;

			bool IsKeyframe() const
			{ return key_frame == 0; }
		};

		bool ELECTRADECODERS_API ParseVP8UncompressedHeader(FVP8UncompressedHeader& OutHeader, const void* Data, int64 Size);

	} // namespace VPxVideo

} // namespace ElectraDecodersUtil
