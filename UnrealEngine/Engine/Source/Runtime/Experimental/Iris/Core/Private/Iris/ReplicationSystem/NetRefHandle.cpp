// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "Containers/UnrealString.h"
#include "Misc/StringBuilder.h"

namespace UE::Net
{

FString FNetRefHandle::ToString() const
{
	FString Result;
#if UE_NET_ALLOW_MULTIPLE_REPLICATION_SYSTEMS
	if (IsCompleteHandle())
	{
		const uint32 ReplicationSystemIdToDisplay = GetReplicationSystemId();
		Result = FString::Printf(TEXT("NetRefHandle (Id=%" UINT64_FMT "):(RepSystemId=%u)"), GetId(), ReplicationSystemIdToDisplay);
	}
	else
	{
		Result = FString::Printf(TEXT("NetRefHandle (Id=%" UINT64_FMT "):(Incomplete)"), GetId());
	}
#else
	Result = FString::Printf(TEXT("NetRefHandle (Id=%" UINT64_FMT ")"), GetId());
#endif
	return Result;
}

}

FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const UE::Net::FNetRefHandle& NetRefHandle) 
{ 	
#if UE_NET_ALLOW_MULTIPLE_REPLICATION_SYSTEMS
	if (NetRefHandle.IsCompleteHandle())
	{
		const uint32 ReplicationSystemIdToDisplay = NetRefHandle.GetReplicationSystemId();
		Builder.Appendf(TEXT("NetRefHandle (Id=%" UINT64_FMT "):(RepSystemId=%u)"), NetRefHandle.GetId(), ReplicationSystemIdToDisplay);
	}
	else
	{
		Builder.Appendf(TEXT("NetRefHandle (Id=%" UINT64_FMT "):(Incomplete)"), NetRefHandle.GetId());
	}
#else
	Builder.Appendf(TEXT("NetRefHandle (Id=%" UINT64_FMT ")"), NetRefHandle.GetId());
#endif
	return Builder;
}

FAnsiStringBuilderBase& operator<<(FAnsiStringBuilderBase& Builder, const UE::Net::FNetRefHandle& NetRefHandle)
{
#if UE_NET_ALLOW_MULTIPLE_REPLICATION_SYSTEMS
	if (NetRefHandle.IsCompleteHandle())
	{
		const uint32 ReplicationSystemIdToDisplay = NetRefHandle.GetReplicationSystemId();
		Builder.Appendf("NetRefHandle (Id=%" UINT64_FMT "):(RepSystemId=%u)", NetRefHandle.GetId(), ReplicationSystemIdToDisplay);
	}
	else
	{
		Builder.Appendf("NetRefHandle (Id=%" UINT64_FMT "):(Incomplete)", NetRefHandle.GetId());
	}
#else
	Builder.Appendf("NetRefHandle (Id=%" UINT64_FMT ")", NetRefHandle.GetId());
#endif
	return Builder;
}
