// Copyright Epic Games, Inc. All Rights Reserved.

#include <string.h>
#include <stdio.h>
#include <windows.h>
#include "ComputeChannel.h"
#include "ComputeSocket.h"

FComputeChannel::FComputeChannel()
{
}

FComputeChannel::~FComputeChannel()
{
	if (SendBufferWriter.IsValid())
	{
		SendBufferWriter.MarkComplete();
	}
}

void FComputeChannel::Attach(FComputeSocket& Socket, int ChannelId, FComputeBuffer SendBuffer, FComputeBuffer RecvBuffer)
{
	RecvBufferReader = RecvBuffer.GetReader();
	SendBufferWriter = SendBuffer.GetWriter();
	Socket.AttachRecvBuffer(ChannelId, RecvBuffer.GetWriter());
	Socket.AttachSendBuffer(ChannelId, SendBuffer.GetReader());
}

bool FComputeChannel::Attach(FComputeSocket& Socket, int ChannelId, const FComputeBuffer::FParams& Params)
{
	return Attach(Socket, ChannelId, Params, Params);
}

bool FComputeChannel::Attach(FComputeSocket& Socket, int ChannelId, const FComputeBuffer::FParams& SendBufferParams, const FComputeBuffer::FParams& RecvBufferParams)
{
	Detach();

	FComputeBuffer SendBuffer;
	if (!SendBuffer.CreateNew(SendBufferParams))
	{
		return false;
	}

	FComputeBuffer RecvBuffer;
	if (!RecvBuffer.CreateNew(RecvBufferParams))
	{
		return false;
	}

	Attach(Socket, ChannelId, SendBuffer, RecvBuffer);
	return true;
}

void FComputeChannel::Detach()
{
	RecvBufferReader = FComputeBufferReader();
	SendBufferWriter = FComputeBufferWriter();
}

void FComputeChannel::MarkComplete()
{
	SendBufferWriter.MarkComplete();
}

size_t FComputeChannel::Send(const void* Data, size_t Size, int TimeoutMs)
{
	unsigned char* SendData = SendBufferWriter.WaitToWrite(1, TimeoutMs);
	if (SendData == nullptr)
	{
		return 0;
	}

	size_t SendSize = SendBufferWriter.GetMaxWriteSize();
	if (Size < SendSize)
	{
		SendSize = Size;
	}

	memcpy(SendData, Data, SendSize);
	SendBufferWriter.AdvanceWritePosition(SendSize);
	return SendSize;
}

size_t FComputeChannel::Recv(void* Data, size_t Size, int TimeoutMs)
{
	const unsigned char* RecvData = RecvBufferReader.WaitToRead(1, TimeoutMs);
	if (RecvData == nullptr)
	{
		return 0;
	}

	size_t RecvSize = RecvBufferReader.GetMaxReadSize();
	if (Size < RecvSize)
	{
		RecvSize = Size;
	}

	memcpy(Data, RecvData, RecvSize);
	RecvBufferReader.AdvanceReadPosition(RecvSize);
	return RecvSize;
}
