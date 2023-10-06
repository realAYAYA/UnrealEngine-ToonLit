// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CookOnTheFlyServerConnection.h"

class FTCPTransport
	: public ICookOnTheFlyServerTransport
{

public:

	FTCPTransport();
	~FTCPTransport();

	// ITransport Interface. 
	virtual bool Initialize(const TCHAR* HostIp) override;
	virtual bool SendPayload(const TArray<uint8>& Payload) override;
	virtual bool ReceivePayload(FArrayReader& Payload) override;
	virtual bool HasPendingPayload() override;
	virtual void Disconnect() override;

private: 

	class FSocket*		FileSocket;
	class FMultichannelTcpSocket* MCSocket;
	FString HostName;
};
