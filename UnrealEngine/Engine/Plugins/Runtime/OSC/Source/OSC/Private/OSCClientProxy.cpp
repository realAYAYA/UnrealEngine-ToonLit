// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCClientProxy.h"

#include "Common/UdpSocketBuilder.h"
#include "Sockets.h"
#include "SocketTypes.h"

#include "OSCBundlePacket.h"
#include "OSCLog.h"
#include "OSCClient.h"
#include "OSCMessagePacket.h"

namespace
{
	static const int32 OUTPUT_BUFFER_SIZE = 1024;
} // namespace <>


FOSCClientProxy::FOSCClientProxy(const FString& InClientName)
	: Socket(FUdpSocketBuilder(*InClientName).Build())
{
	ClientName = InClientName;
}

FOSCClientProxy::~FOSCClientProxy()
{
	Stop();
}

void FOSCClientProxy::GetSendIPAddress(FString& InIPAddress, int32& Port) const
{
	const bool bAppendPort = false;
	InIPAddress = IPAddress->ToString(bAppendPort);
	Port = IPAddress->GetPort();
}

bool FOSCClientProxy::SetSendIPAddress(const FString& InIPAddress, const int32 Port)
{
	IPAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();

	bool bIsValidAddress = true;
	IPAddress->SetIp(*InIPAddress, bIsValidAddress);
	IPAddress->SetPort(Port);

	if (bIsValidAddress)
	{
		UE_LOG(LogOSC, Verbose, TEXT("OSCClient '%s' SetSendIpAddress: %s:%d"), *ClientName, *InIPAddress, Port);
	}
	else
	{
		UE_LOG(LogOSC, Warning, TEXT("OSCClient '%s' SetSendIpAddress Failed for input: %s:%d"), *ClientName, *InIPAddress, Port);
	}

	return bIsValidAddress;
}

bool FOSCClientProxy::IsActive() const
{
	return Socket && Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected;
}

void FOSCClientProxy::SendPacket(IOSCPacket& Packet)
{
	if (!Socket)
	{
		UE_LOG(LogOSC, Error, TEXT("Socket has been closed. OSCClient '%s' failed to send msg"), *ClientName);
		return;
	}

	const FOSCAddress* OSCAddress = nullptr;
	if (Packet.IsMessage())
	{
		const FOSCMessagePacket& MessagePacket = static_cast<FOSCMessagePacket&>(Packet);
		OSCAddress = &MessagePacket.GetAddress();
		if (!OSCAddress->IsValidPath())
		{
			UE_LOG(LogOSC, Warning, TEXT("Failed to write packet data. Invalid OSCAddress '%s'"), *OSCAddress->GetFullPath());
			return;
		}
	}

	if (IPAddress)
	{
		FOSCStream Stream = FOSCStream();
		Packet.WriteData(Stream);
		const uint8* DataPtr = Stream.GetData();

		int32 BytesSent = 0;

		const int32 AttemptedLength = Stream.GetPosition();
		int32 Length = AttemptedLength;
		while (Length > 0)
		{
			const bool bSuccess = Socket->SendTo(DataPtr, Length, BytesSent, *IPAddress);
			if (!bSuccess || BytesSent <= 0)
			{
				UE_LOG(LogOSC, Verbose, TEXT("OSC Packet failed: Client '%s', OSC Identifier '%s', Send IPAddress %s, Attempted Bytes = %d"),
					*ClientName, OSCAddress ? *OSCAddress->GetFullPath() : *OSC::BundleTag, *IPAddress->ToString(true /*bAppendPort*/), AttemptedLength);
				return;
			}

			Length -= BytesSent;
			DataPtr += BytesSent;
		}

		UE_LOG(LogOSC, Verbose, TEXT("OSC Packet sent: Client '%s', OSC Identifier '%s', Send IPAddress %s, Bytes Sent = %d"),
			*ClientName, OSCAddress ? *OSCAddress->GetFullPath() : *OSC::BundleTag, *IPAddress->ToString(true /*bAppendPort*/), AttemptedLength);
	}
}

void FOSCClientProxy::SendMessage(FOSCMessage& Message)
{
	const TSharedPtr<IOSCPacket>& Packet = Message.GetPacket();
	check(Packet.IsValid());
	SendPacket(*Packet.Get());
}

void FOSCClientProxy::SendBundle(FOSCBundle& Bundle)
{
	const TSharedPtr<IOSCPacket>& Packet = Bundle.GetPacket();
	check(Packet.IsValid());
	SendPacket(*Packet.Get());
}

void FOSCClientProxy::Stop()
{
	if (Socket)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}
