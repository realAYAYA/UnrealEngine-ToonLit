// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Iris/IrisConfig.h"
#include "Containers/StringFwd.h"
#include "Templates/TypeHash.h"

// Forward declarations
class FString;

namespace UE::Net::Private
{
	class FNetRefHandleManager;
}

namespace UE::Net
{

/**
 * FNetRefHandle
 */
class FNetRefHandle
{
public:
	
	inline static FNetRefHandle GetInvalid() { return FNetRefHandle(); }

private:
	enum { InvalidValue = 0 };
	enum { IdBits = 60 };
	enum { ReplicationSystemIdBits = 4 };

public:

	FNetRefHandle() : Value(InvalidValue) {}

	uint64 GetId() const { return Id; }
	uint32 GetReplicationSystemId() const { check(ReplicationSystemId != 0); return (uint32)(ReplicationSystemId - 1); }
	bool IsValid() const { return Value != InvalidValue; }

	bool IsCompleteHandle() const { return Value != InvalidValue && ReplicationSystemId != 0U; }

	bool IsStatic() const { return Id & StaticIdMask; }
	bool IsDynamic() const { return IsValid() && !IsStatic(); }

	bool operator==(const FNetRefHandle& Other)const { return Id == Other.Id; }
	bool operator<(const FNetRefHandle& Other)const { return Id < Other.Id; }
	bool operator!=(const FNetRefHandle& Other)const { return Id != Other.Id; }

	IRISCORE_API FString ToString() const;

	static bool FullCompare(FNetRefHandle A, FNetRefHandle B) { return A.Value == B.Value; }
	
private:
	friend uint32 GetTypeHash(const FNetRefHandle& Handle);
	friend Private::FNetRefHandleManager;

	static constexpr uint64 StaticIdMask = 1;
	static constexpr uint64 IdMask = (1ULL << IdBits) - 1;
	static constexpr uint64 MaxReplicationSystemId = (1ULL << ReplicationSystemIdBits) - 1;

	union 
	{
		struct
		{
			uint64 Id : IdBits;										// Id, lowest bit indicates if the handle is static or dynamic
			uint64 ReplicationSystemId : ReplicationSystemIdBits;	// ReplicationSystemId, when running in pie, we track the owning instance
		};
		uint64 Value;
	};
};

FORCEINLINE uint32 GetTypeHash(const FNetRefHandle& Handle)
{
	return ::GetTypeHash(Handle.GetId());
}

FORCEINLINE uint64 GetObjectIdForNetTrace(const FNetRefHandle& Handle)
{
	return Handle.GetId();
}

}

IRISCORE_API FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const UE::Net::FNetRefHandle& NetHandle);
IRISCORE_API FAnsiStringBuilderBase& operator<<(FAnsiStringBuilderBase& Builder, const UE::Net::FNetRefHandle& NetHandle);
