// Copyright Epic Games, Inc. All Rights Reserved.

#include "PcmCodec.h"
#include "IAudioCodec.h"
#include "IAudioCodecRegistry.h"

#include "Templates/SubclassOf.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PcmCodec)

namespace Audio
{	
	class FPcmAudioCodec : public ICodec
	{
	public:
		static const uint32	CodecVersion = 2;

		struct FPcmHeader : public FEncodedSectionBase
		{
			static const uint32 kHeaderVer = 1;
			static constexpr FFcc NAME_PcmHeader = { 'P','c','m','H' };

			uint32 NumChannels = 0;
			TEnumAsByte<Audio::EBitRepresentation>	BitFormat = EBitRepresentation::Int16_Interleaved;

			FPcmHeader(uint32 InNumChannels = 0, EBitRepresentation InBitFormat = EBitRepresentation::Int16_Interleaved)
				: FEncodedSectionBase(NAME_PcmHeader, kHeaderVer)
				, NumChannels(InNumChannels)
				, BitFormat(InBitFormat)
			{}

			void Serialize(FArchive& Ar) override
			{
				Ar << BitFormat;
				Ar << NumChannels;
			}
		};

		struct FEncoder : public IEncoder
		{
			IEncoder::FSourcePtr Input;

			FEncoder(IEncoder::FSourcePtr InDataSource)
				: Input(InDataSource)
			{}

			TSubclassOf<UAudioCodecEncoderSettings> GetSettingsClass() const override
			{
				return UAudioPcmEncoderSettings::StaticClass();
			}

			// Given input and settings determine DDC key.
			FString GenerateDDCKey(
				FSourcePtr SrcObject,
				USettingsPtr SettingsObject) const override
			{
				check(SettingsObject);//.IsValid());

				FString SrcHash = SrcObject->GetHashForDDC();
				FString SettingsHash = SettingsObject->GetHashForDDC();			// Includes Version and Codec Name.
				uint32 BinaryLibsHash = /* Some hash for the binaries */ 12345;

				// Src + Settings + Codec.
				return FString::Printf(TEXT("AudioCodec-%s-%s-%u"), *SrcHash, *SettingsHash, BinaryLibsHash);
			}
									
			// Encode 
			bool Encode(
				FSourcePtr InSrc,
				FDestPtr InDst,
				USettingsPtr InSettingsObject ) const override
			{
				// Settings (use defaults if we were not fed anything).
				const UAudioPcmEncoderSettings* PcmSettings = Cast<UAudioPcmEncoderSettings>(InSettingsObject);
				if( !PcmSettings )
				{					
					PcmSettings = Cast<UAudioPcmEncoderSettings>(UAudioPcmEncoderSettings::StaticClass()->GetDefaultObject(true));
				}

				if (!InSrc || !InDst || !PcmSettings )
				{
					// TODO: Logging.
					return false;
				}

				// Query input for details.
				FDecodedFormatInfo Format;
				if (!InSrc->GetFormat(Format))
				{
					// TODO: Logging.
					return false;
				}

				const TSampleBuffer<int16>& SampleData = InSrc->GetSamples();

				// Start header.
				InDst->BeginHeader();
				{
					// Standard Descriptor Header, identifying codec and version of codec.
					FFormatDescriptorSection Desc{ 
						TEXT("PcmInternal"), 
						TEXT("Pcm"), 
						CodecVersion, 
						Format.NumChannels, 
						(uint32)SampleData.GetNumFrames(), 
						Format.NumFramesPerSec, 
						(uint32)sizeof(int16)*Format.NumChannels	// <-- FIXME. Each packet is just a frame, very small.
					};
					InDst->WriteSection(Desc);

					// Format specific header.
					FPcmHeader PcmHeader(Format.NumChannels, Format.BitRep);
					InDst->WriteSection(PcmHeader);
				}								
				InDst->EndHeader();

				//uint32 NumFramesPerPacket = 1;// (PcmSettings->PacketSizeBytes / sizeof(int16)) / SampleData.GetNumChannels();
				//uint32 NumFramesRemaining = SampleData.GetNumFrames();
				//uint32 NumFramesPerBlock = NumFramesPerPacket * PcmSettings->PacketsPerBlock;
				
				TArrayView<const int16> SampleAV = SampleData.GetArrayView();
				uint32 SampleIndex = 0;

				// Sample Data (hello world v1, write everything as one big blast).
				InDst->BeginSampleData();			
				InDst->WriteRaw(SampleAV.GetData(), SampleAV.Num()*SampleAV.GetTypeSize() );
				InDst->EndSampleData();

			/*	while (NumFramesRemaining > 0)
				{
					uint32 NumFramesToWrite = FMath::Min(NumFramesRemaining, NumFramesPerPacket);
					TArrayView<const int16> PacketView = SampleAV.Slice(SampleIndex, NumFramesToWrite * SampleData.GetNumChannels());

					InDst->WritePacket(PacketView.GetData(), PacketView.Num()*PacketView.GetTypeSize());

					NumFramesRemaining -= NumFramesToWrite;
					SampleIndex += NumFramesToWrite * SampleData.GetNumChannels();
				}*/				

				// Success.
				return true;
			}
		};

		template<typename TSampleType>
		struct TDecoder : public IDecoder
		{
			bool bHasError = false;
			FDecoderInputPtr Src;
			FDecoderOutputPtr Dst;
			FPcmHeader Header;
			FFormatDescriptorSection Desc;
			IDecoderOutput::FRequirements Reqs;
			int32 PlayPositionOffsetFrames = 0;
		
			TDecoder( 
				FDecoderInputPtr InSrc,
				FDecoderOutputPtr InDst,
				const FPcmHeader& InPcmHeader,
				const FFormatDescriptorSection& InFormatDesc,
				const IDecoderOutput::FRequirements& InRequirements )
				: Src(InSrc)
				, Dst(InDst)
				, Header(InPcmHeader)
				, Desc(InFormatDesc)
				, Reqs(InRequirements)
			{
			}

			bool HasError() const override
			{
				return bHasError;
			}	

			FDecodeReturn Decode(bool bIsLooping = true) override
			{
				// Reject bad input
				if( !audio_ensure(Src) || !audio_ensure(Dst) || !audio_ensure(Header.NumChannels > 0))
				{
					return FDecodeReturn { FDecodeReturn::Fail };
				}

				constexpr int32 NumFramesPerPacket = 1;
				int32 PacketSizeBytes = (sizeof(TSampleType) * Header.NumChannels) * NumFramesPerPacket;
				int32 FramesRemaining = Reqs.NumSampleFramesWanted;	
				IDecoderOutput::FPushedAudioDetails Details(Desc.NumFramesPerSec,Desc.NumChannels, PlayPositionOffsetFrames);

				while(!Src->IsEndOfStream() && FramesRemaining > 0)
				{
					TArrayView<const uint8> PacketBytes = Src->PopNextPacket(PacketSizeBytes);	
					bool bLastPacket = PacketBytes.Num() == PacketSizeBytes;

					TArrayView<const TSampleType> PacketPcm = MakeArrayView(
						reinterpret_cast<const TSampleType*>(PacketBytes.GetData()), PacketBytes.Num() / sizeof(TSampleType));

					Details.SampleFramesStartOffset = PlayPositionOffsetFrames;
					Dst->PushAudio(Details, PacketPcm);
					
					FramesRemaining -= PacketPcm.Num();
					PlayPositionOffsetFrames += PacketPcm.Num();
				}
				if( Src->IsEndOfStream() )
				{
					return FDecodeReturn::Finished;
				}
				return FDecodeReturn::MoreDataRemaining;
			}
		};

		FPcmAudioCodec() = default;
		virtual ~FPcmAudioCodec() = default;

		bool SupportsPlatform(FName InPlatformName) const override
		{
			// All platforms support PCM.
			return true;
		}
		const FCodecDetails& GetDetails() const override
		{
			static FCodecDetails sDeets = FCodecDetails({
				TEXT("PcmInternal"),
				TEXT("Pcm"),
				CodecVersion,
				FCodecFeatures({
					FCodecFeatures::HasEncoder,
					FCodecFeatures::HasDecoder,
					FCodecFeatures::Seekable,
					FCodecFeatures::Streamable,
					FCodecFeatures::Surround
				})
			});
			return sDeets;
		}
		FEncoderPtr CreateEncoder(
			IEncoder::FSourcePtr InDataSource) override
		{
			return FEncoderPtr(new FEncoder(InDataSource));
		}

		virtual FDecoderPtr CreateDecoder(
			IDecoder::FDecoderInputPtr InSrc,
			IDecoder::FDecoderOutputPtr InDst )
		{
			FFormatDescriptorSection Desc;
			if (!audio_ensure(InSrc->FindSection(Desc)))
			{
				return nullptr;
			}
			FPcmHeader PcmHeader;
			if( !audio_ensure(InSrc->FindSection(PcmHeader)) )
			{
				return nullptr;
			}
			
			FDecodedFormatInfo Info;
			const IDecoderOutput::FRequirements& Reqs = InDst->GetRequirements(Info);

			switch( PcmHeader.BitFormat )
			{
			case EBitRepresentation::Float32_Interleaved:
			{
				return FDecoderPtr( new TDecoder<float>( InSrc, InDst, PcmHeader, Desc, Reqs ) );
			}
			case EBitRepresentation::Int16_Interleaved:
			{
				return FDecoderPtr( new TDecoder<int16>( InSrc, InDst, PcmHeader, Desc, Reqs ) );
			}
			default:
				checkNoEntry();
			}

			// Fail.
			return nullptr;
		}
	};

	TUniquePtr<Audio::ICodec> Create_PcmCodec()
	{
		return MakeUnique<FPcmAudioCodec>();
	}

	// Clang < C++ 17 requires constexp require storage for linkage
	constexpr FFcc FPcmAudioCodec::FPcmHeader::NAME_PcmHeader;

} // namespace Audio

//UAudioPcmEncoderSettings::UAudioPcmEncoderSettings(
//	const FObjectInitializer& ObjectInitializer) 
//	: Super(ObjectInitializer)
//	, Version(1)
//{}

FString UAudioPcmEncoderSettings::GetHashForDDC() const
{
	//// Do this with reflection. (one probably exists already).
	//// Also do base types in base class.
	//uint32 Hash = HashCombine(
	//	HashCombine(
	//		GetTypeHash(PacketsPerBlock),
	//		GetTypeHash(TestTweaker)
	//	),
	//	HashCombine(
	//		GetTypeHash(MaxChannelsSupported),
	//		GetTypeHash(PacketSizeBytes)
	//	)
	//);
	int32 Hash = 123456789; // FIXME.
	return FString::FromInt((int32)Hash);
}

