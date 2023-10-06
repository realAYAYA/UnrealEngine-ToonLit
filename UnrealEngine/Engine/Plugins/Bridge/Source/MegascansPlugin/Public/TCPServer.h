// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once


#include "Containers/UnrealString.h"
#include <string>
#include "Containers/Queue.h"
#include "HAL/Runnable.h"

class FRunnableThread;
class FSocket;
struct FIPv4Endpoint;
struct FScriptContainerElement;

class MEGASCANSPLUGIN_API FTCPServer : public FRunnable
{
public:	

	FTCPServer();
	~FTCPServer();	
	virtual bool Init() override;	
	virtual uint32 Run() override;

	virtual void Stop() override
	{
		Stopping = true;
	}

	
	bool RecvMessage(FSocket *Socket, uint32 DataSize, FString& Message);
	bool HandleListenerConnectionAccepted(class FSocket *ClientSocket, const FIPv4Endpoint& ClientEndpoint);

	FSocket* ListenerSocket;
	FString LocalHostIP = "127.0.0.1";
	int32 PortNum = 13429; 
	int32 ConnectionTimeout;
	TArray<class FSocket*> Clients;

	void HandleIncomingSocket(FSocket* IncomingConnection);
	static TQueue<FString> ImportQueue;

private:	
	TQueue<class FSocket*, EQueueMode::Mpsc> PendingClients;
	bool Stopping;
	FRunnableThread* ClientThread = NULL;
	class FTcpListener *Listener = NULL;	
	TArray<int32> ConnectionTimer;
};



#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Async/AsyncWork.h"
#include "Common/TcpListener.h"
#include "Common/UdpSocketReceiver.h"
#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "Sockets.h"
#endif
