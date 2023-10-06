// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Video/VideoDecoder.h"
#include "Video/CodecUtils/CodecUtilsH265.h"

enum class EPictureUsage : uint8
{
	UNUSED			= 0u,
	SHORT_TERM		= 1u << 0,
	LONG_TERM		= 1u << 1,		
};

ENUM_CLASS_FLAGS(EPictureUsage);

/*
 * Configuration settings for H265 decoders.
 */
struct FVideoDecoderConfigH265 : public FVideoDecoderConfig
{
	FVideoDecoderConfigH265(EAVPreset Preset = EAVPreset::Default)
		: FVideoDecoderConfig(Preset)
	{
	}

	AVCODECSCORE_API FAVResult Parse(TSharedRef<FAVInstance> const& Instance, FVideoPacket const& Packet, TArray<TSharedPtr<UE::AVCodecCore::H265::FNaluH265>>& Slices);

	AVCODECSCORE_API FAVResult UpdateScalingLists(TSharedRef<UE::AVCodecCore::H265::FNaluSlice> CurrentSlice);

	AVCODECSCORE_API FAVResult UpdateRPS(TSharedRef<UE::AVCodecCore::H265::FNaluSlice> CurrentSlice);

	TMap<uint32, TSharedPtr<UE::AVCodecCore::H265::FNaluVPS>> ParsedVPS = {};
	TMap<uint32, TSharedPtr<UE::AVCodecCore::H265::FNaluSPS>> ParsedSPS = {};
	TMap<uint32, TSharedPtr<UE::AVCodecCore::H265::FNaluPPS>> ParsedPPS = {};
	TArray<TSharedPtr<UE::AVCodecCore::H265::FNaluSEI>> ParsedSEI = {};

	// Decoder state
	int32 CurrPicIdx = -1;
	int32 LastRpsIdx = 0;
	int32 LastPicOrderCntValue = 0;

	// Scaling lists
	uint8 ScalingList4x4[6][16];
	uint8 ScalingList8x8[6][16];
	uint8 ScalingList16x16[6][16];
	uint8 ScalingList32x32[6][16];

	int16 ScalingListDCCoeff4x4[6];
	int16 ScalingListDCCoeff8x8[6];
	int16 ScalingListDCCoeff16x16[6];
	int16 ScalingListDCCoeff32x32[6];
	
	struct FReferencePictureSet
	{
		uint8 NumPicTotalCurr;
		
		int32 PicOrderCntVal[16] = { 0 };
		uint8 PicAvaliability[16] = { 0 };
		EPictureUsage PicUsage[16]  = { EPictureUsage::UNUSED };
		uint8 PicLayerId[16] = { 0 };
		
		uint8 NumPocStCurrBefore;
		int32 PocStCurrBefore[16] = { 0 };
		uint8 RefPicSetStCurrBefore[8] = { 0 };
		
		uint8 NumPocStCurrAfter;
		int32 PocStCurrAfter[16] = { 0 };
		uint8 RefPicSetStCurrAfter[8] = { 0 };

		uint8 NumPocStFoll;
		int32 PocStFoll[16] = { 0 };
		uint8 RefPicSetStFoll[16] = { 0 };

		uint8 NumPocLtCurr;
		int32 PocLtCurr[16] = { 0 };
		uint8 RefPicSetLtCurr[16] = { 0 };

		uint8 NumPocLtFoll;
		int32 PocLtFoll[16] = { 0 };
		uint8 RefPicSetLtFoll[16] = { 0 };

		uint8 RefPicList0[16] = { 0 };
		uint8 RefPicList1[16] = { 0 };

		uint8 RefPicSetInterLayer0[8] = { 0 };
		uint8 RefPicSetInterLayer1[8] = { 0 };

		void PrepState()
		{
			NumPocStCurrBefore = 0;
			FMemory::Memset( PocStCurrBefore, 0, sizeof(PocStCurrBefore));
			
			NumPocStCurrAfter = 0;
			FMemory::Memset( PocStCurrAfter, 0, sizeof(PocStCurrAfter));
			
			NumPocStFoll = 0;
			FMemory::Memset( PocStFoll, 0, sizeof(PocStFoll));
			
			NumPocLtCurr = 0;
			FMemory::Memset( PocLtCurr, 0, sizeof(PocLtCurr));
			
			NumPocLtFoll = 0;
			FMemory::Memset( PocLtFoll, 0, sizeof(PocLtFoll));

			NumPicTotalCurr = 0;

			FMemory::Memset( PicUsage, 0, sizeof(PicUsage));

			FMemory::Memset( PicOrderCntVal, 0, sizeof(PicOrderCntVal));
		}
	} ReferencePictureSet;
};

DECLARE_TYPEID(FVideoDecoderConfigH265, AVCODECSCORE_API);
