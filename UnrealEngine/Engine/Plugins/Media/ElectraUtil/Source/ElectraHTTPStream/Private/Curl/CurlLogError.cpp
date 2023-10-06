// Copyright Epic Games, Inc. All Rights Reserved.

#if ELECTRA_HTTPSTREAM_LIBCURL

#include "Curl/CurlLogError.h"
#include "ElectraHTTPStreamModule.h"



namespace ElectraHTTPStreamLibCurl
{

FString GetErrorMessage(CURLcode ErrorCode)
{
	FString msg(curl_easy_strerror(ErrorCode));
	return msg;
}

FString GetErrorMessage(CURLMcode ErrorCode)
{
	FString msg(curl_multi_strerror(ErrorCode));
	return msg;
}

void LogError(const FString& Message)
{
	UE_LOG(LogElectraHTTPStream, Error, TEXT("%s"), *Message);
}

}

#endif // ELECTRA_HTTPSTREAM_LIBCURL
