// Copyright Epic Games, Inc. All Rights Reserved.

#include <iostream>
#include "ComputeChannel.h"
#include "ComputeBuffer.h"
#include "ComputeSocket.h"

#include <Windows.h>

int main()
{
	const int ChannelId = 100;

	FWorkerComputeSocket Socket;
	if (!Socket.Open())
	{
		std::cout << "Environment variable not set correctly" << std::endl;
		return 1;
	}

	FComputeChannel Channel;
	if (!Channel.Attach(Socket, ChannelId, FComputeBuffer::FParams()))
	{
		std::cout << "Unable to create channel to initiator" << std::endl;
		return 1;
	}

	std::cout << "Connected to initiator" << std::endl;

	size_t Length = 0;
	char Buffer[4];

	Buffer[0] = 0;
	Channel.Send(Buffer, 1); // Send a dummy one-byte message to let the remote know we're listening

	for (;;)
	{
		size_t RecvLength = Channel.Recv(Buffer + Length, sizeof(Buffer) - Length);
		if (RecvLength == 0)
		{
			return 0;
		}

		Length += RecvLength;

		if (Length >= 4)
		{
			std::cout << "Read value " << *(int*)Buffer << std::endl;
			Length = 0;
		}
	}

	Channel.MarkComplete();
	return 0;
}

