// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/Dsp.h"
#include "Features/IModularFeatures.h"
#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogMacros.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "Math/VectorRegister.h"
#include "Misc/AssertionMacros.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "SampleBuffer.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryReader.h"
#include "Templates/Casts.h"
#include "Templates/TypeHash.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"

#include <initializer_list>

#include "IAudioCodec.generated.h"

template <class TClass> class TSubclassOf;

AUDIOEXTENSIONS_API DECLARE_LOG_CATEGORY_EXTERN(LogAudioCodec, Display, All);

// TODO: Move this somewhere better
#ifdef AUDIO_ENSURE_NO_BREAKS
	#define audio_ensure(EXP) (!!(EXP))
#else //AUDIO_ENSURE_NO_BREAKS
	#define audio_ensure(EXP) ensure(EXP)
#endif //AUDIO_VERIFY_NO_BREAKS

// Forward declares.
class USoundWave;

#pragma region Decoder

namespace Audio
{
	// Forward declares
	struct FEncodedSectionBase;
	struct FMixerSourceVoiceBuffer;

	// Four character code class.
	class FFcc
	{
	public:
		FFcc() = default;

		constexpr FFcc(const char One, const char Two, const char Three, const char Four)
		{
			Value = static_cast<uint32>(One) |
				(static_cast<uint32>(Two) << 8) |
				(static_cast<uint32>(Three) << 16) |
				(static_cast<uint32>(Four) << 24);
		}

		constexpr bool operator==(const FFcc& InRhs) const
		{
			return Value == InRhs.Value;
		}
		friend const FFcc& operator<<(FArchive& Ar, FFcc& InFcc)
		{
			Ar << InFcc.Value;
			return InFcc;
		}
		constexpr bool IsNone() const { return Value==0; }
		friend uint32 GetTypeHash(FFcc InVal)
		{
			return ::GetTypeHash(InVal.Value);
		}
	private:
		uint32 Value = 0;
	};

	// TODO: Move this somewhere central.
	template<typename From, typename To>
	static constexpr float GetConvertScalar()
	{
		if (TIsSame<From, To>::Value)
		{
			return 1.0f;
		}
		else if (TIsSame<From, int16>::Value && TIsSame<To, float>::Value)
		{
			return 1.f / (static_cast<float>(TNumericLimits<int16>::Max()) + 1.f);
		}
		else if (TIsSame<From, float>::Value && TIsSame<To, int16>::Value)
		{
			return static_cast<float>(TNumericLimits<int16>::Max()) + 1.f;
		}
	}
		
	enum EBitRepresentation
	{
		Int16_Interleaved,
		Float32_Interleaved
	};

	struct AUDIOEXTENSIONS_API FDecodedFormatInfo 
	{		
		uint32 NumChannels = 0;
		uint32 NumFramesPerSec = 0;
		EBitRepresentation BitRep = Int16_Interleaved;

		int32 LoopStartOffset = INDEX_NONE;	// Where the loop start point is in the entire stream
		int32 LoopEndOffset = INDEX_NONE;	// Where the loop end point is in the entire stream
	};
	
	struct AUDIOEXTENSIONS_API IDecoderInput
	{
		// Factory.
		static TUniquePtr<IDecoderInput> Create(
			TArrayView<const uint8> InBytes, uint32 InOffsetOfBytes, FName InOldFormatName=NAME_None );
						
		virtual ~IDecoderInput() {}

		virtual bool HasError() const = 0;
		virtual bool IsEndOfStream() const = 0;

		virtual bool FindSection(
			FEncodedSectionBase& OutSection) = 0;

		virtual int64 Tell() const = 0;

		virtual bool SeekToTime(const float InSeconds) { return false; }

		virtual TArrayView<const uint8> PeekNextPacket(
			int32 InMaxPacketLength) const = 0;
				
		virtual TArrayView<const uint8> PopNextPacket(
			int32 InPacketSize ) = 0;
	};
	
	struct AUDIOEXTENSIONS_API IDecoderOutput
	{	
		struct FRequirements
		{
			EBitRepresentation DownstreamFormat	= Float32_Interleaved;	
			int32 NumSampleFramesWanted			= 256;
			int32 NumSampleFramesPerSecond		= 48000;
			int32 NumChannels                   = 1;
		};

		// Factory.
		static TUniquePtr<IDecoderOutput> Create( 
			const FRequirements& Requirements );

		// Factory.
		static TUniquePtr<IDecoderOutput> Create(
			const FRequirements& Requirements, TArrayView<uint8> InExternalMemory );
		
		struct FPushedAudioDetails
		{
			int32 NumFramesPerSec;					
			int32 NumChannels;

			int32 SampleFramesStartOffset;				// Where these samples are from in the entire stream

			uint32 bContainsLoopPoint : 1;				// True if *this* set of samples contains the loop point
			uint32 bHasFinished : 1;					// True if this the last of the data
			uint32 bHasError : 1;						// True if the decoder has encountered an error

			FPushedAudioDetails(
				int32 InFramesPerSec = 0,
				int32 InNumChannels = 0,
				int32 InSampleOffset = 0,
				bool In_bContainsLoopPoint = false,
				bool In_bHasFinished = false,
				bool In_bHasError = false )			
				: NumFramesPerSec(InFramesPerSec) 
				, NumChannels(InNumChannels)
				, SampleFramesStartOffset(InSampleOffset)
				, bContainsLoopPoint(In_bContainsLoopPoint)
				, bHasFinished(In_bHasFinished)
				, bHasError(In_bHasError)
			{}
		};

		virtual ~IDecoderOutput() = default;
		
		virtual FRequirements GetRequirements(
			const FDecodedFormatInfo& InFormat) const = 0;
		
		virtual int32 PushAudio(
			const FPushedAudioDetails& InDetails, 
			TArrayView<const int16> In16BitInterleave ) = 0;

		virtual int32 PushAudio(
			const FPushedAudioDetails& InDetails,
			TArrayView<const float> InFloat32Interleave) = 0;

		virtual int32 PopAudio( 
			TArrayView<int16> InExternalBuffer,
			FPushedAudioDetails& OutDetails ) = 0;

		virtual int32 PopAudio(
			TArrayView<float> OutExternalBuffer,
			FPushedAudioDetails& OutDetails ) = 0;		

		// Convenience convert TArray -> TArrayView
		int32 PopAudio(
			TArray<int16>& InExternalBuffer,
			FPushedAudioDetails& OutDetails)
		{
			return PopAudio(MakeArrayView(InExternalBuffer.GetData(), InExternalBuffer.Num()), OutDetails);
		}
		int32 PopAudio(
			TArray<float>& InExternalBuffer,
			FPushedAudioDetails& OutDetails)
		{
			return PopAudio(MakeArrayView(InExternalBuffer.GetData(), InExternalBuffer.Num()), OutDetails);
		}
	};
		

	// Decode input wrappers.

	struct AUDIOEXTENSIONS_API FCodecFeatures
	{	
		enum EFeatureBitField
		{
			HwDecode,
			HwResample,
		
			Seekable,
			Streamable,
			Surround,
			SeamlessLooping,
		
			HasDecoder,
			HasEncoder,
		};
		uint32_t FeaturesBitField = 0;		// Store as bitfield.

		bool HasFeature(EFeatureBitField InBit) const { return ( FeaturesBitField & (uint32)InBit); };

		constexpr FCodecFeatures(std::initializer_list<EFeatureBitField> InitList)
		{
			for (EFeatureBitField i : InitList)
			{
				FeaturesBitField |= 1 << i;
			}
		}
	};

	struct AUDIOEXTENSIONS_API IDecoder
	{		
		enum EDecodeResult
		{
			Fail,					// Decoder failed somehow.
			MoreDataRemaining,		// Data has produced and there's more remaining
			Looped,					// Data has produced and we've also looped
			Busy,					// The decoder may not have produced anything, and is currently busy
			Finished				// The decoder has finished and can be deleted
		};

		using FDecoderInputPtr = IDecoderInput*;
		using FDecoderOutputPtr = IDecoderOutput*;
		using FDecodeReturn = EDecodeResult;
					
		virtual ~IDecoder() = default;
		virtual bool HasError() const = 0;

		// Do the decode.
		virtual FDecodeReturn Decode(bool bIsLooping = false) = 0;	
	};

	struct AUDIOEXTENSIONS_API FEncodedSectionBase
	{
		// Serialized members.
		FFcc SectionName;
		uint32 SectionSize = 0;
		uint32 SectionVersion = 0;

		// This is not serialized and is used for accounting during serialization
		uint32 SizePos = 0;

		FEncodedSectionBase(FFcc InSectionName,uint32 InVersion) 
			: SectionName(InSectionName)
			, SectionVersion(InVersion)
		{}

		virtual ~FEncodedSectionBase() {}

		virtual bool BeginSection(FArchive& Ar);
		virtual bool EndSection(FArchive& Ar);
		virtual void Serialize(FArchive& Ar) = 0;
	};		

	struct AUDIOEXTENSIONS_API FFormatDescriptorSection : public FEncodedSectionBase
	{
		static constexpr FFcc kSectionName = { 'D','e','s','c' };
		static const uint32 kSectionVer = 2;
		
		// Codec name, family and version.
		FName CodecName;
		FName CodecFamilyName;
		uint32 CodecVersion = 0;

		// Info on the compressed data
		uint32 NumChannels			= 0;
		uint32 NumFrames			= 0;
		uint32 NumFramesPerSec		= 0;
		uint32 NumBytesPerPacket	= 0;
				
		FFormatDescriptorSection() 
			: FEncodedSectionBase(kSectionName,kSectionVer)
		{}
		
		FFormatDescriptorSection(
			FName InCodecName,
			FName InCodecFamilyName,
			uint32 InCodecVersion,
			uint32 InNumChannels,
			uint32 InNumSamples,
			uint32 InNumSamplesPerSec,
			uint32 InNumBytesPerPacket
		);

		bool operator==(const FFormatDescriptorSection& Rhs) const 
		{			
			return CodecName		==	Rhs.CodecName
			&& CodecFamilyName		==	Rhs.CodecFamilyName
			&& CodecVersion			==	Rhs.CodecVersion
			&& NumChannels			==	Rhs.NumChannels
			&& NumFrames			==	Rhs.NumFrames
			&& NumFramesPerSec		==	Rhs.NumFramesPerSec;		
		}
		
		void Serialize(FArchive& Ar) override;
	};

	struct AUDIOEXTENSIONS_API FNestedSection : public FEncodedSectionBase
	{
		FNestedSection(FFcc InName = FFcc())  : FEncodedSectionBase(InName, 0) {}
		void Serialize(FArchive& Ar) override {}
	};

	struct AUDIOEXTENSIONS_API FHeaderSection : public FNestedSection
	{
		static constexpr FFcc kName = {'H','d','r','!' };
		FHeaderSection() : FNestedSection(kName) {}
		void Serialize(FArchive& Ar) override {}
	};
	struct AUDIOEXTENSIONS_API FSampleSection : public FNestedSection
	{
		static constexpr FFcc kName = {'S','m','p','l'};
		FSampleSection() : FNestedSection(kName) {}
		void Serialize(FArchive& Ar) override {}
	};
	
	struct AUDIOEXTENSIONS_API FRawSection : public FEncodedSectionBase
	{
		TArrayView<const uint8> Buffer;
		FRawSection(FFcc InName, TArrayView<const uint8> InBuffer = TArrayView<const uint8>() )
			: FEncodedSectionBase(InName, 0) 
			, Buffer(InBuffer)
		{}

		void Serialize(FArchive& Ar) override {}
	};

	class AUDIOEXTENSIONS_API FDecoderInputBase : public Audio::IDecoderInput
	{
	protected:
		FArchive* Archive = nullptr;
		TArray<uint8> PopBuffer;
		TMap<FFcc, int32> Toc;
		uint32 OffsetInStream = 0;

		bool bSampleSection = false;
		bool bError = false;

		bool SeekToSamplesStart(
			FArchive& Ar );
		
		bool ParseSections(
			FArchive& Ar);

		bool MakeTocNested(
			FArchive &Ar, uint32 HeaderStart, FNestedSection &Header);

	public: 
		void SetArchive(
			FArchive* InArchive, uint32 InOffsetInStream);

		bool FindSection(
			FEncodedSectionBase& OutSection) override;

		bool HasError() const override { return bError && Archive->IsError(); }
		bool IsEndOfStream() const override { return Archive->AtEnd(); }

		bool BeginSampleData();		
		bool EndSampleData();

		TArrayView<const uint8> PeekNextPacket(
			int32 InMaxPacketLength) const override;

		TArrayView<const uint8> PopNextPacket(
			int32 InPacketSize) override;	

		// We need this for back compatibility.
		int64 Tell() const override;
	};

	struct AUDIOEXTENSIONS_API FDecoderInputArrayView : public FDecoderInputBase
	{
		TUniquePtr<FArchive> Reader;

		static bool IsValidHeader(
			TArrayView<const uint8> InBytes);

		void SetArrayViewBytes(TArrayView<const uint8> InBytes, uint32 InOffsetInStream)
		{
			Reader = MakeUnique<FMemoryReaderView>(InBytes);
			SetArchive(Reader.Get(), InOffsetInStream);
		}
		
		// Pass in array view and we'll wrap around it
		FDecoderInputArrayView( 
			TArrayView<const uint8> InData, uint32 InOffsetInStream )
			: Reader(MakeUnique<FMemoryReaderView>(InData))
		{		
			SetArchive(Reader.Get(), InOffsetInStream);
		}

		// Pass in an archive
		FDecoderInputArrayView( 
			TUniquePtr<FArchive>&& InReader, uint32 InOffsetInStream )
			: Reader(MoveTemp(InReader))
		{
			SetArchive(Reader.Get(), InOffsetInStream);
		}
	};
		
	template<typename TSampleType>
	struct TDecoderOutputArrayView : public IDecoderOutput
	{
		FRequirements Reqs;
		TArrayView<TSampleType> Buffer;
		int64 Offset = 0;

		TDecoderOutputArrayView(const FRequirements& InReqs)
			: Reqs(InReqs)
			, Buffer()
		{}

		void SetArrayView(TArrayView<TSampleType> InBuffer)
		{
			Buffer = InBuffer;
		}
		void SetArrayViewBytes(TArrayView<uint8> InBytes)
		{
			SetArrayView(MakeArrayView(reinterpret_cast<TSampleType*>(InBytes.GetData()), InBytes.Num() / sizeof(TSampleType)));
		}

		FRequirements GetRequirements(
			const FDecodedFormatInfo& InFormat) const override
		{
			return Reqs;
		}	
		
		template<typename T> int32 PushAudioInternal(
			const FPushedAudioDetails& InDetails,
			TArrayView<const T> InAudio) 
		{
			const T* Src = InAudio.GetData();
			TSampleType* Dst = Buffer.GetData() + Offset;

			int32 AvailSpace = Buffer.Num() - Offset;
			int32 Num = FMath::Min(AvailSpace, InAudio.Num());

			if (TIsSame<TSampleType, T>::Value)
			{
				FMemory::Memcpy(Dst, Src, Num * sizeof(T));
			}
			else
			{
				constexpr float Scalar = GetConvertScalar<T,TSampleType>();
				for (int32 i = 0; i < Num; ++i)
				{
					*Dst++ = static_cast<TSampleType>(*Src++ * Scalar);
				}
			}

			Offset += Num;
			return Num;
		}

		int32 PushAudio(
			const FPushedAudioDetails& InDetails,
			TArrayView<const float> InFloat32Interleaved ) override
		{
			return PushAudioInternal(InDetails,InFloat32Interleaved );
		}
		
		int32 PushAudio(
			const FPushedAudioDetails& InDetails,
			TArrayView<const int16> In16BitInterleave) override
		{
			return PushAudioInternal(InDetails,In16BitInterleave);
		}
	
		// This a bit naive, but ok for proof of concept.
		template<typename T> int32 PopAudioInternal(
			TArrayView<T> InExternalBuffer )
		{
			check(InExternalBuffer.Num() > 0);
			if (TIsSame<T, TSampleType>::Value && Offset > 0)
			{
				int32 NumSamplesPopped = Offset;
				Offset = 0;
				NumSamplesPopped = FMath::Min(InExternalBuffer.Num(), NumSamplesPopped);
				TArrayView<TSampleType> AV = Buffer.Slice(0, NumSamplesPopped);
				FMemory::Memcpy(InExternalBuffer.GetData(), Buffer.GetData(), sizeof(T)* NumSamplesPopped);
				return NumSamplesPopped;
			}
			audio_ensure(false);
			return 0;
		}

		int32 PopAudio(
			TArrayView<float> InExternalFloat32Buffer,
			FPushedAudioDetails& OutDetails ) override
		{
			return PopAudioInternal(InExternalFloat32Buffer);
		}

		int32 PopAudio(
			TArrayView<int16> InExternalInt16Buffer,
			FPushedAudioDetails& OutDetails ) override
		{
			return PopAudioInternal(InExternalInt16Buffer);
		}	
	};

	template<typename T>
	struct TDecoderOutputOwnBuffer : public TDecoderOutputArrayView<T>
	{
		TArray<T, TAlignedHeapAllocator<SIMD_ALIGNMENT>> AlignedMemory;
		TDecoderOutputOwnBuffer(
			const IDecoderOutput::FRequirements& Requirements)
			: TDecoderOutputArrayView<T>(Requirements)
		{
			AlignedMemory.SetNumZeroed(Requirements.NumSampleFramesWanted);
			TDecoderOutputArrayView<T>::SetArrayView(MakeArrayView(AlignedMemory));
		}
	};

	template<typename TSampleType>
	class TCircularOutputBuffer : public Audio::IDecoderOutput
	{
		Audio::TCircularAudioBuffer<TSampleType> Buffer;
		FRequirements Reqs;
	public:
		TCircularOutputBuffer(
			const FRequirements& InReqs)
			: Buffer(InReqs.NumSampleFramesWanted * 4) // Double buffered & assume stereo for now.
			, Reqs(InReqs)
		{
		}

		FRequirements GetRequirements(const FDecodedFormatInfo& InFormat) const override
		{
			return Reqs;
		}

		template<typename T>
		int32 PushAudioInternal(
			const FPushedAudioDetails& InDetails, TArrayView<const T> InBuffer) 
		{
			if (TIsSame<T, TSampleType>::Value)
			{
				return Buffer.Push(reinterpret_cast<const TSampleType*>(InBuffer.GetData()), InBuffer.Num());
			}
			else
			{
				constexpr float Scalar = GetConvertScalar<T, TSampleType>();
				const T* Src = InBuffer.GetData();
				int32 Num = InBuffer.Num();

				// Convert 1 sample at a time, slow.
				for (int32 i = 0; i < Num; ++i)
				{
					TSampleType Val = static_cast<TSampleType>(static_cast<float>(*Src++ * Scalar));
					Buffer.Push(Val);
				}

				return Num;
			}
		}

		int32 PushAudio(const FPushedAudioDetails& InDetails, TArrayView<const int16> In16BitInterleave) override
		{
			return PushAudioInternal(InDetails, In16BitInterleave);
		}
		int32 PushAudio(const FPushedAudioDetails& InDetails, TArrayView<const float> InFloat32Interleave) override
		{
			return PushAudioInternal(InDetails, InFloat32Interleave);
		}
		int32 PopAudio(TArrayView<int16> InExternalInt16Buffer, FPushedAudioDetails& OutDetails) override
		{
			if( TIsSame<TSampleType, int16>::Value )
			{
				return Buffer.Pop(reinterpret_cast<TSampleType*>(InExternalInt16Buffer.GetData()), InExternalInt16Buffer.Num());
			}
			audio_ensure(false);
			return 0;
		}

		int32 PopAudio(TArrayView<float> InExternalFloat32Buffer, FPushedAudioDetails& OutDetails) override
		{
			if( TIsSame<TSampleType, float>::Value )
			{
				return Buffer.Pop(reinterpret_cast<TSampleType*>(InExternalFloat32Buffer.GetData()), InExternalFloat32Buffer.Num());
			}
			audio_ensure(false);
			return 0;			
		}
	};
};

#pragma endregion Decoder

#pragma region Encoder

UCLASS(Abstract)
class AUDIOEXTENSIONS_API UAudioCodecEncoderSettings : public UObject
{
public:
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = 0;

	// TODO: Ideally we could use reflection generate a hash of all the settings.
	// ... But we could just override this per type.
	virtual FString GetHashForDDC() const PURE_VIRTUAL(UAudioCodecEncoderSettings::GetHashForDDC, return TEXT(""););
};	

namespace Audio
{	
	class AUDIOEXTENSIONS_API IEncoderOutput
	{
	public:
		using FPtr = TUniquePtr<IEncoderOutput>;
		virtual ~IEncoderOutput() = default;

		// Create one.
		static FPtr Create();
		static FPtr Create(TArray<uint8>& OutBuffer);

		// Named section "format" for example.
		virtual bool WriteSection(FEncodedSectionBase& InSection) = 0;
		
		virtual bool WriteRaw(const void* InRawData, int32 InSize) = 0;

		// Header. (header is a bunch of named sections).
		virtual bool BeginHeader() = 0;
		virtual bool EndHeader() = 0;

		// Sample data. (a series of named/chunks).
		virtual bool BeginSampleData() = 0;
		virtual bool EndSampleData() = 0;
	};

	class AUDIOEXTENSIONS_API IEncoderInput
	{
	public:
		using FFormat = FDecodedFormatInfo;

		// Factory.
		static TUniquePtr<IEncoderInput> Create(
			TArrayView<const int16> InSamples,
			const FFormat& InInfo
		);

		virtual ~IEncoderInput() {}

		// Calculate hash for input source.
		virtual FString GetHashForDDC() const = 0;

		// Get Format.
		virtual bool GetFormat(
			FFormat& OutFormat) const = 0;

		virtual const TSampleBuffer<int16>& 
			GetSamples() const = 0;
	};

	struct AUDIOEXTENSIONS_API IEncoder
	{
		virtual ~IEncoder() {}

		// Get the class used for this encoder. 
		virtual TSubclassOf<UAudioCodecEncoderSettings> GetSettingsClass() const = 0;

		// Types.
		using FSourcePtr = IEncoderInput * ;
		using FDestPtr = IEncoderOutput * ;
		using USettingsPtr = const UAudioCodecEncoderSettings *;

		// look at the ISoundfieldFormat IEncodingSoundfieldSetttingProxy! :)

		// Given input and settings determine DDC key.
		virtual FString GenerateDDCKey(
			FSourcePtr Src,
			USettingsPtr SettingsObject) const = 0;

		// Encode 
		virtual bool Encode(
			FSourcePtr InSrc,
			FDestPtr InDst,
			USettingsPtr InSettingsObject) const = 0;
	};
}

#pragma endregion Encoder

#pragma region Codec

namespace Audio
{
	struct AUDIOEXTENSIONS_API FCodecDetails
	{
		FName Name;								// Unique name for this implementation. e.g. "OpusXbox"
		FName FamilyName;						// Family name e.g. "Opus". Same as old Format name.	
		uint32 Version = 0;						// For Compatibility (all codecs in this family will be compatible with this data).	
		FCodecFeatures Features;				// If defined, is the feature set for this codec 	

		FString ToString() const { return FString::Printf(TEXT("%s:%s:%d"), *FamilyName.ToString(), *Name.ToString(), Version); }
		
		bool operator==(const FCodecDetails& Rhs) const 
		{ 
			return Rhs.FamilyName == FamilyName &&
				   Rhs.Name == Name &&
				   Rhs.Version == Version;
		}

		FCodecDetails(FName InName, FName InFamilyName, uint32 InVersion, const FCodecFeatures& InFeatures )
			: Name(InName), FamilyName(InFamilyName), Version(InVersion), Features(InFeatures) 
		{}
	};

	struct AUDIOEXTENSIONS_API ICodec 
	{
		virtual ~ICodec() {}

		// Query.
		virtual bool SupportsPlatform(FName InPlatformName) const = 0;	
		virtual const FCodecDetails& GetDetails() const = 0;			
		FName GetName() const { return GetDetails().Name; };

		// Factory for encoders
		using FEncoderPtr = TUniquePtr<IEncoder>;
		virtual FEncoderPtr CreateEncoder(
			IEncoder::FSourcePtr) 
		{ 
			return nullptr; 
		}
	
		// Factory for decoders
		using FDecoderPtr = TUniquePtr<IDecoder>;
		virtual FDecoderPtr CreateDecoder(
			IDecoder::FDecoderInputPtr InSrc,
			IDecoder::FDecoderOutputPtr InDst)
		{ 
			return nullptr; 
		}			
	};
} // namespace Audio

#pragma endregion Codec
