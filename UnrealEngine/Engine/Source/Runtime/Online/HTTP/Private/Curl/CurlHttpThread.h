// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_CURL

#include "HttpThread.h"

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

#endif //WITH_CURL

class IHttpThreadedRequest;

#if WITH_CURL

class FCurlHttpThread
	: public FLegacyHttpThread
{
public:
	
	FCurlHttpThread();

protected:
	//~ Begin FHttpThread Interface
	virtual void HttpThreadTick(float DeltaSeconds) override;
	virtual bool StartThreadedRequest(IHttpThreadedRequest* Request) override;
	virtual void CompleteThreadedRequest(IHttpThreadedRequest* Request) override;
	//~ End FHttpThread Interface
protected:

	/** Mapping of libcurl easy handles to HTTP requests */
	TMap<CURL*, IHttpThreadedRequest*> HandlesToRequests;
};


#endif //WITH_CURL
