// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCore.h"
#include "Utilities/Utilities.h"
#include "Utilities/UtilsMPEGVideo.h"
#include "BitDataStream.h"


namespace Electra
{
	namespace MPEG
	{
		void FAVCDecoderConfigurationRecord::SetRawData(const void* Data, int64 Size)
		{
			RawData.Empty();
			if (Size)
			{
				RawData.Reserve((uint32)Size);
				RawData.SetNumUninitialized((uint32)Size);
				FMemory::Memcpy(RawData.GetData(), Data, Size);
			}
		}

		const TArray<uint8>& FAVCDecoderConfigurationRecord::GetRawData() const
		{
			return RawData;
		}

		const TArray<uint8>& FAVCDecoderConfigurationRecord::GetCodecSpecificData() const
		{
			return CodecSpecificData;
		}

		const TArray<uint8>& FAVCDecoderConfigurationRecord::GetCodecSpecificDataSPS() const
		{
			return CodecSpecificDataSPSOnly;
		}

		const TArray<uint8>& FAVCDecoderConfigurationRecord::GetCodecSpecificDataPPS() const
		{
			return CodecSpecificDataPPSOnly;
		}

		int32 FAVCDecoderConfigurationRecord::GetNumberOfSPS() const
		{
			return ParsedSPSs.Num();
		}

		const FISO14496_10_seq_parameter_set_data& FAVCDecoderConfigurationRecord::GetParsedSPS(int32 SpsIndex) const
		{
			check(SpsIndex < GetNumberOfSPS());
			return ParsedSPSs[SpsIndex];
		}


		bool FAVCDecoderConfigurationRecord::Parse()
		{
			CodecSpecificData.Empty();

			if (RawData.Num())
			{
				FByteReader BitReader(RawData.GetData(), RawData.Num());

				if (!BitReader.ReadByte(ConfigurationVersion))
				{
					return false;
				}
				if (!BitReader.ReadByte(AVCProfileIndication))
				{
					return false;
				}
				if (!BitReader.ReadByte(ProfileCompatibility))
				{
					return false;
				}
				if (!BitReader.ReadByte(AVCLevelIndication))
				{
					return false;
				}
				if (!BitReader.ReadByte(NALUnitLength))
				{
					return false;
				}
				NALUnitLength = (NALUnitLength & 3) + 1;
				uint8 nSPS = 0;
				if (!BitReader.ReadByte(nSPS))
				{
					return false;
				}
				nSPS &= 31;
				if (nSPS)
				{
					SequenceParameterSets.Reserve(nSPS);
					ParsedSPSs.Reserve(nSPS);
				}
				int32 TotalSPSSize = 0;
				for(int32 i = 0; i < nSPS; ++i)
				{
					uint16 spsLen = 0;
					if (!BitReader.ReadByte(spsLen))
					{
						return false;
					}
					TArray<uint8>& sps = SequenceParameterSets.AddDefaulted_GetRef();
					sps.Reserve(spsLen);
					sps.SetNumUninitialized(spsLen);
					if (!BitReader.ReadBytes(sps.GetData(), spsLen))
					{
						return false;
					}
					TotalSPSSize += 4 + spsLen;		// 4 because we always use 32 bit startcode and not NALUnitLength
					ParseH264SPS(ParsedSPSs.AddDefaulted_GetRef(), sps.GetData(), sps.Num());
				}
				uint8 nPPS = 0;
				if (!BitReader.ReadByte(nPPS))
				{
					return false;
				}
				if (nPPS)
				{
					PictureParameterSets.Reserve(nPPS);
				}
				int32 TotalPPSSize = 0;
				for(int32 i = 0; i < nPPS; ++i)
				{
					uint16 ppsLen = 0;
					if (!BitReader.ReadByte(ppsLen))
					{
						return false;
					}
					TArray<uint8>& pps = PictureParameterSets.AddDefaulted_GetRef();
					pps.Reserve(ppsLen);
					pps.SetNumUninitialized(ppsLen);
					if (!BitReader.ReadBytes(pps.GetData(), ppsLen))
					{
						return false;
					}
					TotalPPSSize += 4 + ppsLen;		// 4 because we always use 32 bit startcode and not NALUnitLength
				}

				if (AVCProfileIndication == 100 || AVCProfileIndication == 110 || AVCProfileIndication == 122 || AVCProfileIndication == 144)
				{
					// At least according to the ISO 14496-15:2014 standard these values must appear.
					// I do have however files that are of AVC profile 100 but omit these values.
					// Therefore let's do a quick check if we can read at least 4 more bytes.
					if (BitReader.BytesRemaining() >= 4)
					{
						if (!BitReader.ReadByte(ChromaFormat))
						{
							return false;
						}
						ChromaFormat &= 3;
						if (!BitReader.ReadByte(BitDepthLumaMinus8))
						{
							return false;
						}
						BitDepthLumaMinus8 &= 7;
						if (!BitReader.ReadByte(BitDepthChromaMinus8))
						{
							return false;
						}
						BitDepthChromaMinus8 &= 7;
						if (!BitReader.ReadByte(nSPS))
						{
							return false;
						}
						if (nSPS)
						{
							SequenceParameterSetsExt.Reserve(nSPS);
						}
						for(int32 i = 0; i < nSPS; ++i)
						{
							uint16 spsLenExt = 0;
							if (!BitReader.ReadByte(spsLenExt))
							{
								return false;
							}
							TArray<uint8>& spsExt = SequenceParameterSetsExt.AddDefaulted_GetRef();
							spsExt.Reserve(spsLenExt);
							spsExt.SetNumUninitialized(spsLenExt);
							if (!BitReader.ReadBytes(spsExt.GetData(), spsLenExt))
							{
								return false;
							}
						}
						bHaveAdditionalProfileIndication = true;
					}
				}

				int32 TotalCSDSize = TotalSPSSize + TotalPPSSize;
				if (TotalCSDSize)
				{
					CodecSpecificData.Reserve(TotalCSDSize);
					CodecSpecificDataSPSOnly.Reserve(TotalSPSSize);
					for(int32 i = 0; i < SequenceParameterSets.Num(); ++i)
					{
						CodecSpecificDataSPSOnly.Push(0);
						CodecSpecificDataSPSOnly.Push(0);
						CodecSpecificDataSPSOnly.Push(0);
						CodecSpecificDataSPSOnly.Push(1);
						CodecSpecificData.Push(0);
						CodecSpecificData.Push(0);
						CodecSpecificData.Push(0);
						CodecSpecificData.Push(1);
						for(int32 j = 0, jMax = SequenceParameterSets[i].Num(); j < jMax; ++j)
						{
							CodecSpecificDataSPSOnly.Push((SequenceParameterSets[i].GetData())[j]);
							CodecSpecificData.Push((SequenceParameterSets[i].GetData())[j]);
						}
					}
					CodecSpecificDataPPSOnly.Reserve(TotalPPSSize);
					for(int32 i = 0; i < PictureParameterSets.Num(); ++i)
					{
						CodecSpecificDataPPSOnly.Push(0);
						CodecSpecificDataPPSOnly.Push(0);
						CodecSpecificDataPPSOnly.Push(0);
						CodecSpecificDataPPSOnly.Push(1);
						CodecSpecificData.Push(0);
						CodecSpecificData.Push(0);
						CodecSpecificData.Push(0);
						CodecSpecificData.Push(1);
						for(int32 j = 0, jMax = PictureParameterSets[i].Num(); j < jMax; ++j)
						{
							CodecSpecificDataPPSOnly.Push((PictureParameterSets[i].GetData())[j]);
							CodecSpecificData.Push((PictureParameterSets[i].GetData())[j]);
						}
					}
				}

				return true;
			}
			return false;
		}



		void FHEVCDecoderConfigurationRecord::SetRawData(const void* Data, int64 Size)
		{
			RawData.Empty();
			if (Size)
			{
				RawData.Reserve((uint32)Size);
				RawData.SetNumUninitialized((uint32)Size);
				FMemory::Memcpy(RawData.GetData(), Data, Size);
			}
		}

		const TArray<uint8>& FHEVCDecoderConfigurationRecord::GetRawData() const
		{
			return RawData;
		}

		const TArray<uint8>& FHEVCDecoderConfigurationRecord::GetCodecSpecificData() const
		{
			return CodecSpecificData;
		}

		int32 FHEVCDecoderConfigurationRecord::GetNumberOfSPS() const
		{
			return ParsedSPSs.Num();
		}
		
		const FISO23008_2_seq_parameter_set_data& FHEVCDecoderConfigurationRecord::GetParsedSPS(int32 SpsIndex) const
		{
			return ParsedSPSs[SpsIndex];
		}

		void FHEVCDecoderConfigurationRecord::Reset()
		{
			RawData.Empty();
			CodecSpecificData.Empty();
			Arrays.Empty();
			ConfigurationVersion = 0;
			GeneralProfileSpace = 0;
			GeneralTierFlag = 0;
			GeneralProfileIDC = 0;
			GeneralProfileCompatibilityFlags = 0;
			GeneralConstraintIndicatorFlags = 0;
			GeneralLevelIDC = 0;
			MinSpatialSegmentationIDC = 0;
			ParallelismType = 0;
			ChromaFormat = 0;
			BitDepthLumaMinus8 = 0;
			BitDepthChromaMinus8 = 0;
			AverageFrameRate = 0;
			ConstantFrameRate = 0;
			NumTemporalLayers = 0;
			TemporalIdNested = 0;
			NALUnitLengthMinus1 = 0;
			ParsedSPSs.Empty();
		}

		bool FHEVCDecoderConfigurationRecord::Parse()
		{
			CodecSpecificData.Empty();

			if (RawData.Num() >= 23)
			{
				FBitDataStream BitReader(RawData.GetData(), RawData.Num());

				ConfigurationVersion = (uint8)BitReader.GetBits(8);
				if (ConfigurationVersion != 1)
				{
					return false;
				}
				GeneralProfileSpace = (uint8)BitReader.GetBits(2);
				GeneralTierFlag = (uint8)BitReader.GetBits(1);
				GeneralProfileIDC = (uint8)BitReader.GetBits(5);
				GeneralProfileCompatibilityFlags  = BitReader.GetBits(32);
				for(int32 i=0; i<48; ++i)
				{
					GeneralConstraintIndicatorFlags = (GeneralConstraintIndicatorFlags << 1U) | uint64(BitReader.GetBits(1));
				}
				GeneralLevelIDC = (uint8)BitReader.GetBits(8);
				if (BitReader.GetBits(4) != 15)
				{
					return false;
				}
				MinSpatialSegmentationIDC = (uint16)BitReader.GetBits(12);
				if (BitReader.GetBits(6) != 63)
				{
					return false;
				}
				ParallelismType = (uint8)BitReader.GetBits(2);
				if (BitReader.GetBits(6) != 63)
				{
					return false;
				}
				ChromaFormat = (uint8)BitReader.GetBits(2);
				if (BitReader.GetBits(5) != 31)
				{
					return false;
				}
				BitDepthLumaMinus8 = (uint8)BitReader.GetBits(3);
				if (BitReader.GetBits(5) != 31)
				{
					return false;
				}
				BitDepthChromaMinus8 = (uint8)BitReader.GetBits(3);
				AverageFrameRate = (uint16)BitReader.GetBits(16);
				ConstantFrameRate = (uint8)BitReader.GetBits(2);
				NumTemporalLayers = (uint8)BitReader.GetBits(3);
				TemporalIdNested = (uint8)BitReader.GetBits(1);
				NALUnitLengthMinus1 = (uint8)BitReader.GetBits(2);
				uint32 numArrays = BitReader.GetBits(8);
				Arrays.Reserve(numArrays);
				for(uint32 i=0; i<numArrays; ++i)
				{
					FArray &a = Arrays.AddDefaulted_GetRef();
					a.Completeness = (uint8)BitReader.GetBits(1);
					if (BitReader.GetBits(1) != 0)
					{
						return false;
					}
					a.NALUnitType = (uint8)BitReader.GetBits(6);
					uint32 numNALUs = BitReader.GetBits(16);
					a.NALUs.Reserve(numNALUs);
					for(uint32 j=0; j<numNALUs; ++j)
					{
						uint16 naluLen = (uint16)BitReader.GetBits(16);
						TArray<uint8> &n = a.NALUs.AddDefaulted_GetRef();
						n.Reserve(naluLen);
						for(uint32 k=0; k<naluLen; ++k)
						{
							n.Emplace((uint8)BitReader.GetBits(8));
						}
					}
				}
				if (BitReader.GetRemainingBits() != 0)
				{
					return false;
				}

				// CSD
				CodecSpecificData.Empty();
				for(int32 i=0; i<Arrays.Num(); ++i)
				{
					const FArray& a = Arrays[i];
					// SPS nut?
					if (a.NALUnitType == 33)
					{
						FISO23008_2_seq_parameter_set_data sps;
						if (ParseH265SPS(sps, a.NALUs[0].GetData(), a.NALUs[0].Num()))
						{
							ParsedSPSs.Emplace(MoveTemp(sps));
						}
					}

					for(int32 j=0; j<a.NALUs.Num(); ++j)
					{
						CodecSpecificData.Add(0);
						CodecSpecificData.Add(0);
						CodecSpecificData.Add(0);
						CodecSpecificData.Add(1);
						CodecSpecificData.Append(a.NALUs[j]);
					}
				}
				return true;
			}
			return false;
		}



		static int32 FindStartCode(const uint8* InData, SIZE_T InDataSize, int32& NALUnitLength)
		{
			for(const uint8* Data = InData; InDataSize >= 3; ++Data, --InDataSize)
			{
				if (Data[0] == 0 && Data[1] == 0 && (Data[2] == 1 || (InDataSize >= 4 && Data[2] == 0 && Data[3] == 1)))
				{
					NALUnitLength = Data[2] ? 3 : 4;
					return Data - reinterpret_cast<const uint8*>(InData);
				}
			}
			NALUnitLength = -1;
			return -1;
		}



		void ParseBitstreamForNALUs(TArray<FNaluInfo>& outNALUs, const void* InBitstream, uint64 InBitstreamLength)
		{
			outNALUs.Reset();

			uint64 Pos = 0;
			uint64 BytesToGo = InBitstreamLength;
			const uint8* BitstreamData = (const uint8*)InBitstream;
			while(1)
			{
				int32 UnitLength = 0, StartCodePos = FindStartCode(BitstreamData, BytesToGo, UnitLength);
				if (StartCodePos >= 0)
				{
					if (outNALUs.Num())
					{
						outNALUs.Last().Size = StartCodePos;
						outNALUs.Last().Type = *BitstreamData;
					}
					FNaluInfo n;
					n.Offset = Pos + StartCodePos;
					n.Size = 0;
					n.Type = 0;
					n.UnitLength = UnitLength;
					outNALUs.Push(n);
					BitstreamData = Electra::AdvancePointer(BitstreamData, StartCodePos + UnitLength);
					Pos += StartCodePos + UnitLength;
					BytesToGo -= StartCodePos + UnitLength;
				}
				else
				{
					if (outNALUs.Num())
					{
						outNALUs.Last().Size = BytesToGo;
						outNALUs.Last().Type = *BitstreamData;
					}
					break;
				}
			}

		}



		int32 EBSPtoRBSP(uint8* OutBuf, const uint8* InBuf, int32 NumBytesIn)
		{
			uint8* OutBase = OutBuf;
			while(NumBytesIn-- > 0)
			{
				uint8 b = *InBuf++;
				*OutBuf++ = b;
				if (b == 0)
				{
					if (NumBytesIn > 1)
					{
						if (InBuf[0] == 0x00 && InBuf[1] == 0x03)
						{
							*OutBuf++ = 0x00;
							InBuf += 2;
							NumBytesIn -= 2;
						}
					}
				}
			}
			return OutBuf - OutBase;
		}



		template <typename T>
		struct FScopedDataPtr
		{
			FScopedDataPtr(void* Addr)
				: Data(static_cast<T*>(Addr))
			{
			}
			~FScopedDataPtr()
			{
				FMemory::Free(static_cast<void*>(Data));
			}
			operator T* ()
			{
				return Data;
			}
			T* Data;
		};




		bool ParseH264SPS(FISO14496_10_seq_parameter_set_data& OutSPS, const void* Data, int32 Size)
		{
			struct FSyntaxElement
			{
				static uint32 ue_v(FBitDataStream& BitStream)
				{
					int32 lz = -1;
					for(uint32 b = 0; b == 0; ++lz)
					{
						b = BitStream.GetBits(1);
					}
					if (lz)
					{
						return ((1 << lz) | BitStream.GetBits(lz)) - 1;
					}
					return 0;
				}
				static int32 se_v(FBitDataStream& BitStream)
				{
					uint32 c = ue_v(BitStream);
					return c & 1 ? int32((c + 1) >> 1) : -int32((c + 1) >> 1);
				}
			};

			// SPS is usually an EBSP so we need to strip it down.
			FScopedDataPtr<uint8> RBSP(FMemory::Malloc(Size));
			int32 RBSPsize = EBSPtoRBSP(RBSP, static_cast<const uint8*>(Data), Size);
			FBitDataStream BitReader(RBSP, RBSPsize);

			FMemory::Memzero(OutSPS);

			uint8 nalUnitType = (uint8)BitReader.GetBits(8);
			if ((nalUnitType & 0x1f) != 0x7)	// SPS NALU?
			{
				return false;
			}

			OutSPS.profile_idc = (uint8)BitReader.GetBits(8);
			OutSPS.constraint_set0_flag = (uint8)BitReader.GetBits(1);
			OutSPS.constraint_set1_flag = (uint8)BitReader.GetBits(1);
			OutSPS.constraint_set2_flag = (uint8)BitReader.GetBits(1);
			OutSPS.constraint_set3_flag = (uint8)BitReader.GetBits(1);
			OutSPS.constraint_set4_flag = (uint8)BitReader.GetBits(1);
			OutSPS.constraint_set5_flag = (uint8)BitReader.GetBits(1);
			BitReader.SkipBits(2);
			OutSPS.level_idc = (uint8)BitReader.GetBits(8);
			OutSPS.seq_parameter_set_id = FSyntaxElement::ue_v(BitReader);
			if (OutSPS.profile_idc == 100 || OutSPS.profile_idc == 110 || OutSPS.profile_idc == 122 || OutSPS.profile_idc == 244 ||
				OutSPS.profile_idc == 44 || OutSPS.profile_idc == 83 || OutSPS.profile_idc == 86 || OutSPS.profile_idc == 118 || OutSPS.profile_idc == 128)
			{
				OutSPS.chroma_format_idc = FSyntaxElement::ue_v(BitReader);
				if (OutSPS.chroma_format_idc == 3)
				{
					OutSPS.separate_colour_plane_flag = (uint8)BitReader.GetBits(1);
				}
				OutSPS.bit_depth_luma_minus8 = FSyntaxElement::ue_v(BitReader);
				OutSPS.bit_depth_chroma_minus8 = FSyntaxElement::ue_v(BitReader);
				OutSPS.qpprime_y_zero_transform_bypass_flag = (uint8)BitReader.GetBits(1);
				OutSPS.seq_scaling_matrix_present_flag = (uint8)BitReader.GetBits(1);
				if (OutSPS.seq_scaling_matrix_present_flag)
				{
					auto scaling_list = [&BitReader](int32* scalingList, int32 sizeOfScalingList, bool& useDefaultScalingMatrixFlag) -> void
					{
						int32 lastScale = 8;
						int32 nextScale = 8;
						for(int32 j=0; j<sizeOfScalingList; ++j)
						{
							if (nextScale)
							{
								int32 delta_scale = FSyntaxElement::se_v(BitReader);
								nextScale = (lastScale + delta_scale + 256) % 256;
								useDefaultScalingMatrixFlag = (j == 0 && nextScale == 0);
							}
							scalingList[j] = (nextScale == 0) ? lastScale : nextScale;
							lastScale = scalingList[j];
						}
					};

					// Skip over the scaling matrices.
					int32 dummyScalingMatrix[64] = {0};
					bool bDummyDefaultScalingMatrixFlag = false;
					for(int32 i=0, iMax=OutSPS.chroma_format_idc!=3?8:12; i<iMax; ++i)
					{
						uint8 seq_scaling_list_present_flag = (uint8)BitReader.GetBits(1);
						if (seq_scaling_list_present_flag)
						{
							if (i < 6)
							{
								scaling_list(dummyScalingMatrix, 16, bDummyDefaultScalingMatrixFlag);
							}
							else
							{
								scaling_list(dummyScalingMatrix, 64, bDummyDefaultScalingMatrixFlag);
							}
						}
					}
				}
			}
			OutSPS.log2_max_frame_num_minus4 = FSyntaxElement::ue_v(BitReader);
			OutSPS.pic_order_cnt_type = FSyntaxElement::ue_v(BitReader);
			if (OutSPS.pic_order_cnt_type == 0)
			{
				OutSPS.log2_max_pic_order_cnt_lsb_minus4 = FSyntaxElement::ue_v(BitReader);
			}
			else if (OutSPS.pic_order_cnt_type == 1)
			{
				OutSPS.delta_pic_order_always_zero_flag = FSyntaxElement::ue_v(BitReader);
				OutSPS.offset_for_non_ref_pic = FSyntaxElement::se_v(BitReader);
				OutSPS.offset_for_top_to_bottom_field = FSyntaxElement::se_v(BitReader);
				OutSPS.num_ref_frames_in_pic_order_cnt_cycle = FSyntaxElement::ue_v(BitReader);
				for(uint32 i = 0; i < OutSPS.num_ref_frames_in_pic_order_cnt_cycle; ++i)
				{
					FSyntaxElement::se_v(BitReader);		// discard
				}
			}
			OutSPS.max_num_ref_frames = FSyntaxElement::ue_v(BitReader);
			OutSPS.gaps_in_frame_num_value_allowed_flag = (uint8)BitReader.GetBits(1);
			OutSPS.pic_width_in_mbs_minus1 = FSyntaxElement::ue_v(BitReader);
			OutSPS.pic_height_in_map_units_minus1 = FSyntaxElement::ue_v(BitReader);
			OutSPS.frame_mbs_only_flag = (uint8)BitReader.GetBits(1);
			if (!OutSPS.frame_mbs_only_flag)
			{
				OutSPS.mb_adaptive_frame_field_flag = (uint8)BitReader.GetBits(1);
			}
			OutSPS.direct_8x8_inference_flag = (uint8)BitReader.GetBits(1);
			OutSPS.frame_cropping_flag = (uint8)BitReader.GetBits(1);
			if (OutSPS.frame_cropping_flag)
			{
				OutSPS.frame_crop_left_offset = FSyntaxElement::ue_v(BitReader);
				OutSPS.frame_crop_right_offset = FSyntaxElement::ue_v(BitReader);
				OutSPS.frame_crop_top_offset = FSyntaxElement::ue_v(BitReader);
				OutSPS.frame_crop_bottom_offset = FSyntaxElement::ue_v(BitReader);
			}
			OutSPS.vui_parameters_present_flag = (uint8)BitReader.GetBits(1);
			if (OutSPS.vui_parameters_present_flag)
			{
				OutSPS.aspect_ratio_info_present_flag = (uint8)BitReader.GetBits(1);
				if (OutSPS.aspect_ratio_info_present_flag)
				{
					OutSPS.aspect_ratio_idc = (uint8)BitReader.GetBits(8);
					if (OutSPS.aspect_ratio_idc == 255)
					{
						OutSPS.sar_width = (uint16)BitReader.GetBits(16);
						OutSPS.sar_height = (uint16)BitReader.GetBits(16);
					}
				}
				OutSPS.overscan_info_present_flag = (uint8)BitReader.GetBits(1);
				if (OutSPS.overscan_info_present_flag)
				{
					OutSPS.overscan_appropriate_flag = (uint8)BitReader.GetBits(1);
				}
				OutSPS.video_signal_type_present_flag = (uint8)BitReader.GetBits(1);
				if (OutSPS.video_signal_type_present_flag)
				{
					OutSPS.video_format = (uint8)BitReader.GetBits(3);
					OutSPS.video_full_range_flag = (uint8)BitReader.GetBits(1);
					OutSPS.colour_description_present_flag = (uint8)BitReader.GetBits(1);
					if (OutSPS.colour_description_present_flag)
					{
						OutSPS.colour_primaries = (uint8)BitReader.GetBits(8);
						OutSPS.transfer_characteristics = (uint8)BitReader.GetBits(8);
						OutSPS.matrix_coefficients = (uint8)BitReader.GetBits(8);
					}
				}
				OutSPS.chroma_loc_info_present_flag = (uint8)BitReader.GetBits(1);
				if (OutSPS.chroma_loc_info_present_flag)
				{
					OutSPS.chroma_sample_loc_type_top_field = FSyntaxElement::ue_v(BitReader);
					OutSPS.chroma_sample_loc_type_bottom_field = FSyntaxElement::ue_v(BitReader);
				}
				OutSPS.timing_info_present_flag = (uint8)BitReader.GetBits(1);
				if (OutSPS.timing_info_present_flag)
				{
					OutSPS.num_units_in_tick = BitReader.GetBits(32);
					OutSPS.time_scale = BitReader.GetBits(32);
					OutSPS.fixed_frame_rate_flag = (uint8)BitReader.GetBits(1);
				}
				// The remainder is of no interest to us at the moment.
			}
			return true;
		}


		//! Parses a H.265 (ISO/IEC 23008-2) SPS NALU.
		bool ParseH265SPS(FISO23008_2_seq_parameter_set_data& OutSPS, const void* Data, int32 Size)
		{
			struct FSyntaxElement
			{
				static uint32 ue_v(FBitDataStream& BitStream)
				{
					int32 lz = -1;
					for(uint32 b = 0; b == 0; ++lz)
					{
						b = BitStream.GetBits(1);
					}
					if (lz)
					{
						return ((1 << lz) | BitStream.GetBits(lz)) - 1;
					}
					return 0;
				}
				static int32 se_v(FBitDataStream& BitStream)
				{
					uint32 c = ue_v(BitStream);
					return c & 1 ? int32((c + 1) >> 1) : -int32((c + 1) >> 1);
				}
			};

			// SPS is usually an EBSP so we need to strip it down.
			FScopedDataPtr<uint8> RBSP(FMemory::Malloc(Size));
			int32 RBSPsize = EBSPtoRBSP(RBSP, static_cast<const uint8*>(Data), Size);
			FBitDataStream BitReader(RBSP, RBSPsize);

			FMemory::Memzero(OutSPS);

			if (BitReader.GetBits(1) != 0)	// forbidden_zero_bit
				return false;
			if (BitReader.GetBits(6) != 33)	// sps_nut ?
				return false;
			BitReader.SkipBits(6);			// nuh_layer_id
			BitReader.SkipBits(3);			// nuh_temporal_id_plus1

			OutSPS.sps_video_parameter_set_id = BitReader.GetBits(4);
			OutSPS.sps_max_sub_layers_minus1 = BitReader.GetBits(3);
			OutSPS.sps_temporal_id_nesting_flag = BitReader.GetBits(1);

			// profile_tier_level( sps_max_sub_layers_minus1 )
			OutSPS.general_profile_space = BitReader.GetBits(2);
			OutSPS.general_tier_flag = BitReader.GetBits(1);
			OutSPS.general_profile_idc = BitReader.GetBits(5);
			for(int32 i=0; i<32; ++i)
			{
				OutSPS.general_profile_compatibility_flag = (OutSPS.general_profile_compatibility_flag << 1) | BitReader.GetBits(1);
			}
			OutSPS.general_progressive_source_flag = BitReader.GetBits(1);
			OutSPS.general_interlaced_source_flag = BitReader.GetBits(1);
			OutSPS.general_non_packed_constraint_flag = BitReader.GetBits(1);
			OutSPS.general_frame_only_constraint_flag = BitReader.GetBits(1);
			// We skip over the next 44 bits that vary in layout depending on general_profile_idc and general_profile_compatibility_flag.
			// For the moment none of the flags interest us, so we do not parse them for simplicity.
			OutSPS.general_reserved_zero_44bits = ((uint64)BitReader.GetBits(32) << 32) | BitReader.GetBits(12);

			OutSPS.general_level_idc = BitReader.GetBits(8);
			for(uint32 i=0; i<OutSPS.sps_max_sub_layers_minus1; ++i)
			{
				OutSPS.sub_layer_profile_present_flag[i] = BitReader.GetBits(1);
				OutSPS.sub_layer_level_present_flag[i] = BitReader.GetBits(1);
			}
			if (OutSPS.sps_max_sub_layers_minus1 > 0)
			{
				for(uint32 i = OutSPS.sps_max_sub_layers_minus1; i<8; ++i)
				{
					BitReader.SkipBits(2);		// reserved_zero_2bits[ i ]
				}
			}
			for(uint32 i=0; i<OutSPS.sps_max_sub_layers_minus1; ++i)
			{
				// Similarly, we skip all of these flags that are of no interest to us right now.
				if (OutSPS.sub_layer_profile_present_flag[i])
				{
					BitReader.SkipBits(2);		// sub_layer_profile_space[ i ]
					BitReader.SkipBits(1);		// sub_layer_tier_flag[ i ]
					BitReader.SkipBits(5);		// sub_layer_profile_idc[ i ]
					BitReader.SkipBits(32);		// for( j = 0; j < 32; j++ ) sub_layer_profile_compatibility_flag[ i ][ j ] u(1)
					BitReader.SkipBits(1);		// sub_layer_progressive_source_flag[ i ]
					BitReader.SkipBits(1);		// sub_layer_interlaced_source_flag[ i ]
					BitReader.SkipBits(1);		// sub_layer_non_packed_constraint_flag[ i ]
					BitReader.SkipBits(1);		// sub_layer_frame_only_constraint_flag[ i ]
					BitReader.SkipBits(44);		// sub_layer_reserved_zero_44bits[ i ]
				}
				if (OutSPS.sub_layer_level_present_flag[i])
				{
					BitReader.SkipBits(8);		// sub_layer_level_idc[ i ]
				}
			}

			OutSPS.sps_seq_parameter_set_id = FSyntaxElement::ue_v(BitReader);
			OutSPS.chroma_format_idc = FSyntaxElement::ue_v(BitReader);
			if (OutSPS.chroma_format_idc == 3)
			{
				OutSPS.separate_colour_plane_flag = BitReader.GetBits(1);			// separate_colour_plane_flag
			}
			OutSPS.pic_width_in_luma_samples = FSyntaxElement::ue_v(BitReader);
			OutSPS.pic_height_in_luma_samples = FSyntaxElement::ue_v(BitReader);

			OutSPS.conformance_window_flag = BitReader.GetBits(1);
			if (OutSPS.conformance_window_flag)
			{
				OutSPS.conf_win_left_offset = FSyntaxElement::ue_v(BitReader);
				OutSPS.conf_win_right_offset = FSyntaxElement::ue_v(BitReader);
				OutSPS.conf_win_top_offset = FSyntaxElement::ue_v(BitReader);
				OutSPS.conf_win_bottom_offset = FSyntaxElement::ue_v(BitReader);
			}
			OutSPS.bit_depth_luma_minus8 = FSyntaxElement::ue_v(BitReader);
			OutSPS.bit_depth_chroma_minus8 = FSyntaxElement::ue_v(BitReader);
			OutSPS.log2_max_pic_order_cnt_lsb_minus4 = FSyntaxElement::ue_v(BitReader);
			OutSPS.sps_sub_layer_ordering_info_present_flag = BitReader.GetBits(1);
			for(uint32 i=(OutSPS.sps_sub_layer_ordering_info_present_flag ? 0 : OutSPS.sps_max_sub_layers_minus1); i<=OutSPS.sps_max_sub_layers_minus1; ++i)
			{
				OutSPS.sps_max_dec_pic_buffering_minus1[i] = FSyntaxElement::ue_v(BitReader);
				OutSPS.sps_max_num_reorder_pics[i] = FSyntaxElement::ue_v(BitReader);
				OutSPS.sps_max_latency_increase_plus1[i] = FSyntaxElement::ue_v(BitReader);
			}
			OutSPS.log2_min_luma_coding_block_size_minus3 = FSyntaxElement::ue_v(BitReader);
			OutSPS.log2_diff_max_min_luma_coding_block_size = FSyntaxElement::ue_v(BitReader);
			OutSPS.log2_min_transform_block_size_minus2 = FSyntaxElement::ue_v(BitReader);
			OutSPS.log2_diff_max_min_transform_block_size = FSyntaxElement::ue_v(BitReader);
			OutSPS.max_transform_hierarchy_depth_inter = FSyntaxElement::ue_v(BitReader);
			OutSPS.max_transform_hierarchy_depth_intra = FSyntaxElement::ue_v(BitReader);
			OutSPS.scaling_list_enabled_flag = BitReader.GetBits(1);
			if (OutSPS.scaling_list_enabled_flag)
			{
				// Parse the scaling list into temp variables only. We do not need them right now.
				OutSPS.sps_scaling_list_data_present_flag = BitReader.GetBits(1);
				if (OutSPS.sps_scaling_list_data_present_flag)
				{
					// scaling_list_data( )
					for(uint32 sizeId=0; sizeId<4; ++sizeId)
					{
						for(int32 matrixId=0; matrixId<((sizeId==3)?2:6); ++matrixId)
						{
							uint8 scaling_list_pred_mode_flag = BitReader.GetBits(1);	// [ sizeId ][ matrixId ] u(1)
							if (!scaling_list_pred_mode_flag/*[sizeId][matrixId]*/)
							{
								uint32 scaling_list_pred_matrix_id_delta/*[sizeId][matrixId]*/ = FSyntaxElement::ue_v(BitReader);
							}
							else
							{
								//nextCoef = 8;
								uint32 coefNum = Utils::Min((uint32)64U, (uint32)(1 << (4 + (sizeId << 1))));
								if (sizeId > 1)
								{
									uint32 scaling_list_dc_coef_minus8/*[sizeId ? 2][matrixId]*/ = FSyntaxElement::se_v(BitReader);
									//nextCoef = scaling_list_dc_coef_minus8[sizeId ? 2][matrixId] + 8;
								}
								for(uint32 i=0; i<coefNum; i++)
								{
									uint32 scaling_list_delta_coef = FSyntaxElement::se_v(BitReader);
									//nextCoef = ( nextCoef + scaling_list_delta_coef + 256 ) % 256
									//ScalingList[ sizeId ][ matrixId ][ i ] = nextCoef
								}
							}
						}
					}
				}
			}

			OutSPS.amp_enabled_flag = BitReader.GetBits(1);
			OutSPS.sample_adaptive_offset_enabled_flag = BitReader.GetBits(1);
			OutSPS.pcm_enabled_flag = BitReader.GetBits(1);
			if (OutSPS.pcm_enabled_flag)
			{
				OutSPS.pcm_sample_bit_depth_luma_minus1 = BitReader.GetBits(4);
				OutSPS.pcm_sample_bit_depth_chroma_minus1 = BitReader.GetBits(4);
				OutSPS.log2_min_pcm_luma_coding_block_size_minus3 = FSyntaxElement::ue_v(BitReader);
				OutSPS.log2_diff_max_min_pcm_luma_coding_block_size = FSyntaxElement::ue_v(BitReader);
				OutSPS.pcm_loop_filter_disabled_flag = BitReader.GetBits(1);
			}

			OutSPS.num_short_term_ref_pic_sets = FSyntaxElement::ue_v(BitReader);
			uint32 unused_num_delta_pocs[64] = {0};
			for(uint32 i=0; i<OutSPS.num_short_term_ref_pic_sets; ++i)
			{
				struct short_term_ref_pic_set
				{
					static bool parse(uint32 stRpsIdx, uint32 num_short_term_ref_pic_sets, uint32* num_delta_pocs, FBitDataStream& BitReader)
					{
						uint32 inter_ref_pic_set_prediction_flag = stRpsIdx != 0 ? BitReader.GetBits(1) : 0;
						if (inter_ref_pic_set_prediction_flag)
						{
							uint32 delta_idx_minus1 = 0;
							if (stRpsIdx == num_short_term_ref_pic_sets)
							{
								delta_idx_minus1 = FSyntaxElement::ue_v(BitReader);
							}
							BitReader.SkipBits(1);	// delta_rps_sign
							uint32 abs_delta_rps_minus1 = FSyntaxElement::ue_v(BitReader);
							uint32 RefRpsIdx = stRpsIdx - (delta_idx_minus1 + 1);
							num_delta_pocs[stRpsIdx] = 0;
							for(uint32 j=0; j<=num_delta_pocs[RefRpsIdx]; ++j)
							{
								uint8 used_by_curr_pic_flag/*[j]*/ = BitReader.GetBits(1);
								uint8 use_delta_flag = 0;
								if (!used_by_curr_pic_flag/*[j]*/)
								{
									use_delta_flag = BitReader.GetBits(1);
								}
								if (used_by_curr_pic_flag || use_delta_flag)
								{
									++num_delta_pocs[stRpsIdx];
								}
							}
						}
						else
						{
							uint32 num_negative_pics = FSyntaxElement::ue_v(BitReader);
							uint32 num_positive_pics = FSyntaxElement::ue_v(BitReader);

							if (((uint64)num_positive_pics + (uint64)num_negative_pics) * 2 > BitReader.GetRemainingBits())
							{
								return false;
							}

							num_delta_pocs[stRpsIdx] = num_negative_pics + num_positive_pics;

							for(uint32 j=0; j<num_negative_pics; ++j)
							{
								uint32 delta_poc_s0_minus1/*[j]*/ = FSyntaxElement::ue_v(BitReader);
								(void)delta_poc_s0_minus1;
								BitReader.SkipBits(1);				// used_by_curr_pic_s0_flag[ j ]
							}
							for(uint32 j=0; j<num_positive_pics; ++j)
							{
								uint32 delta_poc_s1_minus1/*[j]*/ = FSyntaxElement::ue_v(BitReader);
								(void)delta_poc_s1_minus1;
								BitReader.SkipBits(1);				// used_by_curr_pic_s1_flag[ j ]
							}
						}
						return true;
					}
				};
				if (!short_term_ref_pic_set::parse(i, OutSPS.num_short_term_ref_pic_sets, unused_num_delta_pocs, BitReader))
				{
					return false;
				}
			}
			
			uint8 long_term_ref_pics_present_flag = BitReader.GetBits(1);
			if (long_term_ref_pics_present_flag)
			{
				uint32 num_long_term_ref_pics_sps = FSyntaxElement::ue_v(BitReader);
				for(uint32 i=0; i<num_long_term_ref_pics_sps; ++i)
				{
					uint32 lt_ref_pic_poc_lsb_sps = FSyntaxElement::ue_v(BitReader);	//[ i ] u(v)
					BitReader.SkipBits(1);							//	used_by_curr_pic_lt_sps_flag[ i ]
				}
			}
			
			OutSPS.sps_temporal_mvp_enabled_flag = BitReader.GetBits(1);
			OutSPS.strong_intra_smoothing_enabled_flag = BitReader.GetBits(1);
			OutSPS.vui_parameters_present_flag = BitReader.GetBits(1);
			if (OutSPS.vui_parameters_present_flag)
			{
				// vui_parameters()
				OutSPS.aspect_ratio_info_present_flag = BitReader.GetBits(1);
				if (OutSPS.aspect_ratio_info_present_flag)
				{
					OutSPS.aspect_ratio_idc = BitReader.GetBits(8);
					if (OutSPS.aspect_ratio_idc == 255)
					{
						OutSPS.sar_width = BitReader.GetBits(16);
						OutSPS.sar_height = BitReader.GetBits(16);
					}
				}
				OutSPS.overscan_info_present_flag = BitReader.GetBits(1);
				if (OutSPS.overscan_info_present_flag)
				{
					OutSPS.overscan_appropriate_flag = BitReader.GetBits(1);
				}
				OutSPS.video_signal_type_present_flag = BitReader.GetBits(1);
				if (OutSPS.video_signal_type_present_flag)
				{
					OutSPS.video_format = BitReader.GetBits(3);
					OutSPS.video_full_range_flag = BitReader.GetBits(1);
					OutSPS.colour_description_present_flag = BitReader.GetBits(1);
					if (OutSPS.colour_description_present_flag)
					{
						OutSPS.colour_primaries = BitReader.GetBits(8);
						OutSPS.transfer_characteristics = BitReader.GetBits(8);
						OutSPS.matrix_coeffs = BitReader.GetBits(8);
					}
				}
				OutSPS.chroma_loc_info_present_flag = BitReader.GetBits(1);
				if (OutSPS.chroma_loc_info_present_flag)
				{
					OutSPS.chroma_sample_loc_type_top_field = FSyntaxElement::ue_v(BitReader);
					OutSPS.chroma_sample_loc_type_bottom_field = FSyntaxElement::ue_v(BitReader);
				}
				OutSPS.neutral_chroma_indication_flag = BitReader.GetBits(1);
				OutSPS.field_seq_flag = BitReader.GetBits(1);
				OutSPS.frame_field_info_present_flag = BitReader.GetBits(1);
				OutSPS.default_display_window_flag = BitReader.GetBits(1);
				if (OutSPS.default_display_window_flag)
				{
					OutSPS.def_disp_win_left_offset = FSyntaxElement::ue_v(BitReader);
					OutSPS.def_disp_win_right_offset = FSyntaxElement::ue_v(BitReader);
					OutSPS.def_disp_win_top_offset = FSyntaxElement::ue_v(BitReader);
					OutSPS.def_disp_win_bottom_offset = FSyntaxElement::ue_v(BitReader);
				}
				OutSPS.vui_timing_info_present_flag = BitReader.GetBits(1);
				if (OutSPS.vui_timing_info_present_flag)
				{
					OutSPS.vui_num_units_in_tick = BitReader.GetBits(32);
					OutSPS.vui_time_scale = BitReader.GetBits(32);
					OutSPS.vui_poc_proportional_to_timing_flag = BitReader.GetBits(1);
					if (OutSPS.vui_poc_proportional_to_timing_flag)
					{
						OutSPS.vui_num_ticks_poc_diff_one_minus1 = FSyntaxElement::ue_v(BitReader);
// None of the following is of interest to us now, so we stop parsing at this point.
					}
				}
			return true;
			}
		return false;
		}


		uint64 FISO23008_2_seq_parameter_set_data::GetConstraintFlags() const
		{
			uint64 ConstraintFlags = ((uint64)((general_progressive_source_flag << 3) | (general_interlaced_source_flag << 2) | (general_non_packed_constraint_flag << 1) | general_frame_only_constraint_flag) << 44) | general_reserved_zero_44bits;
			return ConstraintFlags;
		}

		FString FISO23008_2_seq_parameter_set_data::GetRFC6381(const TCHAR* SampleTypePrefix) const
		{
			// As per ISO/IEC 14496-15:2014 Annex E.3
			uint32 cf = Utils::BitReverse32(general_profile_compatibility_flag);
			uint64 ConstraintFlags = GetConstraintFlags();
			FString cfs;
			bool bNonZero = false;
			for(int32 i=5; i>=0; --i, ConstraintFlags >>= 8)
			{
				if ((ConstraintFlags & 255) == 0 && !bNonZero)
				{
					continue;
				}
				bNonZero = true;
				cfs = FString::Printf(TEXT(".%02X"), (uint8)(ConstraintFlags & 255)) + cfs;
			}

			if (general_profile_space == 0)
			{
				return FString::Printf(TEXT("%s.%d.%X.%c%d%s"), SampleTypePrefix, general_profile_idc, cf, general_tier_flag ? TCHAR('H') : TCHAR('L'), general_level_idc, *cfs);
			}
			else
			{
				return FString::Printf(TEXT("%s.%c%d.%X.%c%d%s"), SampleTypePrefix, TCHAR('A')+general_profile_space-1, general_profile_idc, cf, general_tier_flag ? TCHAR('H') : TCHAR('L'), general_level_idc, *cfs);
			}
		}


	} // namespace MPEG
} // namespace Electra

