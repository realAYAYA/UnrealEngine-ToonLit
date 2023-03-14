// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "OSCAddress.h"
#include "OSCPacket.h"
#include "OSCStream.h"
#include "OSCTypes.h"


class FOSCMessagePacket : public IOSCPacket
{
public:
	FOSCMessagePacket();
	virtual ~FOSCMessagePacket();

	/** Set OSC message address. */
	void SetAddress(const FOSCAddress& InAddress);

	/** Get OSC message address. */
	virtual const FOSCAddress& GetAddress() const;

	/** Get arguments array. */
	TArray<FOSCType>& GetArguments();

	/** Returns false to indicate type is not OSC bundle. */
	virtual bool IsBundle();

	/** Returns true to indicate its an OSC message. */
	virtual bool IsMessage();

	/** Write message data into an OSC stream. */
	virtual void WriteData(FOSCStream& Stream) override;

	/** Reads message data from an OSC stream and creates new argument. */
	virtual void ReadData(FOSCStream& Stream) override;

private:
	/** OSC address. */
	FOSCAddress Address;

	/** List of argument types. */
	TArray<FOSCType> Arguments;
};
