// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCServer.h"

#include "Common/UdpSocketBuilder.h"
#include "Common/UdpSocketReceiver.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Runtime/Core/Public/Async/TaskGraphInterfaces.h"
#include "Sockets.h"

#include "OSCStream.h"
#include "OSCMessage.h"
#include "OSCMessagePacket.h"
#include "OSCBundle.h"
#include "OSCBundlePacket.h"
#include "OSCLog.h"
#include "OSCServerProxy.h"


UOSCServer::UOSCServer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ServerProxy(MakeUnique<FOSCServerProxy>(*this))
{
}

bool UOSCServer::GetMulticastLoopback() const
{
	check(ServerProxy.IsValid());
	return ServerProxy->GetMulticastLoopback();
}

bool UOSCServer::IsActive() const
{
	check(ServerProxy.IsValid());
	return ServerProxy->IsActive();
}

void UOSCServer::Listen()
{
	check(ServerProxy.IsValid());
	ServerProxy->Listen(GetName());
}

bool UOSCServer::SetAddress(const FString& InReceiveIPAddress, int32 InPort)
{
	check(ServerProxy.IsValid());
	return ServerProxy->SetAddress(InReceiveIPAddress, InPort);
}

void UOSCServer::SetMulticastLoopback(bool bInMulticastLoopback)
{
	check(ServerProxy.IsValid());
	ServerProxy->SetMulticastLoopback(bInMulticastLoopback);
}

#if WITH_EDITOR
void UOSCServer::SetTickInEditor(bool bInTickInEditor)
{
	check(ServerProxy.IsValid());
	ServerProxy->SetTickableInEditor(bInTickInEditor);
}
#endif // WITH_EDITOR


void UOSCServer::Stop()
{
	if (ServerProxy.IsValid())
	{
		ServerProxy->Stop();
	}
}

void UOSCServer::BeginDestroy()
{
	Stop();
	Super::BeginDestroy();
}

void UOSCServer::SetAllowlistClientsEnabled(bool bEnabled)
{
	check(ServerProxy.IsValid());
	ServerProxy->SetFilterClientsByAllowList(bEnabled);
}

void UOSCServer::AddAllowlistedClient(const FString& InIPAddress)
{
	check(ServerProxy.IsValid());
	ServerProxy->AddClientToAllowList(InIPAddress);
}

void UOSCServer::RemoveAllowlistedClient(const FString& InIPAddress)
{
	check(ServerProxy.IsValid());
	ServerProxy->RemoveClientFromAllowList(InIPAddress);
}

void UOSCServer::ClearAllowlistedClients()
{
	check(ServerProxy.IsValid());
	ServerProxy->ClearClientAllowList();
}

FString UOSCServer::GetIpAddress(bool bIncludePort) const
{
	check(ServerProxy.IsValid());

	FString Address = ServerProxy->GetIpAddress();
	if (bIncludePort)
	{
		Address += TEXT(":");
		Address.AppendInt(ServerProxy->GetPort());
	}

	return Address;
}

int32 UOSCServer::GetPort() const
{
	check(ServerProxy.IsValid());
	return ServerProxy->GetPort();
}

TSet<FString> UOSCServer::GetAllowlistedClients() const
{
	check(ServerProxy.IsValid());
	return ServerProxy->GetClientAllowList();
}

void UOSCServer::BindEventToOnOSCAddressPatternMatchesPath(const FOSCAddress& InOSCAddressPattern, const FOSCDispatchMessageEventBP& InEvent)
{
	if (InOSCAddressPattern.IsValidPattern())
	{
		FOSCDispatchMessageEvent& MessageEvent = AddressPatterns.FindOrAdd(InOSCAddressPattern);
		MessageEvent.AddUnique(InEvent);
	}
}

void UOSCServer::UnbindEventFromOnOSCAddressPatternMatchesPath(const FOSCAddress& InOSCAddressPattern, const FOSCDispatchMessageEventBP& InEvent)
{
	if (InOSCAddressPattern.IsValidPattern())
	{
		if (FOSCDispatchMessageEvent* AddressPatternEvent = AddressPatterns.Find(InOSCAddressPattern))
		{
			AddressPatternEvent->Remove(InEvent);
			if (!AddressPatternEvent->IsBound())
			{
				AddressPatterns.Remove(InOSCAddressPattern);
			}
		}
	}
}

void UOSCServer::UnbindAllEventsFromOnOSCAddressPatternMatchesPath(const FOSCAddress& InOSCAddressPattern)
{
	if (InOSCAddressPattern.IsValidPattern())
	{
		AddressPatterns.Remove(InOSCAddressPattern);
	}
}

void UOSCServer::UnbindAllEventsFromOnOSCAddressPatternMatching()
{
	AddressPatterns.Reset();
}

TArray<FOSCAddress> UOSCServer::GetBoundOSCAddressPatterns() const
{
	TArray<FOSCAddress> OutAddressPatterns;
	for (const TPair<FOSCAddress, FOSCDispatchMessageEvent>& Pair : AddressPatterns)
	{
		OutAddressPatterns.Add(Pair.Key);
	}
	return MoveTemp(OutAddressPatterns);
}

void UOSCServer::ClearPackets()
{
	OSCPackets.Empty();
}

void UOSCServer::EnqueuePacket(TSharedPtr<IOSCPacket> InPacket)
{
	OSCPackets.Enqueue(InPacket);
}

void UOSCServer::DispatchBundle(const FString& InIPAddress, uint16 InPort, const FOSCBundle& InBundle)
{
	OnOscBundleReceived.Broadcast(InBundle, InIPAddress, InPort);
	OnOscBundleReceivedNative.Broadcast(InBundle, InIPAddress, InPort);

	TSharedPtr<FOSCBundlePacket> BundlePacket = StaticCastSharedPtr<FOSCBundlePacket>(InBundle.GetPacket());
	FOSCBundlePacket::FPacketBundle Packets = BundlePacket->GetPackets();
	for (TSharedPtr<IOSCPacket>& Packet : Packets)
	{
		if (Packet->IsMessage())
		{
			DispatchMessage(InIPAddress, InPort, FOSCMessage(Packet));
		}
		else if (Packet->IsBundle())
		{
			DispatchBundle(InIPAddress, InPort, FOSCBundle(Packet));
		}
		else
		{
			UE_LOG(LogOSC, Warning, TEXT("Failed to parse invalid received message. Invalid OSC type (packet is neither identified as message nor bundle)."));
		}
	}
}

void UOSCServer::DispatchMessage(const FString& InIPAddress, uint16 InPort, const FOSCMessage& InMessage)
{
	OnOscMessageReceived.Broadcast(InMessage, InIPAddress, InPort);
	OnOscMessageReceivedNative.Broadcast(InMessage, InIPAddress, InPort);

	UE_LOG(LogOSC, Verbose, TEXT("Message received from endpoint '%s', OSCAddress of '%s'."), *InIPAddress, *InMessage.GetAddress().GetFullPath());

	for (const TPair<FOSCAddress, FOSCDispatchMessageEvent>& Pair : AddressPatterns)
	{
		const FOSCDispatchMessageEvent& DispatchEvent = Pair.Value;
		if (Pair.Key.Matches(InMessage.GetAddress()))
		{
			DispatchEvent.Broadcast(Pair.Key, InMessage, InIPAddress, InPort);
			UE_LOG(LogOSC, Verbose, TEXT("Message dispatched from endpoint '%s', OSCAddress path of '%s' matched OSCAddress pattern '%s'."),
				*InIPAddress,
				*InMessage.GetAddress().GetFullPath(),
				*Pair.Key.GetFullPath());
		}
	}
}

void UOSCServer::PumpPacketQueue(const TSet<uint32>* AllowlistedClients)
{
	TSharedPtr<IOSCPacket> Packet;
	while (OSCPackets.Dequeue(Packet))
	{
		FIPv4Address IPAddr;
		const FString& Address = Packet->GetIPAddress();
		if (!AllowlistedClients || (FIPv4Address::Parse(Address, IPAddr) && AllowlistedClients->Contains(IPAddr.Value)))
		{
			uint16 Port = Packet->GetPort();
			if (Packet->IsMessage())
			{
				DispatchMessage(Address, Port, FOSCMessage(Packet));
			}
			else if (Packet->IsBundle())
			{
				DispatchBundle(Address, Port, FOSCBundle(Packet));
			}
			else
			{
				UE_LOG(LogOSC, Warning, TEXT("Failed to parse invalid received message. Invalid OSC type (packet is neither identified as message nor bundle)."));
			}
		}
	}
}