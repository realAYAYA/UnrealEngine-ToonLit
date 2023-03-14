// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if ELECTRA_HTTPSTREAM_LIBCURL

#include "CoreMinimal.h"

#if PLATFORM_MICROSOFT
#include "Microsoft/WindowsHWrapper.h"
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#endif

#if ELECTRA_HTTPSTREAM_XCURL
//We copied this template to include the windows file from WindowsHWrapper's way if including MinWindows.h, since including xcurl.h directly caused gnarly build errors
#include "CoreTypes.h"
#include "HAL/PlatformMemory.h"
#include "Microsoft/PreWindowsApi.h"
#ifndef STRICT
#define STRICT
#endif
#include "XCurl.h"
#include "Microsoft/PostWindowsApi.h"
#else
	#include "curl/curl.h"
#endif

#if PLATFORM_MICROSOFT
#include "Microsoft/HideMicrosoftPlatformTypes.h"
#endif


#endif
