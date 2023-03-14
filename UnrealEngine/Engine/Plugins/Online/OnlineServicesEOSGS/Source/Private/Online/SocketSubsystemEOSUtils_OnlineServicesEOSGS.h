// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_ENGINE

#include "SocketSubsystemEOS.h"

namespace UE::Online {

class FOnlineServicesEOSGS;

class FSocketSubsystemEOSUtils_OnlineServicesEOS : public ISocketSubsystemEOSUtils
{
public:
	FSocketSubsystemEOSUtils_OnlineServicesEOS(FOnlineServicesEOSGS& InServicesEOS);
	virtual ~FSocketSubsystemEOSUtils_OnlineServicesEOS();

	virtual EOS_ProductUserId GetLocalUserId() override;
	virtual FString GetSessionId() override;
	virtual FName GetSubsystemInstanceName() override;

private:
	FOnlineServicesEOSGS& ServicesEOSGS;
};

/* UE::Online */}

#endif // WITH_ENGINE