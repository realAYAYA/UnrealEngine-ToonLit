// Copyright Epic Games, Inc. All Rights Reserved.

#include "Unix/UnixPlatformHttp.h"
#include "Curl/CurlHttp.h"
#include "Curl/CurlHttpManager.h"

void FUnixPlatformHttp::Init()
{
	FCurlHttpManager::InitCurl();
}

class FHttpManager * FUnixPlatformHttp::CreatePlatformHttpManager()
{
	return new FCurlHttpManager();
}

void FUnixPlatformHttp::Shutdown()
{
	FCurlHttpManager::ShutdownCurl();
}

IHttpRequest* FUnixPlatformHttp::ConstructRequest()
{
	return new FCurlHttpRequest();
}

