// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CookOnTheFly.h"

class FArrayReader;

class ICookOnTheFlyServerTransport
{
public:
	ICookOnTheFlyServerTransport() {};
	virtual ~ICookOnTheFlyServerTransport() {};

	virtual bool Initialize(const TCHAR*) = 0;
	virtual void Disconnect() = 0;
	virtual bool SendPayload(const TArray<uint8>& Payload) = 0;
	virtual bool HasPendingPayload() = 0;
	virtual bool ReceivePayload(FArrayReader& Payload) = 0;
};

UE::Cook::ICookOnTheFlyServerConnection* MakeCookOnTheFlyServerConnection(TUniquePtr<ICookOnTheFlyServerTransport> InTransport, const FString& InHost);
