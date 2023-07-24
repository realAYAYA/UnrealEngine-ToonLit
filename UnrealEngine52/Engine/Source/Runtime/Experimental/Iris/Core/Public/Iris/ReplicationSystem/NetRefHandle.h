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
	enum { Invalid = 0 };
	enum { IdBits = 32 };
	enum { ReplicationSystemIdBits = 4 };

public:
	FNetRefHandle() : Value(Invalid) {}

	uint32 GetId() const { return Id; }
	uint32 GetReplicationSystemId() const { check(ReplicationSystemId != 0U); return ReplicationSystemId - 1U; }
	bool IsValid() const { return Value != Invalid; }

	bool IsCompleteHandle() const { return Value != Invalid && ReplicationSystemId != 0U; }

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

	static constexpr uint32 StaticIdMask = 1U;
	static constexpr uint32 IdMask = ~0U;
	static constexpr uint32 MaxReplicationSystemId = (1 << ReplicationSystemIdBits) - 1;

	union 
	{
		struct
		{
			uint32 Id : IdBits;										// Id, lowest bit indicates if the handle is static or dynamic
			uint32 ReplicationSystemId : ReplicationSystemIdBits;	// ReplicationSystemId, when running in pie, we track the owning instance
		};
		uint64 Value;
	};
};

FORCEINLINE uint32 GetTypeHash(const FNetRefHandle& Handle)
{
	return ::GetTypeHash(Handle.GetId());
}

FORCEINLINE uint32 GetObjectIdForNetTrace(const FNetRefHandle& Handle)
{
	return Handle.GetId();
}

}

IRISCORE_API FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const UE::Net::FNetRefHandle& NetHandle);
IRISCORE_API FAnsiStringBuilderBase& operator<<(FAnsiStringBuilderBase& Builder, const UE::Net::FNetRefHandle& NetHandle);
