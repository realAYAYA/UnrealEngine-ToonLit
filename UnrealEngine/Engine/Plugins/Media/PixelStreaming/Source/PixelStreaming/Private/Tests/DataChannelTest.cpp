// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "TestUtils.h"
#include "PixelStreamingVideoInputBackBuffer.h"
#include "WebRTCIncludes.h"
#include "IPixelStreamingModule.h"
#include "IPixelStreamingStreamer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::PixelStreaming
{
    IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataChannelEchoTest, "System.Plugins.PixelStreaming.FDataChannelEchoTest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
    bool FDataChannelEchoTest::RunTest(const FString& Parameters)
    {
        // need to be able to accept codec to handshake otherwise setting local description fails when generating an answer
        SetCodec(EPixelStreamingCodec::VP8, false /*bUseComputeShader*/);

        int32 StreamerPort = 7567;
		int32 PlayerPort = 4581;

		TSharedPtr<UE::PixelStreamingServers::IServer> SignallingServer = CreateSignallingServer(StreamerPort, PlayerPort);

		TSharedPtr<IPixelStreamingStreamer> Streamer = CreateStreamer(StreamerPort);
        TSharedPtr<FPixelStreamingVideoInputI420> VideoInput = MakeShared<FPixelStreamingVideoInputI420>();
		Streamer->SetVideoInput(VideoInput);

		TSharedPtr<FMockPlayer> Player = CreatePlayer(FMockPlayer::EMode::AcceptOffers);
        TSharedPtr<FMockVideoSink> VideoSink = MakeShared<FMockVideoSink>();
		Player->VideoSink = VideoSink;

        uint8 ToStreamerEchoId = IPixelStreamingModule::Get().GetProtocol().ToStreamerProtocol.Find("TestEcho")->Id;
        uint8 FromStreamerEchoId = IPixelStreamingModule::Get().GetProtocol().FromStreamerProtocol.Find("TestEcho")->Id;
        FString EchoContent = TEXT("EchoTest");
        // The player sends an "echo" message to the streamer. The streamer then sends this message back to the player and we check that we receive this echo
        TSharedPtr<bool> bComplete = MakeShared<bool>(false);
        TFunction<void(uint8, const webrtc::DataBuffer&)> Callback = [this, bComplete, FromStreamerEchoId, EchoContent](uint8 Type, const webrtc::DataBuffer& RawBuffer) {
            if(Type == FromStreamerEchoId)
            {
                *bComplete.Get() = true;
                const size_t DescriptorSize = (RawBuffer.data.size() - 1) / sizeof(TCHAR);
				const TCHAR* DescPtr = reinterpret_cast<const TCHAR*>(RawBuffer.data.data() + 1);
				const FString Message(DescriptorSize, DescPtr);
                TestTrue(FString::Printf(TEXT("Received echo (%s) != sent echo (%s)."), *Message, *EchoContent), Message == EchoContent);
            }
        };

		Streamer->StartStreaming();

		ADD_LATENT_AUTOMATION_COMMAND(FConnectPlayerAfterStreamerConnectedOrTimeout(5.0, Streamer, Player, PlayerPort))
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForPlayerConnectedOrTimeout(5.0, Player, PlayerPort))
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForICEConnectedOrTimeout(5.0, Player))
        ADD_LATENT_AUTOMATION_COMMAND(FSendDataChannelMessageToStreamer(Player, ToStreamerEchoId, EchoContent))
        ADD_LATENT_AUTOMATION_COMMAND(FWaitForDataChannelMessageOrTimeout(15.0, Player, Callback, bComplete))
		ADD_LATENT_AUTOMATION_COMMAND(FCleanupAll(SignallingServer, Streamer, Player))
        return true;
    }
} // namespace UE::PixelStreaming

#endif // WITH_DEV_AUTOMATION_TESTS