// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/RotatorNetSerializers.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Math/Rotator.h"

namespace UE::Net
{

struct FRotatorAsShortNetSerializer
{
	static constexpr uint32 Version = 0;

	struct FQuantizedType
	{
		// Using three bits to indicate whether the components are zero or not.
		uint16 XYZIsNotZero;
		uint16 X;
		uint16 Y;
		uint16 Z;
	};

	typedef FRotator SourceType;
	typedef FQuantizedType QuantizedType;
	typedef FRotatorAsShortNetSerializerConfig ConfigType;

	inline static const ConfigType DefaultConfig;

	static void Serialize(FNetSerializationContext&, const FNetSerializeArgs& Args);
	static void Deserialize(FNetSerializationContext&, const FNetDeserializeArgs& Args);

	static void SerializeDelta(FNetSerializationContext&, const FNetSerializeDeltaArgs& Args);
	static void DeserializeDelta(FNetSerializationContext&, const FNetDeserializeDeltaArgs& Args);

	static void Quantize(FNetSerializationContext&, const FNetQuantizeArgs& Args);
	static void Dequantize(FNetSerializationContext&, const FNetDequantizeArgs& Args);

	static bool IsEqual(FNetSerializationContext&, const FNetIsEqualArgs& Args);
	static bool Validate(FNetSerializationContext&, const FNetValidateArgs& Args);

protected:
	using ScalarType = decltype(SourceType::Pitch);

	enum Constants : uint16
	{
		XDiffersMask = 1U,
		YDiffersMask = 2U,
		ZDiffersMask = 4U,

		XYZDiffersMask = 7U,
	};

	static constexpr ScalarType Scale = ScalarType(65536)/ScalarType(360);
	static constexpr ScalarType Bias = ScalarType(0.5f);
	static constexpr ScalarType InvScale = ScalarType(360)/ScalarType(65536);
};
UE_NET_IMPLEMENT_SERIALIZER(FRotatorAsShortNetSerializer);

struct FRotatorNetSerializer : public FRotatorAsShortNetSerializer
{
	static constexpr uint32 Version = 0;

	typedef FRotatorNetSerializerConfig ConfigType;

	inline static const ConfigType DefaultConfig;
};
UE_NET_IMPLEMENT_SERIALIZER(FRotatorNetSerializer);

struct FRotatorAsByteNetSerializer
{
	static constexpr uint32 Version = 0;

	struct FQuantizedType
	{
		// Using three bits to indicate whether the components are zero or not.
		uint8 XYZIsNotZero;
		uint8 X;
		uint8 Y;
		uint8 Z;
	};

	typedef FRotator SourceType;
	typedef FQuantizedType QuantizedType;
	typedef FRotatorAsByteNetSerializerConfig ConfigType;

	inline static const ConfigType DefaultConfig;

	static void Serialize(FNetSerializationContext&, const FNetSerializeArgs& Args);
	static void Deserialize(FNetSerializationContext&, const FNetDeserializeArgs& Args);

	static void SerializeDelta(FNetSerializationContext&, const FNetSerializeDeltaArgs& Args);
	static void DeserializeDelta(FNetSerializationContext&, const FNetDeserializeDeltaArgs& Args);

	static void Quantize(FNetSerializationContext&, const FNetQuantizeArgs& Args);
	static void Dequantize(FNetSerializationContext&, const FNetDequantizeArgs& Args);

	static bool IsEqual(FNetSerializationContext&, const FNetIsEqualArgs& Args);
	static bool Validate(FNetSerializationContext&, const FNetValidateArgs& Args);

protected:
	using ScalarType = decltype(SourceType::Pitch);

	enum Constants : uint16
	{
		XDiffersMask = 1U,
		YDiffersMask = 2U,
		ZDiffersMask = 4U,

		XYZDiffersMask = 7U,
	};

	static constexpr ScalarType Scale = ScalarType(256)/ScalarType(360);
	static constexpr ScalarType Bias = ScalarType(0.5f);
	static constexpr ScalarType InvScale = ScalarType(360)/ScalarType(256);
};
UE_NET_IMPLEMENT_SERIALIZER(FRotatorAsByteNetSerializer);

// FRotatorAsShortNetSerializer implementation
void FRotatorAsShortNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
{
	const QuantizedType& Value = *reinterpret_cast<const QuantizedType*>(Args.Source);

	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
	Writer->WriteBits(Value.XYZIsNotZero, 3U);
	if (Value.XYZIsNotZero & XDiffersMask)
	{
		Writer->WriteBits(Value.X, 16U);
	}
	if (Value.XYZIsNotZero & YDiffersMask)
	{
		Writer->WriteBits(Value.Y, 16U);
	}
	if (Value.XYZIsNotZero & ZDiffersMask)
	{
		Writer->WriteBits(Value.Z, 16U);
	}
}

void FRotatorAsShortNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	QuantizedType Value;
	const uint32 XYZIsNotZero = Reader->ReadBits(3U);
	Value.XYZIsNotZero = XYZIsNotZero;
	Value.X = XYZIsNotZero & XDiffersMask ? Reader->ReadBits(16U) : 0U;
	Value.Y = XYZIsNotZero & YDiffersMask ? Reader->ReadBits(16U) : 0U;
	Value.Z = XYZIsNotZero & ZDiffersMask ? Reader->ReadBits(16U) : 0U;

	QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
	Target = Value;
}

void FRotatorAsShortNetSerializer::SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args)
{
	// Per component equality. With the current scaling a 10 degree change would require 12 bits to replicate
	// due to 11 bits for the value and 1 for the sign. Add a small lookup table index for that and you're
	// up to 14 bits instead of 1+16 bits.

	const QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Source);
	const QuantizedType& PrevValue = *reinterpret_cast<QuantizedType*>(Args.Prev);

	const uint16 DX = Value.X - PrevValue.X;
	const uint16 DY = Value.Y - PrevValue.Y;
	const uint16 DZ = Value.Z - PrevValue.Z;

	uint32 XYZDiffers = 0;
	XYZDiffers |= (DX != 0) ? uint32(XDiffersMask) : uint32(0);
	XYZDiffers |= (DY != 0) ? uint32(YDiffersMask) : uint32(0);
	XYZDiffers |= (DZ != 0) ? uint32(ZDiffersMask) : uint32(0);

	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
	Writer->WriteBits(XYZDiffers, 3U);
	if (XYZDiffers & XDiffersMask)
	{
		Writer->WriteBits(DX, 16U);
	}
	if (XYZDiffers & YDiffersMask)
	{
		Writer->WriteBits(DY, 16U);
	}
	if (XYZDiffers & ZDiffersMask)
	{
		Writer->WriteBits(DZ, 16U);
	}
}

void FRotatorAsShortNetSerializer::DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	const QuantizedType& PrevValue = *reinterpret_cast<QuantizedType*>(Args.Prev);
	QuantizedType TempValue = PrevValue;

	const uint32 XYZDiffers = Reader->ReadBits(3U);
	if (XYZDiffers & XDiffersMask)
	{
		const uint16 DX = Reader->ReadBits(16U);
		TempValue.X += DX;
	}
	if (XYZDiffers & YDiffersMask)
	{
		const uint16 DY = Reader->ReadBits(16U);
		TempValue.Y += DY;
	}
	if (XYZDiffers & ZDiffersMask)
	{
		const uint16 DZ = Reader->ReadBits(16U);
		TempValue.Z += DZ;
	}

	// Reconstruct flags
	uint32 XYZIsNotZero = 0U;
	XYZIsNotZero |= (TempValue.X != 0 ? XDiffersMask : 0U);
	XYZIsNotZero |= (TempValue.Y != 0 ? YDiffersMask : 0U);
	XYZIsNotZero |= (TempValue.Z != 0 ? ZDiffersMask : 0U);
	TempValue.XYZIsNotZero = XYZIsNotZero;

	QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
	Target = TempValue;
}

void FRotatorAsShortNetSerializer::Quantize(FNetSerializationContext&, const FNetQuantizeArgs& Args)
{
	const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
	QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);

	QuantizedType TempValue = {};
	TempValue.X = uint16(uint32(int32(Source.Pitch*Scale + Bias)));
	TempValue.Y = uint16(uint32(int32(Source.Yaw*Scale + Bias)));
	TempValue.Z = uint16(uint32(int32(Source.Roll*Scale + Bias)));
	TempValue.XYZIsNotZero |= (TempValue.X != 0) ? XDiffersMask : uint16(0);
	TempValue.XYZIsNotZero |= (TempValue.Y != 0) ? YDiffersMask : uint16(0);
	TempValue.XYZIsNotZero |= (TempValue.Z != 0) ? ZDiffersMask : uint16(0);

	Target = TempValue;
}

void FRotatorAsShortNetSerializer::Dequantize(FNetSerializationContext&, const FNetDequantizeArgs& Args)
{
	const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
	SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);

	SourceType TempValue;
	TempValue.Pitch = Source.X*InvScale;
	TempValue.Yaw = Source.Y*InvScale;
	TempValue.Roll = Source.Z*InvScale;

	Target = TempValue;
}

bool FRotatorAsShortNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
{
	if (Args.bStateIsQuantized)
	{
		const QuantizedType& SourceValue0 = *reinterpret_cast<const QuantizedType*>(Args.Source0);
		const QuantizedType& SourceValue1 = *reinterpret_cast<const QuantizedType*>(Args.Source1);

		const bool bXIsEqual = (SourceValue0.X == SourceValue1.X);
		const bool bYIsEqual = (SourceValue0.Y == SourceValue1.Y);
		const bool bZIsEqual = (SourceValue0.Z == SourceValue1.Z);
		const bool bFlagsAreEqual = (SourceValue0.XYZIsNotZero == SourceValue1.XYZIsNotZero);
		return bXIsEqual & bYIsEqual & bZIsEqual & bFlagsAreEqual;
	}
	else
	{
		const SourceType& SourceValue0 = *reinterpret_cast<const SourceType*>(Args.Source0);
		const SourceType& SourceValue1 = *reinterpret_cast<const SourceType*>(Args.Source1);

		const bool bPitchIsEqual = (SourceValue0.Pitch == SourceValue1.Pitch);
		const bool bYawIsEqual = (SourceValue0.Yaw == SourceValue1.Yaw);
		const bool bRollIsEqual = (SourceValue0.Roll == SourceValue1.Roll);
		return bPitchIsEqual & bYawIsEqual & bRollIsEqual;
	}
}

bool FRotatorAsShortNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
{
	const SourceType& Value = *reinterpret_cast<SourceType*>(Args.Source);
	// Make sure values are valid degree angles. This should catch NaNs as well.
	return (Value.Pitch >= 0.0f && Value.Pitch < 360.0f) && (Value.Yaw >= 0.0f && Value.Yaw < 360.0f) & (Value.Roll >= 0.0f && Value.Roll < 360.0f); 
}

// FRotatorAsByteNetSerializer implementation
void FRotatorAsByteNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
{
	const QuantizedType& Value = *reinterpret_cast<const QuantizedType*>(Args.Source);

	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
	Writer->WriteBits(Value.XYZIsNotZero, 3U);
	if (Value.XYZIsNotZero & XDiffersMask)
	{
		Writer->WriteBits(Value.X, 8U);
	}
	if (Value.XYZIsNotZero & YDiffersMask)
	{
		Writer->WriteBits(Value.Y, 8U);
	}
	if (Value.XYZIsNotZero & ZDiffersMask)
	{
		Writer->WriteBits(Value.Z, 8U);
	}
}

void FRotatorAsByteNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	QuantizedType Value;
	const uint32 XYZIsNotZero = Reader->ReadBits(3U);
	Value.XYZIsNotZero = XYZIsNotZero;
	Value.X = XYZIsNotZero & XDiffersMask ? Reader->ReadBits(8U) : 0U;
	Value.Y = XYZIsNotZero & YDiffersMask ? Reader->ReadBits(8U) : 0U;
	Value.Z = XYZIsNotZero & ZDiffersMask ? Reader->ReadBits(8U) : 0U;

	QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
	Target = Value;
}

void FRotatorAsByteNetSerializer::SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args)
{
	const QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Source);
	const QuantizedType& PrevValue = *reinterpret_cast<QuantizedType*>(Args.Prev);

	uint32 XYZDiffers = 0;
	XYZDiffers |= (Value.X != PrevValue.X) ? uint32(XDiffersMask) : uint32(0);
	XYZDiffers |= (Value.Y != PrevValue.Y) ? uint32(YDiffersMask) : uint32(0);
	XYZDiffers |= (Value.Z != PrevValue.Z) ? uint32(ZDiffersMask) : uint32(0);

	FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
	Writer->WriteBits(XYZDiffers, 3U);
	if (XYZDiffers & XDiffersMask)
	{
		Writer->WriteBits(Value.X, 8U);
	}
	if (XYZDiffers & YDiffersMask)
	{
		Writer->WriteBits(Value.Y, 8U);
	}
	if (XYZDiffers & ZDiffersMask)
	{
		Writer->WriteBits(Value.Z, 8U);
	}
}

void FRotatorAsByteNetSerializer::DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args)
{
	FNetBitStreamReader* Reader = Context.GetBitStreamReader();

	const QuantizedType& PrevValue = *reinterpret_cast<QuantizedType*>(Args.Prev);
	QuantizedType TempValue = PrevValue;

	const uint32 XYZDiffers = Reader->ReadBits(3U);
	if (XYZDiffers & XDiffersMask)
	{
		TempValue.X = Reader->ReadBits(8U);
	}
	if (XYZDiffers & YDiffersMask)
	{
		TempValue.Y = Reader->ReadBits(8U);
	}
	if (XYZDiffers & ZDiffersMask)
	{
		TempValue.Z = Reader->ReadBits(8U);
	}

	// Reconstruct flags
	uint32 XYZIsNotZero = 0U;
	XYZIsNotZero |= (TempValue.X != 0 ? XDiffersMask : 0U);
	XYZIsNotZero |= (TempValue.Y != 0 ? YDiffersMask : 0U);
	XYZIsNotZero |= (TempValue.Z != 0 ? ZDiffersMask : 0U);
	TempValue.XYZIsNotZero = XYZIsNotZero;

	QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
	Target = TempValue;
}

void FRotatorAsByteNetSerializer::Quantize(FNetSerializationContext&, const FNetQuantizeArgs& Args)
{
	const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
	QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);

	QuantizedType TempValue = {};
	TempValue.X = uint8(uint32(int32(Source.Pitch*Scale + Bias)));
	TempValue.Y = uint8(uint32(int32(Source.Yaw*Scale + Bias)));
	TempValue.Z = uint8(uint32(int32(Source.Roll*Scale + Bias)));
	TempValue.XYZIsNotZero |= (TempValue.X != 0) ? XDiffersMask : uint8(0);
	TempValue.XYZIsNotZero |= (TempValue.Y != 0) ? YDiffersMask : uint8(0);
	TempValue.XYZIsNotZero |= (TempValue.Z != 0) ? ZDiffersMask : uint8(0);

	Target = TempValue;
}

void FRotatorAsByteNetSerializer::Dequantize(FNetSerializationContext&, const FNetDequantizeArgs& Args)
{
	const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
	SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);

	SourceType TempValue;
	TempValue.Pitch = Source.X*InvScale;
	TempValue.Yaw = Source.Y*InvScale;
	TempValue.Roll = Source.Z*InvScale;

	Target = TempValue;
}

bool FRotatorAsByteNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
{
	if (Args.bStateIsQuantized)
	{
		const QuantizedType& SourceValue0 = *reinterpret_cast<const QuantizedType*>(Args.Source0);
		const QuantizedType& SourceValue1 = *reinterpret_cast<const QuantizedType*>(Args.Source1);

		const bool bXIsEqual = (SourceValue0.X == SourceValue1.X);
		const bool bYIsEqual = (SourceValue0.Y == SourceValue1.Y);
		const bool bZIsEqual = (SourceValue0.Z == SourceValue1.Z);
		const bool bFlagsAreEqual = (SourceValue0.XYZIsNotZero == SourceValue1.XYZIsNotZero);
		return bXIsEqual & bYIsEqual & bZIsEqual & bFlagsAreEqual;
	}
	else
	{
		const SourceType& SourceValue0 = *reinterpret_cast<const SourceType*>(Args.Source0);
		const SourceType& SourceValue1 = *reinterpret_cast<const SourceType*>(Args.Source1);

		const bool bPitchIsEqual = (SourceValue0.Pitch == SourceValue1.Pitch);
		const bool bYawIsEqual = (SourceValue0.Yaw == SourceValue1.Yaw);
		const bool bRollIsEqual = (SourceValue0.Roll == SourceValue1.Roll);
		return bPitchIsEqual & bYawIsEqual & bRollIsEqual;
	}
}

bool FRotatorAsByteNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
{
	const SourceType& Value = *reinterpret_cast<SourceType*>(Args.Source);
	// Make sure values are valid degree angles. This should catch NaNs as well.
	return (Value.Pitch >= 0.0f && Value.Pitch < 360.0f) && (Value.Yaw >= 0.0f && Value.Yaw < 360.0f) & (Value.Roll >= 0.0f && Value.Roll < 360.0f); 
}

}
