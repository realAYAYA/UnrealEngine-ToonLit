// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Online/SessionsLAN.h"

namespace UE::Online {

class FOnlineServicesNull;

class FOnlineSessionIdRegistryNull : public FOnlineSessionIdRegistryLAN
{
public:
	static FOnlineSessionIdRegistryNull& Get();

private:
	FOnlineSessionIdRegistryNull();
};

typedef FSessionLAN FSessionNull;

class FSessionsNull : public FSessionsLAN
{
public:
	using Super = FSessionsLAN;

	FSessionsNull(FOnlineServicesNull& InServices);
	virtual ~FSessionsNull() = default;

private:
	// FSessionsLAN
	virtual void AppendSessionToPacket(FNboSerializeToBuffer& Packet, const FSessionLAN& Session) override;
	virtual void ReadSessionFromPacket(FNboSerializeFromBuffer& Packet, FSessionLAN& Session) override;
};

/* UE::Online */ }