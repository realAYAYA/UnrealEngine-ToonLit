// Copyright Epic Games, Inc. All Rights Reserved.

#include <windows.h>
#include <iostream>
#include <assert.h>
#include <uchar.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "ComputeSocket.h"

FComputeSocket::~FComputeSocket()
{
}


//////////////////////////////////////////////////////

const wchar_t* const FWorkerComputeSocket::EnvVarName = L"UE_HORDE_COMPUTE_IPC";

enum class FWorkerComputeSocket::EMessageType
{
	AttachRecvBuffer = 0,
	AttachSendBuffer = 1,
};

FWorkerComputeSocket::FWorkerComputeSocket()
{
}

FWorkerComputeSocket::~FWorkerComputeSocket()
{
	Close();
}

bool FWorkerComputeSocket::Open()
{
	wchar_t EnvVar[MAX_PATH];
	int Length = GetEnvironmentVariableW(EnvVarName, EnvVar, sizeof(EnvVar) / sizeof(EnvVar[0]));

	if (Length <= 0 || Length >= sizeof(EnvVar))
	{
		return false;
	}

	return Open(EnvVar);
}

bool FWorkerComputeSocket::Open(const wchar_t* CommandBufferName)
{
	return CommandBuffer.OpenExisting(CommandBufferName);
}

void FWorkerComputeSocket::Close()
{
	CommandBuffer.Close();
}

void FWorkerComputeSocket::AttachRecvBuffer(int ChannelId, FComputeBufferWriter Writer)
{
	AttachBuffer(ChannelId, EMessageType::AttachRecvBuffer, Writer.GetName());
}

void FWorkerComputeSocket::AttachSendBuffer(int ChannelId, FComputeBufferReader Reader)
{
	AttachBuffer(ChannelId, EMessageType::AttachSendBuffer, Reader.GetName());
}

void FWorkerComputeSocket::AttachBuffer(int ChannelId, EMessageType Type, const wchar_t* Name)
{
	FComputeBufferWriter& Writer = CommandBuffer.GetWriter();
	unsigned char* Data = Writer.WaitToWrite(1024);

	size_t Len = 0;
	Len += WriteVarUInt(Data + Len, (unsigned char)Type);
	Len += WriteVarUInt(Data + Len, (unsigned int)ChannelId);
	Len += WriteString(Data + Len, Name);

	Writer.AdvanceWritePosition(Len);
}

void FWorkerComputeSocket::RunServer(FComputeBufferReader& CommandBufferReader, FComputeSocket& Socket)
{
	const unsigned char* Message;
	while ((Message = CommandBufferReader.WaitToRead(1)) != nullptr)
	{
		size_t Len = 0;

		unsigned int Type;
		Len += ReadVarUInt(Message + Len, &Type);

		EMessageType MessageType = (EMessageType)*Message;
		switch (MessageType)
		{
		case EMessageType::AttachSendBuffer:
		{
			unsigned int ChannelId;
			Len += ReadVarUInt(Message + Len, &ChannelId);

			wchar_t Name[MAX_PATH];
			Len += ReadString(Message + Len, Name, MAX_PATH);

			FComputeBuffer Buffer;
			if (Buffer.OpenExisting(Name))
			{
				Socket.AttachSendBuffer(ChannelId, Buffer.GetReader());
			}
			else
			{
				assert(false);
			}
		}
		break;
		case EMessageType::AttachRecvBuffer:
		{
			unsigned int ChannelId;
			Len += ReadVarUInt(Message + Len, &ChannelId);

			wchar_t Name[MAX_PATH];
			Len += ReadString(Message + Len, Name, MAX_PATH);

			FComputeBuffer Buffer;
			if (Buffer.OpenExisting(Name))
			{
				Socket.AttachRecvBuffer(ChannelId, Buffer.GetWriter());
			}
			else
			{
				assert(false);
			}
		}
		break;
		default:
			assert(false);
			return;
		}

		CommandBufferReader.AdvanceReadPosition(Len);
	}
}

size_t FWorkerComputeSocket::ReadVarUInt(const unsigned char* Pos, unsigned int* OutValue)
{
	size_t ByteCount = CountLeadingZeros((unsigned char)(~*static_cast<const unsigned char*>(Pos))) - 23;

	unsigned int Value = *Pos++ & (unsigned char)(0xff >> ByteCount);
	switch (ByteCount - 1)
	{
	case 8: Value <<= 8; Value |= *Pos++;
	case 7: Value <<= 8; Value |= *Pos++;
	case 6: Value <<= 8; Value |= *Pos++;
	case 5: Value <<= 8; Value |= *Pos++;
	case 4: Value <<= 8; Value |= *Pos++;
	case 3: Value <<= 8; Value |= *Pos++;
	case 2: Value <<= 8; Value |= *Pos++;
	case 1: Value <<= 8; Value |= *Pos++;
	default:
		break;
	}

	*OutValue = Value;
	return ByteCount;
}

size_t FWorkerComputeSocket::ReadString(const unsigned char* Pos, wchar_t* OutText, size_t OutTextMaxLen)
{
	unsigned int TextLen;
	size_t Len = ReadVarUInt(Pos, &TextLen);

	int DecodedLen = MultiByteToWideChar(CP_UTF8, 0, (const char*)Pos + Len, TextLen, OutText, (int)OutTextMaxLen);
	OutText[DecodedLen] = 0;

	return Len + TextLen;
}

size_t FWorkerComputeSocket::WriteVarUInt(unsigned char* Pos, unsigned int Value)
{
	// Use BSR to return the log2 of the integer
	// return 0 if value is 0
	unsigned int ByteCount = (unsigned int)(int(FloorLog2(Value)) / 7 + 1);

	unsigned char* OutBytes = Pos + ByteCount - 1;
	switch (ByteCount - 1)
	{
	case 4: *OutBytes-- = (unsigned char)(Value); Value >>= 8;
	case 3: *OutBytes-- = (unsigned char)(Value); Value >>= 8;
	case 2: *OutBytes-- = (unsigned char)(Value); Value >>= 8;
	case 1: *OutBytes-- = (unsigned char)(Value); Value >>= 8;
	default: break;
	}
	*OutBytes = (unsigned char)(0xff << (9 - ByteCount)) | (unsigned char)(Value);

	return ByteCount;
}

size_t FWorkerComputeSocket::WriteString(unsigned char* Pos, const wchar_t* Text)
{
	int EncodedLen = WideCharToMultiByte(CP_UTF8, 0, Text, -1, nullptr, 0, nullptr, nullptr);
	EncodedLen--; // Ignore null terminator

	size_t Len = WriteVarUInt(Pos, EncodedLen);
	WideCharToMultiByte(CP_UTF8, 0, Text, -1, (char*)Pos + Len, EncodedLen, nullptr, nullptr);

	return Len + EncodedLen;
}

unsigned int FWorkerComputeSocket::FloorLog2(unsigned int Value)
{
	// Use BSR to return the log2 of the integer
	// return 0 if value is 0
	unsigned long BitIndex;
	return _BitScanReverse(&BitIndex, Value) ? BitIndex : 0;
}

unsigned int FWorkerComputeSocket::CountLeadingZeros(unsigned int Value)
{
	// return 32 if value is zero
	unsigned long BitIndex;
	_BitScanReverse64(&BitIndex, (unsigned long long)(Value) * 2 + 1);
	return 32 - BitIndex;
}

//////////////////////////////////////////////////////

enum class FRemoteComputeSocket::EControlMessageType
{
	Attach = -1,
	Detach = -2,
};

struct FRemoteComputeSocket::FFrameHeader
{
	int Channel;
	int Size;
};

struct FRemoteComputeSocket::FDetail
{
	class FCriticalSectionLock
	{
	public:
		FCriticalSectionLock(CRITICAL_SECTION* InCriticalSection) : CriticalSection(InCriticalSection)
		{
			EnterCriticalSection(CriticalSection);
		}

		FCriticalSectionLock(const FCriticalSectionLock& Other) = delete;

		~FCriticalSectionLock()
		{
			LeaveCriticalSection(CriticalSection);
		}

	private:
		CRITICAL_SECTION* const CriticalSection;
	};

	class FSendThread
	{
	public:
		FSendThread(FDetail& InDetail, int InChannel, FComputeBufferReader& InReader)
			: Detail(InDetail)
			, Channel(InChannel)
			, Reader(InReader)
			, ThreadHandle(nullptr)
		{
		}

		~FSendThread()
		{
			Stop();
		}

		void Start()
		{
			Stop();
			ThreadHandle = CreateThread(nullptr, 0, &SendThreadProc, this, 0, nullptr);
		}

		void Stop()
		{
			if (ThreadHandle != nullptr)
			{
				WaitForSingleObject(ThreadHandle, INFINITE);
				ThreadHandle = nullptr;
			}
		}

	private:
		FDetail& Detail;
		const int Channel;
		FComputeBufferReader& Reader;
		HANDLE ThreadHandle;

		static DWORD __stdcall SendThreadProc(void* Param)
		{
			FSendThread* SendBufferInfo = (FSendThread*)Param;
			FRemoteComputeSocket& Socket = SendBufferInfo->Detail.Socket;
			FComputeBufferReader& Reader = SendBufferInfo->Reader;

			FFrameHeader Header;
			Header.Channel = SendBufferInfo->Channel;

			const unsigned char* Data;
			while ((Data = Reader.WaitToRead(1)) != nullptr)
			{
				FCriticalSectionLock Lock(&SendBufferInfo->Detail.CriticalSection);
				Header.Size = (int)Reader.GetMaxReadSize();
				Socket.SendFull(&Header, sizeof(Header));
				Socket.SendFull(Data, Header.Size);
			}

			if (Reader.IsComplete())
			{
				FCriticalSectionLock Lock(&SendBufferInfo->Detail.CriticalSection);
				Header.Size = (int)EControlMessageType::Detach;
				Socket.SendFull(&Header, sizeof(Header));
			}

			delete SendBufferInfo;
			return 0;
		}
	};

	FRemoteComputeSocket& Socket;
	CRITICAL_SECTION CriticalSection;

	HANDLE RecvThreadHandle;

	std::unordered_map<int, FComputeBufferWriter*> Writers;
	std::unordered_map<int, std::unique_ptr<FSendThread>> SendThreads;

	std::unordered_set<int> AttachedRemoteBuffers;
	std::unordered_map<int, HANDLE> AttachEvents;

	FDetail(FRemoteComputeSocket& InSocket)
		: Socket(InSocket)
	{
		InitializeCriticalSection(&CriticalSection);
		RecvThreadHandle = CreateThread(nullptr, 0, &RecvThread, this, 0, nullptr);
	}

	~FDetail()
	{
		WaitForSingleObject(RecvThreadHandle, INFINITE);
		DeleteCriticalSection(&CriticalSection);
	}

	static DWORD _stdcall RecvThread(void* Param)
	{
		FDetail& Detail = *(FDetail*)Param;
		FRemoteComputeSocket& Socket = Detail.Socket;

		std::unordered_map<int, FComputeBufferWriter*> CachedWriters;

		// Process messages from the remote
		FFrameHeader Header;
		while (Socket.RecvFull(&Header, sizeof(Header)))
		{
			if (Header.Size >= 0)
			{
				Detail.ReadFrame(CachedWriters, Header.Channel, Header.Size);
			}
			else if (Header.Size == (int)EControlMessageType::Attach)
			{
				Detail.AttachRemoteRecvBuffer(Header.Channel);
			}
			else if (Header.Size == (int)EControlMessageType::Detach)
			{
				Detail.DetachRecvBuffer(CachedWriters, Header.Channel);
			}
			else
			{
				assert(false);
			}
		}
		return 0;
	}

	bool ReadFrame(std::unordered_map<int, FComputeBufferWriter*>& CachedWriters, int Channel, int Size)
	{
		std::unordered_map<int, FComputeBufferWriter*>::iterator Iter = CachedWriters.find(Channel);
		if (Iter == CachedWriters.end())
		{
			FCriticalSectionLock Lock(&CriticalSection);

			Iter = Writers.find(Channel);
			if (Iter == Writers.end())
			{
				return false;
			}

			Iter = CachedWriters.insert(*Iter).first;
		}

		FComputeBufferWriter* Writer = Iter->second;

		unsigned char* Data = Writer->WaitToWrite(Size);
		if (!Socket.RecvFull(Data, Size))
		{
			return false;
		}

		Writer->AdvanceWritePosition(Size);
		return true;
	}

	void AttachRecvBuffer(int ChannelId, FComputeBufferWriter& Writer)
	{
		FCriticalSectionLock Lock(&CriticalSection);
		Writers.insert(std::pair<int, FComputeBufferWriter*>(ChannelId, &Writer));

		FFrameHeader Header;
		Header.Channel = ChannelId;
		Header.Size = (int)EControlMessageType::Attach;
		Socket.SendFull(&Header, sizeof(Header));
	}

	void AttachSendBuffer(int ChannelId, FComputeBufferReader& Reader)
	{
		FCriticalSectionLock Lock(&CriticalSection);
		SendThreads.insert(std::make_pair(ChannelId, std::make_unique<FSendThread>(*this, ChannelId, Reader)));
	}

	void AttachRemoteRecvBuffer(int Channel)
	{
		FCriticalSectionLock Lock(&CriticalSection);
		AttachedRemoteBuffers.insert(Channel);

		std::unordered_map<int, HANDLE>::iterator AttachEventIter = AttachEvents.find(Channel);
		if (AttachEventIter != AttachEvents.end())
		{
			SetEvent(AttachEventIter->second);
			AttachEvents.erase(AttachEventIter);
		}
	}

	void DetachRecvBuffer(std::unordered_map<int, FComputeBufferWriter*>& CachedWriters, int Channel)
	{
		CachedWriters.erase(Channel);

		FCriticalSectionLock Lock(&CriticalSection);

		std::unordered_map<int, FComputeBufferWriter*>::iterator Iter = Writers.find(Channel);
		if (Iter != Writers.end())
		{
			Iter->second->MarkComplete();
			Writers.erase(Iter);
		}
	}
};

FRemoteComputeSocket::FRemoteComputeSocket()
	: Detail(nullptr)
{
}

FRemoteComputeSocket::~FRemoteComputeSocket()
{
	delete Detail;
}

void FRemoteComputeSocket::Start()
{
	if (Detail == nullptr)
	{
		Detail = new FDetail(*this);
	}
}

void FRemoteComputeSocket::AttachRecvBuffer(int ChannelId, FComputeBufferWriter Writer)
{
	Detail->AttachRecvBuffer(ChannelId, Writer);
}

void FRemoteComputeSocket::AttachSendBuffer(int ChannelId, FComputeBufferReader Reader)
{
	Detail->AttachSendBuffer(ChannelId, Reader);
}

bool FRemoteComputeSocket::SendFull(const void* Data, size_t Size)
{
	const unsigned char* RemainingData = (const unsigned char*)Data;
	for (size_t RemainingSize = Size; RemainingSize > 0; )
	{
		size_t SentSize = Send(RemainingData, RemainingSize);
		if (SentSize == 0)
		{
			return false;
		}

		RemainingData += SentSize;
		RemainingSize -= SentSize;
	}
	return true;
}

bool FRemoteComputeSocket::RecvFull(void* Data, size_t Size)
{
	unsigned char* RemainingData = (unsigned char*)Data;
	for (size_t RemainingSize = Size; RemainingSize > 0; )
	{
		size_t RecvSize = Recv(RemainingData, RemainingSize);
		if (RecvSize == 0)
		{
			return false;
		}

		RemainingData += RecvSize;
		RemainingSize -= RecvSize;
	}
	return true;
}

