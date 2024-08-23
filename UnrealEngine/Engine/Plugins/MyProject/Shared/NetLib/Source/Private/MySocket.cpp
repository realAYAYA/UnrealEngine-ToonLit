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

void FMySocketServerSide::Init(INetworkingWebSocket* Socket)
{
	auto Ptr = TSharedPtr<INetworkingWebSocket>(Socket);
	WebSocket = MoveTemp(Ptr);
}

bool FMySocketServerSide::IsOpen() const
{
	return WebSocket.IsValid();
}

void FMySocketServerSide::Start()
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
			const auto DataBuffer = MakeShared<FMyDataBuffer>(Data, Size);
			if (const auto Ptr = Self.Pin())
				Ptr->HandleReceived(&DataBuffer.Get());
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

void FMySocketServerSide::Shutdown()
{
	
}

void FMySocketServerSide::Send(const FMyDataBufferPtr& Buffer)
{
	WebSocket->Send(reinterpret_cast<const uint8*>(Buffer->Peek()), Buffer->ReadableBytes());

	LastSentTime = FMyTools::Now().GetTicks();
}


// ========================================================================= //

void FMySocketClientSide::Start()
{
	WebSocket = FWebSocketsModule::Get().CreateWebSocket(Url, Protocol);
	TWeakPtr<FMySocket, ESPMode::ThreadSafe> Self = AsShared();
	
	WebSocket->OnConnected().AddLambda([Self]()
	{
		if (const auto Ptr = Self.Pin())
			Ptr->HandleConnected();
	});
	
	WebSocket->OnClosed().AddLambda([Self](int32 StatusCode, const FString& Reason, bool bWasClean)
	{
		if (const auto Ptr = Self.Pin())
			Ptr->HandleClosed();

		UE_LOG(LogNetLib, Warning, TEXT("Socket closed, Code=%d, Reason=%s, WasClean=%d"), StatusCode, *Reason, bWasClean);
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
			const auto DataBuffer = MakeShared<FMyDataBuffer>(Data, Size);
			Ptr->HandleReceived(&DataBuffer.Get());
		}
	});
	
	//WebSocket->OnBinaryMessage().AddUObject(this, &UMGameClientSubsystem::OnBinaryMessage);
	//WebSocket->OnMessageSent().AddUObject(this, &UMGameClientSubsystem::OnMessageSent);
	
	WebSocket->Connect();
}

void FMySocketClientSide::Shutdown()
{
	WebSocket->Close();
}

bool FMySocketClientSide::IsOpen() const
{
	if (WebSocket)
		return WebSocket->IsConnected();

	return false;
}

void FMySocketClientSide::Send(const FMyDataBufferPtr& Buffer)
{
	if (IsOpen())
		WebSocket->Send(Buffer->Peek(), Buffer->ReadableBytes());

	LastSentTime = FMyTools::Now().GetTicks();
}
