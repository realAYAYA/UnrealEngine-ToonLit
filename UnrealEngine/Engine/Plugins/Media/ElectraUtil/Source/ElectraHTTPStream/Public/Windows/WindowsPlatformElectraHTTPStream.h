// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if ELECTRA_HTTPSTREAM_WINHTTP
#include "WinHttp/WinHttpElectraHTTPStream.h"

#elif ELECTRA_HTTPSTREAM_LIBCURL
#include "Curl/CurlElectraHTTPStream.h"

#endif
