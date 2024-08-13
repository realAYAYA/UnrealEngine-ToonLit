#include "MySocket.h"
#include "MTools.h"

#include "WebSocketsModule.h"
#include "INetworkingWebSocket.h"
#include "IWebSocket.h"


FMySocket::FMySocket(const uint64 InId): Id(InId)
{
}

FMySocket::~FMySocket()
{
}

void FMySocket::HandleReceived(FMyDataBuffer* Buffer)
{
	if (ReceivedCallback)
		ReceivedCallback(Buffer);

	LastReceivedTime = FMyTools::Now().GetTicks();
}

void FMySocket::HandleConnected()
{
	if (ConnectedCallback)
		ConnectedCallback();
}

void FMySocket::HandleClosed()
{
	if (ClosedCallback)
		ClosedCallback();
}

void FMySocket::HandleError()
{
	if (ErrorCallback)
		ErrorCallback();
}

// ========================================================================= //

void FMySocketServer::Init(INetworkingWebSocket* Socket)
{
	auto Ptr = TSharedPtr<INetworkingWebSocket>(Socket);
	WebSocket = MoveTemp(Ptr);
}

bool FMySocketServer::IsOpen() const
{
	return WebSocket.IsValid();
}

void FMySocketServer::Start()
{
	TWeakPtr<FMySocket, ESPMode::ThreadSafe> Self = AsShared();
	
	{
		FWebSocketInfoCallBack Callback;
		Callback.BindLambda([Self]()
		{
			if (const auto Ptr = Self.Pin())
				Ptr->HandleConnected();
		});
		WebSocket->SetConnectedCallBack(Callback);
	}
	
	{
		FWebSocketInfoCallBack Callback;
		Callback.BindLambda([Self]()
		{
			if (const auto Ptr = Self.Pin())
				Ptr->HandleError();
		});
		WebSocket->SetErrorCallBack(Callback);
	}

	{
		FWebSocketPacketReceivedCallBack Callback;
		Callback.BindLambda([Self](const void* Data, const int32 Size)
		{
			FMyDataBuffer DataBuffer(Data, Size);
			if (const auto Ptr = Self.Pin())
				Ptr->HandleReceived(&DataBuffer);
		});
		WebSocket->SetReceiveCallBack(Callback);
	}

	{
		FWebSocketInfoCallBack Callback;
		Callback.BindLambda([Self]()
		{
			if (const auto Ptr = Self.Pin())
				Ptr->HandleClosed();
		});
		WebSocket->SetSocketClosedCallBack(Callback);
	}
}

void FMySocketServer::Shutdown()
{
	
}

void FMySocketServer::Send(const FMyDataBufferPtr& Buffer)
{
	WebSocket->Send(reinterpret_cast<uint8*>(const_cast<char*>(Buffer->Peek())), Buffer->ReadableBytes() * sizeof(char));

	LastSentTime = FMyTools::Now().GetTicks();
}


// ========================================================================= //


void FMySocketClient::Init(const FString& ServerURL, const FString& ServerProtocol)
{
	Url = ServerURL;
	Protocol = ServerProtocol;
}

void FMySocketClient::Start()
{
	WebSocket = FWebSocketsModule::Get().CreateWebSocket(Url, Protocol);
	TWeakPtr<FMySocket, ESPMode::ThreadSafe> Self = AsShared();
	
	WebSocket->OnConnected().AddLambda([Self]()
	{
		if (const auto Ptr = Self.Pin())
			Ptr->HandleConnected();
	});
	
	WebSocket->OnClosed().AddLambda([Self](int32 /* StatusCode */, const FString& /* Reason */, bool /* bWasClean */)
	{
		if (const auto Ptr = Self.Pin())
			Ptr->HandleClosed();
	});
	
	WebSocket->OnConnectionError().AddLambda([Self](const FString& Error)
	{
		if (const auto Ptr = Self.Pin())
			Ptr->HandleError();
	});
	
	//WebSocket->OnMessage().AddUObject(this, &UMGameClientSubsystem::OnMessage);
	WebSocket->OnRawMessage().AddLambda([Self](const void* Data, const SIZE_T Size, SIZE_T BytesRemaining)
	{
		if (const auto Ptr = Self.Pin())
		{
			FMyDataBuffer DataBuffer(Data, Size);
			Ptr->HandleReceived(&DataBuffer);
		}
	});
	
	//WebSocket->OnBinaryMessage().AddUObject(this, &UMGameClientSubsystem::OnBinaryMessage);
	//WebSocket->OnMessageSent().AddUObject(this, &UMGameClientSubsystem::OnMessageSent);
	
	WebSocket->Connect();
}

void FMySocketClient::Shutdown()
{
	WebSocket->Close();
}

bool FMySocketClient::IsOpen() const
{
	if (WebSocket)
		return WebSocket->IsConnected();

	return false;
}

void FMySocketClient::Send(const FMyDataBufferPtr& Buffer)
{
}
