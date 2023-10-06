// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if ELECTRA_HTTPSTREAM_LIBCURL

#include "Curl/CurlElectraHTTPStreamConfig.h"
#include "Curl/CurlElectra.h"
#include "ElectraHTTPStreamResponse.h"

#if WITH_SSL
#include "Ssl.h"
#include <openssl/ssl.h>
#endif

class FElectraHTTPStreamLibCurl;
class FElectraHTTPStreamResponse;


/**
 * libCurl version of a HTTP stream request.
 */
class FElectraHTTPStreamRequestLibCurl : public TSharedFromThis<FElectraHTTPStreamRequestLibCurl, ESPMode::ThreadSafe>, public IElectraHTTPStreamRequest
{
public:
	// This enum needs to be kept in order of data flow so states can be compared with < and >
	enum EState
	{
		Inactive,
		Connecting,
		SendingRequest,
		HeadersAvailable,
		AwaitingResponseData,
		ReceivingResponseData,
		Completed,
		Finished,
		Error
	};

	enum EReadResponseResult
	{
		Success,
		Failed,
		EndOfData
	};

	FElectraHTTPStreamRequestLibCurl();
	virtual ~FElectraHTTPStreamRequestLibCurl();

	void SetVerb(const FString& InVerb) override;
	void EnableTimingTraces() override;
	IElectraHTTPStreamBuffer& POSTDataBuffer() override;
	void SetUserAgent(const FString& InUserAgent) override;
	void SetURL(const FString& InURL) override;
	void SetRange(const FString& InRange) override;
	void AllowCompression(bool bInAllowCompression) override;
	void AllowUnsafeRequestsForDebugging() override;
	void AddHeader(const FString& Header, const FString& Value, bool bAppendIfExists) override;
	FElectraHTTPStreamNotificationDelegate& NotificationDelegate() override;
	void Cancel() override;
	IElectraHTTPStreamResponsePtr GetResponse() override;
	bool HasFailed() override;
	FString GetErrorMessage() override;

	EState GetCurrentState();
	bool WasCanceled();
	bool HasFinished();
	bool Setup(FElectraHTTPStreamLibCurl* OwningManager);
	virtual void SetupSSL(bool bAllowUnsafeConnection);
	virtual void SetupProxy(const FString& InProxy);
	bool Execute(FElectraHTTPStreamLibCurl* OwningManager);
	void Close();
	void Terminate();
	void SetError(CURLcode InErrorCode);
	void SetError(CURLMcode InErrorCode);
	void NotifyHeaders();
	void NotifyDownloading();
	void SetCompleted();
	bool HasCompleted();
	void SetFinished();
	void SetResponseStatus(IElectraHTTPStreamResponse::EStatus InStatus);
	void NotifyCallback(EElectraHTTPStreamNotificationReason InReason, int64 InParam);

protected:
	FElectraHTTPStreamRequestLibCurl(const FElectraHTTPStreamRequestLibCurl&) = delete;
	FElectraHTTPStreamRequestLibCurl& operator=(const FElectraHTTPStreamRequestLibCurl&) = delete;

	bool ParseHeaders();

#if ELECTRA_HTTPSTREAM_CURL_VERBOSE_DEBUGLOG
	virtual int CurlDebugCallback(CURL* InEasyHandle, curl_infotype InInfoType, char* InData, size_t InLength);
	static int _CurlDebugCallback(CURL* InEasyHandle, curl_infotype InInfoType, char* InData, size_t InLength, void* InUserData)
	{ return static_cast<FElectraHTTPStreamRequestLibCurl*>(InUserData)->CurlDebugCallback(InEasyHandle, InInfoType, InData, InLength); }
#endif
#if ELECTRA_HTTPSTREAM_CURL_USE_XFERINFO
	virtual int CurlXferInfoCallback(curl_off_t InDlTotal, curl_off_t InDlNow, curl_off_t InUlTotal, curl_off_t InUlNow);
	static int _CurlXferInfoCallback(void* InUserData, curl_off_t InDlTotal, curl_off_t InDlNow, curl_off_t InUlTotal, curl_off_t InUlNow)
	{ return static_cast<FElectraHTTPStreamRequestLibCurl*>(InUserData)->CurlXferInfoCallback(InDlTotal, InDlNow, InUlTotal, InUlNow); }
#else
	virtual int CurlProgressCallback(double InDlTotal, double InDlNow, double InUlTotal, double InUlNow);
	static int _CurlProgressCallback(void* InUserData, double InDlTotal, double InDlNow, double InUlTotal, double InUlNow)
	{ return static_cast<FElectraHTTPStreamRequestLibCurl*>(InUserData)->CurlProgressCallback(InDlTotal, InDlNow, InUlTotal, InUlNow); }
#endif
	virtual size_t CurlResponseHeaderCallback(void* InData, size_t InBlockSize, size_t InNumBlocks);
	static size_t _CurlResponseHeaderCallback(void* InData, size_t InBlockSize, size_t InNumBlocks, void* InUserData)
	{ return static_cast<FElectraHTTPStreamRequestLibCurl*>(InUserData)->CurlResponseHeaderCallback(InData, InBlockSize, InNumBlocks); }
	virtual size_t CurlResponseBodyCallback(void* InData, size_t InBlockSize, size_t InNumBlocks);
	static size_t _CurlResponseBodyCallback(void* InData, size_t InBlockSize, size_t InNumBlocks, void* InUserData)
	{ return static_cast<FElectraHTTPStreamRequestLibCurl*>(InUserData)->CurlResponseBodyCallback(InData, InBlockSize, InNumBlocks); }
	virtual size_t CurlUploadCallback(void* OutData, size_t InBlockSize, size_t InNumBlocks);
	static size_t _CurlUploadCallback(void* OutData, size_t InBlockSize, size_t InNumBlocks, void* InUserData)
	{ return static_cast<FElectraHTTPStreamRequestLibCurl*>(InUserData)->CurlUploadCallback(OutData, InBlockSize, InNumBlocks); }
	virtual int CurlSeekCallback(curl_off_t InOffset, int InOrigin);
	static int _CurlSeekCallback(void* InUserData, curl_off_t InOffset, int InOrigin)
	{ return static_cast<FElectraHTTPStreamRequestLibCurl*>(InUserData)->CurlSeekCallback(InOffset, InOrigin); }
#if WITH_SSL
	virtual CURLcode CurlSSLCtxCallback(CURL* InEasyHandle, void* InSSLCtx);
	static CURLcode _CurlSSLCtxCallback(CURL* InEasyHandle, void* InSSLCtx, void* InUserData)
	{ return static_cast<FElectraHTTPStreamRequestLibCurl*>(InUserData)->CurlSSLCtxCallback(InEasyHandle, InSSLCtx); }
	static int SslCertVerify(int PreverifyOk, X509_STORE_CTX* Context);
#endif

	// User agent. Defaults to a global one but can be set with each individual request.
	FString UserAgent;
	// GET, HEAD or POST
	FString Verb;
	// URL to request
	FString URL;
	// Optional byte range. If set this must be a valid range string.
	FString Range;
	// Host-only part of URL
	FString Host;
	// Set to true to allow gzip/deflate for GET requests.
	bool bAllowCompression = false;
	// Additional headers to be sent with the request.
	TMap<FString, FString> AdditionalHeaders;

	// Configuration
	int MaxRedirections = 10;
	int ConnectionTimeoutMillis = 10000;
#if ELECTRA_HTTPSTREAM_CURL_ALLOW_UNSAFE_CONNECTIONS_FOR_DEBUGGING
	bool bAllowUnsafeConnectionsForDebugging = false;
#endif

	FCriticalSection NotificationLock;
	FElectraHTTPStreamNotificationDelegate NotificationCallback;

	// Curl handles
	CURL* CurlEasyHandle = nullptr;
	curl_slist* CurlHeaderList = nullptr;
	char CurlErrorMessageBuffer[CURL_ERROR_SIZE+1] = {0};
	CURLM* CurlMultiHandle = nullptr;

	bool bIsRedirectionResponse = false;
	TArray<FElectraHTTPStreamHeader> CurrentResponseHeaders;
	FString CurrentHTTPResponseStatusLine;
	bool bResponseHeadersParsed = false;
	long HttpCode = 0;
	long HttpVersion = CURL_HTTP_VERSION_NONE;
	FString EffectiveURL;

	volatile EState CurrentState = EState::Inactive;
	std::atomic_bool bCancel { false };
	std::atomic_bool bTerminated { false };
	int64 LastReportedDownloadSize = 0;

	FElectraHTTPStreamBuffer PostData;
	TSharedPtr<FElectraHTTPStreamResponse, ESPMode::ThreadSafe> Response;
};

#endif // ELECTRA_HTTPSTREAM_LIBCURL
