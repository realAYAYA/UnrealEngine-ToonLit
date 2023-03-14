// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCBundlePacket.h"

#include "OSCBundle.h"
#include "OSCLog.h"
#include "OSCMessage.h"
#include "OSCTypes.h"


FOSCBundlePacket::FOSCBundlePacket()
	: IOSCPacket()
	, TimeTag(0)
{
}

FOSCBundlePacket::~FOSCBundlePacket()
{
}

void FOSCBundlePacket::SetTimeTag(uint64 NewTimeTag)
{
	TimeTag = FOSCType(NewTimeTag);
}

uint64 FOSCBundlePacket::GetTimeTag() const
{
	return TimeTag.GetTimeTag();
}

void FOSCBundlePacket::WriteData(FOSCStream& Stream)
{
	// Write bundle & time tag
	Stream.WriteString(OSC::BundleTag);
	Stream.WriteUInt64(GetTimeTag());

	for (TSharedPtr<IOSCPacket>& Packet : Packets)
	{
		int32 StreamPos = Stream.GetPosition();
		Stream.WriteInt32(0);

		int32 InitPos = Stream.GetPosition();
		Packet->WriteData(Stream);
		int32 NewPos = Stream.GetPosition();

		Stream.SetPosition(StreamPos);
		Stream.WriteInt32(NewPos - InitPos);
		Stream.SetPosition(NewPos);
	}
}

void FOSCBundlePacket::ReadData(FOSCStream& Stream)
{
	Packets.Reset();

	FString BundleTag = Stream.ReadString();
	if (BundleTag != OSC::BundleTag)
	{
		UE_LOG(LogOSC, Warning, TEXT("Failed to parse OSCBundle of invalid format. #bundle identifier not first item in packet."));
		return;
	}

	TimeTag = FOSCType(Stream.ReadUInt64());

	while (!Stream.HasReachedEnd())
	{
		int32 PacketLength = Stream.ReadInt32();

		int32 StartPos = Stream.GetPosition();
		TSharedPtr<IOSCPacket> Packet = IOSCPacket::CreatePacket(Stream.GetData() + Stream.GetPosition(), IPAddress, Port);
		if (!Packet.IsValid())
		{
			break;
		}

		Packet->ReadData(Stream);
		Packets.Add(Packet);
		int32 EndPos = Stream.GetPosition();

		if (EndPos - StartPos != PacketLength)
		{
			UE_LOG(LogOSC, Warning, TEXT("Failed to parse OSCBundle of invalid format. Element size mismatch."));
			break;
		}
	}
}

FOSCBundlePacket::FPacketBundle& FOSCBundlePacket::GetPackets()
{
	return Packets;
}

bool FOSCBundlePacket::IsBundle()
{
	return true;
}

bool FOSCBundlePacket::IsMessage()
{
	return false;
}
