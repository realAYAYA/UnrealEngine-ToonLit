// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Containers/Map.h>
#include <Misc/Variant.h>

namespace ElectraDecodersUtil
{
	struct FFractionalValue
	{
		FFractionalValue(int64 InNumerator=0, uint32 InDenominator=0)
			: Num(InNumerator), Denom(InDenominator)
		{ }
		int64 Num = 0;
		uint32 Denom = 0;
	};

	// Advance a pointer by a number of bytes.
	template <typename T, typename C>
	inline T AdvancePointer(T pPointer, C numBytes)
	{
		return(T(UPTRINT(pPointer) + UPTRINT(numBytes)));
	}

	template <typename T>
	inline T AbsoluteValue(T Value)
	{
		return Value >= T(0) ? Value : -Value;
	}

	template <typename T>
	inline T Min(T a, T b)
	{
		return a < b ? a : b;
	}

	template <typename T>
	inline T Max(T a, T b)
	{
		return a > b ? a : b;
	}

	inline uint32 BitReverse32(uint32 InValue)
	{
		uint32 rev = 0;
		for (int32 i = 0; i < 32; ++i)
		{
			rev = (rev << 1) | (InValue & 1);
			InValue >>= 1;
		}
		return rev;
	}

	struct FMimeTypeCodecInfo
	{
		FString Codec;
	};

	struct FMimeTypeVideoCodecInfo : public FMimeTypeCodecInfo
	{
		int32 Profile = 0;
		int32 Level = 0;
		int32 ProfileSpace = 0;
		uint32 CompatibilityFlags = 0;
		uint64 Constraints = 0;
		int32 Tier = 0;
		int32 NumBitsLuma = 0;
		int32 Extras[8] {0};
	};


	struct FMimeTypeAudioCodecInfo : public FMimeTypeCodecInfo
	{
		int32 ObjectType = 0;
		int32 Profile = 0;
		uint32 ChannelConfiguration = 0;
		int32 NumberOfChannels = 0;
	};


	bool ELECTRADECODERS_API ParseMimeTypeWithCodec(FMimeTypeVideoCodecInfo& OutInfo, const FString& InMimeType);
	bool ELECTRADECODERS_API ParseMimeTypeWithCodec(FMimeTypeAudioCodecInfo& OutInfo, const FString& InMimeType);


	bool ELECTRADECODERS_API ParseCodecMP4A(FMimeTypeAudioCodecInfo& OutInfo, const FString& InCodecFormat);

	bool ELECTRADECODERS_API ParseCodecH264(FMimeTypeVideoCodecInfo& OutInfo, const FString& InCodecFormat);
	bool ELECTRADECODERS_API ParseCodecH265(FMimeTypeVideoCodecInfo& OutInfo, const FString& InCodecFormat);
	bool ELECTRADECODERS_API ParseCodecVP8(FMimeTypeVideoCodecInfo& OutInfo, const FString& InCodecFormat, const TArray<uint8>& InvpcCBox);
	bool ELECTRADECODERS_API ParseCodecVP9(FMimeTypeVideoCodecInfo& OutInfo, const FString& InCodecFormat, const TArray<uint8>& InvpcCBox);


	int64 ELECTRADECODERS_API GetVariantValueSafeI64(const TMap<FString, FVariant>& InFromMap, const FString& InName, int64 InDefaultValue=0);
	uint64 ELECTRADECODERS_API GetVariantValueSafeU64(const TMap<FString, FVariant>& InFromMap, const FString& InName, uint64 InDefaultValue=0);
	double ELECTRADECODERS_API GetVariantValueSafeDouble(const TMap<FString, FVariant>& InFromMap, const FString& InName, double InDefaultValue=0.0);

	TArray<uint8> ELECTRADECODERS_API GetVariantValueUInt8Array(const TMap<FString, FVariant>& InFromMap, const FString& InName);
	FString ELECTRADECODERS_API GetVariantValueFString(const TMap<FString, FVariant>& InFromMap, const FString& InName);
}
