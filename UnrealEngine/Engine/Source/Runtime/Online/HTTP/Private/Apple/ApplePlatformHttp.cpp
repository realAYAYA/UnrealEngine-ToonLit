// Copyright Epic Games, Inc. All Rights Reserved.

#include "Apple/ApplePlatformHttp.h"
#include "AppleHTTP.h"

#if WITH_SSL
#include "Ssl.h"
#endif

void FApplePlatformHttp::Init()
{
#if WITH_SSL
	// Load SSL module during HTTP module's StatupModule() to make sure module manager figures out the dependencies correctly
	// and doesn't unload SSL before unloading HTTP module at exit
	FSslModule::Get();
#endif
}


void FApplePlatformHttp::Shutdown()
{
}


IHttpRequest* FApplePlatformHttp::ConstructRequest()
{
	return new FAppleHttpRequest();
}
