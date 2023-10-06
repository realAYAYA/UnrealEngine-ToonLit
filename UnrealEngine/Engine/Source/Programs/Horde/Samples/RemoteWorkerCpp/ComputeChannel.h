// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeBuffer.h"

class FComputeSocket;

//
// Allows bi-directional communication between two nodes using compute buffers
//
class FComputeChannel
{
public:
	FComputeChannel();
	~FComputeChannel();

	// Creates a new channel using default parameters
	void Attach(FComputeSocket& Socket, int ChannelId, FComputeBuffer SendBuffer, FComputeBuffer RecvBuffer);

	// Creates a new channel using the same parameters for the send and receive buffers
	bool Attach(FComputeSocket& Socket, int ChannelId, const FComputeBuffer::FParams& Params);

	// Creates a new channel using the same parameters for the send and receive buffers
	bool Attach(FComputeSocket& Socket, int ChannelId, const FComputeBuffer::FParams& SendParams, const FComputeBuffer::FParams& RecvParams);

	// Close the current buffer and release all allocated resources
	void Detach();

	// Indicate to the remote that no more data will be sent.
	void MarkComplete();

	// Sends bytes to the remote. 
	size_t Send(const void* Data, size_t Size, int TimeoutMs = -1);

	// Reads as many bytes as are available from the socket.
	size_t Recv(void* Data, size_t Size, int TimeoutMs = -1);

private:
	FComputeBufferReader RecvBufferReader;
	FComputeBufferWriter SendBufferWriter;
};
