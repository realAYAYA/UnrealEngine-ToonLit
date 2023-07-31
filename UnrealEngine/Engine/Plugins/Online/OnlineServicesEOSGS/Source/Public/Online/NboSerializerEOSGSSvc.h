// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/NboSerializer.h"

#include "Online/AuthEOSGS.h"
#include "Online/LobbiesEOSGS.h"
#include "Online/SessionsEOSGS.h"
#include "Online/CoreOnline.h"
#include "Online/NboSerializerCommonSvc.h"

/**
 * Serializes data in network byte order form into a buffer
 */
namespace UE::Online {

namespace NboSerializerEOSGSSvc {

/** NboSerializeToBuffer methods */

inline void SerializeToBuffer(FNboSerializeToBuffer& Ar, const FAccountId& UniqueId)
{
	TArray<uint8> Data = FOnlineAccountIdRegistryEOSGS::GetRegistered().ToReplicationData(UniqueId);
	Ar << Data.Num();
	Ar.WriteBinary(Data.GetData(), Data.Num());
}

inline void SerializeToBuffer(FNboSerializeToBuffer& Ar, const FOnlineSessionId& SessionId)
{
	TArray<uint8> Data = FOnlineSessionIdRegistryEOSGS::Get().ToReplicationData(SessionId);
	Ar << Data.Num();
	Ar.WriteBinary(Data.GetData(), Data.Num());
}

inline void SerializeToBuffer(FNboSerializeToBuffer& Packet, const FSessionMemberIdsSet& SessionMembersSet)
{
	Packet << SessionMembersSet.Num();

	for (const FAccountId& SessionMember : SessionMembersSet)
	{
		SerializeToBuffer(Packet, SessionMember);
	}
}

/** NboSerializeFromBuffer methods */

inline void SerializeFromBuffer(FNboSerializeFromBuffer& Ar, FAccountId& UniqueId)
{
	TArray<uint8> Data;
	int32 Size;
	Ar >> Size;
	Ar.ReadBinaryArray(Data, Size);
	UniqueId = FOnlineAccountIdRegistryEOSGS::GetRegistered().FromReplicationData(Data);
}

inline void SerializeFromBuffer(FNboSerializeFromBuffer& Ar, FOnlineSessionId& SessionId)
{
	TArray<uint8> Data;
	int32 Size;
	Ar >> Size;
	Ar.ReadBinaryArray(Data, Size);
	SessionId = FOnlineSessionIdRegistryEOSGS::Get().FromReplicationData(Data);
}

inline void SerializeFromBuffer(FNboSerializeFromBuffer& Packet, FSessionMemberIdsSet& SessionMembersSet)
{
	int32 NumEntries = 0;
	Packet >> NumEntries;

	for (int32 Index = 0; Index < NumEntries; ++Index)
	{
		FAccountId Key;
		SerializeFromBuffer(Packet, Key);

		SessionMembersSet.Emplace(Key);
	}
}

/* NboSerializerNullSvc */ }

/* UE::Online */ }