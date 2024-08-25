// Copyright Epic Games, Inc. All Rights Reserved.


#include "WebSocketMessageTransport.h"
#include "Backends/CborStructSerializerBackend.h"
#include "CborReader.h"
#include "CborWriter.h"
#include "IMessageContext.h"
#include "IMessageTransportHandler.h"
#include "INetworkingWebSocket.h"
#include "IWebSocket.h"
#include "IWebSocketNetworkingModule.h"
#include "IWebSocketServer.h"
#include "JsonObjectConverter.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/ArrayWriter.h"
#include "StructSerializer.h"
#include "WebSocketDeserializedMessage.h"
#include "WebSocketMessaging.h"
#include "WebSocketMessagingSettings.h"
#include "WebSocketsModule.h"

bool FWebSocketMessageConnection::IsConnected() const
{
	if (WebSocketConnection)
	{
		return WebSocketConnection->IsConnected();
	}

	return WebSocketServerConnection != nullptr;
}

void FWebSocketMessageConnection::Close()
{
	if (WebSocketConnection)
	{
		return WebSocketConnection->Close();
	}
}

FWebSocketMessageTransport::FWebSocketMessageTransport()
{
}

FWebSocketMessageTransport::~FWebSocketMessageTransport()
{
}

bool FWebSocketMessageTransport::StartTransport(IMessageTransportHandler& Handler)
{
	const UWebSocketMessagingSettings* Settings = GetDefault<UWebSocketMessagingSettings>();

	TransportHandler = &Handler;

	if (Settings->ServerPort > 0)
	{
		IWebSocketNetworkingModule* WebSocketNetworkingModule = FModuleManager::Get().LoadModulePtr<IWebSocketNetworkingModule>(TEXT("WebSocketNetworking"));
		if (WebSocketNetworkingModule)
		{
			Server = WebSocketNetworkingModule->CreateServer();
			if (Server)
			{
				if (!Server->Init(Settings->ServerPort, FWebSocketClientConnectedCallBack::CreateThreadSafeSP(this, &FWebSocketMessageTransport::ClientConnected)))
				{
					UE_LOG(LogWebSocketMessaging, Log, TEXT("Unable to start WebSocketMessaging Server on port %d"), Settings->ServerPort);
				}
				else
				{
					ServerTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateThreadSafeSP(this, &FWebSocketMessageTransport::ServerTick));
					UE_LOG(LogWebSocketMessaging, Log, TEXT("WebSocketMessaging Server started on port %d"), Settings->ServerPort);
				}
			}
		}
		else
		{
			UE_LOG(LogWebSocketMessaging, Log, TEXT("Unable to load WebSocketNetworking module, ensure to enable it"));
		}
	}

	for (const FString& Url : Settings->ConnectToEndpoints)
	{
		FGuid Guid = FGuid::NewGuid();

		TMap<FString, FString> Headers;
		Headers.Add(WebSocketMessaging::Header::TransportId, Guid.ToString());
		for (const TPair<FString, FString>& Pair : Settings->HttpHeaders)
		{
			Headers.Add(Pair.Key, Pair.Value);
		}

		TSharedRef<IWebSocket, ESPMode::ThreadSafe> WebSocketConnection = FWebSocketsModule::Get().CreateWebSocket(Url, FString(), Headers);

		FWebSocketMessageConnectionRef WebSocketMessageConnection = MakeShared<FWebSocketMessageConnection>(Url, Guid, WebSocketConnection);

		WebSocketConnection->OnMessage().AddThreadSafeSP(this, &FWebSocketMessageTransport::OnJsonMessage, WebSocketMessageConnection);
		WebSocketConnection->OnClosed().AddThreadSafeSP(this, &FWebSocketMessageTransport::OnClosed, WebSocketMessageConnection);
		WebSocketConnection->OnConnected().AddThreadSafeSP(this, &FWebSocketMessageTransport::OnConnected, WebSocketMessageConnection);
		WebSocketConnection->OnConnectionError().AddThreadSafeSP(this, &FWebSocketMessageTransport::OnConnectionError, WebSocketMessageConnection);

		WebSocketMessageConnections.Add(Guid, WebSocketMessageConnection);

		WebSocketConnection->Connect();

	}

	return true;
}

void FWebSocketMessageTransport::StopTransport()
{
	if (Server.IsValid())
	{
		Server.Reset();
	}

	for (TPair<FGuid, FWebSocketMessageConnectionRef> Pair : WebSocketMessageConnections)
	{
		Pair.Value->bDestroyed = true;
		Pair.Value->Close();
	}
	WebSocketMessageConnections.Empty();
}

void FWebSocketMessageTransport::OnClosed(int32 Code, const FString& Reason, bool bUserClose, FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	UE_LOG(LogWebSocketMessaging, Log, TEXT("Connection to %s closed, Code: %d Reason: \"%s\" UserClose: %s, retrying..."), *WebSocketMessageConnection->Url, Code, *Reason, bUserClose ? TEXT("true") : TEXT("false"));
	ForgetTransportNode(WebSocketMessageConnection);
	WebSocketMessageConnection->bIsConnecting = false;
	RetryConnection(WebSocketMessageConnection);
}

void FWebSocketMessageTransport::OnConnectionError(const FString& Message, FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	if (!WebSocketMessageConnection->bIsConnecting)
	{
		UE_LOG(LogWebSocketMessaging, Log, TEXT("Connection to %s error: %s, retrying..."), *WebSocketMessageConnection->Url, *Message);
	}
	ForgetTransportNode(WebSocketMessageConnection);
	WebSocketMessageConnection->bIsConnecting = false;
	RetryConnection(WebSocketMessageConnection);
}

void FWebSocketMessageTransport::OnJsonMessage(const FString& Message, FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	TSharedRef<FWebSocketDeserializedMessage> Context = MakeShared<FWebSocketDeserializedMessage>();
	if (Context->ParseJson(Message))
	{
		TransportHandler->ReceiveTransportMessage(Context, WebSocketMessageConnection->Guid);
	}
	else
	{
		UE_LOG(LogWebSocketMessaging, Verbose, TEXT("Invalid Json Message received on %s"), *WebSocketMessageConnection->Url);
	}
}

void FWebSocketMessageTransport::OnServerJsonMessage(void* Data, int32 DataSize, FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	FString Message(DataSize, reinterpret_cast<UTF8CHAR*>(Data));
	OnJsonMessage(Message, WebSocketMessageConnection);
}

class FWebSocketMessageTransportSerializeHelper
{
public:
	static const TMap<EMessageScope, FString> MessageScopeStringMapping;

	static bool Serialize(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context, FString& OutJsonMessage)
	{
		TSharedRef<FJsonObject> JsonRoot = MakeShared<FJsonObject>();
		JsonRoot->SetStringField(WebSocketMessaging::Tag::Sender, Context->GetSender().ToString());
		TArray<TSharedPtr<FJsonValue>> JsonRecipients;
		for (const FMessageAddress& Recipient : Context->GetRecipients())
		{
			JsonRecipients.Add(MakeShared<FJsonValueString>(Recipient.ToString()));
		}
		JsonRoot->SetArrayField(WebSocketMessaging::Tag::Recipients, JsonRecipients);
		JsonRoot->SetStringField(WebSocketMessaging::Tag::MessageType, Context->GetMessageTypePathName().ToString());
		JsonRoot->SetNumberField(WebSocketMessaging::Tag::Expiration, Context->GetExpiration().ToUnixTimestamp());
		JsonRoot->SetNumberField(WebSocketMessaging::Tag::TimeSent, Context->GetTimeSent().ToUnixTimestamp());
		JsonRoot->SetStringField(WebSocketMessaging::Tag::Scope, MessageScopeStringMapping[Context->GetScope()]);


		TSharedRef<FJsonObject> JsonAnnotations = MakeShared<FJsonObject>();
		for (const TPair<FName, FString>& Pair : Context->GetAnnotations())
		{
			JsonAnnotations->SetStringField(Pair.Key.ToString(), Pair.Value);
		}
		JsonRoot->SetObjectField(WebSocketMessaging::Tag::Annotations, JsonAnnotations);

		TSharedRef<FJsonObject> OutJsonObject = MakeShared<FJsonObject>();

		// Remark: This will change the case of field names, see StandardizeCase.
		if (!FJsonObjectConverter::UStructToJsonObject(Context->GetMessageTypeInfo().Get(), Context->GetMessage(), OutJsonObject))
		{
			return false;
		}

		JsonRoot->SetObjectField(WebSocketMessaging::Tag::Message, OutJsonObject);

		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJsonMessage);
		return FJsonSerializer::Serialize(JsonRoot, Writer);
	}

	static bool Serialize(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context, FArrayWriter& OutCborBinaryWriter)
	{
		FCborHeader Header(ECborCode::Map | ECborCode::Indefinite);
		OutCborBinaryWriter << Header;
			
		{
			FCborWriter CborWriter(&OutCborBinaryWriter);
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::Sender));
			CborWriter.WriteValue(Context->GetSender().ToString());
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::Recipients));
			CborWriter.WriteContainerStart(ECborCode::Array, -1);
			for (const FMessageAddress& Recipient : Context->GetRecipients())
			{
				CborWriter.WriteValue(Recipient.ToString());
			}
			CborWriter.WriteContainerEnd();
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::MessageType));
			CborWriter.WriteValue(Context->GetMessageTypePathName().ToString());
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::Expiration));
			CborWriter.WriteValue(Context->GetExpiration().ToUnixTimestamp());
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::TimeSent));
			CborWriter.WriteValue(Context->GetTimeSent().ToUnixTimestamp());
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::Scope));
			CborWriter.WriteValue(MessageScopeStringMapping[Context->GetScope()]);
			
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::Annotations));
			CborWriter.WriteContainerStart(ECborCode::Map, -1);
			for (const TPair<FName, FString>& Annotation : Context->GetAnnotations())
			{
				CborWriter.WriteValue(Annotation.Key.ToString());
				CborWriter.WriteValue(Annotation.Value);
			}
			CborWriter.WriteContainerEnd();
			
			CborWriter.WriteValue(FString(WebSocketMessaging::Tag::Message));
		}
		FCborStructSerializerBackend Backend(OutCborBinaryWriter, EStructSerializerBackendFlags::Default);
		FStructSerializer::Serialize(Context->GetMessage(), *Context->GetMessageTypeInfo().Get(), Backend);
			
		Header.Set(ECborCode::Break);
		OutCborBinaryWriter << Header;

		return true;
	}

	template<typename OutputType>
	struct TOnDemandSerializer
	{
		OutputType OutputMessage;
		bool bIsSerializeAttempted = false;
		bool bIsSerialized = false;

		bool SerializeOnDemand(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
		{
			if(!bIsSerializeAttempted)
			{
				bIsSerializeAttempted = true;
				bIsSerialized = FWebSocketMessageTransportSerializeHelper::Serialize(Context, OutputMessage);
			}
			return bIsSerialized;
		}
	};
};

const TMap<EMessageScope, FString> FWebSocketMessageTransportSerializeHelper::MessageScopeStringMapping =
{
	{EMessageScope::Thread, "Thread"},
	{EMessageScope::Process, "Process"},
	{EMessageScope::Network, "Network"},
	{EMessageScope::All, "All"}
};

bool FWebSocketMessageTransport::TransportMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context, const TArray<FGuid>& Recipients)
{
	TMap<FGuid, FWebSocketMessageConnectionRef> RecipientConnections;

	if (Recipients.Num() == 0)
	{
		// broadcast the message to all valid connections
		RecipientConnections = WebSocketMessageConnections.FilterByPredicate([](const TPair<FGuid, FWebSocketMessageConnectionRef>& Pair) -> bool
			{
				return !Pair.Value->bDestroyed && Pair.Value->IsConnected();
			});
	}
	else
	{
		// Find connections for each recipient.  We do not transport unicast messages for unknown nodes.
		for (const FGuid& Recipient : Recipients)
		{
			FWebSocketMessageConnectionRef* RecipientConnection = WebSocketMessageConnections.Find(Recipient);
			if (RecipientConnection && !(*RecipientConnection)->bDestroyed && (*RecipientConnection)->IsConnected())
			{
				RecipientConnections.Add(Recipient, *RecipientConnection);
			}
		}
	}

	if (RecipientConnections.Num() == 0)
	{
		return false;
	}

	// Remark: Json serializer uses UStructToJsonObject, which is going to change
	// the field name case (see StandardizeCase), which is going to cause a difference
	// with the field names in Cbor (which doesn't change the case).

	FWebSocketMessageTransportSerializeHelper::TOnDemandSerializer<FString> JsonSerializer;
	FWebSocketMessageTransportSerializeHelper::TOnDemandSerializer<FArrayWriter> CborSerializer;
	
	// Serialize the message on demand in the appropriate format for each peer connections.
	for (const TPair<FGuid, FWebSocketMessageConnectionRef>& Pair : RecipientConnections)
	{
		if (Pair.Value->WebSocketConnection.IsValid())
		{
			// Remark: client connections are always text/json
			if(JsonSerializer.SerializeOnDemand(Context))
			{
				Pair.Value->WebSocketConnection->Send(JsonSerializer.OutputMessage);
			}
		}
		else if (Pair.Value->WebSocketServerConnection)
		{
			// Remark: server connections are always binary.
			const UWebSocketMessagingSettings* Settings = GetDefault<UWebSocketMessagingSettings>();

			if(Settings->ServerTransportFormat == EWebSocketMessagingTransportFormat::Json)
			{
				if(JsonSerializer.SerializeOnDemand(Context))
				{
					FTCHARToUTF8 Converted(*JsonSerializer.OutputMessage);
					Pair.Value->WebSocketServerConnection->Send(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length(), false);
				}
			}
			else
			{
				if(CborSerializer.SerializeOnDemand(Context))
				{
					Pair.Value->WebSocketServerConnection->Send(CborSerializer.OutputMessage.GetData(), CborSerializer.OutputMessage.Num(), false);
				}
			}
		}
	}

	return true;
}

void FWebSocketMessageTransport::OnConnected(FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	UE_LOG(LogWebSocketMessaging, Log, TEXT("Connected to %s"), *WebSocketMessageConnection->Url);
	WebSocketMessageConnection->bIsConnecting = false;
}

void FWebSocketMessageTransport::OnServerConnectionClosed(FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	UE_LOG(LogWebSocketMessaging, Log, TEXT("%s disconnected"), *WebSocketMessageConnection->Url);
	ForgetTransportNode(WebSocketMessageConnection);
	WebSocketMessageConnections.Remove(WebSocketMessageConnection->Guid);
}

void FWebSocketMessageTransport::RetryConnection(FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	if (WebSocketMessageConnection->bIsConnecting)
	{
		return;
	}

	WebSocketMessageConnection->RetryHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float DeltaTime, FWebSocketMessageConnectionRef WebSocketMessageConnection)
		{
			if (!WebSocketMessageConnection->bDestroyed && !WebSocketMessageConnection->bIsConnecting && !WebSocketMessageConnection->WebSocketConnection->IsConnected())
			{
				WebSocketMessageConnection->bIsConnecting = true;
				WebSocketMessageConnection->WebSocketConnection->Connect();
			}
			return false;
		}, WebSocketMessageConnection), 1.0f);
}

void FWebSocketMessageTransport::ClientConnected(INetworkingWebSocket* NetworkingWebSocket)
{
	FString RemoteEndPoint = NetworkingWebSocket->RemoteEndPoint(true);
	UE_LOG(LogWebSocketMessaging, Log, TEXT("New WebSocket Server connection: %s"), *RemoteEndPoint);

	FGuid Guid = FGuid::NewGuid();

	FWebSocketMessageConnectionRef WebSocketMessageConnection = MakeShared<FWebSocketMessageConnection>(RemoteEndPoint, Guid, NetworkingWebSocket);

	NetworkingWebSocket->SetReceiveCallBack(FWebSocketPacketReceivedCallBack::CreateThreadSafeSP(this, &FWebSocketMessageTransport::OnServerJsonMessage, WebSocketMessageConnection));
	NetworkingWebSocket->SetSocketClosedCallBack(FWebSocketInfoCallBack::CreateThreadSafeSP(this, &FWebSocketMessageTransport::OnServerConnectionClosed, WebSocketMessageConnection));
	NetworkingWebSocket->SetErrorCallBack(FWebSocketInfoCallBack::CreateThreadSafeSP(this, &FWebSocketMessageTransport::OnServerConnectionClosed, WebSocketMessageConnection));

	WebSocketMessageConnections.Add(Guid, WebSocketMessageConnection);
}

bool FWebSocketMessageTransport::ServerTick(float DeltaTime)
{
	if (Server.IsValid())
	{
		Server->Tick();
	}

	return true;
}

void FWebSocketMessageTransport::ForgetTransportNode(FWebSocketMessageConnectionRef WebSocketMessageConnection)
{
	if(TransportHandler)
	{
		TransportHandler->ForgetTransportNode(WebSocketMessageConnection->Guid);
	}
}
