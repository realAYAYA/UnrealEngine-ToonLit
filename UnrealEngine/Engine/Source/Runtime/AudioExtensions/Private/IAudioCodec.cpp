// Copyright Epic Games, Inc. All Rights Reserved.

#include "IAudioCodec.h"
#include "Serialization/MemoryWriter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IAudioCodec)

namespace Audio
{
	// If < C++17, constexpr requires storage for linkage on Clang.
	constexpr FFcc FFormatDescriptorSection::kSectionName;
	constexpr FFcc FHeaderSection::kName;
	constexpr FFcc FSampleSection::kName;
	
	// Encoder INPUT object. (simple for now).
	template<typename TSampleType>
	struct TEncoderInputObject : public IEncoderInput
	{
		using FSampleType = TSampleType;
		TSampleBuffer<FSampleType> Samples;
		FFormat Format;

		TEncoderInputObject(TArrayView<const FSampleType> InSamples, const FFormat& InFormat)
			: Samples(InSamples.GetData(), InSamples.Num(), InFormat.NumChannels, InFormat.NumFramesPerSec)
			, Format(InFormat)
		{}
		
		TEncoderInputObject(const TSampleBuffer<int16>& InBuffer, const FFormat& InFormat)
			: Samples(InBuffer)
			, Format(InFormat)
		{}

		const TSampleBuffer<int16>& GetSamples() const override 
		{ 
			return Samples; 
		}
		FString GetHashForDDC() const override 
		{ 
			return FMD5::HashBytes((const uint8*)Samples.GetData(), Samples.GetNumFrames() * sizeof(FSampleType)); 
		}
		bool GetFormat(FFormat& OutFormat) const override 
		{ 
			OutFormat = Format; 
			return true; 
		}
	};
	
	TUniquePtr<IEncoderInput>
	IEncoderInput::Create(
		TArrayView<const int16> In16bitInterleave, const FFormat& InFmt )
	{				
		if (!audio_ensure(InFmt.NumChannels > 0))
		{
			return nullptr;		
		}
		if (!audio_ensure(InFmt.NumFramesPerSec > 0))
		{
			return nullptr;
		}
		return MakeUnique<TEncoderInputObject<int16>>(In16bitInterleave, InFmt);	
	}
		
	struct FEncoderOutputBase : public IEncoderOutput
	{
		TUniquePtr<FArchive> Archive;
		TUniquePtr<FNestedSection> HeaderSection;
		TUniquePtr<FNestedSection> SampleSection;
		TSet<FFcc> SectionNames;

		virtual ~FEncoderOutputBase() = default;

		FEncoderOutputBase(TUniquePtr<FArchive>&& InArchive)
			: Archive(MoveTemp(InArchive))
		{}

		// Named section "format" for example.
		bool WriteSection(
			FEncodedSectionBase& InOutSection) override
		{
			// Prevent unnamed sections.
			if (!audio_ensure(!InOutSection.SectionName.IsNone()))
			{
				return false;
			}
			
			// Section names must be unique.
			if (!audio_ensure(SectionNames.Find(InOutSection.SectionName)==nullptr))
			{
				return false;
			}
			
			// Make sure the archive is valid and is in write mode.
			if (audio_ensure(Archive.IsValid() && Archive->IsSaving()))
			{
				InOutSection.BeginSection(*Archive);
				InOutSection.Serialize(*Archive);
				InOutSection.EndSection(*Archive);
				SectionNames.Add(InOutSection.SectionName);
				return true;
			}
			return false;
		}

		// Header. (header is a bunch of named sections).
		bool BeginHeader() override
		{
			if (audio_ensure(!HeaderSection.IsValid()))
			{
				HeaderSection = MakeUnique<FHeaderSection>();
				HeaderSection->BeginSection(*Archive);
				return true;
			}
			return false;
		}
		bool EndHeader() override
		{
			if (audio_ensure(HeaderSection.IsValid()))
			{
				HeaderSection->EndSection(*Archive);
				HeaderSection.Reset();
				return true;
			}
			return false;
		}

		// Sample data.
		bool BeginSampleData() override
		{
			if (audio_ensure(!SampleSection.IsValid()))
			{
				SampleSection = MakeUnique<FSampleSection>();
				SampleSection->BeginSection(*Archive);
				return true;
			}
			return false;
		}
		bool EndSampleData() override
		{
			if (audio_ensure(SampleSection.IsValid()))
			{
				SampleSection->EndSection(*Archive);
				SampleSection.Reset();
				return true;
			}
			return false;
		}	

		bool WriteRaw(const void* InRawData, int32 InSize) override
		{
			if (!audio_ensure(InRawData) || !audio_ensure(InSize > 0))
			{
				return false;
			}
			if (!audio_ensure(Archive.IsValid()) || !audio_ensure(!Archive->IsError()) || !audio_ensure(Archive->IsSaving()))
			{
				return false;
			}
			Archive->Serialize(const_cast<void*>(InRawData),InSize);
			return Archive->IsError();
		}
	};
	
	// Encoder Output object. (simple for now).
	IEncoderOutput::FPtr 
	IEncoderOutput::Create(
		TArray<uint8>& OutExternalBuffer)
	{
		return MakeUnique<FEncoderOutputBase>(MakeUnique<FMemoryWriter>(OutExternalBuffer));
	}

	Audio::IEncoderOutput::FPtr IEncoderOutput::Create()
	{
		struct FArrayOutputObject : public FEncoderOutputBase
		{
			TArray<uint8> Buffer;
			FArrayOutputObject() : FEncoderOutputBase(MakeUnique<FMemoryWriter>(Buffer)) {}
		};
		return MakeUnique<FArrayOutputObject>();
	}

	TUniquePtr<IDecoderInput> IDecoderInput::Create(
		TArrayView<const uint8> InBytes, uint32 InOffsetInStream, FName InOldFormatName )
	{
		if (InOffsetInStream == 0)
		{
			if (!FDecoderInputArrayView::IsValidHeader(InBytes))
			{
				// Back compatibility of OLD data, these will be per codec. 
				return nullptr;
			}
		}		
		return MakeUnique<FDecoderInputArrayView>(InBytes, InOffsetInStream);
	}

	bool FDecoderInputBase::ParseSections(FArchive& Ar)
	{		
		// HEADER section.
		{
			// Make sure this looks sane.
			FHeaderSection Header;
			uint32 HeaderStart = Ar.Tell();
			Header.BeginSection(Ar);
			if (!audio_ensure(!Header.SectionName.IsNone()))
			{
				return false;
			}
			if (!audio_ensure(Header.SectionName == FHeaderSection::kName))
			{
				return false;
			}

			Toc.Add(Header.SectionName, HeaderStart);
			if (!audio_ensure(MakeTocNested(Ar, HeaderStart, Header)))
			{
				return false;
			}
			Header.EndSection(Ar);
		}

		// SAMPLES section.
		{		
			FSampleSection SampleSection;
			uint32 SamplesStart = Ar.Tell();
			SampleSection.BeginSection(Ar);
			if (!audio_ensure(!SampleSection.SectionName.IsNone()))
			{
				return false;
			}
			if (!audio_ensure(SampleSection.SectionName == FSampleSection::kName))
			{
				return false;
			}

			Toc.Add(SampleSection.SectionName, SamplesStart);
			SampleSection.EndSection(Ar);
		}
			
		// Looks sane.
		return true;
	}

	void FDecoderInputBase::SetArchive(FArchive* InArchive, uint32 InOffsetInStream)
	{
		if (audio_ensure(InArchive))
		{
			Archive = InArchive;
			OffsetInStream = InOffsetInStream;
			if (InOffsetInStream == 0)
			{
				bError = !ParseSections(*Archive);
			}
			else
			{
				Toc.Empty();
			}
		}
	}

	bool FDecoderInputBase::FindSection(FEncodedSectionBase& OutSection)
	{
		if (!audio_ensure(Archive->IsLoading()))
		{
			return false;
		}

		if (int32* pPos = Toc.Find(OutSection.SectionName))
		{
			Archive->Seek(*pPos);
			OutSection.BeginSection(*Archive);
			OutSection.Serialize(*Archive);
			OutSection.EndSection(*Archive);
			return true;
		}
		return false;
	}

	TArrayView<const uint8> FDecoderInputBase::PeekNextPacket(
		int32 InMaxPacketLength) const
	{		
		// TODO.
		check(false);
		return MakeArrayView(static_cast<const uint8*>(0), 0);
	}

	TArrayView<const uint8> FDecoderInputBase::PopNextPacket(
		int32 InPacketSize)
	{	
		if (PopBuffer.Num() < InPacketSize)
		{
			PopBuffer.SetNumZeroed(InPacketSize);
		}		
		if (!bSampleSection )
		{
			// To start serving packets, we must be in the sample section.
			if( !audio_ensure(BeginSampleData()))
			{
				// Fail.
				return MakeArrayView(static_cast<const uint8*>(0), 0);
			}
		}
		if (!audio_ensure(!Archive->IsError()))
		{
			return MakeArrayView(static_cast<const uint8*>(0), 0);
		}
		Archive->Serialize(PopBuffer.GetData(), InPacketSize);
		if (!audio_ensure(!Archive->IsError()))
		{
			return MakeArrayView(static_cast<const uint8*>(0), 0);
		}
		return MakeArrayView(PopBuffer.GetData(),InPacketSize);
	}
	
	bool FDecoderInputBase::BeginSampleData() 
	{	
		if (!audio_ensure(!bSampleSection))
		{
			return false;
		}		
		int32* pSampleDataStart = Toc.Find(FSampleSection::kName);
		if (!audio_ensure(pSampleDataStart))
		{
			return false;
		}
		Archive->Seek(*pSampleDataStart);
		FNestedSection Nested;
		Nested.BeginSection(*Archive);		
		if (!audio_ensure(Nested.SectionName == FSampleSection::kName))
		{
			return false;
		}
		bSampleSection = true;
		return true;
	}
	bool FDecoderInputBase::EndSampleData() 
	{	
		bSampleSection = false;
		return true;
	}

	bool FDecoderInputBase::MakeTocNested(FArchive &Ar, uint32 HeaderStart, FNestedSection &Header)
	{
		while (Ar.Tell() - HeaderStart < Header.SectionSize)
		{
			FNestedSection Sect;
			uint32_t Pos = Ar.Tell();
			Sect.BeginSection(Ar);

			if (!audio_ensure(!Sect.SectionName.IsNone()) ||
				!audio_ensure(Toc.Find(Sect.SectionName) == nullptr))
			{
				return false;
			}

			Toc.Add(Sect.SectionName, Pos);
			Sect.EndSection(Ar);
		}
		return true;
	}

	int64 FDecoderInputBase::Tell() const 
	{
		return Archive->Tell() + OffsetInStream;
	}

	FFormatDescriptorSection::FFormatDescriptorSection(
		FName InCodecName, 
		FName InCodecFamilyName, 
		uint32 InCodecVersion, 
		uint32 InNumChannels, 
		uint32 InNumFrames, 
		uint32 InNumFramesPerSec,
		uint32 InNumBytesPerPacket ) 
		: FEncodedSectionBase(kSectionName, kSectionVer) 
		, CodecName(InCodecName)
		, CodecFamilyName(InCodecFamilyName)
		, CodecVersion(InCodecVersion)
		, NumChannels(InNumChannels)
		, NumFrames(InNumFrames)
		, NumFramesPerSec(InNumFramesPerSec)
		, NumBytesPerPacket(InNumBytesPerPacket)
	{
	}

	void FFormatDescriptorSection::Serialize(FArchive& Ar)
	{
		Ar << CodecName;
		Ar << CodecFamilyName;
		Ar << CodecVersion;

		Ar << NumChannels;
		Ar << NumFrames;
		Ar << NumFramesPerSec;
	}

	bool FEncodedSectionBase::BeginSection(FArchive& Ar)
	{
		Ar << SectionName;
		Ar << SectionVersion;

		// Save where the size is in the stream, so we can return to it, once we know the size of the section.
		SizePos = Ar.Tell();
		Ar << SectionSize;
		return !Ar.IsError();
	}

	bool FEncodedSectionBase::EndSection(FArchive& Ar)
	{
		if (Ar.IsSaving())
		{
			uint64 EndPos = Ar.Tell();
			Ar.Seek(SizePos);
			SectionSize = (EndPos - SizePos); 
			Ar << SectionSize;
			Ar.Seek(EndPos);
		}
		else if(Ar.IsLoading())
		{
			Ar.Seek(SizePos + SectionSize);
		}
		return !Ar.IsError();
	}

	TUniquePtr<Audio::IDecoderOutput> IDecoderOutput::Create(
		const FRequirements& Requirements)
	{
		if (Requirements.DownstreamFormat == EBitRepresentation::Float32_Interleaved)
		{
			return MakeUnique<TCircularOutputBuffer<float>>(Requirements);
			//return MakeUnique<TDecoderOutputOwnBuffer<float>>(Requirements);
		}
		else if (Requirements.DownstreamFormat == EBitRepresentation::Int16_Interleaved)
		{
			return MakeUnique<TCircularOutputBuffer<int16>>(Requirements);
			//return MakeUnique<TDecoderOutputOwnBuffer<int16>>(Requirements);
		}
		checkNoEntry(); //-V779
		return nullptr;
	}

	TUniquePtr<Audio::IDecoderOutput> IDecoderOutput::Create(
		const FRequirements& Requirements, 
		TArrayView<uint8> InExternalMemory)
	{
		if (Requirements.DownstreamFormat == EBitRepresentation::Float32_Interleaved)
		{
			 auto Output = MakeUnique<TDecoderOutputArrayView<float>>(Requirements);
			 Output->SetArrayViewBytes(InExternalMemory);
			 return Output;
		}
		else if (Requirements.DownstreamFormat == EBitRepresentation::Int16_Interleaved)
		{
			auto Output = MakeUnique<TDecoderOutputArrayView<int16>>(Requirements);
			Output->SetArrayViewBytes(InExternalMemory);
			return Output;
		}
		checkNoEntry(); //-V779
		return nullptr;
	}

	bool FDecoderInputArrayView::IsValidHeader(
		TArrayView<const uint8> InBytes)
	{
		FMemoryReaderView Ar(InBytes);
		FHeaderSection Header;
		if (Header.BeginSection(Ar) && Header.SectionName == Header.kName && Header.EndSection(Ar))
		{
			return true;
		}
		return false;
	}
}

