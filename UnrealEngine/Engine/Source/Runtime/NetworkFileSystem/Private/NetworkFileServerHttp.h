// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/Guid.h"
#include "HAL/Runnable.h"
#include "INetworkFileServer.h"
#include "INetworkFileSystemModule.h"

class FInternetAddr;
class FNetworkFileServerClientConnectionHTTP;
class ITargetPlatform;

#if ENABLE_HTTP_FOR_NFS

#if PLATFORM_WINDOWS
	#include "Windows/WindowsHWrapper.h"
	#include "Windows/AllowWindowsPlatformTypes.h"
#endif

/*
	ObjectBase.h (somehow included here) defines a namespace called UI,
	openssl headers, included by libwebsockets define a typedef with the same names
	The define will move the openssl define out of the way.
*/
#define UI UI_ST
	THIRD_PARTY_INCLUDES_START
	#include "libwebsockets.h"
	THIRD_PARTY_INCLUDES_END
#undef UI

#if PLATFORM_WINDOWS
	#include "Windows/HideWindowsPlatformTypes.h"
#endif


class FNetworkFileServerHttp
	:	public INetworkFileServer // It is a NetworkFileServer
	,	private FRunnable // Also spins up a thread but others don't need to know.
{

public:
	FNetworkFileServerHttp(FNetworkFileServerOptions InFileServerOptions);

	// INetworkFileServer Interface.

	virtual bool IsItReadyToAcceptConnections(void) const;
	virtual FString GetSupportedProtocol() const override;
	virtual bool GetAddressList(TArray<TSharedPtr<FInternetAddr> >& OutAddresses) const override;
	virtual int32 NumConnections() const;
	virtual void Shutdown();

	virtual ~FNetworkFileServerHttp();


	// static functions. callbacks for libwebsocket.
	static int CallBack_HTTP(struct lws *wsi,
		enum lws_callback_reasons reason, void *user,
		void *in, size_t len);

private:

	//FRunnable Interface.
	virtual bool Init();
	virtual uint32 Run();
	virtual void Stop();
	virtual void Exit();

	static void Process (FArchive&, TArray<uint8>&, FNetworkFileServerHttp* );


	// factory method for creating a new Client Connection.
	class FNetworkFileServerClientConnectionHTTP* CreateNewConnection();

	// File server options
	FNetworkFileServerOptions FileServerOptions;

	/** OpenSSL context */
	SSL_CTX* SslContext;

	// libwebsocket context. All access to the library happens via this context.
	struct lws_context *Context;

	// Service Http connections on this thread.
	FRunnableThread* WorkerThread;

	// used to send simple message.
	FThreadSafeCounter StopRequested;

	// Has successfully Initialized;
	FThreadSafeCounter Ready;

	// Clients being served.
	TMap< FGuid, FNetworkFileServerClientConnectionHTTP* > RequestHandlers;
};

#endif
