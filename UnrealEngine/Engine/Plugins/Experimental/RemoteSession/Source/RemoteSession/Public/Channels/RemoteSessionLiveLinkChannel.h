// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteSessionChannel.h"


class FBackChannelOSCDispatch;
class IBackChannelPacket;


class REMOTESESSION_API FRemoteSessionLiveLinkChannel :	public IRemoteSessionChannel
{
public:

	FRemoteSessionLiveLinkChannel(ERemoteSessionChannelMode InRole, TSharedPtr<IBackChannelConnection, ESPMode::ThreadSafe> InConnection);

	virtual ~FRemoteSessionLiveLinkChannel();

	virtual void Tick(const float InDeltaTime) override;

	/** Sends the current location and rotation for the XRTracking system to the remote */
	void SendLiveLinkHello(const FStringView InSubjectName);

	/* Begin IRemoteSessionChannel implementation */
	static const TCHAR* StaticType() { return TEXT("FRemoteSessionLiveLinkChannel"); }
	virtual const TCHAR* GetType() const override { return StaticType(); }
	/* End IRemoteSessionChannel implementation */

protected:
	

	/** Handles data coming from the client */
	void ReceiveLiveLinkHello(IBackChannelPacket& Message);

	TSharedPtr<IBackChannelConnection, ESPMode::ThreadSafe> Connection;

	ERemoteSessionChannelMode Role;

	/** So we can manage callback lifetimes properly */
	FDelegateHandle RouteHandle;

};
