// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreHttp/Client.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "IO/IoBuffer.h"
#include "IO/IoOffsetLength.h"
#include "IO/IoStatus.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"

namespace UE::IO::IAS
{

struct FHttpClientConfig
{
	TArray<FString> Endpoints;
	int32 PrimaryEndpoint = 0;
	int32 MaxConnectionCount = 8;
	int32 PipelineLength = 1;
	int32 MaxRetryCount = 1;
	int32 ReceiveBufferSize = -1;
	bool bChangeEndpointAfterSuccessfulRetry = true;
};

class FHttpClient
{
public:
	using FGetCallback = TFunction<void(TIoStatusOr<FIoBuffer>, uint64 DurationMs)>;

	static TUniquePtr<FHttpClient> Create(FHttpClientConfig&& ClientConfig);
	static TUniquePtr<FHttpClient> Create(const FString& Endpoint);

	void Get(FAnsiStringView Url, const FIoOffsetAndLength& Range, FGetCallback&& Callback);
	void Get(FAnsiStringView Url, FGetCallback&& Callback);

	bool Tick(int32 WaitTimeMs, uint32 MaxKiBPerSecond);
	bool Tick() { return Tick(-1, 0); }

	int32 GetEndpoint() const { return CurrentEndpoint; }
	int32 GetPrimaryEndpoint() const { return Config.PrimaryEndpoint; }
	void SetEndpoint(int32 Endpoint);
	bool IsUsingPrimaryEndpoint() const { return CurrentEndpoint == Config.PrimaryEndpoint; }

private:
	struct FRequestParams
	{
		FString Url;
		FIoOffsetAndLength Range;
		FGetCallback Callback;
		int32 Attempt = 0;
		int32 Endpoint = INDEX_NONE;
	};

	FHttpClient(FHttpClientConfig&& ClientConfig);
	void RetryRequest(FRequestParams&& Params, bool bNextEndpoint);
	void IssueRequest(FRequestParams&& Params);
	TUniquePtr<HTTP::FConnectionPool> CreateConnection(const FStringView& HostAddr);
	void EnsureConnection(int32 Connection);
	const FString& GetEndpointUrl(int32 Endpoint);

	FHttpClientConfig Config;
	HTTP::FEventLoop EventLoop;
	TArray<TUniquePtr<HTTP::FConnectionPool>> Connections;
	TArray<FRequestParams> Retries;
	int32 CurrentEndpoint = INDEX_NONE;
};

} // namespace UE::IO::IAS
