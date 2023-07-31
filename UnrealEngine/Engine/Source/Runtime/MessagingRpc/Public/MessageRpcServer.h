// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "Containers/Ticker.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "IMessageContext.h"
#include "IMessageRpcServer.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"
#include "UObject/TopLevelAssetPath.h"

class FMessageEndpoint;
class FMessagingRpcModule;
class IAsyncProgress;
class IAsyncTask;
class IMessageBus;
class IMessageRpcHandler;
class IMessageRpcReturn;
struct FMessageEndpointBuilder;
struct FMessageRpcCancel;


/**
 * Implements an RPC server.
 */
class MESSAGINGRPC_API FMessageRpcServer
	: public IMessageRpcServer
{
public:
	/** Default constructor. */
	FMessageRpcServer();
	FMessageRpcServer(const FString& InDebugName, const TSharedRef<IMessageBus, ESPMode::ThreadSafe>& InMessageBus);

	/** Virtual destructor. */
	virtual ~FMessageRpcServer();

public:

	//~ IMessageRpcServer interface

	virtual void AddHandler(const FTopLevelAssetPath& RequestMessageType, const TSharedRef<IMessageRpcHandler>& Handler) override;
	virtual const FMessageAddress& GetAddress() const override;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual FOnMessageRpcNoHandler& OnNoHandler() override;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual FOnMessagePathNameRpcNoHandler& OnNoHandlerWithPathName() override;
	virtual void SetSendProgressUpdate(bool InSendProgress) override;
protected:
	explicit FMessageRpcServer(FMessageEndpointBuilder&& InEndpointBuilder);

	struct FReturnInfo
	{
		FMessageAddress ClientAddress;
		FDateTime LastProgressSent;
		TSharedPtr<IAsyncProgress> Progress;
		TSharedPtr<IMessageRpcReturn> Return;
		TSharedPtr<IAsyncTask> Task;
	};

	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> GetEndpoint() const;

	/** Processes an FMessageRpcCancel message. */
	void ProcessCancelation(const FMessageRpcCancel& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Processes an RPC request message. */
	void ProcessRequest(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Send a progress message to the RPC client that made the RPC call. */
	void SendProgress(const FGuid& CallId, const FReturnInfo& ReturnInfo);

	/** Send a result message to the RPC client that made the RPC call. */
	void SendResult(const FGuid& CallId, const FReturnInfo& ReturnInfo);

protected:

	/** Message endpoint. */
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

private:

	/** Handles all incoming messages. */
	void HandleMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles the ticker. */
	bool HandleTicker(float DeltaTime);

private:

	/** Registered request message handlers. */
	TMap<FTopLevelAssetPath, TSharedPtr<IMessageRpcHandler>> Handlers;

	/* Delegate that is executed when a received RPC message has no registered handler. */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FOnMessageRpcNoHandler NoHandlerDelegate;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/* Delegate that is executed when a received RPC message has no registered handler. */
	FOnMessagePathNameRpcNoHandler NoHandlerDelegateWithPathName;

	/** Collection of pending RPC returns. */
	TMap<FGuid, FReturnInfo> Returns;

	/** Handle to the registered ticker. */
	FTSTicker::FDelegateHandle TickerHandle;

	/** If the server sends progress updates. */
	bool bSendProgress = true;
};
