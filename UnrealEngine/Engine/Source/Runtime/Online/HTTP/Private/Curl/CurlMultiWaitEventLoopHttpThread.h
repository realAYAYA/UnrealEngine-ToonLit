// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#if WITH_CURL
#if WITH_CURL_MULTIWAIT

#include "EventLoopHttpThread.h"
#include "EventLoop/EventLoop.h"
#include "Templates/Function.h"

#if PLATFORM_MICROSOFT
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#endif
#ifdef PLATFORM_CURL_INCLUDE
#include PLATFORM_CURL_INCLUDE
#else
#include "curl/curl.h"
#endif
#if PLATFORM_MICROSOFT
#include "Microsoft/HideMicrosoftPlatformTypes.h"
#endif

class IHttpThreadedRequest;
class FCurlMultiWaitIOManager;

class FCurlMultiWaitIOManagerIOAccess final : public FNoncopyable
{
public:
	FCurlMultiWaitIOManagerIOAccess(FCurlMultiWaitIOManager& InIOManager);

	// Nothing needed for now.

private:
	FCurlMultiWaitIOManager& IOManager;
};

class FCurlMultiWaitIOManager final : public UE::EventLoop::IIOManager
{
public:
	using FIOAccess = FCurlMultiWaitIOManagerIOAccess;

	struct FParams
	{
		CURLM* MultiHandle = nullptr;
		TUniqueFunction<void()> ProcessCurlRequests;
	};

	FCurlMultiWaitIOManager(UE::EventLoop::IEventLoop& EventLoop, FParams&& Params);
	virtual ~FCurlMultiWaitIOManager() = default;
	virtual bool Init() override;
	virtual void Shutdown() override;
	virtual void Notify() override;
	virtual void Poll(FTimespan WaitTime) override;

	FIOAccess& GetIOAccess()
	{
		return IOAccess;
	}

private:
	void CheckMultiCodeOk(const CURLMcode Code);

	FIOAccess IOAccess;
	UE::EventLoop::IEventLoop& EventLoop;
	FParams Params;
	int32 EmptySequentialWaitCount;
};

class FCurlMultiWaitEventLoopHttpThread
	: public FEventLoopHttpThread
{
public:

	FCurlMultiWaitEventLoopHttpThread();

protected:
	//~ Begin FHttpThread Interface
	virtual bool StartThreadedRequest(IHttpThreadedRequest* Request) override;
	virtual void CompleteThreadedRequest(IHttpThreadedRequest* Request) override;
	//~ End FHttpThread Interface
protected:
	virtual void CreateEventLoop() override;
	virtual void DestroyEventLoop() override;
	virtual void UpdateEventLoopConfigs() override;
	virtual UE::EventLoop::IEventLoop* GetEventLoop() override;
	virtual UE::EventLoop::IEventLoop& GetEventLoopChecked() override;

	void ProcessCurlRequests();

	TOptional<UE::EventLoop::TEventLoop<FCurlMultiWaitIOManager>> EventLoop;

	/** Mapping of libcurl easy handles to HTTP requests */
	TMap<CURL*, IHttpThreadedRequest*> HandlesToRequests;
};

#endif // WITH_CURL_MULTIWAIT
#endif // WITH_CURL
