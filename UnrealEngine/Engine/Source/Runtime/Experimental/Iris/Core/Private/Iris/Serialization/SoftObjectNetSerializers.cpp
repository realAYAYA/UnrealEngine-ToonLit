// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/SoftObjectNetSerializers.h"
#include "Iris/Serialization/StringNetSerializerUtils.h"
#include "Iris/Serialization/InternalNetSerializer.h"
#include "UObject/SoftObjectPtr.h"

namespace UE::Net
{

struct FSoftObjectNetSerializer : public Private::FStringNetSerializerBase
{
	// Version
	static const uint32 Version = 0;

	// Types
	typedef FSoftObjectPtr SourceType;
	typedef FSoftObjectNetSerializerConfig ConfigType;

	static const ConfigType DefaultConfig;

	// Implementation
	static void Quantize(FNetSerializationContext&, const FNetQuantizeArgs& Args);
	static void Dequantize(FNetSerializationContext&, const FNetDequantizeArgs& Args);

	static bool IsEqual(FNetSerializationContext&, const FNetIsEqualArgs& Args);
	static bool Validate(FNetSerializationContext&, const FNetValidateArgs& Args);
};
UE_NET_IMPLEMENT_SERIALIZER_INTERNAL(FSoftObjectNetSerializer);
const FSoftObjectNetSerializer::ConfigType FSoftObjectNetSerializer::DefaultConfig;

struct FSoftObjectPathNetSerializer : public Private::FStringNetSerializerBase
{
	// Version
	static const uint32 Version = 0;

	// Types
	typedef FSoftObjectPath SourceType;
	typedef FSoftObjectPathNetSerializerConfig ConfigType;

	static const ConfigType DefaultConfig;

	// Implementation
	static void Quantize(FNetSerializationContext&, const FNetQuantizeArgs& Args);
	static void Dequantize(FNetSerializationContext&, const FNetDequantizeArgs& Args);

	static bool IsEqual(FNetSerializationContext&, const FNetIsEqualArgs& Args);
	static bool Validate(FNetSerializationContext&, const FNetValidateArgs& Args);
};
UE_NET_IMPLEMENT_SERIALIZER_INTERNAL(FSoftObjectPathNetSerializer);
const FSoftObjectPathNetSerializer::ConfigType FSoftObjectPathNetSerializer::DefaultConfig;

struct FSoftClassPathNetSerializer : public Private::FStringNetSerializerBase
{
	// Version
	static const uint32 Version = 0;

	// Types
	typedef FSoftClassPath SourceType;
	typedef FSoftClassPathNetSerializerConfig ConfigType;

	static const ConfigType DefaultConfig;

	// Implementation
	static void Quantize(FNetSerializationContext&, const FNetQuantizeArgs& Args);
	static void Dequantize(FNetSerializationContext&, const FNetDequantizeArgs& Args);

	static bool IsEqual(FNetSerializationContext&, const FNetIsEqualArgs& Args);
	static bool Validate(FNetSerializationContext&, const FNetValidateArgs& Args);
};
UE_NET_IMPLEMENT_SERIALIZER_INTERNAL(FSoftClassPathNetSerializer);
const FSoftClassPathNetSerializer::ConfigType FSoftClassPathNetSerializer::DefaultConfig;

// FSoftObjectNetSerializer
void FSoftObjectNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
{
	const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
	const FSoftObjectPath& SoftObjectPath = Source.GetUniqueID();
	const FString& SoftObjectPathString = SoftObjectPath.ToString();
	return FStringNetSerializerBase::Quantize(Context, Args, SoftObjectPathString);
}

void FSoftObjectNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
{
	FString SoftObjectPathString;
	FStringNetSerializerBase::Dequantize(Context, Args, SoftObjectPathString);

	SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);
	Target.ResetWeakPtr();
	FSoftObjectPath& SoftObjectPath = Target.GetUniqueID();
	SoftObjectPath.SetPath(SoftObjectPathString);
}

bool FSoftObjectNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
{
	if (Args.bStateIsQuantized)
	{
		return IsQuantizedEqual(Context, Args);
	}
	else
	{
		const SourceType& Source0 = *reinterpret_cast<const SourceType*>(Args.Source0);
		const SourceType& Source1 = *reinterpret_cast<const SourceType*>(Args.Source1);
		const FString& SoftObjectPathString0 = Source0.GetUniqueID().ToString();
		const FString& SoftObjectPathString1 = Source1.GetUniqueID().ToString();
		// Might not need case sensitive string compare, but not having it would require the quantization to
		// transform the string to lowercase first.
		return SoftObjectPathString0.Equals(SoftObjectPathString1, ESearchCase::CaseSensitive);
	}
}

bool FSoftObjectNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
{
	const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
	const FSoftObjectPath& SoftObjectPath = Source.GetUniqueID();
	const FString& SoftObjectPathString = SoftObjectPath.ToString();
	return FStringNetSerializerBase::Validate(Context, Args, SoftObjectPathString);
}

// FSoftObjectPathNetSerializer
void FSoftObjectPathNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
{
	const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
	const FString& SoftObjectPathString = Source.ToString();
	return FStringNetSerializerBase::Quantize(Context, Args, SoftObjectPathString);
}

void FSoftObjectPathNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
{
	FString SoftObjectPathString;
	FStringNetSerializerBase::Dequantize(Context, Args, SoftObjectPathString);

	SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);
	Target.SetPath(SoftObjectPathString);
}

bool FSoftObjectPathNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
{
	if (Args.bStateIsQuantized)
	{
		return IsQuantizedEqual(Context, Args);
	}
	else
	{
		const SourceType& Source0 = *reinterpret_cast<const SourceType*>(Args.Source0);
		const SourceType& Source1 = *reinterpret_cast<const SourceType*>(Args.Source1);
		const FString& SoftObjectPathString0 = Source0.ToString();
		const FString& SoftObjectPathString1 = Source1.ToString();
		return SoftObjectPathString0.Equals(SoftObjectPathString1, ESearchCase::CaseSensitive);
	}
}

bool FSoftObjectPathNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
{
	const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
	const FString& SoftObjectPathString = Source.ToString();
	return FStringNetSerializerBase::Validate(Context, Args, SoftObjectPathString);
}

// FSoftClassPathNetSerializer
void FSoftClassPathNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
{
	const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
	const FString& SoftClassPathString = Source.ToString();
	return FStringNetSerializerBase::Quantize(Context, Args, SoftClassPathString);
}

void FSoftClassPathNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
{
	FString SoftClassPathString;
	FStringNetSerializerBase::Dequantize(Context, Args, SoftClassPathString);

	SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);
	Target.SetPath(SoftClassPathString);
}

bool FSoftClassPathNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
{
	if (Args.bStateIsQuantized)
	{
		return IsQuantizedEqual(Context, Args);
	}
	else
	{
		const SourceType& Source0 = *reinterpret_cast<const SourceType*>(Args.Source0);
		const SourceType& Source1 = *reinterpret_cast<const SourceType*>(Args.Source1);
		const FString& SoftClassPathString0 = Source0.ToString();
		const FString& SoftClassPathString1 = Source1.ToString();
		return SoftClassPathString0.Equals(SoftClassPathString1, ESearchCase::CaseSensitive);
	}
}

bool FSoftClassPathNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
{
	const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
	const FString& SoftClassPathString0 = Source.ToString();
	return Private::FStringNetSerializerBase::Validate(Context, Args, SoftClassPathString0);
}

}
