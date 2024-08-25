// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if ELECTRA_HTTPSTREAM_LIBCURL

#include "CoreMinimal.h"

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


#endif
