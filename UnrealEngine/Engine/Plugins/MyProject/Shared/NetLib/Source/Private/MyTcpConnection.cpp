#include "MyTcpConnection.h"

#include "MTools.h"
#include "MyDataBuffer.h"

DEFINE_LOG_CATEGORY(LogNetLib);

FMyTcpConnection::FMyTcpConnection(const int64 Id): Id(Id)
{
}

FMyTcpConnection::~FMyTcpConnection()
{
	if (Socket)
	{
		
		Socket.Reset();
	}
}

void FMyTcpConnection::Init(const FMySocketPtr& InSocket)
{
	Socket = InSocket;

	TWeakPtr<FMyTcpConnection, ESPMode::ThreadSafe> Self = AsShared();
	Socket->SetConnectedCallback([Self]()
	{
		if (const auto Ptr = Self.Pin())
		{
			Ptr->HandleConnected();
		}
	});

	Socket->SetClosedCallback([Self]()
	{
		if (const auto Ptr = Self.Pin())
		{
			Ptr->HandleClosed();
		}
	});

	Socket->SetErrorCallback([Self]()
	{
		if (const auto Ptr = Self.Pin())
		{
			Ptr->HandleError();
		}
	});
	
	Socket->SetReceivedCallback([Self](FMyDataBuffer* Buffer)
	{
		if (const auto Ptr = Self.Pin())
		{
			Ptr->HandleReceived(Buffer);
		}
	});
}

void FMyTcpConnection::Start()
{
	if (!Socket)
		return;

	Socket->Start();

	HandleConnected();
}

void FMyTcpConnection::Shutdown() const
{
	if (!Socket)
		return;

	Socket->Shutdown();
}

FDateTime FMyTcpConnection::GetLastSentTime() const
{
	if (Socket)
		return Socket->GetLastSentTime();
	return 0;
}

FDateTime FMyTcpConnection::GetLastReceivedTime() const
{
	if (Socket)
		return Socket->GetLastReceivedTime();
	return 0;
}

void FMyTcpConnection::SendRaw(const char* Ptr, uint32 Length) const
{
	const auto Data = MakeShared<FMyDataBuffer, ESPMode::ThreadSafe>(Ptr, Length);
	SendRaw(Data);
}

void FMyTcpConnection::SendRaw(const FMyDataBufferPtr& Data) const
{
	if (!Socket)
		return;

	Socket->Send(Data);
}

uint32 FMyTcpConnection::GenerateSerialNum()
{
	return NextSerialNum++;
}



// ========================================================================= //



FPbConnection::FPbConnection(const uint64 Id): FMyTcpConnection(Id)
{
}

FPbConnection::~FPbConnection()
{
}

void FPbConnection::SendMessage(const FPbMessagePtr& InMessage)
{
	return DirectSendPb(InMessage);
}

void FPbConnection::Send(const FPbMessagePtr& InMessage)
{
	return DirectSendPb(InMessage);
}

void FPbConnection::SendSerializedPb(const uint64 PbTypeId, const char* InDataPtr, const int32 InDataLen)
{
	return DirectSendPb(PbTypeId, InDataPtr, InDataLen);
}

void FPbConnection::SetPackageCallback(const FPackageCallback& InCallback)
{
	PackageCallback = InCallback;
}

void FPbConnection::SetConnectedCallback(const FConnectedCallback& InCallback)
{
	ConnectedCallback = InCallback;
}

void FPbConnection::SetDisconnectedCallback(const FDisconnectedCallback& InCallback)
{
	DisconnectedCallback = InCallback;
}

void FPbConnection::SetErrorCallback(const FErrorCallback& InCallback)
{
	ErrorCallback = InCallback;
}

void FPbConnection::SetTimerCallback(const FTimerCallback& InCallback)
{
	TimerCallback = InCallback;
}


// ========================================================================= //
// ========================================================================= //

#pragma pack(push, 1)
struct FPacketHeadType
{
	uint32 SerialNum = 0;  // 消息序号
	uint64 Timestamp = 0;  // 消息时间戳
	uint64 Code = 0;  // 消息号
	uint32 Length = 0;  // 包体长度
	uint32 OriginalBodyLength = 0;  // ==0 说明包体未压缩， >0则表明包体被压缩，且该值为压缩前的原始包体大小 
	uint32 UnusedField = 0;  // 暂未使用
};
#pragma pack(pop)

static constexpr uint32 HeadLength = sizeof(FPacketHeadType);  // 包头长度
static constexpr uint32 MaxBodyLength = 1024*1024*8;  // 最大包体长度
static constexpr uint32 MaxOriginalBodyLength = MaxBodyLength * 8;
static constexpr uint32 CompressThresholdLength = 128;  // 触发压缩的大小

void FPbConnection::HandleReceived(FMyDataBuffer* InBuffer)
{
	while (InBuffer->ReadableBytes() >= HeadLength)
	{
		uint32 OriginalLength = 0;
		uint64 PackageCode;
		FMyDataBufferPtr PackagePtr;
		{
			const auto* HeadPtr = reinterpret_cast<const FPacketHeadType*>(InBuffer->Peek());
			const uint32 BodyLength = HeadPtr->Length;
			if (BodyLength > MaxBodyLength)
			{
				UE_LOG(LogNetLib, Error,
					TEXT("[网络模块] HandleReceived 数据包体大小超标 ConnId=%llu SN=%d Code=%llu Len=%d, %d"),
					this->GetId(), HeadPtr->SerialNum, HeadPtr->Code, BodyLength, MaxBodyLength);
				
				Shutdown();
				return;		
			}
		
			const uint32 PackageLength = BodyLength + HeadLength;
			if (InBuffer->ReadableBytes() < PackageLength)
			{
				InBuffer->EnsureWritableBytes(BodyLength);
				break;  // 数据包还不完整
			}

			const auto* BodyPtr = reinterpret_cast<const char*>(InBuffer->Peek() + HeadLength);

			PackagePtr = MakeShared<FMyDataBuffer, ESPMode::ThreadSafe>(BodyPtr, BodyLength);
			PackageCode = HeadPtr->Code;
			OriginalLength = HeadPtr->OriginalBodyLength;
			
			InBuffer->Retrieve(PackageLength);  // 移动数据指针			
		}
		
		if (PackagePtr)
		{
			if (PackageCallback)
			{
				if (const auto Self = StaticCastSharedPtr<FPbConnection, FMyTcpConnection, ESPMode::ThreadSafe>(AsShared()))
				{
					if (OriginalLength > 0)  // 数据包体需要解压
					{
						if (OriginalLength > MaxOriginalBodyLength)
						{
							UE_LOG(LogNetLib, Error,
								TEXT("[网络模块] HandleReceived 原始数据大小超标 Len=%d, %d"),
								OriginalLength, MaxOriginalBodyLength);
							
							Shutdown();
							return;
						}

						const auto OldPackagePtr = PackagePtr;
						const char* CompressedDataPtr = OldPackagePtr->Peek();
						const uint32 CompressedDataSize =  OldPackagePtr->ReadableBytes();
						
						PackagePtr = MakeShared<FMyDataBuffer, ESPMode::ThreadSafe>(OriginalLength);
						if (!FCompression::UncompressMemory(NAME_Zlib, PackagePtr->BeginWrite(), PackagePtr->WritableBytes(), CompressedDataPtr, CompressedDataSize))
						{
							UE_LOG(LogNetLib, Error,
								TEXT("[网络模块] 数据解压失败 PackageCode=%llu OriginalLength=%d OldLenght=%d"),
								PackageCode, OriginalLength, CompressedDataSize);
							
							return;
						}
						
						PackagePtr->HasWritten(OriginalLength);

#if WITH_EDITOR						
						{
							const float SpaceSavingPercent = (1.0 - static_cast<float>(CompressedDataSize) / static_cast<float>(OriginalLength)) * 100.0f;
							UE_LOG(LogNetLib, Log,
								TEXT("[网络模块] 数据解压 PbTypeId=%llu %d->%d Saving=%d"),
								PackageCode, OldPackagePtr->ReadableBytes(), PackagePtr->ReadableBytes(), static_cast<int>(SpaceSavingPercent));
						}
#endif						
					}
					
					(void)PackageCallback(Self, PackageCode, PackagePtr);  // 调用回调进行处理
				}
			}			
		}
	}
	
	InBuffer->EnsureWritableBytes(HeadLength);
}

void FPbConnection::HandleConnected()
{
	if (ConnectedCallback)
	{
		if (const auto Self = StaticCastSharedPtr<FPbConnection, FMyTcpConnection, ESPMode::ThreadSafe>(AsShared()))
		{
			ConnectedCallback(Self);
		}
	}
}

void FPbConnection::HandleClosed()
{
	if (DisconnectedCallback)
	{
		if (const auto Self = StaticCastSharedPtr<FPbConnection, FMyTcpConnection, ESPMode::ThreadSafe>(AsShared()))
		{
			DisconnectedCallback(Self);
		}
	}
}

void FPbConnection::HandleError()
{
	if (ErrorCallback)
	{
		if (const auto Self = StaticCastSharedPtr<FPbConnection, FMyTcpConnection, ESPMode::ThreadSafe>(AsShared()))
		{
			ErrorCallback(Self);
		}
	}
}

void FPbConnection::HandleTimer()
{
	if (TimerCallback)
	{
		if (const auto Self = StaticCastSharedPtr<FPbConnection, FMyTcpConnection, ESPMode::ThreadSafe>(AsShared()))
		{
			TimerCallback(Self);
		}
	}
}

void FPbConnection::DirectSendPb(const FPbMessagePtr& InMessage)
{
	const uint32 BodyLength = InMessage->ByteSizeLong();  // 包体长度
	if (BodyLength > CompressThresholdLength)  // 走压缩发送
	{
		const std::string Bin = InMessage->SerializeAsString();
		DirectSendCompressData(FMyTools::GeneratePbMessageTypeId(&*InMessage), Bin.c_str(), Bin.size());
		return;
	}
	if (BodyLength > MaxBodyLength)
	{
		UE_LOG(LogNetLib, Error, TEXT("[网络模块] DirectSendPb PB大小超标 Len=%d,%d"), BodyLength, MaxBodyLength);
		return;
	}
	
	const uint32 TotalLength = HeadLength + BodyLength;  // 数据包总长度

	const auto Buffer = MakeShared<FMyDataBuffer, ESPMode::ThreadSafe>();
	Buffer->EnsureWritableBytes(TotalLength);
	auto* HeadPtr = reinterpret_cast<FPacketHeadType*>(Buffer->BeginWrite());
	auto* BodyPtr = reinterpret_cast<char*>(Buffer->BeginWrite() + HeadLength);	
	{
		if (!InMessage->SerializeToArray(BodyPtr, BodyLength))
		{
			UE_LOG(LogNetLib, Error,TEXT("[网络模块] DirectSendPb 序列化PB失败 ConnId=%llu Len=%d"), this->GetId(), BodyLength);
			return;
		}

		{
			HeadPtr->SerialNum = GenerateSerialNum();
			HeadPtr->Timestamp = FMyTools::Now().GetTicks();
			HeadPtr->Code = FMyTools::GeneratePbMessageTypeId(&*InMessage);
			HeadPtr->Length = BodyLength;
		}		
	}
	Buffer->HasWritten(TotalLength);
	
	SendRaw(Buffer);
}

void FPbConnection::DirectSendPb(const uint64 PbTypeId, const char* InDataPtr, const int32 InDataLen)
{
	const uint32 BodyLength = InDataLen;  // 包体长度
	if (BodyLength > CompressThresholdLength)  // 走压缩发送
	{
		DirectSendCompressData(PbTypeId, InDataPtr, InDataLen);
		return;
	}	
	if (BodyLength > MaxBodyLength)
	{
		UE_LOG(LogNetLib, Error, TEXT("[网络模块] DirectSendPb PB大小超标 Len=%d,%d"), BodyLength, MaxBodyLength);
		return;
	}
	
	const uint32 TotalLength = HeadLength + BodyLength;  // 数据包总长度

	const auto Buffer = MakeShared<FMyDataBuffer, ESPMode::ThreadSafe>();
	Buffer->EnsureWritableBytes(TotalLength);
	auto* HeadPtr = reinterpret_cast<FPacketHeadType*>(Buffer->BeginWrite());
	{
		auto* BodyPtr = reinterpret_cast<char*>(Buffer->BeginWrite() + HeadLength);
		FMemory::Memcpy(BodyPtr, InDataPtr, BodyLength);
		{
			HeadPtr->SerialNum = GenerateSerialNum();
			HeadPtr->Timestamp = FMyTools::Now().GetTicks();
			HeadPtr->Code = PbTypeId;
			HeadPtr->Length = BodyLength;
			// HeadPtr->Hash = FFnv::MemFnv64(BodyPtr, BodyLength);
		}
	}
	Buffer->HasWritten(TotalLength);
	
	SendRaw(Buffer);
}

void FPbConnection::DirectSendCompressData(uint64 PbTypeId, const char* InDataPtr, int32 InDataLen)
{
	if (InDataLen > MaxOriginalBodyLength)
	{
		UE_LOG(LogNetLib, Error, TEXT("[网络模块] DirectSendCompressData 原始数据大小超标 Len=%d,%d"), InDataLen, MaxOriginalBodyLength);
		Shutdown();
		return;
	}
	int32 CompressedSize = FCompression::CompressMemoryBound(NAME_Zlib, InDataLen);
	if (CompressedSize > MaxBodyLength)
	{
		UE_LOG(LogNetLib, Error, TEXT("[网络模块] DirectSendCompressData 预计压缩后数据大小超标 Len=%d,%d"), CompressedSize, MaxBodyLength);
		Shutdown();
		return;
	}

	const auto Buffer = MakeShared<FMyDataBuffer, ESPMode::ThreadSafe>();
	Buffer->EnsureWritableBytes(HeadLength + CompressedSize);
	auto* HeadPtr = reinterpret_cast<FPacketHeadType*>(Buffer->BeginWrite());

	if (auto* BodyPtr = reinterpret_cast<char*>(Buffer->BeginWrite() + HeadLength); FCompression::CompressMemory(NAME_Zlib, BodyPtr, CompressedSize, InDataPtr, InDataLen))
	{
#if WITH_EDITOR
		const float SpaceSavingPercent = (1.0 - static_cast<float>(CompressedSize) / static_cast<float>(InDataLen)) * 100.0f; 
		UE_LOG(LogNetLib, Log, TEXT("[网络模块] 数据压缩 PbTypeId=%llu %d->%d Saving=%d"), PbTypeId, InDataLen, CompressedSize, static_cast<int>(SpaceSavingPercent));
#endif
	}
	else
	{
		UE_LOG(LogNetLib, Error, TEXT("[网络模块] 数据压缩失败 PbTypeId=%llu DataLen=%d CompressedSize=%d"), PbTypeId, InDataLen, CompressedSize);
		Shutdown();
		return;
	}

	{
		HeadPtr->SerialNum = GenerateSerialNum();
		HeadPtr->Timestamp = FMyTools::Now().GetTicks();
		HeadPtr->Code = PbTypeId;
		HeadPtr->Length = CompressedSize;  // 记录包体压缩后大小
		HeadPtr->OriginalBodyLength = InDataLen;  // 记录包体压缩前大小 
	}

	Buffer->HasWritten(HeadLength + CompressedSize);

	SendRaw(Buffer);
}
