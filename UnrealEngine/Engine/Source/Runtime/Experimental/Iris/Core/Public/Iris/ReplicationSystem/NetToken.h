// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Templates/TypeHash.h"

namespace UE::Net
{
	class FNetBitStreamReader;
	class FNetBitStreamWriter;
}

namespace UE::Net
{

class FNetToken
{
public:
	typedef uint32 FTypeId;

	enum : uint32 { InvalidTokenTypeId = ~FTypeId(0) };
	enum : uint32 { InvalidTokenIndex = 0U };
	enum : uint32 { TokenTypeIdBits = 2U };
	enum : uint32 { TokenBits = 20U };
	enum : uint32 { MaxTypeIdCount = 1U << TokenTypeIdBits };
	enum : uint32 { MaxNetTokenCount = 1U << TokenBits };

public:	
	FNetToken() : Index(InvalidTokenIndex) {}
	inline bool IsValid() const { return Index != InvalidTokenIndex; }
	uint32 GetIndex() const { return Index; }
	bool operator==(const FNetToken& Other) const { return Index == Other.Index; }
	FString ToString() const;
	static FNetToken MakeNetToken(uint32 Index) { check(Index < MaxNetTokenCount); return FNetToken(Index); }

private:
	explicit FNetToken(uint32 InIndex) : Index(InIndex) {}

private:
	friend IRISCORE_API FNetToken ReadNetToken(FNetBitStreamReader*);

	uint32 Index;
};

class FNetTokenStoreKey
{
public:
	enum { Invalid = 0U };

	FNetTokenStoreKey() : Value(Invalid) {}
	bool IsValid() const { return Value != Invalid; }
	uint32 GetKeyValue() const { return Key; }
	FNetToken::FTypeId GetTypeId() const { return TypeId; }
	bool operator==(const FNetTokenStoreKey& Other) const { return Value == Other.Value; }

private:
	friend class FNetTokenStore;
	friend class FNetTokenDataStore;

private:
	union
	{
		struct
		{
			uint32 TypeId : FNetToken::TokenTypeIdBits;
			uint32 Key : FNetToken::TokenBits;
		};
		uint32 Value;
	};
};

static_assert(sizeof(FNetTokenStoreKey) == sizeof(uint32), "FNetTokenKey should fit in a uint32");

FORCEINLINE uint32 GetTypeHash(const FNetToken& Token)
{
	return ::GetTypeHash(Token.GetIndex());
}

inline FString FNetToken::ToString() const
{
	FString Result;
	Result = FString::Printf(TEXT("NetToken (Index=%u)"), Index);
	return Result;
}

// Read and write tokens
IRISCORE_API FNetToken ReadNetToken(FNetBitStreamReader* Reader);
IRISCORE_API void WriteNetToken(FNetBitStreamWriter* Writer, FNetToken Token);

}
