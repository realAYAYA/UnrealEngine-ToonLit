// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteSessionHost.h"
#include "BackChannel/IBackChannelPacket.h"
#include "BackChannel/Transport/IBackChannelTransport.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCConnection.h"
#include "Framework/Application/SlateApplication.h"
#include "BackChannel/Transport/IBackChannelSocketConnection.h"
#include "RemoteSessionModule.h"
#include "Trace/Trace.inl"

#if WITH_EDITOR
#endif

namespace RemoteSessionEd
{
	static FAutoConsoleVariable SlateDragDistanceOverride(TEXT("RemoteSessionEd.SlateDragDistanceOverride"), 10.0f, TEXT("How many pixels you need to drag before a drag and drop operation starts in remote app"));
};


FRemoteSessionHost::FRemoteSessionHost(TArray<FRemoteSessionChannelInfo> InSupportedChannels)
	: HostTCPPort(0)
	, IsListenerConnected(false)
{
	SupportedChannels = InSupportedChannels;
	SavedEditorDragTriggerDistance = FSlateApplication::Get().GetDragTriggerDistance();
}

FRemoteSessionHost::~FRemoteSessionHost()
{
	// close this manually to force the thread to stop before things start to be 
	// destroyed
	if (Listener.IsValid())
	{
		Listener->Close();
	}

	CloseConnections();
}

void FRemoteSessionHost::CloseConnections()
{
	FRemoteSessionRole::CloseConnections();

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetDragTriggerDistance(SavedEditorDragTriggerDistance);
	}
}

void FRemoteSessionHost::SetScreenSharing(const bool bEnabled)
{
}


bool FRemoteSessionHost::StartListening(const uint16 InPort)
{
	if (Listener.IsValid())
	{
		return false;
	}

	if (IBackChannelTransport* Transport = IBackChannelTransport::Get())
	{
		Listener = Transport->CreateConnection(IBackChannelTransport::TCP);

		if (Listener->Listen(InPort) == false)
		{
			Listener = nullptr;
		}
		HostTCPPort = InPort;
	}

	return Listener.IsValid();
}

bool FRemoteSessionHost::ProcessStateChange(const ConnectionState NewState, const ConnectionState OldState)
{
	if (NewState == FRemoteSessionRole::ConnectionState::UnversionedConnection)
	{
		BindEndpoints(OSCConnection);

		// send these both. Hello will always win
		SendHello();

		SendLegacyVersionCheck();

		SetPendingState(FRemoteSessionRole::ConnectionState::EstablishingVersion);
	}
	else if (NewState == FRemoteSessionRole::ConnectionState::Connected)
	{
		ClearChannels();

		IsListenerConnected = true;

		SendChannelListToConnection();
	}

	return true;
}


void FRemoteSessionHost::BindEndpoints(TBackChannelSharedPtr<IBackChannelConnection> InConnection)
{
	FRemoteSessionRole::BindEndpoints(InConnection);	
}


void FRemoteSessionHost::SendChannelListToConnection()
{	
	FRemoteSessionModule& RemoteSession = FModuleManager::GetModuleChecked<FRemoteSessionModule>("RemoteSession");

	TBackChannelSharedPtr<IBackChannelPacket> Packet = OSCConnection->CreatePacket();

	if (IsLegacyConnection())
	{
		Packet->SetPath(kLegacyChannelSelectionEndPoint);
	}
	else
	{
		Packet->SetPath(kChannelListEndPoint);
		Packet->Write(TEXT("ChannelCount"), SupportedChannels.Num());
	}
	

	// send these across as a name/mode pair
	for (const FRemoteSessionChannelInfo& Channel : SupportedChannels)
	{
		ERemoteSessionChannelMode ClientMode = (Channel.Mode == ERemoteSessionChannelMode::Write) ? ERemoteSessionChannelMode::Read : ERemoteSessionChannelMode::Write;

		Packet->Write(TEXT("ChannelName"), Channel.Type);

		if (IsLegacyConnection())
		{
			// legacy mode is an int where 0 = read and 1 = write
			int32 ClientInt = ClientMode == ERemoteSessionChannelMode::Read ? 0 : 1;
			Packet->Write(TEXT("ChannelMode"), ClientInt);
		}
		else
		{
			// new protocol is a string
			Packet->Write(TEXT("ChannelMode"), ::LexToString(ClientMode));
		}

		UE_LOG(LogRemoteSession, Log, TEXT("Offering channel %s with mode %d"), *Channel.Type, int(ClientMode));
	}
	
	OSCConnection->SendPacket(Packet);

	if (IsLegacyConnection())
	{
		UE_LOG(LogRemoteSession, Log, TEXT("Pre-creating channels for legacy connection"));
		CreateChannels(SupportedChannels);
	}
}


void FRemoteSessionHost::Tick(float DeltaTime)
{
	// non-threaded listener
	if (IsConnected() == false)
	{
		if (Listener.IsValid() && IsListenerConnected)
		{
			Listener->Close();
			Listener = nullptr;

			//reset the host TCP socket
			StartListening(HostTCPPort);
			IsListenerConnected = false;
		}
        
        if (Listener.IsValid())
        {
            Listener->WaitForConnection(0, [this](TSharedRef<IBackChannelSocketConnection> InConnection) {
                CloseConnections();
				Connection = InConnection;
                CreateOSCConnection(InConnection);
                return true;
            });
        }
	}
	
	FRemoteSessionRole::Tick(DeltaTime);
}
