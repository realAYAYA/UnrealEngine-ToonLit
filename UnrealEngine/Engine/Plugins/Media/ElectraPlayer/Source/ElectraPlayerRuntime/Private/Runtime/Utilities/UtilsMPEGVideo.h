// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "PlayerCore.h"

#include "PlayerTime.h"
#include "BitDataStream.h"



namespace Electra
{
	namespace MPEG
	{

		/**
		 * Sequence parameter set as per ISO/IEC 14496-10:2012, section 7.3.2.1.1
		 */
		struct FISO14496_10_seq_parameter_set_data
		{
			uint8	profile_idc = 0;
			uint8	constraint_set0_flag = 0;
			uint8	constraint_set1_flag = 0;
			uint8	constraint_set2_flag = 0;
			uint8	constraint_set3_flag = 0;
			uint8	constraint_set4_flag = 0;
			uint8	constraint_set5_flag = 0;
			uint8	level_idc = 0;
			uint32	seq_parameter_set_id = 0;
			uint32	chroma_format_idc = 0;
			uint8	separate_colour_plane_flag = 0;
			uint32	bit_depth_luma_minus8 = 0;
			uint32	bit_depth_chroma_minus8 = 0;
			uint8	qpprime_y_zero_transform_bypass_flag = 0;
			uint8	seq_scaling_matrix_present_flag = 0;
			uint32	log2_max_frame_num_minus4 = 0;
			uint32	pic_order_cnt_type = 0;
			uint32	log2_max_pic_order_cnt_lsb_minus4 = 0;
			uint32	delta_pic_order_always_zero_flag = 0;
			int32	offset_for_non_ref_pic = 0;
			int32	offset_for_top_to_bottom_field = 0;
			uint32	num_ref_frames_in_pic_order_cnt_cycle = 0;
			uint32	max_num_ref_frames = 0;
			uint8	gaps_in_frame_num_value_allowed_flag = 0;
			uint32	pic_width_in_mbs_minus1 = 0;
			uint32	pic_height_in_map_units_minus1 = 0;
			uint8	frame_mbs_only_flag = 0;
			uint8	mb_adaptive_frame_field_flag = 0;
			uint8	direct_8x8_inference_flag = 0;
			uint8	frame_cropping_flag = 0;
			uint32	frame_crop_left_offset = 0;
			uint32	frame_crop_right_offset = 0;
			uint32	frame_crop_top_offset = 0;
			uint32	frame_crop_bottom_offset = 0;
			uint8	vui_parameters_present_flag = 0;
			uint8	aspect_ratio_info_present_flag = 0;
			uint8	aspect_ratio_idc = 0;
			uint16	sar_width = 0;
			uint16	sar_height = 0;
			uint8	overscan_info_present_flag = 0;
			uint8	overscan_appropriate_flag = 0;
			uint8	video_signal_type_present_flag = 0;
			uint8	video_format = 0;
			uint8	video_full_range_flag = 0;
			uint8	colour_description_present_flag = 0;
			uint8	colour_primaries = 0;
			uint8	transfer_characteristics = 0;
			uint8	matrix_coefficients = 0;
			uint8	chroma_loc_info_present_flag = 0;
			uint32	chroma_sample_loc_type_top_field = 0;
			uint32	chroma_sample_loc_type_bottom_field = 0;
			uint8	timing_info_present_flag = 0;
			uint32	num_units_in_tick = 0;
			uint32	time_scale = 0;
			uint8	fixed_frame_rate_flag = 0;

			int32 GetWidth() const
			{
				return (pic_width_in_mbs_minus1 + 1) * 16;
			}

			int32 GetHeight() const
			{
				return (pic_height_in_map_units_minus1 + 1) * 16;
			}

			void GetCrop(int32& Left, int32& Right, int32& Top, int32& Bottom) const
			{
				if (frame_cropping_flag)
				{
					// The scaling factors are determined by the chroma_format_idc (see ISO/IEC 14496-10, table 6.1)
					// For our purposes this will be 1, so the sub width/height are 2.
					const int32 CropUnitX = 2;
					const int32 CropUnitY = 2;
					Left = (uint16)frame_crop_left_offset * CropUnitX;
					Right = (uint16)frame_crop_right_offset * CropUnitX;
					Top = (uint16)frame_crop_top_offset * CropUnitY;
					Bottom = (uint16)frame_crop_bottom_offset * CropUnitY;
				}
				else
				{
					Left = Right = Top = Bottom = 0;
				}
			}

			void GetAspect(int32& SarW, int32& SarH) const
			{
				if (vui_parameters_present_flag && aspect_ratio_info_present_flag)
				{
					switch (aspect_ratio_idc)
					{
						default:	SarW = SarH = 0;		break;
						case 0:		SarW = SarH = 0;		break;
						case 1:		SarW = SarH = 1;		break;
						case 2:		SarW = 12; SarH = 11;	break;
						case 3:		SarW = 10; SarH = 11;	break;
						case 4:		SarW = 16; SarH = 11;	break;
						case 5:		SarW = 40; SarH = 33;	break;
						case 6:		SarW = 24; SarH = 11;	break;
						case 7:		SarW = 20; SarH = 11;	break;
						case 8:		SarW = 32; SarH = 11;	break;
						case 9:		SarW = 80; SarH = 33;	break;
						case 10:	SarW = 18; SarH = 11;	break;
						case 11:	SarW = 15; SarH = 11;	break;
						case 12:	SarW = 64; SarH = 33;	break;
						case 13:	SarW = 160; SarH = 99;	break;
						case 14:	SarW = 4; SarH = 3;		break;
						case 15:	SarW = 3; SarH = 2;		break;
						case 16:	SarW = 2; SarH = 1;		break;
						case 255:	SarW = sar_width; SarH = sar_height;	break;
					}
				}
				else
				{
					SarW = SarH = 1;
				}
			}
			FTimeFraction GetTiming() const
			{
				if (vui_parameters_present_flag && timing_info_present_flag)
				{
					return FTimeFraction(time_scale, num_units_in_tick * 2);
				}
				return FTimeFraction();
			}
		};

		/**
		 * ISO/IEC 14496-15:2014
		 */
		class FAVCDecoderConfigurationRecord
		{
		public:
			void SetRawData(const void* Data, int64 Size);
			const TArray<uint8>& GetRawData() const;

			bool Parse();

			const TArray<uint8>& GetCodecSpecificData() const;
			const TArray<uint8>& GetCodecSpecificDataSPS() const;
			const TArray<uint8>& GetCodecSpecificDataPPS() const;

			int32 GetNumberOfSPS() const;
			const FISO14496_10_seq_parameter_set_data& GetParsedSPS(int32 SpsIndex) const;

			uint8 GetAVCProfileIndication() const
			{ return AVCProfileIndication; }
			uint8 GetProfileCompatibility() const
			{ return ProfileCompatibility; }
			uint8 GetAVCLevelIndication() const
			{ return AVCLevelIndication; }

		private:
			TArray<uint8>													RawData;
			TArray<uint8>													CodecSpecificData;
			TArray<uint8>													CodecSpecificDataSPSOnly;
			TArray<uint8>													CodecSpecificDataPPSOnly;
			TArray<TArray<uint8>>											SequenceParameterSets;
			TArray<TArray<uint8>>											PictureParameterSets;
			TArray<TArray<uint8>>											SequenceParameterSetsExt;
			uint8															ConfigurationVersion = 0;
			uint8															AVCProfileIndication = 0;
			uint8															ProfileCompatibility = 0;
			uint8															AVCLevelIndication = 0;
			uint8															NALUnitLength = 0;
			uint8															ChromaFormat = 0;
			uint8															BitDepthLumaMinus8 = 0;
			uint8															BitDepthChromaMinus8 = 0;
			bool															bHaveAdditionalProfileIndication = false;

			TArray<FISO14496_10_seq_parameter_set_data>						ParsedSPSs;
		};


		struct FISO23008_2_seq_parameter_set_data
		{
			uint8		sps_video_parameter_set_id = 0;
			uint8		sps_max_sub_layers_minus1 = 0;
			uint8		sps_temporal_id_nesting_flag = 0;
			uint8		general_profile_space = 0;
			uint8		general_tier_flag = 0;
			uint8		general_profile_idc = 0;
			uint32		general_profile_compatibility_flag = 0;
			uint8		general_progressive_source_flag = 0;
			uint8		general_interlaced_source_flag = 0;
			uint8		general_non_packed_constraint_flag = 0;
			uint8		general_frame_only_constraint_flag = 0;
			uint64		general_reserved_zero_44bits = 0;
			uint8		general_level_idc = 0;
			uint8		sub_layer_profile_present_flag[8] = {0};
			uint8		sub_layer_level_present_flag[8] = {0};
			uint32		sps_seq_parameter_set_id = 0;
			uint32		chroma_format_idc = 0;
			uint8		separate_colour_plane_flag = 0;
			uint32		pic_width_in_luma_samples = 0;
			uint32		pic_height_in_luma_samples = 0;
			uint8		conformance_window_flag = 0;
			uint32		conf_win_left_offset = 0;
			uint32		conf_win_right_offset = 0;
			uint32		conf_win_top_offset = 0;
			uint32		conf_win_bottom_offset = 0;
			uint32		bit_depth_luma_minus8 = 0;
			uint32		bit_depth_chroma_minus8 = 0;
			uint32		log2_max_pic_order_cnt_lsb_minus4 = 0;
			uint8		sps_sub_layer_ordering_info_present_flag = 0;
			uint32		sps_max_dec_pic_buffering_minus1[8] = {0};
			uint32		sps_max_num_reorder_pics[8] = {0};
			uint32		sps_max_latency_increase_plus1[8] = {0};
			uint32		log2_min_luma_coding_block_size_minus3 = 0;
			uint32		log2_diff_max_min_luma_coding_block_size = 0;
			uint32		log2_min_transform_block_size_minus2 = 0;
			uint32		log2_diff_max_min_transform_block_size = 0;
			uint32		max_transform_hierarchy_depth_inter = 0;
			uint32		max_transform_hierarchy_depth_intra = 0;
			uint8		scaling_list_enabled_flag = 0;
			uint8		sps_scaling_list_data_present_flag = 0;
			uint8		amp_enabled_flag = 0;
			uint8		sample_adaptive_offset_enabled_flag = 0;
			uint8		pcm_enabled_flag = 0;
			uint8		pcm_sample_bit_depth_luma_minus1 = 0;
			uint8		pcm_sample_bit_depth_chroma_minus1 = 0;
			uint32		log2_min_pcm_luma_coding_block_size_minus3 = 0;
			uint32		log2_diff_max_min_pcm_luma_coding_block_size = 0;
			uint8		pcm_loop_filter_disabled_flag = 0;
			uint32		num_short_term_ref_pic_sets = 0;
			uint8		sps_temporal_mvp_enabled_flag = 0;
			uint8		strong_intra_smoothing_enabled_flag = 0;
			uint8		vui_parameters_present_flag = 0;
			uint8		aspect_ratio_info_present_flag = 0;
			uint8		aspect_ratio_idc = 0;
			uint16		sar_width = 0;
			uint16		sar_height = 0;
			uint8		overscan_info_present_flag = 0;
			uint8		overscan_appropriate_flag = 0;
			uint8		video_signal_type_present_flag = 0;
			uint8		video_format = 0;
			uint8		video_full_range_flag = 0;
			uint8		colour_description_present_flag = 0;
			uint8		colour_primaries = 0;
			uint8		transfer_characteristics = 0;
			uint8		matrix_coeffs = 0;
			uint8		chroma_loc_info_present_flag = 0;
			uint32		chroma_sample_loc_type_top_field = 0;
			uint32		chroma_sample_loc_type_bottom_field = 0;
			uint8		neutral_chroma_indication_flag = 0;
			uint8		field_seq_flag = 0;
			uint8		frame_field_info_present_flag = 0;
			uint8		default_display_window_flag = 0;
			uint32		def_disp_win_left_offset = 0;
			uint32		def_disp_win_right_offset = 0;
			uint32		def_disp_win_top_offset = 0;
			uint32		def_disp_win_bottom_offset = 0;
			uint8		vui_timing_info_present_flag = 0;
			uint32		vui_num_units_in_tick = 0;
			uint32		vui_time_scale = 0;
			uint8		vui_poc_proportional_to_timing_flag = 0;
			uint32		vui_num_ticks_poc_diff_one_minus1 = 0;


			int32 GetWidth() const
			{
				return (int32) pic_width_in_luma_samples;
			}

			int32 GetHeight() const
			{
				return (int32) pic_height_in_luma_samples;
			}

			void GetCrop(int32& Left, int32& Right, int32& Top, int32& Bottom) const
			{
				if (conformance_window_flag)
				{
					uint32 SubWidthC = 0;
					uint32 SubHeightC = 0;
					switch(chroma_format_idc)
					{
						default:
						case 0:
							SubWidthC = 1; 
							SubHeightC = 1;	
							break;
						case 1:	
							SubWidthC = 2; 
							SubHeightC = 2;	
							break;
						case 2:	
							SubWidthC = 2; 
							SubHeightC = 1;	
							break;
						case 3:	
							SubWidthC = 1; 
							SubHeightC = 1;	
							break;
					}
					Left = conf_win_left_offset * SubWidthC;
					Right = conf_win_right_offset * SubWidthC;
					Top	= conf_win_top_offset * SubHeightC;
					Bottom = conf_win_bottom_offset * SubHeightC;
				}
				else
				{
					Left = Right = Top = Bottom = 0;
				}
			}

			void GetAspect(int32& SarW, int32& SarH) const
			{
				if (vui_parameters_present_flag && aspect_ratio_info_present_flag)
				{
					switch (aspect_ratio_idc)
					{
						default:	SarW = SarH = 0;		break;
						case 0:		SarW = SarH = 0;		break;
						case 1:		SarW = SarH = 1;		break;
						case 2:		SarW = 12; SarH = 11;	break;
						case 3:		SarW = 10; SarH = 11;	break;
						case 4:		SarW = 16; SarH = 11;	break;
						case 5:		SarW = 40; SarH = 33;	break;
						case 6:		SarW = 24; SarH = 11;	break;
						case 7:		SarW = 20; SarH = 11;	break;
						case 8:		SarW = 32; SarH = 11;	break;
						case 9:		SarW = 80; SarH = 33;	break;
						case 10:	SarW = 18; SarH = 11;	break;
						case 11:	SarW = 15; SarH = 11;	break;
						case 12:	SarW = 64; SarH = 33;	break;
						case 13:	SarW = 160; SarH = 99;	break;
						case 14:	SarW = 4; SarH = 3;		break;
						case 15:	SarW = 3; SarH = 2;		break;
						case 16:	SarW = 2; SarH = 1;		break;
						case 255:	SarW = sar_width; SarH = sar_height;	break;
					}
				}
				else
				{
					SarW = SarH = 1;
				}
			}
			FTimeFraction GetTiming() const
			{
				if (vui_parameters_present_flag && vui_timing_info_present_flag && vui_time_scale)
				{
					return FTimeFraction(vui_time_scale, vui_num_units_in_tick);
				}
				return FTimeFraction();
			}

			uint64 GetConstraintFlags() const;
			FString GetRFC6381(const TCHAR* SampleTypePrefix) const;

		};

		class FHEVCDecoderConfigurationRecord
		{
		public:
			void SetRawData(const void* Data, int64 Size);
			const TArray<uint8>& GetRawData() const;

			void Reset();

			bool Parse();

			const TArray<uint8>& GetCodecSpecificData() const;

			int32 GetNumberOfSPS() const;
			const FISO23008_2_seq_parameter_set_data& GetParsedSPS(int32 SpsIndex) const;

		private:
			struct FArray
			{
				uint8					Completeness = 0;
				uint8					NALUnitType = 0;
				TArray<TArray<uint8>>	NALUs;
			};
			TArray<uint8>								RawData;
			TArray<uint8>								CodecSpecificData;
			TArray<FArray>								Arrays;
			uint8										ConfigurationVersion = 0;
			uint8										GeneralProfileSpace = 0;
			uint8										GeneralTierFlag = 0;
			uint8										GeneralProfileIDC = 0;
			uint32										GeneralProfileCompatibilityFlags = 0;
			uint64										GeneralConstraintIndicatorFlags = 0;
			uint8										GeneralLevelIDC = 0;
			uint16										MinSpatialSegmentationIDC = 0;
			uint8										ParallelismType = 0;
			uint8										ChromaFormat = 0;
			uint8										BitDepthLumaMinus8 = 0;
			uint8										BitDepthChromaMinus8 = 0;
			uint16										AverageFrameRate = 0;
			uint8										ConstantFrameRate = 0;
			uint8										NumTemporalLayers = 0;
			uint8										TemporalIdNested = 0;
			uint8										NALUnitLengthMinus1 = 0;
			TArray<FISO23008_2_seq_parameter_set_data>	ParsedSPSs;
		};



		//! Parses a H.264 (ISO/IEC 14496-10) bitstream for NALUs.
		struct FNaluInfo
		{
			uint64		Offset;
			uint64		Size;
			uint8		Type;
			uint8		UnitLength;
		};
		void ParseBitstreamForNALUs(TArray<FNaluInfo>& outNALUs, const void* InBitstream, uint64 InBitstreamLength);


		//! Parses a H.264 (ISO/IEC 14496-10) SPS NALU.
		bool ParseH264SPS(FISO14496_10_seq_parameter_set_data& OutSPS, const void* Data, int32 Size);

		//! Parses a H.265 (ISO/IEC 23008-2) SPS NALU.
		bool ParseH265SPS(FISO23008_2_seq_parameter_set_data& OutSPS, const void* Data, int32 Size);

	} // namespace MPEG
} // namespace Electra


