// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlWebSocketServer.h"
#include "Containers/Ticker.h"
#include "WebRemoteControlInternalUtils.h"
#include "IPAddress.h"
#include "IRemoteControlModule.h"
#include "IWebSocketNetworkingModule.h"
#include "RemoteControlRequest.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "WebSocketNetworkingDelegates.h"

#define LOCTEXT_NAMESPACE "RCWebSocketServer"

namespace RemoteControlWebSocketServer
{
	static const FString MessageNameFieldName = TEXT("MessageName");
	static const FString PayloadFieldName = TEXT("Parameters");

	TOptional<FRemoteControlWebSocketMessage> ParseWebsocketMessage(TArrayView<uint8> InPayload)
	{
		FRCWebSocketRequest Request;
		bool bSuccess = WebRemoteControlInternalUtils::DeserializeRequestPayload(InPayload, nullptr, Request);

		FString ErrorText;
		if (Request.MessageName.IsEmpty())
		{
			ErrorText = TEXT("Missing MessageName field.");
		}

		FBlockDelimiters PayloadDelimiters = Request.GetParameterDelimiters(FRCWebSocketRequest::ParametersFieldLabel());
		if (PayloadDelimiters.BlockStart == PayloadDelimiters.BlockEnd)
		{
			ErrorText = FString::Printf(TEXT("Missing %s field."), *FRCWebSocketRequest::ParametersFieldLabel());
		}

		TOptional<FRemoteControlWebSocketMessage> ParsedMessage;
		if (!bSuccess)
		{
			UE_LOG(LogRemoteControl, Error, TEXT("%s"), *FString::Format(TEXT("Encountered error while deserializing websocket message. \r\n{0}"), { *ErrorText }));
		}
		else
		{
			FRemoteControlWebSocketMessage Message;
			Message.MessageId = Request.Id;
			if (PayloadDelimiters.BlockStart != PayloadDelimiters.BlockEnd)
			{
				Message.RequestPayload = MakeArrayView(InPayload).Slice(PayloadDelimiters.BlockStart, PayloadDelimiters.BlockEnd - PayloadDelimiters.BlockStart);
			}
			if (!Request.Passphrase.IsEmpty())
			{
				Message.Header.FindOrAdd(WebRemoteControlInternalUtils::PassphraseHeader) = TArray<FString>({Request.Passphrase});
			}
			Message.MessageName = MoveTemp(Request.MessageName);
			ParsedMessage = MoveTemp(Message);
		}

		return ParsedMessage;
	}
}

void FWebsocketMessageRouter::Dispatch(const FRemoteControlWebSocketMessage& Message)
{
	if (FWebSocketMessageDelegate* Callback = DispatchTable.Find(Message.MessageName))
	{
		Callback->ExecuteIfBound(Message);
	}
}

void FWebsocketMessageRouter::AddPreDispatch(TFunction<bool(const FRemoteControlWebSocketMessage& Message)> WebsocketPreprocessor)
{
	DispatchPreProcessor.Add(WebsocketPreprocessor);
}

void FWebsocketMessageRouter::AttemptDispatch(const struct FRemoteControlWebSocketMessage& Message)
{
	if (!PreDispatch(Message))
	{
		return;
	}

	Dispatch(Message);
}

bool FWebsocketMessageRouter::PreDispatch(const FRemoteControlWebSocketMessage& Message) const
{
	for (TFunction<bool(const FRemoteControlWebSocketMessage&)> PreprocessFunction : DispatchPreProcessor)
	{
		if (!PreprocessFunction(Message))
		{
			return false;
		}
	}
	return true;
}

void FWebsocketMessageRouter::BindRoute(const FString& MessageName, FWebSocketMessageDelegate OnMessageReceived)
{
	DispatchTable.Add(MessageName, MoveTemp(OnMessageReceived));
}

void FWebsocketMessageRouter::UnbindRoute(const FString& MessageName)
{
	DispatchTable.Remove(MessageName);
}

bool FRCWebSocketServer::Start(uint32 Port, TSharedPtr<FWebsocketMessageRouter> InRouter)
{
	if (!IsPortAvailable(Port))
	{
		return false;
	}

	FWebSocketClientConnectedCallBack CallBack;
	CallBack.BindRaw(this, &FRCWebSocketServer::OnWebSocketClientConnected);

	Server = FModuleManager::Get().LoadModuleChecked<IWebSocketNetworkingModule>(TEXT("WebSocketNetworking")).CreateServer();
	
	if (!Server || !Server->Init(Port, CallBack))
	{
		Server.Reset();
		return false;
	}

	Router = MoveTemp(InRouter);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FRCWebSocketServer::Tick));

	return true;
}

bool FRCWebSocketServer::IsPortAvailable(uint32 Port) const
{
	if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
		Addr->SetAnyAddress();
		Addr->SetPort(Port);

		if (FUniqueSocket Socket = SocketSubsystem->CreateUniqueSocket(NAME_Stream, TEXT("TemporarySocket")))
		{
			if (Socket->Bind(*Addr))
			{
				return true;
			}
		}
	}
	
	return false;
}

void FRCWebSocketServer::Stop()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	Router.Reset();
	Server.Reset();
}

FRCWebSocketServer::~FRCWebSocketServer()
{
	Stop();
}

void FRCWebSocketServer::Broadcast(const TArray<uint8>& InUTF8Payload)
{
	for (FWebSocketConnection& Connection : Connections)
	{
		if (Connection.Socket)
		{
			Connection.Socket->Send(InUTF8Payload.GetData(), InUTF8Payload.Num(), /*PrependSize=*/false);
		}
	}
}

void FRCWebSocketServer::Send(const FGuid& InTargetClientId, const TArray<uint8>& InUTF8Payload)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRCWebSocketServer::Send);
	if (FWebSocketConnection* Connection = Connections.FindByPredicate([&InTargetClientId](const FWebSocketConnection& InConnection) { return InConnection.Id == InTargetClientId; }))
	{
		Connection->Socket->Send(InUTF8Payload.GetData(), InUTF8Payload.Num(), /*PrependSize=*/false);
	}
}

bool FRCWebSocketServer::IsRunning() const
{
	return !!Server;
}

bool FRCWebSocketServer::Tick(float DeltaTime)
{
	Server->Tick();
	return true;
}

void FRCWebSocketServer::OnWebSocketClientConnected(INetworkingWebSocket* Socket)
{
	if (ensureMsgf(Socket, TEXT("Socket was null while creating a new websocket connection.")))
	{
		FWebSocketConnection Connection = FWebSocketConnection{ Socket };
			
		FWebSocketPacketReceivedCallBack ReceiveCallBack;
		ReceiveCallBack.BindRaw(this, &FRCWebSocketServer::ReceivedRawPacket, Connection.Id);
		Socket->SetReceiveCallBack(ReceiveCallBack);

		FWebSocketInfoCallBack CloseCallback;
		CloseCallback.BindRaw(this, &FRCWebSocketServer::OnSocketClose, Socket);
		Socket->SetSocketClosedCallBack(CloseCallback);

		OnConnectionOpened().Broadcast(Connection.Id);
		Connections.Add(MoveTemp(Connection));
	}
}

void FRCWebSocketServer::ReceivedRawPacket(void* Data, int32 Size, FGuid ClientId)
{
	if (!Router)
	{
		return;
	}
	
	TRACE_CPUPROFILER_EVENT_SCOPE(FRCWebSocketServer::ReceivedRawPacket);
	
	TArray<uint8> Payload;
	WebRemoteControlUtils::ConvertToTCHAR(MakeArrayView(static_cast<uint8*>(Data), Size), Payload);

	if (TOptional<FRemoteControlWebSocketMessage> Message = RemoteControlWebSocketServer::ParseWebsocketMessage(Payload))
	{
		Message->ClientId = ClientId;
		Router->AttemptDispatch(*Message);
	}
}

void FRCWebSocketServer::OnSocketClose(INetworkingWebSocket* Socket)
{
	int32 Index = Connections.IndexOfByPredicate([Socket](const FWebSocketConnection& Connection) { return Connection.Socket == Socket; });
	if (Index != INDEX_NONE)
	{
		OnConnectionClosed().Broadcast(Connections[Index].Id);
		Connections.RemoveAtSwap(Index);
	}
}


#undef LOCTEXT_NAMESPACE /* FRCWebSocketServer */

