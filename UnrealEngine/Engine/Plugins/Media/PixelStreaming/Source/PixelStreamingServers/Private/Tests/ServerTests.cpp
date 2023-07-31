// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "PixelStreamingServers.h"
#include "Misc/Paths.h"
#include "PixelStreamingServersLog.h"
#include "ServerUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::PixelStreamingServers
{

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FWaitForServerOrTimeout, TSharedPtr<UE::PixelStreamingServers::IServer>, Server);
	bool FWaitForServerOrTimeout::Update()
	{
		if (Server)
		{
			return Server->IsTimedOut() || Server->IsReady();
		}
		return true;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCleanupServer, TSharedPtr<IServer>, Server);
	bool FCleanupServer::Update()
	{
		if (Server)
		{
			Server->Stop();
		}
		return true;
	}

	static const int HttpPort = 85;
	static const FString ExpectedWebserverAddress = FString::Printf(TEXT("http://127.0.0.1:%d"), HttpPort);
	static const FString ExpectedPlayerWSAddress = FString::Printf(TEXT("ws://127.0.0.1:%d"), HttpPort);

	static const int SFUPort = 8889;
	static const FString ExpectedSFUAddress = FString::Printf(TEXT("ws://127.0.0.1:%d"), SFUPort);

	static const int StreamerPort = 8989;
	static const FString ExpectedStreamerAddress = FString::Printf(TEXT("ws://127.0.0.1:%d"), StreamerPort);

	static const int MatchmakerPort = 9999;
	static const FString ExpectedMatchmakerAddress = FString::Printf(TEXT("ws://127.0.0.1:%d"), MatchmakerPort);

	static const bool bTestServerBinary = false;

	FString GetCirrusBinaryAbsPath()
	{
		FString ServerPath = 
			FPaths::EnginePluginsDir() / 
			TEXT("Media") / 
			TEXT("PixelStreaming") / 
			TEXT("Resources") /
			TEXT("WebServers") /
			TEXT("SignallingWebServer");

#if PLATFORM_WINDOWS
		ServerPath = ServerPath / TEXT("cirrus.exe");
#elif PLATFORM_LINUX
		ServerPath = ServerPath / TEXT("cirrus");
#else
		UE_LOG(LogPixelStreaming, Error, TEXT("Unsupported platform for Pixel Streaming."));
		return false
#endif

		ServerPath = FPaths::ConvertRelativePathToFull(ServerPath);

		return ServerPath;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaunchDownloadedCirrusTest, "System.Plugins.PixelStreaming.LaunchDownloadedCirrus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FLaunchDownloadedCirrusTest::RunTest(const FString& Parameters)
	{
		UE_LOG(LogPixelStreamingServers, Log, TEXT("----------- LaunchDownloadedCirrusTest -----------"));

		TSharedPtr<IServer> SignallingServer = MakeCirrusServer();
		FLaunchArgs LaunchArgs;
		LaunchArgs.bPollUntilReady = true;
		LaunchArgs.ReconnectionTimeoutSeconds = 30.0f;
		LaunchArgs.ReconnectionIntervalSeconds = 2.0f;
		LaunchArgs.ProcessArgs = FString::Printf(TEXT("--HttpPort=%d --SFUPort=%d --StreamerPort=%d --MatchmakerPort=%d"), HttpPort, SFUPort, StreamerPort, MatchmakerPort);

		if(bTestServerBinary)
		{
			LaunchArgs.ServerBinaryOverridePath = GetCirrusBinaryAbsPath();
		}

		bool bLaunched = SignallingServer->Launch(LaunchArgs);

		if(!bLaunched)
		{
			// If we were unable to launch this means some files were missing, we early exit here because this will always happen on Horde
			// and we don't want a permanently failing test on Horde. 
			// Todo: Determine way to only disable test on Horde but not locally, or to make it actually download the required scripts.
			return true;
		}

		SignallingServer->OnReady.AddLambda([this](TMap<EEndpoint, FURL> Endpoints) {
			TestTrue("Got server OnReady.", true);

			FString ActualWebserverUrl = Utils::ToString(Endpoints[EEndpoint::Signalling_Webserver]);
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Http address for webserver. Actual=%s | Expected=%s"), *ActualWebserverUrl, *ExpectedWebserverAddress);
			TestTrue(FString::Printf(TEXT("Http address for webserver. Actual=%s | Expected=%s"), *ActualWebserverUrl, *ExpectedWebserverAddress), 
			ActualWebserverUrl == ExpectedWebserverAddress);

			FString ActualStreamerUrl = Utils::ToString(Endpoints[EEndpoint::Signalling_Streamer]);
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Websocket address for streamer messages. Actual=%s | Expected=%s"), *ActualStreamerUrl, *ExpectedStreamerAddress);
			TestTrue(FString::Printf(TEXT("Websocket address for streamer messages. Actual=%s | Expected=%s"), *ActualStreamerUrl, *ExpectedStreamerAddress), 
			ActualStreamerUrl == ExpectedStreamerAddress);

			FString ActualPlayersUrl = Utils::ToString(Endpoints[EEndpoint::Signalling_Players]);
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Websocket address for player messages. Actual=%s | Expected=%s"), *ActualPlayersUrl, *ExpectedPlayerWSAddress);
			TestTrue(FString::Printf(TEXT("Websocket address for player messages. Actual=%s | Expected=%s"), *ActualPlayersUrl, *ExpectedPlayerWSAddress), 
			ActualPlayersUrl == ExpectedPlayerWSAddress);

			FString ActualSFUUrl = Utils::ToString(Endpoints[EEndpoint::Signalling_SFU]);
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Websocket address for SFU messages. Actual=%s | Expected=%s"), *ActualSFUUrl, *ExpectedSFUAddress);
			TestTrue(FString::Printf(TEXT("Websocket address for SFU messages. Actual=%s | Expected=%s"), *ActualSFUUrl, *ExpectedSFUAddress), 
			ActualSFUUrl == ExpectedSFUAddress);

			FString ActualMatchmakerUrl = Utils::ToString(Endpoints[EEndpoint::Signalling_Matchmaker]);
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Websocket address for matchmaker messages. Actual=%s | Expected=%s"), *ActualMatchmakerUrl, *ExpectedMatchmakerAddress);
			TestTrue(FString::Printf(TEXT("Websocket address for matchmaker messages. Actual=%s | Expected=%s"), *ActualMatchmakerUrl, *ExpectedMatchmakerAddress), 
			ActualMatchmakerUrl == ExpectedMatchmakerAddress);
		});

		SignallingServer->OnFailedToReady.AddLambda([this]() {
			TestTrue("Server was not ready.", false);
		});

		ADD_LATENT_AUTOMATION_COMMAND(FWaitForServerOrTimeout(SignallingServer));
		ADD_LATENT_AUTOMATION_COMMAND(FCleanupServer(SignallingServer));

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaunchEmbeddedCirrusTest, "System.Plugins.PixelStreaming.LaunchEmbeddedCirrus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FLaunchEmbeddedCirrusTest::RunTest(const FString& Parameters)
	{
		UE_LOG(LogPixelStreamingServers, Log, TEXT("----------- LaunchEmbeddedCirrusTest -----------"));

		TSharedPtr<IServer> SignallingServer = MakeSignallingServer();
		FLaunchArgs LaunchArgs;
		LaunchArgs.bPollUntilReady = true;
		LaunchArgs.ReconnectionTimeoutSeconds = 30.0f;
		LaunchArgs.ReconnectionIntervalSeconds = 2.0f;
		LaunchArgs.ProcessArgs = FString::Printf(TEXT("--HttpPort=%d --StreamerPort=%d"), HttpPort, StreamerPort);

		bool bLaunched = SignallingServer->Launch(LaunchArgs);
		UE_LOG(LogPixelStreamingServers, Log, TEXT("Embedded cirrus launched: %s"), bLaunched ? TEXT("true") : TEXT("false"));
		TestTrue("Embedded cirrus launched.", bLaunched);

		if(!bLaunched)
		{
			return false;
		}

		SignallingServer->OnReady.AddLambda([this](TMap<EEndpoint, FURL> Endpoints) {
			TestTrue("Got server OnReady.", true);

			FString ActualWebserverUrl = Utils::ToString(Endpoints[EEndpoint::Signalling_Webserver]);
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Http address for webserver. Actual=%s | Expected=%s"), *ActualWebserverUrl, *ExpectedWebserverAddress);
			TestTrue(FString::Printf(TEXT("Http address for webserver. Actual=%s | Expected=%s"), *ActualWebserverUrl, *ExpectedWebserverAddress), 
			ActualWebserverUrl == ExpectedWebserverAddress);

			FString ActualStreamerUrl = Utils::ToString(Endpoints[EEndpoint::Signalling_Streamer]);
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Websocket address for streamer messages. Actual=%s | Expected=%s"), *ActualStreamerUrl, *ExpectedStreamerAddress);
			TestTrue(FString::Printf(TEXT("Websocket address for streamer messages. Actual=%s | Expected=%s"), *ActualStreamerUrl, *ExpectedStreamerAddress), 
			ActualStreamerUrl == ExpectedStreamerAddress);

			FString ActualPlayersUrl = Utils::ToString(Endpoints[EEndpoint::Signalling_Players]);
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Websocket address for player messages. Actual=%s | Expected=%s"), *ActualPlayersUrl, *ExpectedPlayerWSAddress);
			TestTrue(FString::Printf(TEXT("Websocket address for player messages. Actual=%s | Expected=%s"), *ActualPlayersUrl, *ExpectedPlayerWSAddress), 
			ActualPlayersUrl == ExpectedPlayerWSAddress);

		});

		SignallingServer->OnFailedToReady.AddLambda([this]() {
			TestTrue("Server was not ready.", false);
		});

		ADD_LATENT_AUTOMATION_COMMAND(FWaitForServerOrTimeout(SignallingServer));
		ADD_LATENT_AUTOMATION_COMMAND(FCleanupServer(SignallingServer));

		return true;
	}

	// Todo test where create and teardown signalling server 10 times in quick succession with probe to ensure ports are freed

} // namespace UE::PixelStreamingServers

#endif // WITH_DEV_AUTOMATION_TESTS