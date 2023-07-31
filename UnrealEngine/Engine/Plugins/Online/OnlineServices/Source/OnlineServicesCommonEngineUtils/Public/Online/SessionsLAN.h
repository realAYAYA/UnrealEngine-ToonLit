// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Online/LANBeacon.h"
#include "Online/NboSerializer.h"
#include "Online/SessionsCommon.h"

namespace UE::Online {

class FOnlineServicesCommon;

class ONLINESERVICESCOMMONENGINEUTILS_API FOnlineSessionIdRegistryLAN : public FOnlineSessionIdStringRegistry
{
public:
	static FOnlineSessionIdRegistryLAN& GetChecked(EOnlineServices ServicesType);

	FOnlineSessionId GetNextSessionId();

protected:
	FOnlineSessionIdRegistryLAN(EOnlineServices ServicesType);
};

class ONLINESERVICESCOMMONENGINEUTILS_API FSessionLAN : public FSessionCommon
{
public:
	FSessionLAN();
	FSessionLAN(const FSessionLAN& InSession) = default;

	static FSessionLAN& Cast(FSessionCommon& InSession);
	static const FSessionLAN& Cast(const ISession& InSession);

	virtual void DumpState() const override;

private:
	void Initialize();

public:
	/** The IP address for the session owner */
	TSharedPtr<FInternetAddr> OwnerInternetAddr;
};

namespace NboSerializerLANSvc {

void ONLINESERVICESCOMMONENGINEUTILS_API SerializeToBuffer(FNboSerializeToBuffer& Packet, const FSessionLAN& Session);
void ONLINESERVICESCOMMONENGINEUTILS_API SerializeFromBuffer(FNboSerializeFromBuffer& Packet, FSessionLAN& Session);

/* NboSerializerLANSvc */ }

class ONLINESERVICESCOMMONENGINEUTILS_API FSessionsLAN : public FSessionsCommon
{
public:
	using Super = FSessionsCommon;

	FSessionsLAN(FOnlineServicesCommon& InServices);
	virtual ~FSessionsLAN() = default;

	virtual void Tick(float DeltaSeconds) override;

	// FSessionsCommon
	virtual TFuture<TOnlineResult<FCreateSession>> CreateSessionImpl(const FCreateSession::Params& Params) override;
	virtual TFuture<TOnlineResult<FUpdateSessionSettings>> UpdateSessionSettingsImpl(const FUpdateSessionSettings::Params& Params) override;
	virtual TFuture<TOnlineResult<FLeaveSession>> LeaveSessionImpl(const FLeaveSession::Params& Params) override;
	virtual TFuture<TOnlineResult<FFindSessions>> FindSessionsImpl(const FFindSessions::Params& Params) override;
	virtual TFuture<TOnlineResult<FJoinSession>> JoinSessionImpl(const FJoinSession::Params& Params) override;

protected:
	TOptional<FOnlineError> TryHostLANSession();
	void FindLANSessions(const FAccountId& LocalAccountId);
	void StopLANSession();
	void OnValidQueryPacketReceived(uint8* PacketData, int32 PacketLength, uint64 ClientNonce);
	void OnValidResponsePacketReceived(uint8* PacketData, int32 PacketLength, const FAccountId LocalAccountId);
	void OnLANSearchTimeout(const FAccountId LocalAccountId);
	virtual void AppendSessionToPacket(FNboSerializeToBuffer& Packet, const FSessionLAN& Session) = 0;
	virtual void ReadSessionFromPacket(FNboSerializeFromBuffer& Packet, FSessionLAN& Session) = 0;

protected:
	uint32 PublicSessionsHosted = 0;

	TSharedRef<FLANSession> LANSessionManager;
};

/* UE::Online */ }