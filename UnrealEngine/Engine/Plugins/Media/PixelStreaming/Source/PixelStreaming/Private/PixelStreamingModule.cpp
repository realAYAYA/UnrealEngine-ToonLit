// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingModule.h"
#include "Streamer.h"
#include "PixelStreamingInputComponent.h"
#include "PixelStreamingDelegates.h"
#include "PixelStreamingSignallingConnection.h"
#include "Settings.h"
#include "PixelStreamingPrivate.h"
#include "AudioSink.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Texture2D.h"
#include "Slate/SceneViewport.h"
#include "Utils.h"
#include "UtilsRender.h"

#if PLATFORM_WINDOWS
	#include "Windows/WindowsHWrapper.h"
#elif PLATFORM_LINUX
	#include "CudaModule.h"
#endif

#if PLATFORM_WINDOWS
	#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
	#include <VersionHelpers.h>
THIRD_PARTY_INCLUDES_END
	#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include "RenderingThread.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RendererInterface.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "GameFramework/GameModeBase.h"
#include "Dom/JsonObject.h"
#include "Misc/App.h"
#include "Misc/MessageDialog.h"
#include "Async/Async.h"
#include "Engine/Engine.h"
#include "VideoEncoderFactory.h"
#include "VideoEncoderFactoryLayered.h"
#include "WebRTCLogging.h"
#include "WebSocketsModule.h"

#if !UE_BUILD_SHIPPING
	#include "DrawDebugHelpers.h"
#endif

#include "PixelStreamingVideoInputBackBuffer.h"
#include "VideoSourceGroup.h"
#include "PixelStreamingPeerConnection.h"
#include "Engine/GameEngine.h"
#include "PixelStreamingApplicationWrapper.h"
#include "PixelStreamingInputHandler.h"
#include "InputHandlers.h"

DEFINE_LOG_CATEGORY(LogPixelStreaming);

IPixelStreamingModule* UE::PixelStreaming::FPixelStreamingModule::PixelStreamingModule = nullptr;

namespace UE::PixelStreaming
{
	typedef Protocol::EPixelStreamingMessageTypes EType;
	/**
	 * IModuleInterface implementation
	 */
	void FPixelStreamingModule::StartupModule()
	{
		// Initialise all settings from command line args etc
		Settings::InitialiseSettings();

		// Pixel Streaming does not make sense without an RHI so we don't run in commandlets without one.
		if (IsRunningCommandlet() && !IsAllowCommandletRendering())
		{
			return;
		}

		if (!FSlateApplication::IsInitialized())
		{
			return;
		}

		const ERHIInterfaceType RHIType = GDynamicRHI ? RHIGetInterfaceType() : ERHIInterfaceType::Hidden;

		IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
		PopulateProtocol();
		
		// only D3D11/D3D12/Vulkan is supported
		if (RHIType == ERHIInterfaceType::D3D11 || RHIType == ERHIInterfaceType::D3D12 || RHIType == ERHIInterfaceType::Vulkan)
		{
			// By calling InitDefaultStreamer post engine init we can use pixel streaming in standalone editor mode
			FCoreDelegates::OnFEngineLoopInitComplete.AddLambda([this]() {
				// Check to see if we can use the Pixel Streaming plugin on this platform.
				// If not then we avoid setting up our delegates to prevent access to the plugin.
				if (!IsPlatformCompatible())
				{
					return;
				}

				if (!ensure(GEngine != nullptr))
				{
					return;
				}

				FApp::SetUnfocusedVolumeMultiplier(1.0f);

				// Allow Pixel Streaming to broadcast to various delegates bound in the application-specific blueprint.
				UPixelStreamingDelegates::CreateInstance();

				// Ensure we have ImageWrapper loaded, used in Freezeframes
				verify(FModuleManager::Get().LoadModule(FName("ImageWrapper")));
				InitDefaultStreamer();
				bModuleReady = true;
				ReadyEvent.Broadcast(*this);
				// We don't want to start immediately streaming in editor
				if (!GIsEditor)
				{
					StartStreaming();
				}
			});
		}
		else
		{
			#if !WITH_DEV_AUTOMATION_TESTS
				UE_LOG(LogPixelStreaming, Warning, TEXT("Only D3D11/D3D12/Vulkan Dynamic RHI is supported. Detected %s"), GDynamicRHI != nullptr ? GDynamicRHI->GetName() : TEXT("[null]"));
			#endif
		}

		rtc::InitializeSSL();
		RedirectWebRtcLogsToUnreal(rtc::LoggingSeverity::LS_VERBOSE);
		FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("AVEncoder"));
		FModuleManager::LoadModuleChecked<FWebSocketsModule>("WebSockets");

		// ExternalVideoSourceGroup is used so that we can have a video source without a streamer
		ExternalVideoSourceGroup = FVideoSourceGroup::Create();
		// ExternalVideoSourceGroup->SetVideoInput(FPixelStreamingVideoInputBackBuffer::Create());
		// ExternalVideoSourceGroup->Start();

		bStartupCompleted = true;
	}

	void FPixelStreamingModule::ShutdownModule()
	{
		if (!bStartupCompleted)
		{
			return;
		}

		// We explicitly call release on streamer so WebRTC gets shutdown before our module is deleted
		Streamers.Empty();
		ExternalVideoSourceGroup->Stop();

		FPixelStreamingPeerConnection::Shutdown();

		rtc::CleanupSSL();
		IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);

		bStartupCompleted = false;
	}

	/**
	 * End IModuleInterface implementation
	 */

	/**
	 * IPixelStreamingModule implementation
	 */
	IPixelStreamingModule* FPixelStreamingModule::GetModule()
	{
		if (PixelStreamingModule)
		{
			return PixelStreamingModule;
		}
		IPixelStreamingModule* Module = FModuleManager::Get().LoadModulePtr<IPixelStreamingModule>("PixelStreaming");
		if (Module)
		{
			PixelStreamingModule = Module;
		}
		return PixelStreamingModule;
	}

	void FPixelStreamingModule::SetCodec(EPixelStreamingCodec Codec)
	{
		Settings::SetCodec(Codec);
	}

	EPixelStreamingCodec FPixelStreamingModule::GetCodec() const
	{
		return Settings::GetSelectedCodec();
	}

	IPixelStreamingModule::FReadyEvent& FPixelStreamingModule::OnReady()
	{
		return ReadyEvent;
	}

	bool FPixelStreamingModule::IsReady()
	{
		return bModuleReady;
	}

	bool FPixelStreamingModule::StartStreaming()
	{
		bool bSuccess = true;
		ForEachStreamer([&bSuccess](TSharedPtr<IPixelStreamingStreamer> Streamer)
		{
			if (Streamer.IsValid())
			{
				Streamer->StartStreaming();
				bSuccess &= true;
			}
			else
			{
				bSuccess = false;
			}
		});
		return bSuccess;
	}

	void FPixelStreamingModule::StopStreaming()
	{
		ForEachStreamer([this](TSharedPtr<IPixelStreamingStreamer> Streamer)
		{
			if (Streamer.IsValid())
			{
				Streamer->StopStreaming();
			}
		});
	}

	TSharedPtr<IPixelStreamingStreamer> FPixelStreamingModule::CreateStreamer(const FString& StreamerId)
	{
		TSharedPtr<IPixelStreamingStreamer> ExistingStreamer = GetStreamer(StreamerId);
		if (ExistingStreamer)
		{
			return ExistingStreamer;
		}

		TSharedPtr<FStreamer> NewStreamer = FStreamer::Create(StreamerId);
		{
			FScopeLock Lock(&StreamersCS);
			Streamers.Add(StreamerId, NewStreamer);
		}

		return NewStreamer;
	}

	TArray<FString> FPixelStreamingModule::GetStreamerIds()
	{
		TArray<FString> StreamerKeys;
		FScopeLock Lock(&StreamersCS);
		Streamers.GenerateKeyArray(StreamerKeys);
		return StreamerKeys;
	}

	TSharedPtr<IPixelStreamingStreamer> FPixelStreamingModule::GetStreamer(const FString& StreamerId)
	{
		FScopeLock Lock(&StreamersCS);
		if (Streamers.Contains(StreamerId))
		{
			return Streamers[StreamerId];
		}
		return nullptr;
	}

	TSharedPtr<IPixelStreamingStreamer> FPixelStreamingModule::DeleteStreamer(const FString& StreamerId)
	{
		TSharedPtr<IPixelStreamingStreamer> ToBeDeleted;
		FScopeLock Lock(&StreamersCS);
		if (Streamers.Contains(StreamerId))
		{
			ToBeDeleted = Streamers[StreamerId];
			Streamers.Remove(StreamerId);
		}
		return ToBeDeleted;
	}

	void FPixelStreamingModule::SetExternalVideoSourceFPS(uint32 InFPS)
	{
		ExternalVideoSourceGroup->SetFPS(InFPS);
	}

	rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> FPixelStreamingModule::CreateExternalVideoSource()
	{
		return ExternalVideoSourceGroup->CreateVideoSource([]() { return true; });
	}

	void FPixelStreamingModule::ReleaseExternalVideoSource(const webrtc::VideoTrackSourceInterface* InVideoSource)
	{
		ExternalVideoSourceGroup->RemoveVideoSource(InVideoSource);
	}

	void FPixelStreamingModule::AddInputComponent(UPixelStreamingInput* InInputComponent)
	{
		InputComponents.Add(InInputComponent);
	}

	void FPixelStreamingModule::RemoveInputComponent(UPixelStreamingInput* InInputComponent)
	{
		InputComponents.Remove(InInputComponent);
	}

	const TArray<UPixelStreamingInput*> FPixelStreamingModule::GetInputComponents()
	{
		return InputComponents;
	}

	TUniquePtr<webrtc::VideoEncoderFactory> FPixelStreamingModule::CreateVideoEncoderFactory()
	{
		return MakeUnique<FVideoEncoderFactoryLayered>();
	}

	FString FPixelStreamingModule::GetDefaultStreamerID()
	{
		return Settings::GetDefaultStreamerID();
	}

	FString FPixelStreamingModule::GetDefaultSignallingURL()
	{
		return Settings::GetDefaultSignallingURL();
	}

	void FPixelStreamingModule::ForEachStreamer(const TFunction<void(TSharedPtr<IPixelStreamingStreamer>)>& Func)
	{
		TSet<FString> KeySet;
		{
			FScopeLock Lock(&StreamersCS);
			Streamers.GetKeys(KeySet);
		}
		for (auto&& StreamerId : KeySet)
		{
			if (TSharedPtr<IPixelStreamingStreamer> Streamer = GetStreamer(StreamerId))
			{
				Func(Streamer);
			}
		}
	}

	const Protocol::FPixelStreamingProtocol& FPixelStreamingModule::GetProtocol()
	{
		return MessageProtocol;
	}

	void FPixelStreamingModule::RegisterMessage(Protocol::EPixelStreamingMessageDirection MessageDirection, const FString& MessageType, Protocol::FPixelStreamingInputMessage Message, const TFunction<void(FMemoryReader)>& Handler)
	{
		if(MessageDirection == Protocol::EPixelStreamingMessageDirection::ToStreamer)
		{
			MessageProtocol.ToStreamerProtocol.Add(MessageType, Message);
			ForEachStreamer([&Handler = Handler, &MessageType = MessageType](TSharedPtr<IPixelStreamingStreamer> Streamer)
			{
				TWeakPtr<IPixelStreamingInputHandler> WeakInputHandler = Streamer->GetInputHandler();
				TSharedPtr<IPixelStreamingInputHandler> InputHandler = WeakInputHandler.Pin();
				if (InputHandler)
				{
					InputHandler->RegisterMessageHandler(MessageType, Handler);
				}
			});
		}
		else if (MessageDirection == Protocol::EPixelStreamingMessageDirection::FromStreamer)
		{
			MessageProtocol.FromStreamerProtocol.Add(MessageType, Message);
		}
		OnProtocolUpdated.Broadcast();
	}

	TFunction<void(FMemoryReader)> FPixelStreamingModule::FindMessageHandler(const FString& MessageType)
	{
		// All streamers have the same protocol so we just use the first streamers input channel to get the message handler
		TSharedPtr<IPixelStreamingStreamer> Streamer = Streamers.begin()->Value;
		TWeakPtr<IPixelStreamingInputHandler> WeakInputHandler = Streamer->GetInputHandler();
		if (TSharedPtr<IPixelStreamingInputHandler> InputHandler = WeakInputHandler.Pin())
		{
			return InputHandler->FindMessageHandler(MessageType);
		}
		// If the channel doesn't exist, just return an empty function
		return ([](FMemoryReader Ar) {});
	}

	/**
	 * End IPixelStreamingModule implementation
	 */

	void FPixelStreamingModule::InitDefaultStreamer()
	{
		UE_LOG(LogPixelStreaming, Log, TEXT("PixelStreaming streamer ID: %s"), *Settings::GetDefaultStreamerID());

		FString SignallingServerURL;
		if (!Settings::GetSignallingServerUrl(SignallingServerURL))
		{
			// didnt get the startup URL for pixel streaming. Check deprecated options...
			FString SignallingServerIP;
			uint16 SignallingServerPort;
			if (Settings::GetSignallingServerIP(SignallingServerIP) && Settings::GetSignallingServerPort(SignallingServerPort))
			{
				// got both old parameters. Warn about deprecation and build the proper url.
				UE_LOG(LogPixelStreaming, Warning, TEXT("PixelStreamingIP and PixelStreamingPort are deprecated flags. Use PixelStreamingURL instead. eg. -PixelStreamingURL=ws://%s:%d"), *SignallingServerIP, SignallingServerPort);
				SignallingServerURL = FString::Printf(TEXT("ws://%s:%d"), *SignallingServerIP, SignallingServerPort);
			}
		}

		TSharedPtr<IPixelStreamingStreamer> Streamer = CreateStreamer(Settings::GetDefaultStreamerID());
		// The PixelStreamingEditorModule handles setting video input in the editor
		if (!GIsEditor)
		{
			// default to the scene viewport if we have a game engine
			if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
			{
				TSharedPtr<FSceneViewport> TargetViewport = GameEngine->SceneViewport;
				if (TargetViewport.IsValid())
				{
					Streamer->SetTargetViewport(TargetViewport->GetViewportWidget());
					Streamer->SetTargetWindow(TargetViewport->FindWindow());
				}
				else
				{
					UE_LOG(LogPixelStreaming, Error, TEXT("Cannot set target viewport/window - target viewport is not valid."));
				}
			}
		}

		if (!SignallingServerURL.IsEmpty())
		{
			// The user has specified a URL on the command line meaning their intention is to start streaming immediately
			// in that case, set up the video input for them (as long as we're not in editor)
			if(!GIsEditor)
			{
				Streamer->SetVideoInput(FPixelStreamingVideoInputBackBuffer::Create());
			}
			Streamer->SetSignallingServerURL(SignallingServerURL);
		}
	}

	bool FPixelStreamingModule::IsPlatformCompatible() const
	{
		bool bCompatible = true;

#if PLATFORM_WINDOWS
		bool bWin8OrHigher = IsWindows8OrGreater();
		if (!bWin8OrHigher)
		{
			FString ErrorString(TEXT("Failed to initialize Pixel Streaming plugin because minimum requirement is Windows 8"));
			FText ErrorText = FText::FromString(ErrorString);
			FText TitleText = FText::FromString(TEXT("Pixel Streaming Plugin"));
			FMessageDialog::Open(EAppMsgType::Ok, ErrorText, &TitleText);
			UE_LOG(LogPixelStreaming, Error, TEXT("%s"), *ErrorString);
			bCompatible = false;
		}
#endif

		if (Settings::CVarPixelStreamingEncoderCodec.GetValueOnAnyThread() == "H264"
			&& !AVEncoder::FVideoEncoderFactory::Get().HasEncoderForCodec(AVEncoder::ECodecType::H264))
		{
			UE_LOG(LogPixelStreaming, Warning, TEXT("Could not setup hardware encoder for H.264. This is usually a driver issue, try reinstalling your drivers."));
			UE_LOG(LogPixelStreaming, Warning, TEXT("Falling back to VP8 software video encoding."));
			Settings::CVarPixelStreamingEncoderCodec.AsVariable()->Set(TEXT("VP8"), ECVF_SetByCommandline);
		}

		return bCompatible;
	}
	/**
	 * End own methods
	 */

	TSharedPtr<IInputDevice> FPixelStreamingModule::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler)
	{
		return MakeShared<FInputHandlers>(InMessageHandler);
	}

	void FPixelStreamingModule::PopulateProtocol()
	{
		using namespace Protocol;

		// Old EToStreamerMsg Commands
		/*
		 * Control Messages.
		 */
		// Simple command with no payload
		// Note, we only specify the ID when creating these messages to preserve backwards compatability
		// when adding your own message type, you can simply do MessageProtocol.Direction.Add("XXX");
		MessageProtocol.ToStreamerProtocol.Add("IFrameRequest", FPixelStreamingInputMessage(0, 0, {}));
		MessageProtocol.ToStreamerProtocol.Add("RequestQualityControl", FPixelStreamingInputMessage(1, 0, {}));
		MessageProtocol.ToStreamerProtocol.Add("FpsRequest", FPixelStreamingInputMessage(2, 0, {}));
		MessageProtocol.ToStreamerProtocol.Add("AverageBitrateRequest", FPixelStreamingInputMessage(3, 0, {}));
		MessageProtocol.ToStreamerProtocol.Add("StartStreaming", FPixelStreamingInputMessage(4, 0, {}));
		MessageProtocol.ToStreamerProtocol.Add("StopStreaming", FPixelStreamingInputMessage(5, 0, {}));
		MessageProtocol.ToStreamerProtocol.Add("LatencyTest", FPixelStreamingInputMessage(6, 0, {}));
		MessageProtocol.ToStreamerProtocol.Add("RequestInitialSettings", FPixelStreamingInputMessage(7, 0, {}));
		MessageProtocol.ToStreamerProtocol.Add("TestEcho", FPixelStreamingInputMessage(8, 0, {}));

		/*
		 * Input Messages.
		 */
		// Generic Input Messages.
		MessageProtocol.ToStreamerProtocol.Add("UIInteraction", FPixelStreamingInputMessage(50, 0, {}));
		MessageProtocol.ToStreamerProtocol.Add("Command", FPixelStreamingInputMessage(51, 0, {}));

		// Keyboard Input Message.
		// Complex command with payload, therefore we specify the length of the payload (bytes) as well as the structure of the payload
		MessageProtocol.ToStreamerProtocol.Add("KeyDown", FPixelStreamingInputMessage(60, 2, { EType::Uint8, EType::Uint8 }));
		MessageProtocol.ToStreamerProtocol.Add("KeyUp", FPixelStreamingInputMessage(61, 1, { EType::Uint8 }));
		MessageProtocol.ToStreamerProtocol.Add("KeyPress", FPixelStreamingInputMessage(62, 2, { EType::Uint16 }));

		// Mouse Input Messages.
		MessageProtocol.ToStreamerProtocol.Add("MouseEnter", FPixelStreamingInputMessage(70));
		MessageProtocol.ToStreamerProtocol.Add("MouseLeave", FPixelStreamingInputMessage(71));
		MessageProtocol.ToStreamerProtocol.Add("MouseDown", FPixelStreamingInputMessage(72, 5, { EType::Uint8, EType::Uint16, EType::Uint16 }));
		MessageProtocol.ToStreamerProtocol.Add("MouseUp", FPixelStreamingInputMessage(73, 5, { EType::Uint8, EType::Uint16, EType::Uint16 }));
		MessageProtocol.ToStreamerProtocol.Add("MouseMove", FPixelStreamingInputMessage(74, 8, { EType::Uint16, EType::Uint16, EType::Uint16, EType::Uint16 }));
		MessageProtocol.ToStreamerProtocol.Add("MouseWheel", FPixelStreamingInputMessage(75, 6, { EType::Int16, EType::Uint16, EType::Uint16 }));
		MessageProtocol.ToStreamerProtocol.Add("MouseDouble", FPixelStreamingInputMessage(76, 5, { EType::Uint8, EType::Uint16, EType::Uint16 }));

		// Touch Input Messages.
		MessageProtocol.ToStreamerProtocol.Add("TouchStart", FPixelStreamingInputMessage(80, 8, { EType::Uint8, EType::Uint16, EType::Uint16, EType::Uint8, EType::Uint8, EType::Uint8}));
		MessageProtocol.ToStreamerProtocol.Add("TouchEnd", FPixelStreamingInputMessage(81, 8, { EType::Uint8, EType::Uint16, EType::Uint16, EType::Uint8, EType::Uint8, EType::Uint8}));
		MessageProtocol.ToStreamerProtocol.Add("TouchMove", FPixelStreamingInputMessage(82, 8, { EType::Uint8, EType::Uint16, EType::Uint16, EType::Uint8, EType::Uint8, EType::Uint8}));

		// Gamepad Input Messages.
		MessageProtocol.ToStreamerProtocol.Add("GamepadButtonPressed", FPixelStreamingInputMessage(90, 3, { EType::Uint8, EType::Uint8, EType::Uint8 }));
		MessageProtocol.ToStreamerProtocol.Add("GamepadButtonReleased", FPixelStreamingInputMessage(91, 3, { EType::Uint8, EType::Uint8, EType::Uint8 }));
		MessageProtocol.ToStreamerProtocol.Add("GamepadAnalog", FPixelStreamingInputMessage(92, 3, { EType::Uint8, EType::Uint8, EType::Double }));

		// Old EToPlayerMsg commands
		MessageProtocol.FromStreamerProtocol.Add("QualityControlOwnership", FPixelStreamingInputMessage(0));
		MessageProtocol.FromStreamerProtocol.Add("Response", FPixelStreamingInputMessage(1));
		MessageProtocol.FromStreamerProtocol.Add("Command", FPixelStreamingInputMessage(2));
		MessageProtocol.FromStreamerProtocol.Add("FreezeFrame", FPixelStreamingInputMessage(3));
		MessageProtocol.FromStreamerProtocol.Add("UnfreezeFrame", FPixelStreamingInputMessage(4));
		MessageProtocol.FromStreamerProtocol.Add("VideoEncoderAvgQP", FPixelStreamingInputMessage(5));
		MessageProtocol.FromStreamerProtocol.Add("LatencyTest", FPixelStreamingInputMessage(6));
		MessageProtocol.FromStreamerProtocol.Add("InitialSettings", FPixelStreamingInputMessage(7));
		MessageProtocol.FromStreamerProtocol.Add("FileExtension", FPixelStreamingInputMessage(8));
		MessageProtocol.FromStreamerProtocol.Add("FileMimeType", FPixelStreamingInputMessage(9));
		MessageProtocol.FromStreamerProtocol.Add("FileContents", FPixelStreamingInputMessage(10));
		MessageProtocol.FromStreamerProtocol.Add("TestEcho", FPixelStreamingInputMessage(11));
		MessageProtocol.FromStreamerProtocol.Add("InputControlOwnership", FPixelStreamingInputMessage(12));
		MessageProtocol.FromStreamerProtocol.Add("Protocol", FPixelStreamingInputMessage(255));
	}
} // namespace UE::PixelStreaming

IMPLEMENT_MODULE(UE::PixelStreaming::FPixelStreamingModule, PixelStreaming)
