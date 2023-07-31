// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "TestUtils.h"
#include "PixelStreamingVideoInputBackBuffer.h"
#include "IPixelStreamingStreamer.h"
#include "PixelCaptureBufferFormat.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::PixelStreaming
{

	void DoFrameReceiveTest()
	{
		int32 StreamerPort = 7564;
		int32 PlayerPort = 4578;

		FMockVideoFrameConfig FrameConfig = { 128 /*Width*/, 128 /*Height*/, 255 /*Y*/, 137 /*U*/, 216 /*V*/ };

		TSharedPtr<UE::PixelStreamingServers::IServer> SignallingServer = CreateSignallingServer(StreamerPort, PlayerPort);

		TSharedPtr<IPixelStreamingStreamer> Streamer = CreateStreamer(StreamerPort);
		TSharedPtr<FPixelStreamingVideoInputI420> VideoInput = MakeShared<FPixelStreamingVideoInputI420>();
		VideoInput->AddOutputFormat(PixelCaptureBufferFormat::FORMAT_I420);
		Streamer->SetVideoInput(VideoInput);

		TSharedPtr<FMockPlayer> Player = CreatePlayer(FMockPlayer::EMode::AcceptOffers);
		TSharedPtr<FMockVideoSink> VideoSink = MakeShared<FMockVideoSink>();
		Player->VideoSink = VideoSink;

		Streamer->StartStreaming();

		ADD_LATENT_AUTOMATION_COMMAND(FConnectPlayerAfterStreamerConnectedOrTimeout(5.0, Streamer, Player, PlayerPort))
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerConnectedOrTimeout(5.0, Player, PlayerPort))
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForICEConnectedOrTimeout(5.0, Player))
		ADD_LATENT_AUTOMATION_COMMAND(FSendSolidColorFrame(VideoInput, FrameConfig))
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForFrameReceived(5.0, VideoSink, FrameConfig))
		ADD_LATENT_AUTOMATION_COMMAND(FCleanupAll(SignallingServer, Streamer, Player))
	}

	void DoFrameResizeMultipleTimesTest()
	{
		int32 StreamerPort = 7564;
		int32 PlayerPort = 4578;

		TSharedPtr<UE::PixelStreamingServers::IServer> SignallingServer = CreateSignallingServer(StreamerPort, PlayerPort);

		TSharedPtr<IPixelStreamingStreamer> Streamer = CreateStreamer(StreamerPort);
		TSharedPtr<FPixelStreamingVideoInputI420> VideoInput = MakeShared<FPixelStreamingVideoInputI420>();
		VideoInput->AddOutputFormat(PixelCaptureBufferFormat::FORMAT_I420);
		Streamer->SetVideoInput(VideoInput);

		TSharedPtr<FMockPlayer> Player = CreatePlayer(FMockPlayer::EMode::AcceptOffers);
		TSharedPtr<FMockVideoSink> VideoSink = MakeShared<FMockVideoSink>();
		Player->VideoSink = VideoSink;

		// Note: Important to couple framerate as we are manually passing frames and don't want any cached frames
		Streamer->SetCoupleFramerate(true);
		Streamer->StartStreaming();

		ADD_LATENT_AUTOMATION_COMMAND(FConnectPlayerAfterStreamerConnectedOrTimeout(5.0, Streamer, Player, PlayerPort))
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerConnectedOrTimeout(5.0, Player, PlayerPort))
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForICEConnectedOrTimeout(5.0, Player))

		for (int Res = 2; Res < 512; Res *= 2)
		{
			FMockVideoFrameConfig FrameConfig = { Res /*Width*/, Res /*Height*/, 255 /*Y*/, 0 /*U*/, 255 /*V*/ };
			ADD_LATENT_AUTOMATION_COMMAND(FSendSolidColorFrame(VideoInput, FrameConfig))
			ADD_LATENT_AUTOMATION_COMMAND(FWaitForFrameReceived(5.0, VideoSink, FrameConfig))
		}

		ADD_LATENT_AUTOMATION_COMMAND(FCleanupAll(SignallingServer, Streamer, Player))
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVP8FrameReceivedTest, "System.Plugins.PixelStreaming.FVP8FrameReceivedTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FVP8FrameReceivedTest::RunTest(const FString& Parameters)
	{
		SetCodec(EPixelStreamingCodec::VP8, false /*bUseComputeShader*/);
		DoFrameReceiveTest();
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVP8ComputeShaderFrameReceivedTest, "System.Plugins.PixelStreaming.FVP8ComuteShaderFrameReceivedTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FVP8ComputeShaderFrameReceivedTest::RunTest(const FString& Parameters)
	{
		SetCodec(EPixelStreamingCodec::VP8, true /*bUseComputeShader*/);
		DoFrameReceiveTest();
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVP9FrameReceivedTest, "System.Plugins.PixelStreaming.FVP9FrameReceivedTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FVP9FrameReceivedTest::RunTest(const FString& Parameters)
	{
		SetCodec(EPixelStreamingCodec::VP9, false /*bUseComputeShader*/);
		DoFrameReceiveTest();
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVP9ComputeShaderFrameReceivedTest, "System.Plugins.PixelStreaming.FVP9ComuteShaderFrameReceivedTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FVP9ComputeShaderFrameReceivedTest::RunTest(const FString& Parameters)
	{
		SetCodec(EPixelStreamingCodec::VP9, true /*bUseComputeShader*/);
		DoFrameReceiveTest();
		return true;
	}

	/* Frame resize test */

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVP8FrameResizeTest, "System.Plugins.PixelStreaming.FVP8FrameResizeTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FVP8FrameResizeTest::RunTest(const FString& Parameters)
	{
		SetCodec(EPixelStreamingCodec::VP8, false /*bUseComputeShader*/);
		DoFrameResizeMultipleTimesTest();
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVP8ComputeShaderFrameResizeTest, "System.Plugins.PixelStreaming.FVP8ComputeShaderFrameResizeTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FVP8ComputeShaderFrameResizeTest::RunTest(const FString& Parameters)
	{
		SetCodec(EPixelStreamingCodec::VP8, true /*bUseComputeShader*/);
		DoFrameResizeMultipleTimesTest();
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVP9FrameResizeTest, "System.Plugins.PixelStreaming.FVP9FrameResizeTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FVP9FrameResizeTest::RunTest(const FString& Parameters)
	{
		SetCodec(EPixelStreamingCodec::VP9, false /*bUseComputeShader*/);
		DoFrameResizeMultipleTimesTest();
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVP9ComputeShaderFrameResizeTest, "System.Plugins.PixelStreaming.FVP9ComputeShaderFrameResizeTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	bool FVP9ComputeShaderFrameResizeTest::RunTest(const FString& Parameters)
	{
		SetCodec(EPixelStreamingCodec::VP9, false /*bUseComputeShader*/);
		DoFrameResizeMultipleTimesTest();
		return true;
	}

} // namespace UE::PixelStreaming

#endif // WITH_DEV_AUTOMATION_TESTS