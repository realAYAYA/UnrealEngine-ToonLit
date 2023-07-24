// Copyright Epic Games, Inc. All Rights Reserved.

#include "Video/CodecUtils/CodecUtilsH264.h"

#include "Containers/Array.h"
#include "AVResult.h"
#include "Video/CodecUtils/CodecUtilsH265.h"

FH264ProfileDefinition GH264ProfileDefinitions[static_cast<uint8>(EH264Profile::MAX)] = {
	{ EH264Profile::Auto, UE::AVCodecCore::H264::EH264ProfileIDC::Auto, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("Auto") },
	{ EH264Profile::CALVLC444Intra, UE::AVCodecCore::H264::EH264ProfileIDC::CALVLC444Intra, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("CALVLC444Intra") },
	{ EH264Profile::Baseline, UE::AVCodecCore::H264::EH264ProfileIDC::Baseline, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("Baseline") },
	{ EH264Profile::ConstrainedBaseline, UE::AVCodecCore::H264::EH264ProfileIDC::Baseline, UE::AVCodecCore::H264::EH264ConstraintFlag::Set1, TEXT("ConstrainedBaseline") },
	{ EH264Profile::Main, UE::AVCodecCore::H264::EH264ProfileIDC::Main, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("Main") },
	{ EH264Profile::ScalableBaseline, UE::AVCodecCore::H264::EH264ProfileIDC::ScalableBaseline, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("ScalableBaseline") },
	{ EH264Profile::ScalableConstrainedBaseline, UE::AVCodecCore::H264::EH264ProfileIDC::ScalableBaseline, UE::AVCodecCore::H264::EH264ConstraintFlag::Set5, TEXT("ScalableConstrainedBaseline") },
	{ EH264Profile::ScalableHigh, UE::AVCodecCore::H264::EH264ProfileIDC::ScalableHigh, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("ScalableHigh") },
	{ EH264Profile::ScalableConstrainedHigh, UE::AVCodecCore::H264::EH264ProfileIDC::ScalableHigh, UE::AVCodecCore::H264::EH264ConstraintFlag::Set5, TEXT("ScalableConstrainedHigh") },
	{ EH264Profile::ScalableHighIntra, UE::AVCodecCore::H264::EH264ProfileIDC::ScalableHigh, UE::AVCodecCore::H264::EH264ConstraintFlag::Set3, TEXT("ScalableHighIntra") },
	{ EH264Profile::Extended, UE::AVCodecCore::H264::EH264ProfileIDC::Extended, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("Extended") },
	{ EH264Profile::High, UE::AVCodecCore::H264::EH264ProfileIDC::High, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("High") },
	{ EH264Profile::ProgressiveHigh, UE::AVCodecCore::H264::EH264ProfileIDC::High, UE::AVCodecCore::H264::EH264ConstraintFlag::Set4, TEXT("ProgressiveHigh") },
	{ EH264Profile::ConstrainedHigh, UE::AVCodecCore::H264::EH264ProfileIDC::High, UE::AVCodecCore::H264::EH264ConstraintFlag::Set4& UE::AVCodecCore::H264::EH264ConstraintFlag::Set5, TEXT("ConstrainedHigh") },
	{ EH264Profile::High10, UE::AVCodecCore::H264::EH264ProfileIDC::High10, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("High10") },
	{ EH264Profile::High10Intra, UE::AVCodecCore::H264::EH264ProfileIDC::High10, UE::AVCodecCore::H264::EH264ConstraintFlag::Set3, TEXT("High10Intra") },
	{ EH264Profile::MultiviewHigh, UE::AVCodecCore::H264::EH264ProfileIDC::MultiviewHigh, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("MultiviewHigh") },
	{ EH264Profile::High422, UE::AVCodecCore::H264::EH264ProfileIDC::High422, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("High422") },
	{ EH264Profile::High422Intra, UE::AVCodecCore::H264::EH264ProfileIDC::High422, UE::AVCodecCore::H264::EH264ConstraintFlag::Set3, TEXT("High422Intra") },
	{ EH264Profile::StereoHigh, UE::AVCodecCore::H264::EH264ProfileIDC::StereoHigh, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("StereoHigh") },
	{ EH264Profile::MultiviewDepthHigh, UE::AVCodecCore::H264::EH264ProfileIDC::MultiviewDepthHigh, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("MultiviewDepthHigh") },
	{ EH264Profile::High444, UE::AVCodecCore::H264::EH264ProfileIDC::High444, UE::AVCodecCore::H264::EH264ConstraintFlag::None, TEXT("High444") },
	{ EH264Profile::High444Intra, UE::AVCodecCore::H264::EH264ProfileIDC::High444, UE::AVCodecCore::H264::EH264ConstraintFlag::Set3, TEXT("High444Intra") }
};

namespace UE::AVCodecCore::H264
{
	FAVResult FindNALUs(FVideoPacket const& InPacket, TArray<FNaluInfo>& FoundNalus)
	{
		FBitstreamReader Reader(InPacket.DataPtr.Get(), InPacket.DataSize);
		if (Reader.NumBytesRemaining() < 3)
		{
			return FAVResult(EAVResult::Warning, TEXT("Bitstream not long enough to hold a NALU"), TEXT("H264"));
		}

		TArrayView64<uint8> const Data = InPacket.GetData();
		
		// Skip over stream in intervals of 3 until Data[i + 2] is either 0 or 1
		for (int64 i = 0; i < Data.Num() - 2;)
		{
			if (Data[i + 2] > 1)
			{
				i += 3;
			}
			else if (Data[i + 2] == 1)
			{
				if (Data[i + 1] == 0 && Data[i] == 0)
				{
					// Found start sequence of NalU but we don't know if it has a 3 or 4 byte start code so we check.
					FNaluInfo NalInfo = { (uint64)i, 0, 3, 0, ENaluType::Unspecified, nullptr };
					if (NalInfo.Start > 0 && Data[NalInfo.Start - 1] == 0)
					{
						++NalInfo.StartCodeSize;
						--NalInfo.Start;
					}

					FBitstreamReader Bitstream(&Data[NalInfo.Start + NalInfo.StartCodeSize], 1);
					verifyf(Bitstream.ReadBits(1) == 0, TEXT("Forbidden Zero bit not Zero in NAL Header"));

					NalInfo.RefIdc = Bitstream.ReadBits(2);
					NalInfo.Type = (ENaluType)Bitstream.ReadBits(5);

					NalInfo.Data = &Data[NalInfo.Start + NalInfo.StartCodeSize + 1];

					// Update length of previous entry.
					if (FoundNalus.Num() > 0)
					{
						FoundNalus.Last().Size = NalInfo.Start - (FoundNalus.Last().Start + FoundNalus.Last().StartCodeSize);
					}

					FoundNalus.Add(NalInfo);
				}

				i += 3;
			}
			else
			{
				++i;
			}
		}

		if (FoundNalus.Num() == 0)
		{
			return FAVResult(EAVResult::PendingInput, TEXT("no NALUs found in BitDataStream"), TEXT("BitstreamParserH264"));
		}

		// Last Nal size is the remaining size of the bitstream minus a trailing zero byte
		FoundNalus.Last().Size = Data.Num() - (FoundNalus.Last().Start + FoundNalus.Last().StartCodeSize + 1);

	#if !IS_PROGRAM

		FAVResult::Log(EAVResult::Success, FString::Printf(TEXT("FAVExtension::TransformConfig found %d NALUs in bitdatastream"), FoundNalus.Num()));

		for (const FNaluInfo& NalUInfo : FoundNalus)
		{
			FAVResult::Log(EAVResult::Success, FString::Printf(TEXT("Found NALU at %llu size %llu with type %u"), NalUInfo.Start, NalUInfo.Size, (uint8)NalUInfo.Type));
		}

	#endif //!IS_PROGRAM

		return EAVResult::Success;
	}

	FAVResult ParseSEI(FBitstreamReader& Bitstream, FNaluInfo const& InNaluInfo, SEI_t& OutSEI)
	{
		//unimplemented();
		return FAVResult(EAVResult::Success, TEXT("SEI Unimplemented"));
	}

	void ParseScalingList(FBitstreamReader& Bitstream, const uint8& chroma_format_idc,  FNalu::U<1> scaling_list_present_flag[12], uint8 ScalingList4x4[6][16], uint8 ScalingList8x8[6][64])
	{
		auto scaling_list = [&Bitstream](uint8* scalingList, int32 sizeOfScalingList, bool& useDefaultScalingMatrixFlag) -> void
		{
			int32 lastScale = 8;
			int32 nextScale = 8;
			for (int32 j = 0; j < sizeOfScalingList; ++j)
			{
				if (nextScale)
				{
					FNalu::SE delta_scale;
					Bitstream.Read(delta_scale);
					
					nextScale = (lastScale + delta_scale + 256) % 256;
					useDefaultScalingMatrixFlag = (j == 0 && nextScale == 0);
				}
				scalingList[j] = (nextScale == 0) ? lastScale : nextScale;
				lastScale = scalingList[j];
			}
		};

		bool useDefaultScalingMatrixFlag = false;
		for (int32 i = 0, iMax = (chroma_format_idc != 3) ? 8 : 12; i < iMax; ++i)
		{
			Bitstream.Read(scaling_list_present_flag[i]);
			if (scaling_list_present_flag[i])
			{
				if (i < 6)
				{
					scaling_list(ScalingList4x4[i], 16, useDefaultScalingMatrixFlag);
					if (useDefaultScalingMatrixFlag)
					{
						if (i < 3)
						{
							FMemory::Memcpy(ScalingList4x4[i], Default_4x4_Intra, 16);
						}
						else
						{
							FMemory::Memcpy(ScalingList4x4[i], Default_4x4_Inter, 16);									
						}
					}
				}
				else
				{
					scaling_list(ScalingList8x8[i - 6], 64, useDefaultScalingMatrixFlag);
					if (useDefaultScalingMatrixFlag)
					{
						if (i % 2)
						{
							FMemory::Memcpy(ScalingList8x8[i - 6], Default_8x8_Intra, 64);
						}
						else
						{
							FMemory::Memcpy(ScalingList8x8[i - 6], Default_8x8_Inter, 64);									
						}
					}
				}
			}
		}
	}

	FAVResult ParseSPS(FBitstreamReader& Bitstream, FNaluInfo const& InNaluInfo, TMap<uint32, SPS_t>& OutMapSPS)
	{
		// Be sure to maintain parity with SPS struct values, or we may miss bits
		FNalu::U<8, EH264ProfileIDC> temp_profile_idc;
		FNalu::U<8, EH264ConstraintFlag> temp_constraint_flags;

		Bitstream.Read(
			temp_profile_idc,
			temp_constraint_flags);
		
		FNalu::SE seq_parameter_set_id;
		Bitstream.Read(seq_parameter_set_id);

		// Find SPS at current ID or set it to the map
		SPS_t& OutSPS = OutMapSPS.FindOrAdd(seq_parameter_set_id);

		OutSPS.profile_idc = temp_profile_idc;
		OutSPS.constraint_flags = temp_constraint_flags;
		
		Bitstream.Read(
			OutSPS.level_idc,
			OutSPS.seq_parameter_set_id);

		if (OutSPS.profile_idc == EH264ProfileIDC::High ||
			OutSPS.profile_idc == EH264ProfileIDC::High10 ||
			OutSPS.profile_idc == EH264ProfileIDC::High422 ||
			OutSPS.profile_idc == EH264ProfileIDC::High444 ||
			OutSPS.profile_idc == EH264ProfileIDC::CALVLC444Intra ||
			OutSPS.profile_idc == EH264ProfileIDC::ScalableBaseline ||
			OutSPS.profile_idc == EH264ProfileIDC::ScalableHigh ||
			OutSPS.profile_idc == EH264ProfileIDC::MultiviewHigh ||
			OutSPS.profile_idc == EH264ProfileIDC::MultiviewDepthHigh)
		{
			Bitstream.Read(OutSPS.chroma_format_idc);

			if (OutSPS.chroma_format_idc == 3)
			{
				Bitstream.Read(OutSPS.separate_colour_plane_flag);
			}

			Bitstream.Read(
				OutSPS.bit_depth_luma_minus8,
				OutSPS.bit_depth_chroma_minus8,
				OutSPS.qpprime_y_zero_transform_bypass_flag,
				OutSPS.seq_scaling_matrix_present_flag);

			if (OutSPS.seq_scaling_matrix_present_flag)
			{
				ParseScalingList(Bitstream, OutSPS.chroma_format_idc, OutSPS.seq_scaling_list_present_flag, OutSPS.ScalingList4x4, OutSPS.ScalingList8x8);
			}
		}

		Bitstream.Read(OutSPS.log2_max_frame_num_minus4);

		Bitstream.Read(OutSPS.pic_order_cnt_type);
		if (OutSPS.pic_order_cnt_type == 0)
		{
			Bitstream.Read(OutSPS.log2_max_pic_order_cnt_lsb_minus4);
		}
		else if (OutSPS.pic_order_cnt_type == 1)
		{
			Bitstream.Read(
				OutSPS.delta_pic_order_always_zero_flag,
				OutSPS.offset_for_non_ref_pic,
				OutSPS.offset_for_top_to_bottom_field,
				OutSPS.num_ref_frames_in_pic_order_cnt_cycle);
			
			for (uint32 i = 0; i < OutSPS.num_ref_frames_in_pic_order_cnt_cycle; ++i)
			{
				Bitstream.Read(OutSPS.offset_for_ref_frame[i]);
			}
		}

		Bitstream.Read(
			OutSPS.max_num_ref_frames,
			OutSPS.gaps_in_frame_num_value_allowed_flag,
			OutSPS.pic_width_in_mbs_minus1,
			OutSPS.pic_height_in_map_units_minus1);

		Bitstream.Read(OutSPS.frame_mbs_only_flag);
		if (!OutSPS.frame_mbs_only_flag)
		{
			Bitstream.Read(OutSPS.mb_adaptive_frame_field_flag);
		}

		Bitstream.Read(OutSPS.direct_8x8_inference_flag);

		Bitstream.Read(OutSPS.frame_cropping_flag);
		if (OutSPS.frame_cropping_flag)
		{
			Bitstream.Read(
				OutSPS.frame_crop_left_offset,
				OutSPS.frame_crop_right_offset,
				OutSPS.frame_crop_top_offset,
				OutSPS.frame_crop_bottom_offset);
		}

		Bitstream.Read(OutSPS.vui_parameters_present_flag);
		if (OutSPS.vui_parameters_present_flag)
		{
			Bitstream.Read(OutSPS.aspect_ratio_info_present_flag);
			if (OutSPS.aspect_ratio_info_present_flag)
			{
				OutSPS.aspect_ratio_idc = (EH264AspectRatioIDC)Bitstream.ReadBits(8);
				if (OutSPS.aspect_ratio_idc == EH264AspectRatioIDC::Extended_SAR)
				{
					Bitstream.Read(OutSPS.sar_width);
					Bitstream.Read(OutSPS.sar_height);
				}
			}

			Bitstream.Read(OutSPS.overscan_info_present_flag);
			if (OutSPS.overscan_info_present_flag)
			{
				Bitstream.Read(OutSPS.overscan_appropriate_flag);
			}

			Bitstream.Read(OutSPS.video_signal_type_present_flag);
			if (OutSPS.video_signal_type_present_flag)
			{
				Bitstream.Read(
					OutSPS.video_format,
					OutSPS.video_full_range_flag);

				Bitstream.Read(OutSPS.colour_description_present_flag);
				if (OutSPS.colour_description_present_flag)
				{
					Bitstream.Read(
						OutSPS.colour_primaries,
						OutSPS.transfer_characteristics,
						OutSPS.matrix_coefficients);
				}
			}

			Bitstream.Read(OutSPS.chroma_loc_info_present_flag);
			if (OutSPS.chroma_loc_info_present_flag)
			{
				Bitstream.Read(
					OutSPS.chroma_sample_loc_type_top_field,
					OutSPS.chroma_sample_loc_type_bottom_field);
			}
			
			Bitstream.Read(OutSPS.timing_info_present_flag);
			if (OutSPS.timing_info_present_flag)
			{
				Bitstream.Read(
					OutSPS.num_units_in_tick,
					OutSPS.time_scale,
					OutSPS.fixed_frame_rate_flag);
			}

			auto hrd_parameters = [&Bitstream, &OutSPS]() -> void
			{
				Bitstream.Read(
					OutSPS.cpb_cnt_minus1,
					OutSPS.bit_rate_scale,
					OutSPS.cpb_size_scale);
				
				for (uint32 SchedSelIdx = 0; SchedSelIdx <= OutSPS.cpb_cnt_minus1; SchedSelIdx++ )
				{
					Bitstream.Read(
						OutSPS.bit_rate_value_minus1[ SchedSelIdx ],
						OutSPS.cpb_size_value_minus1[ SchedSelIdx ],
						OutSPS.cbr_flag[ SchedSelIdx ]);
				}

				Bitstream.Read(
					OutSPS.initial_cpb_removal_delay_length_minus1,
					OutSPS.cpb_removal_delay_length_minus1,
					OutSPS.dpb_output_delay_length_minus1,
					OutSPS.time_offset_length);
			};

			Bitstream.Read(OutSPS.nal_hrd_parameters_present_flag);
			if (OutSPS.nal_hrd_parameters_present_flag)
			{
				hrd_parameters();
			}
			
			Bitstream.Read(OutSPS.vcl_hrd_parameters_present_flag);
			if (OutSPS.vcl_hrd_parameters_present_flag)
			{
				hrd_parameters();
			}

			if (OutSPS.nal_hrd_parameters_present_flag || OutSPS.vcl_hrd_parameters_present_flag)
			{
				Bitstream.Read(OutSPS.low_delay_hrd_flag);
			}

			Bitstream.Read(OutSPS.pic_struct_present_flag);
			Bitstream.Read(OutSPS.bitstream_restriction_flag);
			if( OutSPS.bitstream_restriction_flag )
			{
				Bitstream.Read(
					OutSPS.motion_vectors_over_pic_boundaries_flag,
					OutSPS.max_bytes_per_pic_denom,
					OutSPS.max_bits_per_mb_denom,
					OutSPS.log2_max_mv_length_horizontal,
					OutSPS.log2_max_mv_length_vertical,
					OutSPS.max_num_reorder_frames,
					OutSPS.max_dec_frame_buffering);
			}
		}

		return EAVResult::Success;
	}

	FAVResult ParsePPS(FBitstreamReader& Bitstream, FNaluInfo const& InNaluInfo, TMap<uint32, SPS_t> const& InMapSPS, TMap<uint32, PPS_t>& OutMapPPS)
	{	
		FNalu::UE pic_parameter_set_id;
		Bitstream.Read(pic_parameter_set_id);

		PPS_t& OutPPS = OutMapPPS.FindOrAdd(pic_parameter_set_id);
		OutPPS.pic_parameter_set_id = pic_parameter_set_id;
		
		Bitstream.Read(
			OutPPS.seq_parameter_set_id,
			OutPPS.entropy_coding_mode_flag,
			OutPPS.bottom_field_pic_order_in_frame_present_flag,
			OutPPS.num_slice_groups_minus1);
		
		if( OutPPS.num_slice_groups_minus1 > 0 )
		{
			Bitstream.Read(OutPPS.slice_group_map_type);
			if( OutPPS.slice_group_map_type == 0 )
			{
				OutPPS.run_length_minus1.AddUninitialized(OutPPS.num_slice_groups_minus1 + 1);
				for (uint32 iGroup = 0; iGroup <= OutPPS.num_slice_groups_minus1; iGroup++ )
				{
					Bitstream.Read(OutPPS.run_length_minus1[ iGroup ]);
				}
			}
			else if( OutPPS.slice_group_map_type == 2 )
			{
				OutPPS.top_left.AddUninitialized(OutPPS.num_slice_groups_minus1 + 1);
				OutPPS.bottom_right.AddUninitialized(OutPPS.num_slice_groups_minus1 + 1);
				for (uint32 iGroup = 0; iGroup < OutPPS.num_slice_groups_minus1; iGroup++ )
					{
						Bitstream.Read(
							OutPPS.top_left[ iGroup ],
							OutPPS.bottom_right[ iGroup ]);
					}
			}
			else if( OutPPS.slice_group_map_type == 3 ||
					 OutPPS.slice_group_map_type == 4 ||
					 OutPPS.slice_group_map_type == 5 )
			{
				Bitstream.Read(
					OutPPS.slice_group_change_direction_flag,
					OutPPS.slice_group_change_rate_minus1);
			}
			else if( OutPPS.slice_group_map_type == 6 )
			{
				Bitstream.Read(OutPPS.pic_size_in_map_units_minus1);
				unsigned long numBits = 0;
			#if defined(_MSC_VER)
					_BitScanForward(&numBits, OutPPS.pic_size_in_map_units_minus1 + 1);
			#else
					numBits = __builtin_ctz(OutPPS.pic_size_in_map_units_minus1 + 1);
			#endif
				for( uint32 i = 0; i <= OutPPS.pic_size_in_map_units_minus1; i++ )
				{
					Bitstream.ReadBits(OutPPS.slice_group_id[i], numBits);
				}
			}
		}
		
		Bitstream.Read(OutPPS.num_ref_idx_l0_default_active_minus1,
			OutPPS.num_ref_idx_l1_default_active_minus1,
			OutPPS.weighted_pred_flag,
			OutPPS.weighted_bipred_idc,
			OutPPS.pic_init_qp_minus26,
			OutPPS.pic_init_qs_minus26,
			OutPPS.chroma_qp_index_offset,
			OutPPS.deblocking_filter_control_present_flag,
			OutPPS.constrained_intra_pred_flag,
			OutPPS.redundant_pic_cnt_present_flag);

		auto more_rbsp_data = [&Bitstream]() -> bool
		{
			if(Bitstream.NumBitsRemaining() > 0)
			{
				return true;
			}

			return false;
		};

		if( more_rbsp_data() )
		{
			Bitstream.Read(OutPPS.transform_8x8_mode_flag);
			
			Bitstream.Read(OutPPS.pic_scaling_matrix_present_flag);
			if (OutPPS.pic_scaling_matrix_present_flag)
			{
				ParseScalingList(Bitstream, InMapSPS[OutPPS.seq_parameter_set_id].chroma_format_idc, OutPPS.pic_scaling_list_present_flag, OutPPS.ScalingList4x4, OutPPS.ScalingList8x8);
			}

			Bitstream.Read(OutPPS.second_chroma_qp_index_offset);
		}
		
		return EAVResult::Success;
	}

	FAVResult ParseSliceHeader(FBitstreamReader& Bitstream, FNaluInfo const& InNaluInfo, TMap<uint32, SPS_t> InMapSPS, TMap<uint32, PPS_t> const& InMapPPS, Slice_t& OutSlice)
	{
		Bitstream.Read(
			OutSlice.first_mb_in_slice,
			OutSlice.slice_type,
			OutSlice.pic_parameter_set_id);

		const PPS_t& CurrentPPS = InMapPPS[OutSlice.pic_parameter_set_id];
		const SPS_t& CurrentSPS = InMapSPS[CurrentPPS.seq_parameter_set_id];

		if (CurrentSPS.separate_colour_plane_flag)
		{
			Bitstream.Read(OutSlice.colour_plane_id);
		}

		Bitstream.ReadBits(OutSlice.frame_num, CurrentSPS.log2_max_frame_num_minus4 + 4);

		if (!CurrentSPS.frame_mbs_only_flag)
		{
			Bitstream.Read(OutSlice.field_pic_flag);
			if (OutSlice.field_pic_flag)
			{
				Bitstream.Read(OutSlice.bottom_field_flag);
			}
		}

		if (InNaluInfo.Type == ENaluType::SliceIdrPicture)
		{
			Bitstream.Read(OutSlice.idr_pic_id);
		}

		if (CurrentSPS.pic_order_cnt_type == 0)
		{
			Bitstream.ReadBits(OutSlice.pic_order_cnt_lsb, CurrentSPS.log2_max_pic_order_cnt_lsb_minus4 + 4);

			if (CurrentPPS.bottom_field_pic_order_in_frame_present_flag && !OutSlice.field_pic_flag)
			{
				Bitstream.Read(OutSlice.delta_pic_order_cnt_bottom);
			}
		}

		if (CurrentSPS.pic_order_cnt_type == 1 && !CurrentSPS.delta_pic_order_always_zero_flag)
		{
			Bitstream.Read(OutSlice.delta_pic_order_cnt[0]);

			if (CurrentPPS.bottom_field_pic_order_in_frame_present_flag && !OutSlice.field_pic_flag)
			{
				Bitstream.Read(OutSlice.delta_pic_order_cnt[1]);
			}
		}

		if (CurrentPPS.redundant_pic_cnt_present_flag)
		{
			Bitstream.Read(OutSlice.redundant_pic_cnt);
		}

		const bool IsP = OutSlice.slice_type == 0 || OutSlice.slice_type == 5;
		const bool IsB = OutSlice.slice_type == 1 || OutSlice.slice_type == 6;
		const bool IsI = OutSlice.slice_type == 2 || OutSlice.slice_type == 7;
		const bool IsSP = OutSlice.slice_type == 3 || OutSlice.slice_type == 8;
		const bool IsSI = OutSlice.slice_type == 4 || OutSlice.slice_type == 9;
		
		if (IsB)
		{
			Bitstream.Read(OutSlice.direct_spatial_mv_pred_flag);
		}

		if ( IsP || IsSP || IsB )
		{
			Bitstream.Read(OutSlice.num_ref_idx_active_override_flag);

			if (OutSlice.num_ref_idx_active_override_flag)
			{
				Bitstream.Read(OutSlice.num_ref_idx_l0_active_minus1);

				if ( IsB )
				{
					Bitstream.Read(OutSlice.num_ref_idx_l1_active_minus1);
				}
			}
		}

		// Slice extensions
		if (InNaluInfo.Type == ENaluType::SliceExtension || InNaluInfo.Type == ENaluType::SliceExtensionForDepthView)
		{
			// TODO ref_pic_list_mvc_modification()
		}
		else
		{
			if ( OutSlice.slice_type % 5 != 2 &&  OutSlice.slice_type % 5 != 4 )
			{
				Bitstream.Read(OutSlice.ref_pic_list_modification_flag_l0);
				if (OutSlice.ref_pic_list_modification_flag_l0)
				{
					uint32 modification_of_pic_nums_idc;
					do
					{
						Bitstream.Read(modification_of_pic_nums_idc);
						if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1)
						{
							FNalu::UE pic_num;
							Bitstream.Read(pic_num);
							
							OutSlice.RefPicList0.Add({false, pic_num});	
						}
						else if (modification_of_pic_nums_idc == 2)
						{
							FNalu::UE pic_num;
							Bitstream.Read(pic_num);
							
							OutSlice.RefPicList0.Add({true, pic_num});							
						}
					}
					while (modification_of_pic_nums_idc != 3);
				}
			}

			if (OutSlice.slice_type % 5 == 1)
			{
				Bitstream.Read(OutSlice.ref_pic_list_modification_flag_l1);
				if (OutSlice.ref_pic_list_modification_flag_l1)
				{
					uint32 modification_of_pic_nums_idc;
					do
					{
						Bitstream.Read(modification_of_pic_nums_idc);
						if (modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1)
						{
							FNalu::UE pic_num;
							Bitstream.Read(pic_num);
							
							OutSlice.RefPicList1.Add({false, pic_num});	
						}
						else if (modification_of_pic_nums_idc == 2)
						{
							FNalu::UE pic_num;
							Bitstream.Read(pic_num);
							
							OutSlice.RefPicList1.Add({true, pic_num});							
						}
					}
					while (modification_of_pic_nums_idc != 3);
				}
			}
		}

		if ( (CurrentPPS.weighted_pred_flag && ( IsP || IsSP )) || (CurrentPPS.weighted_bipred_idc == 1 && IsB) )
		{
			// TODO pred_weight_table();
		}

		if (InNaluInfo.RefIdc != 0)
		{
			// TODO dec_ref_pic_marking()
		}

		if (CurrentPPS.entropy_coding_mode_flag && !IsI && !IsSI)
		{
			Bitstream.Read(OutSlice.cabac_init_idc);
		}

		Bitstream.Read(OutSlice.slice_qp_delta);

		if ( IsSP || IsSI )
		{
			if (IsSP)
			{
				Bitstream.Read(OutSlice.sp_for_switch_flag);
			}

			Bitstream.Read(OutSlice.slice_qs_delta);
		}

		if (CurrentPPS.deblocking_filter_control_present_flag)
		{
			Bitstream.Read(OutSlice.disable_deblocking_filter_idc);

			if (OutSlice.disable_deblocking_filter_idc != 1)
			{
				Bitstream.Read(OutSlice.slice_alpha_c0_offset_div2);
				Bitstream.Read(OutSlice.slice_beta_offset_div2);
			}
		}

		if (CurrentPPS.num_slice_groups_minus1 > 0
			&& CurrentPPS.slice_group_map_type >=3
			&& CurrentPPS.slice_group_map_type <= 5)
		{
			const uint32 NumBits = FMath::CeilLogTwo((CurrentPPS.pic_size_in_map_units_minus1 + 1) / ( CurrentPPS.slice_group_change_rate_minus1 + 2) );
			Bitstream.ReadBits(OutSlice.slice_group_change_cycle, NumBits);
		}
		
		return EAVResult::Success;
	}

} // namespace UE::AVCodecCore::H264

// template <>
// DLLEXPORT FAVResult FAVExtension::TransformPacket(FVideoPacketH264& OutPacket, FVideoPacket const& InPacket)
// {
// 	using namespace UE::AVCodecCore::H264;

// 	TArray<FNaluInfo> FoundNalus;

// 	FAVResult Result;
// 	Result = FindNALUs(InPacket, FoundNalus);

// 	if (Result.IsNotSuccess())
// 	{
// 		return Result;
// 	}

// 	for (auto& NaluInfo : FoundNalus)
// 	{		
// 		Result = EAVResult::Success;
		
// 		// NALUs are is usually an EBSP so we need to strip out the emulation prevention 3 byte making them RBSP
// 		TArray64<uint8> RBSP;
// 		RBSP.AddUninitialized(NaluInfo.Size);

// 		int32 RBSPsize = EBSPtoRBSP(RBSP.GetData(), NaluInfo.Data, NaluInfo.Size);
// 		FBitDataStream Bitstream(RBSP.GetData(), RBSPsize);

// 		switch (NaluInfo.Type)
// 		{
// 			case ENaluType::Unspecified:
// 			default:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Unspecified"), TEXT("BitstreamParserH264"));
// 				break;
// 			case ENaluType::SliceOfNonIdrPicture:
// 				OutPacket.Slices.AddUninitialized();
// 				Result = ParseSliceHeader(Bitstream, NaluInfo, OutPacket.SPS, OutPacket.PPS, OutPacket.Slices.Last());
// 				break;
// 			case ENaluType::SliceDataPartitionA:
// 				break;
// 			case ENaluType::SliceDataPartitionB:
// 				break;
// 			case ENaluType::SliceDataPartitionC:
// 				break;
// 			case ENaluType::SliceIdrPicture:
// 				break;
// 			case ENaluType::SupplementalEnhancementInformation:
// 				OutPacket.SEI.AddUninitialized();
// 				Result = ParseSEI(Bitstream, NaluInfo, OutPacket.SEI.Last());
// 				break;
// 			case ENaluType::SequenceParameterSet:
// 				Result = ParseSPS(Bitstream, NaluInfo, OutPacket.SPS);
// 				break;
// 			case ENaluType::PictureParameterSet:
// 				Result = ParsePPS(Bitstream, NaluInfo, OutPacket.SPS, OutPacket.PPS);
// 				break;
// 			case ENaluType::AccessUnitDelimiter:
// 				break;
// 			case ENaluType::EndOfSequence:
// 				break;
// 			case ENaluType::EndOfStream:
// 				break;
// 			case ENaluType::FillerData:
// 				break;
// 			case ENaluType::SequenceParameterSetExtension:
// 				break;
// 			case ENaluType::PrefixNalUnit:
// 				break;
// 			case ENaluType::SubsetSequenceParameterSet:
// 				break;
// 			case ENaluType::Reserved16:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Reserved16"), TEXT("BitstreamParserH264"));
// 				break;
// 			case ENaluType::Reserved17:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Reserved17"), TEXT("BitstreamParserH264"));
// 				break;
// 			case ENaluType::Reserved18:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Reserved18"), TEXT("BitstreamParserH264"));
// 				break;
// 			case ENaluType::SliceOfAnAuxiliaryCoded:
// 				break;
// 			case ENaluType::SliceExtension:
// 				break;
// 			case ENaluType::SliceExtensionForDepthView:
// 				break;
// 			case ENaluType::Reserved22:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Reserved22"), TEXT("BitstreamParserH264"));
// 				break;
// 			case ENaluType::Reserved23:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Reserved23"), TEXT("BitstreamParserH264"));
// 				break;
// 			case ENaluType::Unspecified24:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Unspecified24"), TEXT("BitstreamParserH264"));
// 				break;
// 			case ENaluType::Unspecified25:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Unspecified25"), TEXT("BitstreamParserH264"));
// 				break;
// 			case ENaluType::Unspecified26:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Unspecified26"), TEXT("BitstreamParserH264"));
// 				break;
// 			case ENaluType::Unspecified27:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Unspecified27"), TEXT("BitstreamParserH264"));
// 				break;
// 			case ENaluType::Unspecified28:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Unspecified28"), TEXT("BitstreamParserH264"));
// 				break;
// 			case ENaluType::Unspecified29:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Unspecified29"), TEXT("BitstreamParserH264"));
// 				break;
// 			case ENaluType::Unspecified30:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Unspecified30"), TEXT("BitstreamParserH264"));
// 				break;
// 			case ENaluType::Unspecified31:
// 				Result = FAVResult(EAVResult::Error, TEXT("Recieved ENaluType::Unspecified31"), TEXT("BitstreamParserH264"));
// 				break;
// 		}

// 		if (Result.IsNotSuccess())
// 		{
// 			return Result;
// 		}
// 	}

// 	return Result;
// }
