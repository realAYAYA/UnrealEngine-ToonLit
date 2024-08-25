// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "Containers/UnrealString.h"
#include "Misc/StringBuilder.h"

namespace UE::Net
{

FString FNetRefHandle::ToString() const
{
	if (IsCompleteHandle())
	{
		const uint32 ReplicationSystemIdToDisplay = GetReplicationSystemId();
		return FString::Printf(TEXT("NetRefHandle (Id=%" UINT64_FMT "):(RepSystemId=%u)"), GetId(), ReplicationSystemIdToDisplay);
	}
	else
	{
		return FString::Printf(TEXT("NetRefHandle (Id=%" UINT64_FMT "):(RepSystemId=?)"), GetId());
	}
}

}

FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const UE::Net::FNetRefHandle& NetRefHandle) 
{ 	
	if (NetRefHandle.IsCompleteHandle())
	{
		const uint32 ReplicationSystemIdToDisplay = NetRefHandle.GetReplicationSystemId();
		Builder.Appendf(TEXT("NetRefHandle (Id=%" UINT64_FMT "):(RepSystemId=%u)"), NetRefHandle.GetId(), ReplicationSystemIdToDisplay);
	}
	else
	{
		Builder.Appendf(TEXT("NetRefHandle (Id=%" UINT64_FMT "):(RepSystemId=?)"), NetRefHandle.GetId());
	}

	return Builder;
}

FAnsiStringBuilderBase& operator<<(FAnsiStringBuilderBase& Builder, const UE::Net::FNetRefHandle& NetRefHandle)
{
	if (NetRefHandle.IsCompleteHandle())
	{
		const uint32 ReplicationSystemIdToDisplay = NetRefHandle.GetReplicationSystemId();
		Builder.Appendf("NetRefHandle (Id=%" UINT64_FMT "):(RepSystemId=%u)", NetRefHandle.GetId(), ReplicationSystemIdToDisplay);
	}
	else
	{
		Builder.Appendf("NetRefHandle (Id=%" UINT64_FMT "):(RepSystemId=?)", NetRefHandle.GetId());
	}

	return Builder;
}
