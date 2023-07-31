// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "OSCTypes.h"
#include "OSCStream.h"
#include "OSCAddress.h"

// Forward Declarations
class FOSCStream;


class OSC_API IOSCPacket
{
public:
	IOSCPacket() = default;
	virtual ~IOSCPacket() = default;

	/** Write packet data into stream */
	virtual void WriteData(FOSCStream& OutStream) = 0;
	
	/** Read packet data from stream */
	virtual void ReadData(FOSCStream& OutStream) = 0;
	
	/** Returns true if packet is message */
	virtual bool IsMessage() = 0;
	
	/** Returns true if packet is bundle */
	virtual bool IsBundle() = 0;

	/** Get endpoint IP address responsible for creation/forwarding of packet */
	virtual const FString& GetIPAddress() const;

	/** Get endpoint address responsible for creation/forwarding of packet */
	virtual uint16 GetPort() const;

	/** Create an OSC packet according to the input data. */
	static TSharedPtr<IOSCPacket> CreatePacket(const uint8* InPacketType, const FString& InAddress, uint16 InPort);

protected:
	FString IPAddress;
	uint16 Port;
};
