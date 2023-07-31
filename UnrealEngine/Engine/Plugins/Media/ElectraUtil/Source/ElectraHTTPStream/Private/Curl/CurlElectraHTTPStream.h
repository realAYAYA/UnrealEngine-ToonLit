// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if ELECTRA_HTTPSTREAM_LIBCURL

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "ParameterDictionary.h"

class IElectraHTTPStream;

class ELECTRAHTTPSTREAM_API FPlatformElectraHTTPStreamLibCurl
{
public:
	static void Startup();
	static void Shutdown();
	static TSharedPtr<IElectraHTTPStream, ESPMode::ThreadSafe> Create(const Electra::FParamDict& InOptions);
};


#ifndef ELECTRA_HTTPSTREAM_CURL_PLATFORM_OVERRIDE
#define ELECTRA_HTTPSTREAM_CURL_PLATFORM_OVERRIDE 0
#endif

// Does the platform override the generic libCurl implementation?
#if ELECTRA_HTTPSTREAM_CURL_PLATFORM_OVERRIDE == 0
typedef FPlatformElectraHTTPStreamLibCurl FPlatformElectraHTTPStream;
#endif

#endif
