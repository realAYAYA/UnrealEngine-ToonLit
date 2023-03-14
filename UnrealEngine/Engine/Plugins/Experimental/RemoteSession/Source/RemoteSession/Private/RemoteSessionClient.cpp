// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteSessionClient.h"
#include "BackChannel/Transport/IBackChannelTransport.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCMessage.h"
#include "Framework/Application/SlateApplication.h"	
#include "Channels/RemoteSessionInputChannel.h"
#include "Channels/RemoteSessionXRTrackingChannel.h"
#include "Channels/RemoteSessionARCameraChannel.h"
#include "Channels/RemoteSessionFrameBufferChannel.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Sockets.h"
#include "RemoteSession.h"
#include "Async/Async.h"

DECLARE_CYCLE_STAT(TEXT("RSClientTick"), STAT_RDClientTick, STATGROUP_Game);

FRemoteSessionClient::FRemoteSessionClient(const TCHAR* InHostAddress)
{
	HostAddress = InHostAddress;
	ConnectionAttemptTimer = FLT_MAX;		// attempt a connection asap
	TimeConnectionAttemptStarted = 0;
    ConnectionTimeout = 5;

	IsConnecting = false;

	if (HostAddress.Contains(TEXT(":")) == false)
	{
		HostAddress += FString::Printf(TEXT(":%d"), (int32)IRemoteSessionModule::kDefaultPort);
	}

	UE_LOG(LogRemoteSession, Display, TEXT("Will attempt to connect to %s.."), *HostAddress);
}

FRemoteSessionClient::~FRemoteSessionClient()
{
	CloseConnections();
}

bool FRemoteSessionClient::IsConnected() const
{
	// this is to work-around the UE BSD socket implt always saying
	// things are connected for the first 5 secs...
	return FRemoteSessionRole::IsConnected() && Connection->GetPacketsReceived() > 0;
}

void FRemoteSessionClient::Tick(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_RDClientTick);

	if (IsConnected() == false)
	{
		if (IsConnecting == false && HasError()  == false)
		{
			const double TimeSinceLastAttempt = FPlatformTime::Seconds() - TimeConnectionAttemptStarted;

			if (TimeSinceLastAttempt >= 5.0)
			{
				StartConnection();
			}
		}

		if (IsConnecting)
		{
			CheckConnection();
		}
	}

	FRemoteSessionRole::Tick(DeltaTime);
}

void  FRemoteSessionClient::StartConnection()
{
	check(IsConnecting == false);

	CloseConnections();

	if (IBackChannelTransport* Transport = IBackChannelTransport::Get())
	{
		Connection = Transport->CreateConnection(IBackChannelTransport::TCP);

		if (Connection.IsValid())
		{
			if (Connection->Connect(*HostAddress))
			{
				IsConnecting = true;
				check(Connection->GetSocket());
			}
		}
	}

	TimeConnectionAttemptStarted = FPlatformTime::Seconds();
}

void FRemoteSessionClient::CheckConnection()
{
	check(IsConnected() == false && IsConnecting == true);
	check(Connection->GetSocket());

	// success indicates that our check was successful, if our connection was successful then
	// the delegate code is called
	bool Success = Connection->WaitForConnection(0, [this](auto InConnection)
	{
		CreateOSCConnection(Connection.ToSharedRef());
		
		UE_LOG(LogRemoteSession, Log, TEXT("Connected to host at %s"), *HostAddress);

		IsConnecting = false;

		//SetReceiveInBackground(true);

		return true;
	});

	const double TimeSpentConnecting = FPlatformTime::Seconds() - TimeConnectionAttemptStarted;

	if (IsConnected() == false)
	{
		if (Success == false || TimeSpentConnecting >= ConnectionTimeout)
		{
			IsConnecting = false;
			
			FString Msg;
			
			if (TimeSpentConnecting >= ConnectionTimeout)
			{
				Msg = FString::Printf(TEXT("Timing out connection attempt after %.02f seconds"), TimeSpentConnecting);
			}
			else
			{
				Msg = TEXT("Failed to check for connection. Aborting.");
			}
			
			UE_LOG(LogRemoteSession, Log, TEXT("%s"), *Msg);
			
			CloseConnections();
			TimeConnectionAttemptStarted = FPlatformTime::Seconds();
		}
	}
}

void FRemoteSessionClient::BindEndpoints(TBackChannelSharedPtr<IBackChannelConnection> InConnection)
{
	FRemoteSessionRole::BindEndpoints(InConnection);
		
	// both legacy and new channel messages can go to the same delegate
	auto Delegate = FBackChannelRouteDelegate::FDelegate::CreateRaw(this, &FRemoteSessionClient::OnReceiveChannelList);
	InConnection->AddRouteDelegate(kLegacyChannelSelectionEndPoint, Delegate);
	InConnection->AddRouteDelegate(kChannelListEndPoint, Delegate);
}

bool FRemoteSessionClient::ProcessStateChange(const ConnectionState NewState, const ConnectionState OldState)
{
	if (NewState == FRemoteSessionRole::ConnectionState::UnversionedConnection)
	{
		BindEndpoints(OSCConnection);

		// send these both. Hello will always win
		SendHello();

		SendLegacyVersionCheck();

		SetPendingState(FRemoteSessionRole::ConnectionState::EstablishingVersion);
	}

	return true;
}


void FRemoteSessionClient::OnReceiveChannelList(IBackChannelPacket& Message)
{
	AvailableChannels.Empty();

	// #agrant todo - not safe, how to express this now?
	FBackChannelOSCMessage& OSCMessage = *static_cast<FBackChannelOSCMessage*>(&Message);
	
	int NumChannels = 0; //
	
	if (!IsLegacyConnection())
	{
		OSCMessage.Read(TEXT("ChannelCount"), NumChannels);

		if (NumChannels == 0)
		{
			UE_LOG(LogRemoteSession, Error, TEXT("Received ChannelList messsage with no channels!"));

		}
		int ChannelTags = (OSCMessage.GetArgumentCount() - 1) / 2;
		ensureMsgf(NumChannels == ChannelTags, TEXT("Channel count was %d but number of channel pairs was %d"), NumChannels, ChannelTags);
	}
	else
	{
		// need to interpret this from the argument count
		NumChannels = OSCMessage.GetArgumentCount() / 2;
	}

	for (int i = 0; i < NumChannels; i++)
	{
		FString ChannelName;
		FString ChannelModeStr;
		ERemoteSessionChannelMode ChannelMode = ERemoteSessionChannelMode::Unknown;
		
		OSCMessage.Read(TEXT("Name"), ChannelName);

		if (IsLegacyConnection())
		{
			// in legacy modes we sent an int and only had read/write. 
			int32 ChannelModeInt = 0;
			OSCMessage.Read(TEXT("Mode"), ChannelModeInt);
			ChannelMode = (ERemoteSessionChannelMode)(ChannelModeInt+1);
		}
		else
		{
			// In new protocol a string.
			OSCMessage.Read(TEXT("Mode"), ChannelModeStr);
			LexFromString(ChannelMode, *ChannelModeStr);
		}
		
		if (ChannelName.Len() && ChannelMode > ERemoteSessionChannelMode::Unknown)
		{
			UE_LOG(LogRemoteSession, Log, TEXT("Remote host supports channel %s with mode %s"), *ChannelName, ::LexToString((ERemoteSessionChannelMode)ChannelMode));
			AvailableChannels.Emplace(MoveTemp(ChannelName), ChannelMode);
		}
		else
		{
			UE_LOG(LogRemoteSession, Error, TEXT("Failed to read channel from ChannelSelection message!"));
		}
	}

	if (IsLegacyConnection())
	{
		// Need to create channels on the main thread
		AsyncTask(ENamedThreads::GameThread, [this] {
				CreateChannels(AvailableChannels);
			});
		UE_LOG(LogRemoteSession, Log, TEXT("Creating all channels for legacy connection"));
	}
	else
	{
		// broadcast the delegate on the main thread
		AsyncTask(ENamedThreads::GameThread, [this] {
			ReceiveChannelListDelegate.Broadcast(this, AvailableChannels);
		});
	}
}
