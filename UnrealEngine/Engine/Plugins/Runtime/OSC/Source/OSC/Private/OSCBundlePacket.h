// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "OSCAddress.h"
#include "OSCPacket.h"
#include "OSCStream.h"
#include "OSCTypes.h"


class OSC_API FOSCBundlePacket : public IOSCPacket
{
public:
	FOSCBundlePacket();
	virtual ~FOSCBundlePacket();

	using FPacketBundle = TArray<TSharedPtr<IOSCPacket>>;

	/** Set the bundle time tag. */
	void SetTimeTag(uint64 NewTimeTag);

	/** Get the bundle time tag. */
	uint64 GetTimeTag() const;

	/** Get OSC packet by index. */
	FPacketBundle& GetPackets();

	virtual bool IsBundle() override;
	virtual bool IsMessage() override;

	/** Writes bundle data into the OSC stream. */
	virtual void WriteData(FOSCStream& Stream) override;

	/** Reads bundle data from provided OSC stream,
	  * adding packet data to internal packet bundle. */
	virtual void ReadData(FOSCStream& Stream) override;

private:
	/** Bundle of OSC packets. */
	FPacketBundle Packets;

	/** Bundle time tag. */
	FOSCType TimeTag;
};
