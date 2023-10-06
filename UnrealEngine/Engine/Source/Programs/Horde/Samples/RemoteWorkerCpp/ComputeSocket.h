// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeBuffer.h"
#include "ComputeChannel.h"

//
// Connection to a remote machine that multiplexes data into and out-of multiple buffers
// attached to different channel numbers.
//
class FComputeSocket
{
public:
	virtual ~FComputeSocket();

	// Attaches a new buffer for receiving data
	virtual void AttachRecvBuffer(int ChannelId, FComputeBufferWriter Writer) = 0;

	// Attaches a new buffer for sending data */
	virtual void AttachSendBuffer(int ChannelId, FComputeBufferReader Reader) = 0;
};

//
// Socket used by a worker process to communicate with a host running on the same machine
// using shared memory to attach new buffers.
//
class FWorkerComputeSocket final : public FComputeSocket
{
public:
	static const wchar_t* const EnvVarName;

	FWorkerComputeSocket();
	~FWorkerComputeSocket();

	// Opens a connection to the agent process using a command buffer read from an environment variable (EnvVarName)
	bool Open();

	// Opens a connection to the agent process using a specific command buffer name
	bool Open(const wchar_t* CommandBufferName);

	// Close the current connection
	void Close();

	// Attaches a new buffer for receiving data
	virtual void AttachRecvBuffer(int ChannelId, FComputeBufferWriter Writer) override;

	// Attaches a new buffer for sending data
	virtual void AttachSendBuffer(int ChannelId, FComputeBufferReader Reader) override;

	// Reads and handles a command from the command buffer
	static void RunServer(FComputeBufferReader& CommandBufferReader, FComputeSocket& Socket);

private:
	enum class EMessageType;

	FComputeBuffer CommandBuffer;

	void AttachBuffer(int ChannelId, EMessageType Type, const wchar_t* Name);

	static size_t ReadVarUInt(const unsigned char* Pos, unsigned int* OutValue);
	static size_t ReadString(const unsigned char* Pos, wchar_t* OutText, size_t OutTextMaxLen);

	static size_t WriteVarUInt(unsigned char* Pos, unsigned int Value);
	static size_t WriteString(unsigned char* Pos, const wchar_t* Text);

	static unsigned int FloorLog2(unsigned int Value);
	static unsigned int CountLeadingZeros(unsigned int Value);
};

//
// Implementation of FComputeSocket that communicates via an overridable transport mechanism.
//
class FRemoteComputeSocket : public FComputeSocket
{
public:
	FRemoteComputeSocket();
	~FRemoteComputeSocket();

	// Attaches a new buffer for receiving data
	virtual void AttachRecvBuffer(int ChannelId, FComputeBufferWriter Writer) override final;

	// Attaches a new buffer for sending data */
	virtual void AttachSendBuffer(int ChannelId, FComputeBufferReader Reader) override final;

protected:
	// Starts reading and dispatching data in a background thread
	void Start();

	// Sends data to the remote
	virtual size_t Send(const void* Data, size_t Size) = 0;

	// Receives data from the remote
	virtual size_t Recv(void* Data, size_t Size) = 0;

private:
	struct FDetail;
	struct FFrameHeader;
	enum class EControlMessageType;

	bool SendFull(const void* Data, size_t Size);
	bool RecvFull(void* Data, size_t Size);

	FDetail* Detail;
};
