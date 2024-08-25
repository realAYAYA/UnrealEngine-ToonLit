// Copyright Epic Games, Inc. All Rights Reserved.
#include "Containers/BackgroundableTicker.h"
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Http.h"
#include "Misc/CommandLine.h"
#include "WebSocketsLog.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "TestHarness.h"

/**
 *  WebSockets Tests
 *  -----------------------------------------------------------------------------------------------
 *
 *  PURPOSE:
 *
 *	Integration Tests to make sure all kinds of WebSockets client features in C++ work well on different platforms,
 *  including but not limited to error handing, retrying, threading, SSL and profiling.
 * 
 *  -----------------------------------------------------------------------------------------------
 */

#define WEBSOCKETS_TAG "[WebSockets]"

extern TAutoConsoleVariable<bool> CVarHttpInsecureProtocolEnabled;

class FWebSocketsModuleTestFixture
{
public:
	FWebSocketsModuleTestFixture()
		: WebServerIp(TEXT("127.0.0.1"))
		, WebServerWebSocketsPort(8000)
		, OldVerbosity(LogWebSockets.GetVerbosity())
	{
		CVarHttpInsecureProtocolEnabled->Set(true);

		ParseSettingsFromCommandLine();

		// Init HTTP module because websockets module has dependency on it when get proxy
		HttpModule = new FHttpModule();
		IModuleInterface* HttpModuleInterface = HttpModule;
		HttpModuleInterface->StartupModule();

		WebSocketsModule = new FWebSocketsModule();
		IModuleInterface* Module = WebSocketsModule;
		Module->StartupModule();
	}

	virtual ~FWebSocketsModuleTestFixture()
	{
		IModuleInterface* WebSocketsModuleInterface = WebSocketsModule;
		WebSocketsModuleInterface->ShutdownModule();
		delete WebSocketsModuleInterface;

		IModuleInterface* HttpModuleInterface = HttpModule;
		HttpModuleInterface->ShutdownModule();
		delete HttpModuleInterface;

		if (OldVerbosity != LogWebSockets.GetVerbosity())
		{
			LogWebSockets.SetVerbosity(OldVerbosity);
		}
	}

	void ParseSettingsFromCommandLine()
	{
		FParse::Value(FCommandLine::Get(), TEXT("web_server_ip="), WebServerIp);
		FParse::Value(FCommandLine::Get(), TEXT("web_server_websockets_port="), WebServerWebSocketsPort);
	}

	void DisableWarningsInThisTest()
	{
		LogWebSockets.SetVerbosity(ELogVerbosity::Error);
	}

	const FString UrlWithInvalidPortToTestConnectTimeout() const { return FString::Format(TEXT("ws://{0}:{1}"), { *WebServerIp, 8765 }); }
	const FString UrlBase() const { return FString::Format(TEXT("ws://{0}:{1}"), { *WebServerIp, WebServerWebSocketsPort }); }
	const FString UrlWebSocketsTests() const { return FString::Format(TEXT("{0}/webtests/websocketstests"), { *UrlBase() }); }

	FString WebServerIp;
	uint32 WebServerWebSocketsPort;
	FWebSocketsModule* WebSocketsModule;
	FHttpModule* HttpModule;
	ELogVerbosity::Type OldVerbosity;
};

class FRunUntilQuitRequestedFixture : public FWebSocketsModuleTestFixture
{
public:
	FRunUntilQuitRequestedFixture()
	{
	}

	~FRunUntilQuitRequestedFixture()
	{
		RunUntilQuitRequested();
	}

	void RunUntilQuitRequested()
	{
		while (!bQuitRequested)
		{
			FTSBackgroundableTicker::GetCoreTicker().Tick(TickFrequency);
			FTSTicker::GetCoreTicker().Tick(TickFrequency);

			FPlatformProcess::Sleep(TickFrequency);
		}
	}

	float TickFrequency = 1.0f / 60; /*60 FPS*/;
	bool bQuitRequested = false;
	bool bSucceed = false;
};

TEST_CASE_METHOD(FRunUntilQuitRequestedFixture, "WebSockets can connect then send and receive message", WEBSOCKETS_TAG)
{
	TSharedPtr<IWebSocket> WebSocket = WebSocketsModule->CreateWebSocket(FString::Format(TEXT("{0}/echo/"), { *UrlWebSocketsTests() }));

	WebSocket->OnConnected().AddLambda([this, WebSocket](){
		WebSocket->Send(TEXT("hi websockets tests"));
	});

	WebSocket->OnMessage().AddLambda([this, WebSocket](const FString& MessageString) {
		CHECK(MessageString == TEXT("hi websockets tests"));
		WebSocket->Close();
		bSucceed = true;
	});

	WebSocket->OnClosed().AddLambda([this, WebSocket](int32 /* StatusCode */, const FString& /* Reason */, bool /* bWasClean */){
		CHECK(bSucceed);
		bQuitRequested = true;
	});

	WebSocket->OnConnectionError().AddLambda([this, WebSocket](const FString& /* Error */){
		CHECK(false);
		bQuitRequested = true;
	});

	WebSocket->Connect();
}

TEST_CASE_METHOD(FRunUntilQuitRequestedFixture, "WebSockets module can shut down when there are still websockets connections", WEBSOCKETS_TAG)
{
	DisableWarningsInThisTest();

	TSharedPtr<IWebSocket> WebSocket = WebSocketsModule->CreateWebSocket(FString::Format(TEXT("{0}/echo/"), { *UrlWebSocketsTests() }));

	WebSocket->OnConnected().AddLambda([this, WebSocket](){
		bQuitRequested = true;
	});

	WebSocket->OnConnectionError().AddLambda([this, WebSocket](const FString& /* Error */){
		CHECK(false);
		bQuitRequested = true;
	});

	WebSocket->Connect();
}
