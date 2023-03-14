// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if ELECTRA_HTTPSTREAM_LIBCURL

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Templates/SharedPointer.h"

#include "Curl/CurlElectraHTTPStreamConfig.h"
#include "Curl/CurlElectra.h"
#include "Utilities/TimeWaitableSignal.h"

class FElectraHTTPStreamRequestLibCurl;

/**
 * libCurl version of the ElectraHTTPStream implementation.
 */
class FElectraHTTPStreamLibCurl : public TSharedFromThis<FElectraHTTPStreamLibCurl, ESPMode::ThreadSafe>, public IElectraHTTPStream, private FRunnable
{
public:
	virtual ~FElectraHTTPStreamLibCurl();

	FElectraHTTPStreamLibCurl();
	virtual bool Initialize(const Electra::FParamDict& InOptions);

	void AddThreadHandlerDelegate(FElectraHTTPStreamThreadHandlerDelegate InDelegate) override
	{
		FScopeLock lock(&CallbackLock);
		ThreadHandlerCallback = MoveTemp(InDelegate);
	}
	void RemoveThreadHandlerDelegate() override
	{
		FScopeLock lock(&CallbackLock);
		ThreadHandlerCallback.Unbind();
	}

	void Close() override;

	IElectraHTTPStreamRequestPtr CreateRequest() override;

	void AddRequest(IElectraHTTPStreamRequestPtr Request) override;

#if ELECTRA_HTTPSTREAM_CURL_USE_SHARE
	CURLSH* GetCurlShareHandle()
	{ return CurlShareHandle; }
#endif
	CURLM* GetCurlMultiHandle()
	{ return CurlMultiHandle; }
	bool SupportsHTTP2()
	{ return bSupportsHTTP2; }
	FString GetProxyAddressAndPort()
	{ return ProxyAddressAndPort; }

	void TriggerWorkSignal()
	{
		HaveWorkSignal.Signal();
#if ELECTRA_HTTPSTREAM_CURL_USE_MULTIPOLL
		curl_multi_wakeup(CurlMultiHandle);
#endif
	}

private:
	// Methods from FRunnable
	uint32 Run() override final;
	void Stop() override final;

	void SetupNewRequests();
	void UpdateActiveRequests();
	void HandleCurl();
	void HandleCompletedRequests();
	void WorkInnerLoop();
	void WorkMultiPoll();

	// Configuration
	FString ProxyAddressAndPort;
	double PeriodicRequestUpdateInterval = 0.05;	// 20ms
	double MaxCurlMultiPerformDelay = 0.001;		// 1ms


	// Curl handles
#if ELECTRA_HTTPSTREAM_CURL_USE_SHARE
	CURLSH* CurlShareHandle = nullptr;
#endif
	CURLM* CurlMultiHandle = nullptr;
	bool bSupportsHTTP2 = false;

	FThreadSafeCounter ExitRequest;
	FRunnableThread* Thread = nullptr;
	FTimeWaitableSignal HaveWorkSignal;

	FCriticalSection CallbackLock;
	FElectraHTTPStreamThreadHandlerDelegate ThreadHandlerCallback;

	FCriticalSection RequestLock;
	TArray<TSharedPtr<FElectraHTTPStreamRequestLibCurl, ESPMode::ThreadSafe>> NewRequests;
	TArray<TSharedPtr<FElectraHTTPStreamRequestLibCurl, ESPMode::ThreadSafe>> ActiveRequests;
	TArray<TSharedPtr<FElectraHTTPStreamRequestLibCurl, ESPMode::ThreadSafe>> CompletedRequests;
};

#endif
