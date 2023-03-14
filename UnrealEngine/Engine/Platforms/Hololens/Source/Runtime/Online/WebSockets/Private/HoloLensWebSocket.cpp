// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensWebSocket.h"
#include "Stats/Stats.h"

#if WITH_WEBSOCKETS

#include "WebSocketsLog.h"

FHoloLensWebSocket::FHoloLensWebSocket(const FString& InUri, const TArray<FString>& InProtocols, const TMap<FString, FString>& InHeaders)
	: Uri(InUri)
	, Socket(ref new Windows::Networking::Sockets::MessageWebSocket())
	, bUserClose(false)
{
	Windows::Networking::Sockets::MessageWebSocketControl^ SocketControl = Socket->Control;
	SocketControl->MessageType = Windows::Networking::Sockets::SocketMessageType::Utf8;
	for (const FString& Protocol : InProtocols)
	{
		SocketControl->SupportedProtocols->Append(ref new Platform::String(*Protocol));
	}

	for (TMap<FString, FString>::TConstIterator It(InHeaders); It; ++It)
	{
		Socket->SetRequestHeader(ref new Platform::String(*It->Key), ref new Platform::String(*It->Value));
	}
}

void FHoloLensWebSocket::Connect()
{
	using namespace Windows::Networking::Sockets;

	TWeakPtr<FHoloLensWebSocket> WeakThis = SharedThis(this);
	Socket->MessageReceived += ref new Windows::Foundation::TypedEventHandler<MessageWebSocket^, MessageWebSocketMessageReceivedEventArgs^>(
		[WeakThis](MessageWebSocket^, MessageWebSocketMessageReceivedEventArgs^ EventArgs)
	{
		TSharedPtr<FHoloLensWebSocket> PinnedThis = WeakThis.Pin();
		if (PinnedThis.IsValid())
		{
			try
			{
				bool bIsBinary = EventArgs->MessageType == SocketMessageType::Binary;
				Windows::Storage::Streams::DataReader^ Reader = EventArgs->GetDataReader();
				TArray<uint8> RawMessage;
				RawMessage.AddUninitialized(Reader->UnconsumedBufferLength + 1);
				Reader->ReadBytes(Platform::ArrayReference<uint8>(RawMessage.GetData(), RawMessage.Num() - 1));
				if (!bIsBinary)
				{
					RawMessage.Last() = 0;
				}

				PinnedThis->GameThreadWork.Enqueue(
					[WeakThis, bIsBinary, RawMessage = MoveTemp(RawMessage)]()
				{
					TSharedPtr<FHoloLensWebSocket> PinnedThis = WeakThis.Pin();
					if (PinnedThis.IsValid())
					{
						if (!bIsBinary)
						{
							PinnedThis->OnMessage().Broadcast(UTF8_TO_TCHAR(RawMessage.GetData()));
						}
						PinnedThis->OnRawMessage().Broadcast(RawMessage.GetData(), RawMessage.Num() - 1, 0);
					}
				});
			}
			catch (Platform::Exception^ Ex)
			{
				UE_LOG(LogWebSockets, Warning, TEXT("Read error %s"), Ex->Message->Data());
			}
		}
	});

	Socket->Closed += ref new Windows::Foundation::TypedEventHandler<Windows::Networking::Sockets::IWebSocket^, WebSocketClosedEventArgs^>(
		[WeakThis](Windows::Networking::Sockets::IWebSocket^, WebSocketClosedEventArgs^ EventArgs)
	{
		TSharedPtr<FHoloLensWebSocket> PinnedThis = WeakThis.Pin();
		if (PinnedThis.IsValid())
		{
			int32 Code = EventArgs->Code;
			FString Reason = EventArgs->Reason->Data();
			PinnedThis->GameThreadWork.Enqueue(
				[WeakThis, Code, Reason]()
			{
				TSharedPtr<FHoloLensWebSocket> PinnedThis = WeakThis.Pin();
				if (PinnedThis.IsValid())
				{
					PinnedThis->FinishClose(Code, Reason);
				}
			});
		}
	});

	try
	{
		ConnectAction = Socket->ConnectAsync(ref new Windows::Foundation::Uri(ref new Platform::String(*Uri)));
	}
	catch (Platform::Exception^ Ex)
	{
		FString ErrorMessage = Ex->Message->Data();
		GameThreadWork.Enqueue(
			[this, ErrorMessage]()
		{
			OnConnectionError().Broadcast(ErrorMessage);
		});
	}
}

void FHoloLensWebSocket::Close(int32 Code, const FString& Reason)
{
	bUserClose = true;
	try
	{
		Socket->Close(static_cast<uint16>(Code), ref new Platform::String(*Reason));
	}
	catch (...)
	{

	}
}

bool FHoloLensWebSocket::IsConnected()
{
	return Writer != nullptr;
}

void FHoloLensWebSocket::Send(const FString& Data)
{
	if (Writer != nullptr)
	{
		try
		{
			Writer->WriteString(ref new Platform::String(*Data));
			SendOperations.Add(Writer->StoreAsync());
			OnMessageSent().Broadcast(Data);
		}
		catch (Platform::Exception^ Ex)
		{
			UE_LOG(LogWebSockets, Warning, TEXT("Send error %s"), Ex->Message->Data());
		}
	}
}

void FHoloLensWebSocket::Send(const void* Utf8Data, SIZE_T Size, bool bIsBinary)
{
	if (Writer != nullptr)
	{
		try
		{
			if (bIsBinary)
			{
				Socket->Control->MessageType = Windows::Networking::Sockets::SocketMessageType::Binary;
			}
			Platform::Array<uint8>^ WinRTArray = ref new Platform::Array<uint8>(static_cast<uint8*>(const_cast<void*>(Utf8Data)), Size);
			Writer->WriteBytes(ref new Platform::Array<uint8>(static_cast<uint8*>(const_cast<void*>(Utf8Data)), Size));
			SendOperations.Add(Writer->StoreAsync());
			if (bIsBinary)
			{
				Socket->Control->MessageType = Windows::Networking::Sockets::SocketMessageType::Utf8;
			}
		}
		catch (Platform::Exception^ Ex)
		{
			UE_LOG(LogWebSockets, Warning, TEXT("Send error %s"), Ex->Message->Data());
		}
	}
}

bool FHoloLensWebSocket::Tick(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FHoloLensWebSocket_Tick);

	if (ConnectAction != nullptr)
	{
		switch (ConnectAction->Status)
		{
		case Windows::Foundation::AsyncStatus::Started:
			break;

		case Windows::Foundation::AsyncStatus::Canceled:
			ConnectAction = nullptr;
			break;

		case Windows::Foundation::AsyncStatus::Error:
			OnConnectionError().Broadcast(FString::FromInt(ConnectAction->ErrorCode.Value));
			ConnectAction = nullptr;
			break;

		case Windows::Foundation::AsyncStatus::Completed:
			Writer = ref new Windows::Storage::Streams::DataWriter(Socket->OutputStream);
			Writer->UnicodeEncoding = Windows::Storage::Streams::UnicodeEncoding::Utf8;
			OnConnected().Broadcast();
			ConnectAction = nullptr;
			break;

		default:
			break;
		}
	}

	for (int32 i = 0; i < SendOperations.Num(); ++i)
	{
		if (SendOperations[i]->Status != Windows::Foundation::AsyncStatus::Started)
		{
			SendOperations.RemoveAtSwap(i);
		}
	}

	TFunction<void()> WorkItem;
	while (GameThreadWork.Dequeue(WorkItem))
	{
		WorkItem();
	}

	return true;
}

void FHoloLensWebSocket::FinishClose(int32 Code, const FString& Reason)
{
	try
	{
		Writer->DetachStream();
	}
	catch (...)
	{

	}
	Writer = nullptr;

	OnClosed().Broadcast(Code, Reason, bUserClose);
}

#endif