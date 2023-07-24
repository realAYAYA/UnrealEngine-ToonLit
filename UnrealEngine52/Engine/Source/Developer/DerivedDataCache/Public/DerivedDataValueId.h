// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/StringFwd.h"
#include "Memory/MemoryView.h"
#include "String/BytesToHex.h"

#define UE_API DERIVEDDATACACHE_API

class FCbFieldView;
class FCbObjectId;
class FCbWriter;
struct FIoHash;

namespace UE::DerivedData
{

/** A 12-byte value that uniquely identifies a value in the context that it was created. */
struct FValueId
{
public:
	using ByteArray = uint8[12];

	/** Construct a null ID. */
	FValueId() = default;

	/** Construct an ID from an array of 12 bytes. */
	inline explicit FValueId(const ByteArray& Id);

	/** Construct an ID from a view of 12 bytes. */
	UE_API explicit FValueId(FMemoryView Id);

	/** Construct an ID from a Compact Binary Object ID. */
	UE_API FValueId(const FCbObjectId& Id);

	/** Returns the ID as a Compact Binary Object ID. */
	UE_API operator FCbObjectId() const;

	/** Construct an ID from a non-zero hash. */
	[[nodiscard]] UE_API static FValueId FromHash(const FIoHash& Hash);

	/** Construct an ID from a non-empty name. */
	[[nodiscard]] UE_API static FValueId FromName(FUtf8StringView Name);
	[[nodiscard]] UE_API static FValueId FromName(FWideStringView Name);

	/** Returns a reference to the raw byte array for the ID. */
	inline const ByteArray& GetBytes() const { return Bytes; }

	/** Returns a view of the raw byte array for the ID. */
	inline FMemoryView GetView() const { return MakeMemoryView(Bytes); }

	/** Whether this is null. */
	inline bool IsNull() const;
	/** Whether this is not null. */
	inline bool IsValid() const { return !IsNull(); }

	/** Reset this to null. */
	inline void Reset() { *this = FValueId(); }

	/** A null ID. */
	static const FValueId Null;

private:
	alignas(uint32) ByteArray Bytes{};
};

inline const FValueId FValueId::Null;

inline FValueId::FValueId(const ByteArray& Id)
{
	FMemory::Memcpy(Bytes, Id, sizeof(ByteArray));
}

inline bool operator==(const FValueId& A, const FValueId& B)
{
	return A.GetView().EqualBytes(B.GetView());
}

inline bool operator!=(const FValueId& A, const FValueId& B)
{
	return !A.GetView().EqualBytes(B.GetView());
}

inline bool operator<(const FValueId& A, const FValueId& B)
{
	return A.GetView().CompareBytes(B.GetView()) < 0;
}

inline uint32 GetTypeHash(const FValueId& Id)
{
	return *reinterpret_cast<const uint32*>(Id.GetView().GetData());
}

/** Convert the ID to a 24-character hex string. */
template <typename CharType>
inline TStringBuilderBase<CharType>& operator<<(TStringBuilderBase<CharType>& Builder, const FValueId& Id)
{
	UE::String::BytesToHexLower(Id.GetBytes(), Builder);
	return Builder;
}

inline bool FValueId::IsNull() const
{
	return *this == FValueId();
}

UE_API FCbWriter& operator<<(FCbWriter& Writer, const FValueId& Id);
UE_API bool LoadFromCompactBinary(FCbFieldView Field, FValueId& OutId);

} // UE::DerivedData

#undef UE_API
