// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utilities/UtilsMPEGAudio.h"
#include "BitDataStream.h"

namespace Electra
{
	namespace MPEG
	{
		namespace AACUtils
		{
			int32 GetNumberOfChannelsFromChannelConfiguration(uint32 InChannelConfiguration)
			{
				static const uint8 NumChannelsForConfig[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 7, 8, 0, 8, 0 };
				return InChannelConfiguration < 16 ? NumChannelsForConfig[InChannelConfiguration] : 0;
			}
		}


		namespace AACParseHelper
		{
			static uint32 GIndexToSampleRate[16] =
			{
				96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 0, 0, 0, 0
			};

			static uint32 GetAudioObjectType(FBitDataStream& Bitstream)
			{
				uint32 ObjectType = Bitstream.GetBits(5);
				if (ObjectType == 31)
				{
					ObjectType = 32 + Bitstream.GetBits(6);
				}
				return ObjectType;
			}
			static uint32 GetSamplingRateIndex(FBitDataStream& Bitstream, uint32& Rate)
			{
				uint32 SamplingFrequencyIndex = Bitstream.GetBits(4);
				if (SamplingFrequencyIndex == 15)
				{
					Rate = Bitstream.GetBits(24);
				}
				else
				{
					Rate = GIndexToSampleRate[SamplingFrequencyIndex];
				}
				return SamplingFrequencyIndex;
			}
			static void GetGASpecificConfig(FBitDataStream& Bitstream, uint32 SamplingFrequencyIndex, uint32 ChannelConfiguration, uint32 AudioObjectType)
			{
				int32 FrameLengthFlag = Bitstream.GetBits(1);
				int32 DependsOnCoreDecoder = Bitstream.GetBits(1);
				if (DependsOnCoreDecoder)
				{
					Bitstream.SkipBits(14);		// coreCoderDelay
				}
				int32 ExtensionFlag = Bitstream.GetBits(1);
				if (ChannelConfiguration == 0)
				{
					// TODO:
				}
				if (AudioObjectType == 6 || AudioObjectType == 20)
				{
					Bitstream.SkipBits(3);	// layerNr
				}
				if (ExtensionFlag)
				{
					if (AudioObjectType == 22)
					{
						Bitstream.SkipBits(5);	// numOfSubFrame
						Bitstream.SkipBits(11);	// layer_length
					}
					if (AudioObjectType == 17 || AudioObjectType == 19 || AudioObjectType == 20 || AudioObjectType == 23)
					{
						Bitstream.SkipBits(1);	// aacSectionDataResilienceFlag
						Bitstream.SkipBits(1);	// aacScalefactorDataResilienceFlag;
						Bitstream.SkipBits(1);	// aacSpectralDataResilienceFlag;
					}
					int32 ExtensionFlag3 = Bitstream.GetBits(1);
					if (ExtensionFlag)
					{
						// TODO:
					}
				}
			}

		} // namespace AACParseHelper


		FAACDecoderConfigurationRecord::FAACDecoderConfigurationRecord()
		{
			Reset();
		}

		void FAACDecoderConfigurationRecord::Reset()
		{
			SBRSignal = -1;
			PSSignal = -1;
			ChannelConfiguration = 0;
			SamplingFrequencyIndex = 0;
			SamplingRate = 0;
			ExtSamplingFrequencyIndex = 0;
			ExtSamplingFrequency = 0;
			AOT = 0;
			ExtAOT = 0;
		}


		const TArray<uint8>& FAACDecoderConfigurationRecord::GetCodecSpecificData() const
		{
			return CodecSpecificData;
		}

		bool FAACDecoderConfigurationRecord::ParseFrom(const void* Data, int64 Size)
		{
			CodecSpecificData.Empty();
			CodecSpecificData.SetNumUninitialized(Size);
			FMemory::Memcpy(CodecSpecificData.GetData(), Data, Size);

			FBitDataStream bsp(Data, Size);
			SBRSignal = -1;
			PSSignal = -1;
			SamplingRate = 0;
			ExtSamplingFrequency = 0;
			ExtSamplingFrequencyIndex = 0;
			ExtAOT = 0;
			AOT = AACParseHelper::GetAudioObjectType(bsp);
			SamplingFrequencyIndex = AACParseHelper::GetSamplingRateIndex(bsp, SamplingRate);
			ChannelConfiguration = bsp.GetBits(4);

			if (AOT == 5 /*SBR*/ || AOT == 29 /*PS*/)
			{
				ExtAOT = AOT;
				SBRSignal = 1;
				if (AOT == 29)
				{
					PSSignal = 1;
				}

				ExtSamplingFrequencyIndex = AACParseHelper::GetSamplingRateIndex(bsp, ExtSamplingFrequency);
				AOT = AACParseHelper::GetAudioObjectType(bsp);
			}
			// Handle supported AOT configs
			if (AOT == 2 /*LC*/)		// Only LC for now
			{
				AACParseHelper::GetGASpecificConfig(bsp, SamplingFrequencyIndex, ChannelConfiguration, AOT);
			}
			// Would need to handle epConfig here now for a couple of AOT's we are NOT supporting
			// ...

			// Check for backward compatible SBR signaling.
			if (ExtAOT != 5)
			{
				while (bsp.GetRemainingBits() > 15)
				{
					int32 syncExtensionType = bsp.PeekBits(11);
					if (syncExtensionType == 0x2b7)
					{
						bsp.SkipBits(11);
						ExtAOT = AACParseHelper::GetAudioObjectType(bsp);
						if (ExtAOT == 5)
						{
							SBRSignal = bsp.GetBits(1);
							if (SBRSignal)
							{
								ExtSamplingFrequencyIndex = AACParseHelper::GetSamplingRateIndex(bsp, ExtSamplingFrequency);
							}
						}
						if (bsp.GetRemainingBits() > 11 && bsp.GetBits(11) == 0x548)
						{
							PSSignal = bsp.GetBits(1);
						}
						break;
					}
					else
					{
						bsp.SkipBits(1);
					}
				}
			}

			int64 remainingBits = bsp.GetRemainingBits();
			MEDIA_UNUSED_VAR(remainingBits);
			return true;
		}
	} // namespace MPEG
} // namespace Electra

#if 0

struct ADTSheader
{
	// Refer to http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio
	unsigned mSyncWord : 12;		//!< All set
	unsigned mVersion : 1;			//!< 0=MPEG-4, 1=MPEG-2
	unsigned mLayer : 2;			//!< 0
	unsigned mProtAbsent : 1;		//!< 0 if CRC present, 1 if CRC absent
	unsigned mProfile : 2;			//!< MPEG-4 audio object type -1
	unsigned mSmpFrqIndex : 4;		//!< MPEG-4 sampling frequency index (15 is forbidden)
	unsigned mPrivateStream : 1;
	unsigned mChannelCfg : 3;		//!< MPEG-4 channel configuration
	unsigned mOriginality : 1;
	unsigned mHome : 1;
	unsigned mCopyrighted : 1;
	unsigned mCopyrightStart : 1;
	unsigned mFrameLength : 13;
	unsigned mBufferFullness : 11;
	unsigned mNumFrames : 2;
	unsigned mCRCifPresent : 16;
};

static bool GetADTSHeader(ADTSheader& header, const void* pData, int64 nDataSize)
{
	if (nDataSize < 7)
	{
		return false;
	}

	FBitDataStream bs(pData, nDataSize);
	header.mSyncWord = bs.GetBits(12);
	header.mVersion = bs.GetBits(1);
	header.mLayer = bs.GetBits(2);
	header.mProtAbsent = bs.GetBits(1);
	header.mProfile = bs.GetBits(2);
	header.mSmpFrqIndex = bs.GetBits(4);
	header.mPrivateStream = bs.GetBits(1);
	header.mChannelCfg = bs.GetBits(3);
	header.mOriginality = bs.GetBits(1);
	header.mHome = bs.GetBits(1);
	header.mCopyrighted = bs.GetBits(1);
	header.mCopyrightStart = bs.GetBits(1);
	header.mFrameLength = bs.GetBits(13);
	header.mBufferFullness = bs.GetBits(11);
	header.mNumFrames = bs.GetBits(2);
	if (nDataSize >= 9 && !header.mProtAbsent)
	{
		header.mCRCifPresent = bs.GetBits(16);
	}
	return header.mSyncWord == 0xfff && header.mLayer == 0;
}

/*
According to the Audio Coding Wiki, which is no longer available as of June, 2010, the ADIF format actually is just one header at the beginning of the AAC file.
And the rest of the data are consecutive raw data blocks.
This file format is meant for simple local storing purposes unlike ADTS or LATM which are meant for streaming AAC.
The ADIF header is made up of the following Field name/Field size in bits/Comment (separated by semicolons):
	adif_id/32/Always: "ADIF";
	copyright_id_present/1/[none];
	copyright_id/72/only if copyright_id_present == 1;
	original_copy/1/[none];
	home/1/[none];
	bitstream_type/1/0: CBR, 1: VBR; bitrate/23/for CBR: bitrate, for VBR: peak bitrate, 0 means unknown;
	num_program_config_elements/4/[none].
	The next 2 fields come (num_program_config_elements+1) times: buffer_fullness/20/only if bitstream_type == 0; program_config_element/VAR/[none].
*/


#endif

