// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once



#include "OSCClient.h"

class FInternetAddr;
class FSocket;
class IOSCPacket;


class OSC_API FOSCClientProxy : public IOSCClientProxy
{
public:
	FOSCClientProxy(const FString& InClientName);
	virtual ~FOSCClientProxy();

	void GetSendIPAddress(FString& InIPAddress, int32& Port) const override;
	bool SetSendIPAddress(const FString& InIPAddress, const int32 Port) override;

	bool IsActive() const override;

	void SendMessage(FOSCMessage& Message) override;
	void SendBundle(FOSCBundle& Bundle) override;

	void Stop() override;

private:
	void SendPacket(IOSCPacket& Packet);

	/** Socket used to send the OSC packets. */
	FSocket* Socket;

	/** IP Address used by socket. */
	TSharedPtr<FInternetAddr> IPAddress;

	/** Name of client */
	FString ClientName;
};
