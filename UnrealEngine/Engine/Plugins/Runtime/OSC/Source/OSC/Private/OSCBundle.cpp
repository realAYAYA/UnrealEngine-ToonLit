// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCBundle.h"

#include "OSCAddress.h"
#include "OSCLog.h"
#include "OSCBundlePacket.h"


FOSCBundle::FOSCBundle()
	: Packet(MakeShareable(new FOSCBundlePacket()))
{
}

FOSCBundle::FOSCBundle(const TSharedPtr<IOSCPacket>& InPacket)
	: Packet(InPacket)
{
}

FOSCBundle::~FOSCBundle()
{
	Packet.Reset();
}

void FOSCBundle::SetPacket(const TSharedPtr<IOSCPacket>& InPacket)
{
	Packet = InPacket;
}

const TSharedPtr<IOSCPacket>& FOSCBundle::GetPacket() const
{
	return Packet;
}
