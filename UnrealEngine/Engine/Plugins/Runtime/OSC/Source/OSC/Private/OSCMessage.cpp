// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCMessage.h"

#include "OSCMessagePacket.h"
#include "OSCStream.h"


FOSCMessage::FOSCMessage()
	: Packet(MakeShared<FOSCMessagePacket>())
{
}

FOSCMessage::FOSCMessage(const TSharedPtr<IOSCPacket>& InPacket)
	: Packet(InPacket)
{
}

FOSCMessage::~FOSCMessage()
{
	Packet.Reset();
}

void FOSCMessage::SetPacket(TSharedPtr<IOSCPacket>& InPacket)
{
	Packet = InPacket;
}

const TSharedPtr<IOSCPacket>& FOSCMessage::GetPacket() const
{
	return Packet;
}


bool FOSCMessage::SetAddress(const FOSCAddress& InAddress)
{
	check(Packet.IsValid());

	if (!InAddress.IsValidPath())
	{
		UE_LOG(LogOSC, Warning, TEXT("Attempting to set invalid OSCAddress '%s'. OSC address must begin with '/'"), *InAddress.GetFullPath());
		return false;
	}

	StaticCastSharedPtr<FOSCMessagePacket>(Packet)->SetAddress(InAddress);
	return true;
}

const FOSCAddress& FOSCMessage::GetAddress() const
{
	check(Packet.IsValid());
	return StaticCastSharedPtr<FOSCMessagePacket>(Packet)->GetAddress();
}