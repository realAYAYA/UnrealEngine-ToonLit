// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Iris/IrisConfig.h"
#include "Misc/StringBuilder.h"
#include "Templates/TypeHash.h"

// Forward declarations
namespace UE::Net::Private
{
	class FNetHandleManager;
}

namespace UE::Net
{

/**
* FNetHandle
*/

class FNetHandle
{
public:
	enum { Invalid = 0 };
	enum { IdBits = 32 };
	enum { ReplicationSystemIdBits = 4 };

public:
	FNetHandle() : Value(Invalid) {}

	uint32 GetId() const { return Id; }
	uint32 GetReplicationSystemId() const { check(ReplicationSystemId != 0U); return ReplicationSystemId - 1U; }
	bool IsValid() const { return Value != Invalid; }

	bool IsCompleteHandle() const { return Value != Invalid && ReplicationSystemId != 0U; }

	bool IsStatic() const { return Id & StaticIdMask; }
	bool IsDynamic() const { return IsValid() && !IsStatic(); }

	bool operator==(const FNetHandle& Other)const { return Id == Other.Id; }
	bool operator<(const FNetHandle& Other)const { return Id < Other.Id; }
	bool operator!=(const FNetHandle& Other)const { return Id != Other.Id; }

	FString ToString() const;

	static bool FullCompare(FNetHandle A, FNetHandle B) { return A.Value == B.Value; }
	
private:
	static uint32 MakeNetHandleId(uint32 Seed, bool bIsStatic);
	static FNetHandle MakeNetHandle(uint32 Id, uint32 ReplicationSystemId);
	static FNetHandle MakeNetHandleFromId(uint32 Id);

	friend uint32 GetTypeHash(const FNetHandle& Handle);
	friend Private::FNetHandleManager;

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

FORCEINLINE uint32 GetTypeHash(const FNetHandle& Handle)
{
	// Id and type is relevant
	return ::GetTypeHash(Handle.GetId());
}

FORCEINLINE uint32 GetObjectIdForNetTrace(const FNetHandle& Handle)
{
	return Handle.GetId();
}

inline FString FNetHandle::ToString() const
{
	FString Result;
#if UE_NET_ALLOW_MULTIPLE_REPLICATION_SYSTEMS
	const uint32 ReplicationSystemIdToDisplay = ReplicationSystemId - 1U;
	Result = FString::Printf(TEXT("NetHandle (Id=%u):(RepSystemId=%u)"), GetId(), ReplicationSystemIdToDisplay);
#else
	Result = FString::Printf(TEXT("NetHandle (Id=%u)"), GetId());
#endif
	return Result;
}

}

inline FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const UE::Net::FNetHandle& NetHandle) 
{ 	
#if UE_NET_ALLOW_MULTIPLE_REPLICATION_SYSTEMS
	const uint32 ReplicationSystemIdToDisplay = NetHandle.GetReplicationSystemId() - 1U;
	Builder.Appendf(TEXT("NetHandle (Id=%u):(RepSystemId=%u)"), NetHandle.GetId(), ReplicationSystemIdToDisplay);
#else
	Builder.Appendf(TEXT("NetHandle (Id=%u)"), NetHandle.GetId());
#endif
	return Builder;
}

inline FAnsiStringBuilderBase& operator<<(FAnsiStringBuilderBase& Builder, const UE::Net::FNetHandle& NetHandle)
{
	#if UE_NET_ALLOW_MULTIPLE_REPLICATION_SYSTEMS
		const uint32 ReplicationSystemIdToDisplay = NetHandle.GetReplicationSystemId() - 1U;
		Builder.Appendf("NetHandle (Id=%u):(RepSystemId=%u)", NetHandle.GetId(), ReplicationSystemIdToDisplay);
	#else
		Builder.Appendf("NetHandle (Id=%u)", NetHandle.GetId());
	#endif
		return Builder;
}



