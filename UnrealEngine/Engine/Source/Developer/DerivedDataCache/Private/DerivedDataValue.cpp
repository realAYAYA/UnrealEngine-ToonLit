// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataValue.h"

#include "Compression/OodleDataCompression.h"
#include "Containers/StringConv.h"
#include "Hash/xxhash.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Templates/UnrealTemplate.h"

namespace UE::DerivedData
{

constexpr ECompressedBufferCompressor GDefaultCompressor = ECompressedBufferCompressor::Mermaid;
constexpr ECompressedBufferCompressionLevel GDefaultCompressionLevel = ECompressedBufferCompressionLevel::VeryFast;

FValue FValue::Compress(const FCompositeBuffer& Buffer, const uint64 BlockSize)
{
	return FValue(FCompressedBuffer::Compress(Buffer, GDefaultCompressor, GDefaultCompressionLevel, BlockSize));
}

FValue FValue::Compress(const FSharedBuffer& Buffer, const uint64 BlockSize)
{
	return FValue(FCompressedBuffer::Compress(Buffer, GDefaultCompressor, GDefaultCompressionLevel, BlockSize));
}

FValueId::FValueId(const FMemoryView Id)
{
	checkf(Id.GetSize() == sizeof(ByteArray),
		TEXT("FValueId cannot be constructed from a view of %" UINT64_FMT " bytes."), Id.GetSize());
	FMemory::Memcpy(Bytes, Id.GetData(), sizeof(ByteArray));
}

FValueId::FValueId(const FCbObjectId& Id)
	: FValueId(ImplicitConv<const ByteArray&>(Id))
{
}

FValueId::operator FCbObjectId() const
{
	return FCbObjectId(GetBytes());
}

FValueId FValueId::FromHash(const FIoHash& Hash)
{
	checkf(!Hash.IsZero(), TEXT("FValueId requires a non-zero hash."));
	return FValueId(MakeMemoryView(Hash.GetBytes()).Left(sizeof(ByteArray)));
}

FValueId FValueId::FromName(const FUtf8StringView Name)
{
	checkf(!Name.IsEmpty(), TEXT("FValueId requires a non-empty name."));
	uint8 HashBytes[16];
	FXxHash128::HashBuffer(Name.GetData(), Name.Len()).ToByteArray(HashBytes);
	return FValueId(MakeMemoryView(HashBytes, sizeof(ByteArray)));
}

FValueId FValueId::FromName(const FWideStringView Name)
{
	return FValueId::FromName(FTCHARToUTF8(Name));
}

FCbWriter& operator<<(FCbWriter& Writer, const FValueId& Id)
{
	Writer.AddObjectId(Id);
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FValueId& OutId)
{
	if (const FCbObjectId ObjectId = Field.AsObjectId(); !Field.HasError())
	{
		OutId = ObjectId;
		return true;
	}
	OutId.Reset();
	return false;
}

} // UE::DerivedData
