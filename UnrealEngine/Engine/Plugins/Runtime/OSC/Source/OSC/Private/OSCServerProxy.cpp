// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCServerProxy.h"

#include "Common/UdpSocketBuilder.h"
#include "CoreGlobals.h"
#include "Sockets.h"
#include "Stats/Stats.h"
#include "Tickable.h"

#include "OSCLog.h"
#include "OSCStream.h"
#include "OSCServer.h"


FOSCServerProxy::FOSCServerProxy(UOSCServer& InServer)
	: Server(&InServer)
	, Socket(nullptr)
	, SocketReceiver(nullptr)
	, Port(0)
	, bMulticastLoopback(false)
	, bFilterClientsByAllowList(false)
#if WITH_EDITOR
	, bTickInEditor(false)
#endif // WITH_EDITOR
{
}

FOSCServerProxy::~FOSCServerProxy()
{
	Stop();
}

void FOSCServerProxy::OnPacketReceived(const FArrayReaderPtr& InData, const FIPv4Endpoint& InEndpoint)
{
	TSharedPtr<IOSCPacket> Packet = IOSCPacket::CreatePacket(InData->GetData(), InEndpoint.Address.ToString(), InEndpoint.Port);
	if (!Packet.IsValid())
	{
		UE_LOG(LogOSC, Verbose, TEXT("Message received from endpoint '%s' invalid OSC packet."), *InEndpoint.ToString());
		return;
	}

	FOSCStream Stream = FOSCStream(InData->GetData(), InData->Num());
	Packet->ReadData(Stream);
	Server->EnqueuePacket(Packet);
}

FString FOSCServerProxy::GetIpAddress() const
{
	return ReceiveIPAddress.ToString();
}

int32 FOSCServerProxy::GetPort() const
{
	return Port;
}

bool FOSCServerProxy::GetMulticastLoopback() const
{
	return bMulticastLoopback;
}

bool FOSCServerProxy::IsActive() const
{
	return SocketReceiver != nullptr;
}

void FOSCServerProxy::Listen(const FString& InServerName)
{
	if (IsActive())
	{
		UE_LOG(LogOSC, Error, TEXT("OSCServer currently listening: %s:%d. Failed to start new service prior to calling stop."),
			*InServerName, *ReceiveIPAddress.ToString(), Port);
		return;
	}

	FUdpSocketBuilder Builder(*InServerName);
	Builder.BoundToPort(Port);
	if (ReceiveIPAddress.IsMulticastAddress())
	{
		Builder.JoinedToGroup(ReceiveIPAddress);
		if (bMulticastLoopback)
		{
			Builder.WithMulticastLoopback();
		}
	}
	else
	{
		if (bMulticastLoopback)
		{
			UE_LOG(LogOSC, Warning, TEXT("OSCServer '%s' ReceiveIPAddress provided is not a multicast address.  Not respecting MulticastLoopback boolean."),
				*InServerName);
		}
		Builder.BoundToAddress(ReceiveIPAddress);
	}

	Socket = Builder.Build();
	if (Socket)
	{
		SocketReceiver = new FUdpSocketReceiver(Socket, FTimespan::FromMilliseconds(100), *(InServerName + TEXT("_ListenerThread")));
		SocketReceiver->OnDataReceived().BindRaw(this, &FOSCServerProxy::OnPacketReceived);
		SocketReceiver->Start();

		UE_LOG(LogOSC, Display, TEXT("OSCServer '%s' Listening: %s:%d."), *InServerName, *ReceiveIPAddress.ToString(), Port);
	}
	else
	{
		// This is expected when the server isn't available, so it's not a Warning
		UE_LOG(LogOSC, Display, TEXT("OSCServer '%s' failed to bind to socket on %s:%d. Check that the server is available on the specified address."), *InServerName, *ReceiveIPAddress.ToString(), Port);
	}
}

bool FOSCServerProxy::SetAddress(const FString& InReceiveIPAddress, int32 InPort)
{
	if (IsActive())
	{
		UE_LOG(LogOSC, Error, TEXT("Cannot set address while OSCServer is active."));
		return false;
	}

	if (!FIPv4Address::Parse(InReceiveIPAddress, ReceiveIPAddress))
	{
		UE_LOG(LogOSC, Error, TEXT("Invalid ReceiveIPAddress '%s'. OSCServer ReceiveIP Address not updated."), *InReceiveIPAddress);
		return false;
	}

	Port = InPort;
	return true;
}

void FOSCServerProxy::SetMulticastLoopback(bool bInMulticastLoopback)
{
	if (bInMulticastLoopback != bMulticastLoopback && IsActive())
	{
		UE_LOG(LogOSC, Error, TEXT("Cannot update MulticastLoopback while OSCServer is active."));
		return;
	}

	bMulticastLoopback = bInMulticastLoopback;
}

#if WITH_EDITOR
bool FOSCServerProxy::IsTickableInEditor() const
{
	return bTickInEditor;
}

void FOSCServerProxy::SetTickableInEditor(bool bInTickInEditor)
{
	bTickInEditor = bInTickInEditor;
}
#endif // WITH_EDITOR

void FOSCServerProxy::Stop()
{
	if (SocketReceiver)
	{
		delete SocketReceiver;
		SocketReceiver = nullptr;
	}

	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}


}

void FOSCServerProxy::AddClientToAllowList(const FString& InIPAddress)
{
	FIPv4Address OutAddress;
	if (!FIPv4Address::Parse(InIPAddress, OutAddress))
	{
		UE_LOG(LogOSC, Warning, TEXT("OSCServer '%s' failed to add IP Address '%s' to allow list. Address is invalid."), *InIPAddress);
		return;
	}

	ClientAllowList.Add(OutAddress.Value);
}

void FOSCServerProxy::RemoveClientFromAllowList(const FString& InIPAddress)
{
	FIPv4Address OutAddress;
	if (!FIPv4Address::Parse(InIPAddress, OutAddress))
	{
		UE_LOG(LogOSC, Warning, TEXT("OSCServer '%s' failed to remove IP Address '%s' from allow list. Address is invalid."), *InIPAddress);
		return;
	}

	ClientAllowList.Remove(OutAddress.Value);
}

void FOSCServerProxy::ClearClientAllowList()
{
	ClientAllowList.Reset();
}

TSet<FString> FOSCServerProxy::GetClientAllowList() const
{
	TSet<FString> Result;
	for (uint32 Client : ClientAllowList)
	{
		Result.Add(FIPv4Address(Client).ToString());
	}

	return Result;
}

void FOSCServerProxy::SetFilterClientsByAllowList(bool bInEnabled)
{
	bFilterClientsByAllowList = bInEnabled;
}

void FOSCServerProxy::Tick(float InDeltaTime)
{
	check(IsInGameThread());
	check(Server);

	Server->PumpPacketQueue(bFilterClientsByAllowList ? &ClientAllowList : nullptr);
}

TStatId FOSCServerProxy::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FOSCServerProxy, STATGROUP_Tickables);
}

UWorld* FOSCServerProxy::GetTickableGameObjectWorld() const
{
	check(Server);
	return Server->GetWorld();
}
