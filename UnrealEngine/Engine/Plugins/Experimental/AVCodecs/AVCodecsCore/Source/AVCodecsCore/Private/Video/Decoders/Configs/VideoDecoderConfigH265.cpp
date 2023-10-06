// Copyright Epic Games, Inc. All Rights Reserved.

#include "Video/Decoders/Configs/VideoDecoderConfigH265.h"

#include "AVUtility.h"
#include "Video/CodecUtils/CodecUtilsH265.h"

REGISTER_TYPEID(FVideoDecoderConfigH265);

FAVResult FVideoDecoderConfigH265::Parse(TSharedRef<FAVInstance> const& Instance, FVideoPacket const& Packet, TArray<TSharedPtr<UE::AVCodecCore::H265::FNaluH265>>& Nalus)
{
	using namespace UE::AVCodecCore::H265;

	TArray<FNaluH265> FoundNalus;

	FAVResult Result;
	Result = FindNALUs(Packet, FoundNalus);

	if (Result.IsNotSuccess())
	{
		return Result;
	}

	FVideoDecoderConfigH265& H265 = Instance->Edit<FVideoDecoderConfigH265>();
	TSharedPtr<FNaluVPS> CandidateVPS;
	TSharedPtr<FNaluSPS> CandidateSPS;
	TSharedPtr<FNaluPPS> CandidatePPS;

	for (auto& Nalu : FoundNalus)
	{
		Result = EAVResult::Success;
		switch (Nalu.nal_unit_type)
		{
			case ENaluType::TRAIL_N:
			case ENaluType::TRAIL_R:
			case ENaluType::TSA_N:
			case ENaluType::TSA_R:
			case ENaluType::STSA_N:
			case ENaluType::STSA_R:
			case ENaluType::RADL_N:
			case ENaluType::RADL_R:
			case ENaluType::RASL_N:
			case ENaluType::RASL_R:
				Nalus.Add(MakeShared<FNaluSlice>(Nalu));
				StaticCastSharedPtr<FNaluSlice>(Nalus.Last())->Parse(this);
				UE_LOG(LogTemp, Verbose, TEXT("H256 Parsing: Recieved Slice"));
				break;
			case ENaluType::RSV_VCL_N10:
				checkNoEntry(); // Reserved
				break;
			case ENaluType::RSV_VCL_R11:
				checkNoEntry(); // Reserved
				break;
			case ENaluType::RSV_VCL_N12:
				checkNoEntry(); // Reserved
				break;
			case ENaluType::RSV_VCL_R13:
				checkNoEntry(); // Reserved
				break;
			case ENaluType::RSV_VCL_N14:
				checkNoEntry(); // Reserved
				break;
			case ENaluType::RSV_VCL_R15:
				checkNoEntry(); // Reserved
				break;
			case ENaluType::IDR_W_RADL:
			case ENaluType::IDR_N_LP:
				UE_LOG(LogTemp, Verbose, TEXT("H256 Parsing: Recieved IDR"));
			case ENaluType::BLA_W_LP:
			case ENaluType::BLA_W_RADL:
			case ENaluType::BLA_N_LP:
			case ENaluType::CRA_NUT:
				Nalus.Add(MakeShared<FNaluSlice>(Nalu));
				StaticCastSharedPtr<FNaluSlice>(Nalus.Last())->Parse(this);
				UE_LOG(LogTemp, Verbose, TEXT("H256 Parsing: Recieved Slice"));
				break;
			case ENaluType::RSV_IRAP_VCL22:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_IRAP_VCL23:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_VCL24:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_VCL25:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_VCL26:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_VCL27:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_VCL28:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_VCL29:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_VCL30:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_VCL31:
				checkNoEntry(); // Reserved
				break;
			case ENaluType::VPS_NUT:
				CandidateVPS = MakeShared<FNaluVPS>(Nalu);
				CandidateVPS->Parse();
				H265.ParsedVPS.Add(CandidateVPS->vps_video_parameter_set_id, CandidateVPS);
				Nalus.Add(CandidateVPS);
				UE_LOG(LogTemp, Verbose, TEXT("H256 Parsing: Recieved VPS %u"), CandidateVPS->vps_video_parameter_set_id.Value);
				break;
			case ENaluType::SPS_NUT:
				CandidateSPS = MakeShared<FNaluSPS>(Nalu);
				CandidateSPS->Parse();
				H265.ParsedSPS.Add(CandidateSPS->sps_video_parameter_set_id, CandidateSPS);
				Nalus.Add(CandidateSPS);
				UE_LOG(LogTemp, Verbose, TEXT("H256 Parsing: Recieved SPS %u"), CandidateSPS->sps_video_parameter_set_id.Value);
				break;
			case ENaluType::PPS_NUT:
				CandidatePPS = MakeShared<FNaluPPS>(Nalu);
				CandidatePPS->Parse();
				H265.ParsedPPS.Add(CandidatePPS->pps_pic_parameter_set_id, CandidatePPS);
				Nalus.Add(CandidatePPS);
				UE_LOG(LogTemp, Verbose, TEXT("H256 Parsing: Recieved PPS %u"), CandidatePPS->pps_pic_parameter_set_id.Value);
				break;
			case ENaluType::AUD_NUT:
				Nalus.Add(MakeShared<FNaluH265>(Nalu));
				UE_LOG(LogTemp, Verbose, TEXT("H256 Parsing: Recieved Access Unit Delimiter"));
				break;
			case ENaluType::EOS_NUT:
				Nalus.Add(MakeShared<FNaluH265>(Nalu));
				UE_LOG(LogTemp, Verbose, TEXT("H256 Parsing: Recieved End Of Sequence"));
				break;
			case ENaluType::EOB_NUT:
				Nalus.Add(MakeShared<FNaluH265>(Nalu));
				UE_LOG(LogTemp, Verbose, TEXT("H256 Parsing: Recieved End Of Bitstream"));
				break;
			case ENaluType::FD_NUT:
				Nalus.Add(MakeShared<FNaluH265>(Nalu));
				UE_LOG(LogTemp, Verbose, TEXT("H256 Parsing: Recieved Filler Data"));
				break;
			case ENaluType::PREFIX_SEI_NUT:
			case ENaluType::SUFFIX_SEI_NUT:
				H265.ParsedSEI.Add(MakeShared<FNaluSEI>(Nalu));
				H265.ParsedSEI.Last()->Parse();
				UE_LOG(LogTemp, Verbose, TEXT("H256 Parsing: Recieved SEI"));
				break;
			case ENaluType::RSV_NVCL41:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_NVCL42:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_NVCL43:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_NVCL44:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_NVCL45:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_NVCL46:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::RSV_NVCL47:
				checkNoEntry(); // Reserved or Unspecified
				break;
			case ENaluType::UNSPECIFIED:
				checkNoEntry(); // Reserved or Unspecified
				break;
			default:
				checkNoEntry(); // Reserved or Unspecified
				break;
		}

		if (Result.IsNotSuccess())
		{
			return Result;
		}
	}

	return Result;
}

FAVResult FVideoDecoderConfigH265::UpdateRPS(TSharedRef<UE::AVCodecCore::H265::FNaluSlice> CurrentSlice)
{
	using namespace  UE::AVCodecCore::H265;

	ReferencePictureSet.PicOrderCntVal[LastRpsIdx] = LastPicOrderCntValue;

	// Invalidate all of the usages when a new Intra Decode Refresh frame comes in
	if (CurrentSlice->IsIDR())
	{
		// Clear the current RPS working state and usage
		ReferencePictureSet.PrepState();

		LastRpsIdx = 0;
		LastPicOrderCntValue = CurrentSlice->CurrPicOrderCntVal;
	}
	else
	{		
		TSharedPtr<FNaluSPS> PinnedSPS = CurrentSlice->CurrentSPS.Pin();
		TSharedPtr<FNaluPPS> PinnedPPS = CurrentSlice->CurrentPPS.Pin();
		uint8 i, j, k;
		
		// (8-5) Calculate NumPocTotalCur
		uint8 const& CurrRspIdx = CurrentSlice->CurrRpsIdx;
		short_term_ref_pic_set_t const& ShortTermRefPicSet = PinnedSPS->short_term_ref_pic_sets[CurrRspIdx]; 
		uint32 MaxPicOrderCntLsb = 1 << (PinnedSPS->log2_max_pic_order_cnt_lsb_minus4 + 4);
		bool UsedByCurrPicLt;

		// NOTE (aidan) Should we be creating these with every slice?
		uint8_t CurrDeltaPocMsbPresentFlag[16];
		uint8_t FollDeltaPocMsbPresentFlag[16];
	
		for (i = 0, j = 0, k = 0; i < ShortTermRefPicSet.NumNegativePics; i++)
		{
			if (ShortTermRefPicSet.UsedByCurrPicS0[i])
			{
				ReferencePictureSet.PocStCurrBefore[j++] = CurrentSlice->CurrPicOrderCntVal + ShortTermRefPicSet.DeltaPocS0[i];
			}
			else
			{
				ReferencePictureSet.PocStFoll[k++] = CurrentSlice->CurrPicOrderCntVal + ShortTermRefPicSet.DeltaPocS0[i];
			}
		}
		ReferencePictureSet.NumPocStCurrBefore = j;

		for (i = 0, j = 0; i < ShortTermRefPicSet.NumPositivePics; i++)
		{
			if (ShortTermRefPicSet.UsedByCurrPicS1[i])
			{
				ReferencePictureSet.PocStCurrAfter[j++] = CurrentSlice->CurrPicOrderCntVal + ShortTermRefPicSet.DeltaPocS1[i];
			}
			else
			{
				ReferencePictureSet.PocStFoll[k++] = CurrentSlice->CurrPicOrderCntVal + ShortTermRefPicSet.DeltaPocS1[i];
			}
		}
		ReferencePictureSet.NumPocStCurrAfter = j;
		ReferencePictureSet.NumPocStFoll = k;
	
		uint8 pocLt;
		for (i = 0, j = 0, k = 0; i < (CurrentSlice->num_long_term_sps + CurrentSlice->num_long_term_pics); i++)
		{			
			if (i < CurrentSlice->num_long_term_sps)
			{
				pocLt = PinnedSPS->long_term_ref_pics_sps[CurrentSlice->lt_idx_sps[i]].lt_ref_pic_poc_lsb_sps;
				UsedByCurrPicLt = PinnedSPS->long_term_ref_pics_sps[CurrentSlice->lt_idx_sps[i]].used_by_curr_pic_lt_sps_flag.AsBool();
			}
			else
			{
				pocLt = CurrentSlice->poc_lsb_lt[i];
				UsedByCurrPicLt = CurrentSlice->used_by_curr_pic_lt_flag[CurrentSlice->lt_idx_sps[i]].AsBool();
			}
			
			if (CurrentSlice->delta_poc_msb_present_flag[i])
			{
				pocLt += CurrentSlice->CurrPicOrderCntVal - CurrentSlice->delta_poc_msb_cycle_lt[i] * MaxPicOrderCntLsb - ( CurrentSlice->CurrPicOrderCntVal & (MaxPicOrderCntLsb-1u));
			}

			if (UsedByCurrPicLt)
			{
				ReferencePictureSet.PocLtCurr[j] = pocLt;
				CurrDeltaPocMsbPresentFlag[j++] = CurrentSlice->delta_poc_msb_present_flag[i];
				ReferencePictureSet.NumPicTotalCurr++;
			}
			else
			{
				ReferencePictureSet.PocLtFoll[k] = pocLt;
				FollDeltaPocMsbPresentFlag[k++] = CurrentSlice->delta_poc_msb_present_flag[i];
			}
		}
		ReferencePictureSet.NumPocLtCurr = j;
		ReferencePictureSet.NumPocLtFoll = k;

		auto const& PicXInDPB = [&PinnedSPS, &CurrentSlice, MaxPicOrderCntLsb, PicOrderCntVal = ReferencePictureSet.PicOrderCntVal, PicLayerId = ReferencePictureSet.PicLayerId](int32 PicOrderCount, bool DeltaPocMsbPresentFlag) -> uint8
		{
			if (DeltaPocMsbPresentFlag)
			{
				for (uint8 i = 0; i < 16 && i < PinnedSPS->MaxDpbSize; i++)
				{
					if (PicOrderCntVal[i] == PicOrderCount && CurrentSlice->nuh_layer_id == PicLayerId[i])
					{
						return i;
					}
				}
			}
			else
			{
				for (uint8 i = 0; i < 16 && i < PinnedSPS->MaxDpbSize; i++)
				{
					if ((PicOrderCntVal[i] & (FMath::Max<uint32>(MaxPicOrderCntLsb, 1) - 1)) == PicOrderCount && CurrentSlice->nuh_layer_id == PicLayerId[i])
					{
						return i;
					}
				}
			}
		
			return 255; // NOTE (aidan) placholder for "no reference picture"
		};

		// Clear reference usage flags ready to be set for current frame
		FMemory::Memset(ReferencePictureSet.PicUsage, 0, sizeof(ReferencePictureSet.PicUsage));

		// (8-6) Generate Long-term Reference Pictures
		for( i = 0; i < ReferencePictureSet.NumPocLtCurr; i++ )
		{
			ReferencePictureSet.RefPicSetLtCurr[i] = PicXInDPB(ReferencePictureSet.PocLtCurr[i], CurrDeltaPocMsbPresentFlag[i] > 0);
			if(ReferencePictureSet.RefPicSetLtCurr[i] != 255)
			{
				ReferencePictureSet.PicUsage[i] |= EPictureUsage::LONG_TERM;
			}
		}
	
		for( i = 0; i < ReferencePictureSet.NumPocLtFoll; i++ )
		{
			ReferencePictureSet.RefPicSetLtFoll[i] = PicXInDPB(ReferencePictureSet.PocLtFoll[i], FollDeltaPocMsbPresentFlag[i] > 0);
			if(ReferencePictureSet.RefPicSetLtFoll[i] != 255)
			{
				ReferencePictureSet.PicUsage[i] |= EPictureUsage::LONG_TERM;
			}
		}

		// (8-7) Generate short-term reference pictures
		for( i = 0; i < ReferencePictureSet.NumPocStCurrBefore; i++ )
		{
			ReferencePictureSet.RefPicSetStCurrBefore[i] = PicXInDPB(ReferencePictureSet.PocStCurrBefore[i], true);
			if(ReferencePictureSet.RefPicSetStCurrBefore[i] != 255)
			{
				ReferencePictureSet.PicUsage[i] |= EPictureUsage::SHORT_TERM;
			}
		}
	
		for( i = 0; i < ReferencePictureSet.NumPocStCurrAfter; i++ )
		{
			ReferencePictureSet.RefPicSetStCurrAfter[i] = PicXInDPB(ReferencePictureSet.PocStCurrAfter[i], true);
			if(ReferencePictureSet.RefPicSetStCurrAfter[i] != 255)
			{
				ReferencePictureSet.PicUsage[i] |= EPictureUsage::SHORT_TERM;
			}
		}
	
		for( i = 0; i < ReferencePictureSet.NumPocStFoll; i++ )
		{
			ReferencePictureSet.RefPicSetStFoll[i] = PicXInDPB(ReferencePictureSet.PocStFoll[i], 1);
			if(ReferencePictureSet.RefPicSetStFoll[i] != 255)
			{
				ReferencePictureSet.PicUsage[i] |= EPictureUsage::SHORT_TERM;
			}
		}

		// TODO (aidan) Unavailable frames estimation. Section 8.3.3.1 talks about image generation but not sure how to kick that off

		// (8-8) Store old Ref Picture lists
		const uint32 NumRpsCurrTempList0 = FMath::Max<uint32>(CurrentSlice->num_ref_idx_l0_active_minus1.Value + 1, ReferencePictureSet.NumPicTotalCurr);
	
		TArray<uint32> RefPicListTemp0;
		RefPicListTemp0.SetNumZeroed(NumRpsCurrTempList0);
	
		uint32 rIdx;
		for (rIdx = 0; rIdx < NumRpsCurrTempList0; rIdx++)
		{
			for (i = 0; i < ReferencePictureSet.NumPocStCurrBefore && rIdx < NumRpsCurrTempList0; rIdx++, i++)
			{
				RefPicListTemp0[rIdx] = ReferencePictureSet.RefPicSetStCurrBefore[i];
			}
			for (i = 0; i < ReferencePictureSet.NumPocStCurrAfter && rIdx < NumRpsCurrTempList0; rIdx++, i++)
			{
				RefPicListTemp0[rIdx] = ReferencePictureSet.RefPicSetStCurrAfter[i];
			}
			for (i = 0; i < ReferencePictureSet.NumPocLtCurr && rIdx < NumRpsCurrTempList0; rIdx++, i++)
			{
				RefPicListTemp0[rIdx] = ReferencePictureSet.RefPicSetLtCurr[i];
			}
			if (PinnedPPS->pps_scc_extension.pps_curr_pic_ref_enabled_flag)
			{
				RefPicListTemp0[rIdx++] = CurrPicIdx;
			}
		}

		// (8-9) Construct RefPicList0
		for (rIdx = 0; rIdx <= CurrentSlice->num_ref_idx_l0_active_minus1; rIdx++)
		{
			ReferencePictureSet.RefPicList0[rIdx] = CurrentSlice->ref_pic_list_modification.ref_pic_list_modification_flag_l0 ? RefPicListTemp0[CurrentSlice->ref_pic_list_modification.list_entry_l0[rIdx]] : RefPicListTemp0[rIdx];
		}

		if (PinnedPPS->pps_scc_extension.pps_curr_pic_ref_enabled_flag && !CurrentSlice->ref_pic_list_modification.ref_pic_list_modification_flag_l0 && NumRpsCurrTempList0 > (CurrentSlice->num_ref_idx_l0_active_minus1 + 1))
		{
			ReferencePictureSet.RefPicList0[CurrentSlice->num_ref_idx_l0_active_minus1] = CurrPicIdx;
		}

		// (8-10) Repeat for RefPicList1 when decoding a B slice
		if (CurrentSlice->slice_type == EH265SliceType::B)
		{
			const uint32 NumRpsCurrTempList1 = FMath::Max<uint32>(CurrentSlice->num_ref_idx_l1_active_minus1 + 1, ReferencePictureSet.NumPicTotalCurr);

			rIdx = 0;
			TArray<uint32> RefPicListTemp1;
			RefPicListTemp1.SetNumZeroed(NumRpsCurrTempList1);
			while (rIdx < NumRpsCurrTempList1)
			{
				for (i = 0; i < ReferencePictureSet.NumPocStCurrAfter && rIdx < NumRpsCurrTempList1; rIdx++, i++)
				{
					RefPicListTemp1[rIdx] = ReferencePictureSet.RefPicSetStCurrAfter[i];
				}
				for (i = 0; i < ReferencePictureSet.NumPocStCurrBefore && rIdx < NumRpsCurrTempList1; rIdx++, i++)
				{
					RefPicListTemp1[rIdx] = ReferencePictureSet.RefPicSetStCurrBefore[i];
				}
				for (i = 0; i < ReferencePictureSet.NumPocLtCurr && rIdx < NumRpsCurrTempList1; rIdx++, i++)
				{
					RefPicListTemp1[rIdx] = ReferencePictureSet.RefPicSetLtCurr[i];
				}
				if (PinnedPPS->pps_scc_extension.pps_curr_pic_ref_enabled_flag)
				{
					RefPicListTemp1[rIdx++] = CurrPicIdx;
				}
			}

			for (rIdx = 0; rIdx <= CurrentSlice->num_ref_idx_l0_active_minus1; rIdx++)
			{
				ReferencePictureSet.RefPicList1[rIdx] = CurrentSlice->ref_pic_list_modification.ref_pic_list_modification_flag_l0 ? RefPicListTemp1[CurrentSlice->ref_pic_list_modification.list_entry_l1[rIdx]] : RefPicListTemp1[rIdx];
			}
		}

		LastRpsIdx = CurrentSlice->CurrPicOrderCntVal % PinnedSPS->MaxDpbSize;
		LastPicOrderCntValue = CurrentSlice->CurrPicOrderCntVal;
	}

	return EAVResult::Success;
}
