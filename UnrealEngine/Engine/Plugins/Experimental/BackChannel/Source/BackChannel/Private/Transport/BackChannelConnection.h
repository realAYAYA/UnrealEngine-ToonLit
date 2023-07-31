// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BackChannel/Transport/IBackChannelSocketConnection.h"
#include "HAL/ThreadSafeBool.h"

class FSocket;

/**
* BackChannelClient implementation.
*
*/
class BACKCHANNEL_API FBackChannelConnection : public IBackChannelSocketConnection, public TSharedFromThis<FBackChannelConnection>
{
public:

	FBackChannelConnection();
	~FBackChannelConnection();

	/* Start connecting to the specified port for incoming connections. Use WaitForConnection to check status. */
	virtual bool Connect(const TCHAR* InEndPoint) override;

	/* Start listening on the specified port for incoming connections. Use WaitForConnection to accept one. */
	virtual bool Listen(const int16 Port) override;

	/* Close the connection */
	virtual void Close() override;

	/* Waits for an icoming or outgoing connection to be made */
	virtual bool WaitForConnection(double InTimeout, TFunction<bool(TSharedRef<IBackChannelSocketConnection>)> InDelegate) override;

	/* Attach this connection to the provided socket */
	bool Attach(FSocket* InSocket);

	/* Send data over our connection. The number of bytes sent is returned */
	virtual int32 SendData(const void* InData, const int32 InSize) override;

	/* Read data from our remote connection. The number of bytes received is returned */
	virtual int32 ReceiveData(void* OutBuffer, const int32 BufferSize) override;

	/* Return our current connection state */
	virtual bool IsConnected() const override;

	/* Returns true if this connection is currently listening for incoming connections */
	virtual bool IsListening() const override;

	/* Return a string describing this connection */
	virtual FString	GetDescription() const override;

	/* Return the underlying socket (if any) for this connection */
	virtual FSocket* GetSocket() override { return Socket; }

	/* Todo - Proper stats */
	virtual uint32	GetPacketsReceived() const override;

	/* Set the specified send and receive buffer sizes, if supported */
	virtual void SetBufferSizes(int32 DesiredSendSize, int32 DesiredReceiveSize) override;

	const FConnectionStats& GetConnectionStats() const override { return ConnectionStats; }

private:
	static int32 SendBufferSize;
	static int32 ReceiveBufferSize;

	void					CloseWithError(const TCHAR* Error, FSocket* InSocket=nullptr);
	void					ResetStatsIfTime();
	
	/* Attempts to set the specified buffer size on our socket, will drop by 50% each time until success */
	void					SetSocketBufferSizes(FSocket* NewSocket, int32 DesiredSendSize, int32 DesiredReceiveSize);

	FThreadSafeBool			IsAttemptingConnection;
	FCriticalSection		SocketMutex;
	FSocket*				Socket;
	bool					IsListener;

	FConnectionStats		ConnectionStats;
	FConnectionStats		LastStats;
	double					TimeSinceStatsSet;
};
