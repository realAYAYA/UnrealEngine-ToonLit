// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCPacket.h"

#include "OSCMessagePacket.h"
#include "OSCBundlePacket.h"
#include "OSCLog.h"


TSharedPtr<IOSCPacket> IOSCPacket::CreatePacket(const uint8* InPacketType, const FString& InIPAddress, uint16 InPort)
{
	const FString PacketIdentifier(ANSI_TO_TCHAR((const ANSICHAR*)&InPacketType[0]));
	
	TSharedPtr<IOSCPacket> Packet;
	if (PacketIdentifier.StartsWith(OSC::PathSeparator))
	{
		Packet = MakeShared<FOSCMessagePacket>();
	}
	else if (PacketIdentifier == OSC::BundleTag)
	{
		Packet = MakeShared<FOSCBundlePacket>();
	}
	else
	{
		UE_LOG(LogOSC, Warning, TEXT("Failed to parse lead character of OSC packet. "
			"Lead identifier of '%c' not valid bundle tag ('%s') or message ('%s') identifier."), *PacketIdentifier, *OSC::BundleTag, *OSC::PathSeparator);
		return nullptr;
	}

	Packet->IPAddress = InIPAddress;
	Packet->Port = InPort;
	return Packet;
}

const FString& IOSCPacket::GetIPAddress() const
{
	return IPAddress;
}

uint16 IOSCPacket::GetPort() const
{
	return Port;
}