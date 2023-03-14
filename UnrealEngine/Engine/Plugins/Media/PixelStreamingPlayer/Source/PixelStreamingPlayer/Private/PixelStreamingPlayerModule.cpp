// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingPlayerPrivate.h"
#include "Modules/ModuleManager.h"
#include "WebSocketsModule.h"
#include "WebRTCIncludes.h"

DEFINE_LOG_CATEGORY(LogPixelStreamingPlayer);

class FPixelStreamingPlayerModule : public IModuleInterface
{
public:
private:
	/** IModuleInterface implementation */
	void StartupModule() override
	{
		FModuleManager::LoadModuleChecked<FWebSocketsModule>("WebSockets");
		rtc::InitializeSSL();
	}
	void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FPixelStreamingPlayerModule, PixelStreamingPlayer)
