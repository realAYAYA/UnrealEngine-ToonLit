// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "PixelStreamingPrivate.h"
#include "Misc/DefaultValueHelper.h"
#include "Async/Async.h"
#include "IPixelStreamingModule.h"
#include "PixelStreamingDelegates.h"
#include "Utils.h"

namespace UE::PixelStreaming::Settings
{
	// Begin Encoder CVars
	TAutoConsoleVariable<int32> CVarPixelStreamingEncoderTargetBitrate(
		TEXT("PixelStreaming.Encoder.TargetBitrate"),
		-1,
		TEXT("Target bitrate (bps). Ignore the bitrate WebRTC wants (not recommended). Set to -1 to disable. Default -1."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarPixelStreamingEncoderMaxBitrate(
		TEXT("PixelStreaming.Encoder.MaxBitrateVBR"),
		20000000,
		TEXT("Max bitrate (bps). Does not work in CBR rate control mode with NVENC."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<bool> CVarPixelStreamingDebugDumpFrame(
		TEXT("PixelStreaming.Encoder.DumpDebugFrames"),
		false,
		TEXT("Dumps frames from the encoder to a file on disk for debugging purposes."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarPixelStreamingEncoderMinQP(
		TEXT("PixelStreaming.Encoder.MinQP"),
		0,
		TEXT("0-51, lower values result in better quality but higher bitrate. Default 0 - i.e. no limit on a minimum QP."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarPixelStreamingEncoderMaxQP(
		TEXT("PixelStreaming.Encoder.MaxQP"),
		51,
		TEXT("0-51, lower values result in better quality but higher bitrate. Default 51 - i.e. no limit on a maximum QP."),
		ECVF_Default);

	TAutoConsoleVariable<FString> CVarPixelStreamingEncoderRateControl(
		TEXT("PixelStreaming.Encoder.RateControl"),
		TEXT("CBR"),
		TEXT("PixelStreaming video encoder RateControl mode. Supported modes are `ConstQP`, `VBR`, `CBR`. Default: CBR, which we recommend."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarPixelStreamingEnableFillerData(
		TEXT("PixelStreaming.Encoder.EnableFillerData"),
		false,
		TEXT("Maintains constant bitrate by filling with junk data. Note: Should not be required with CBR and MinQP = -1. Default: false."),
		ECVF_Default);

	TAutoConsoleVariable<FString> CVarPixelStreamingEncoderMultipass(
		TEXT("PixelStreaming.Encoder.Multipass"),
		TEXT("FULL"),
		TEXT("PixelStreaming encoder multipass. Supported modes are `DISABLED`, `QUARTER`, `FULL`"),
		ECVF_Default);

	TAutoConsoleVariable<FString> CVarPixelStreamingH264Profile(
		TEXT("PixelStreaming.Encoder.H264Profile"),
		TEXT("BASELINE"),
		TEXT("PixelStreaming encoder profile. Supported modes are `AUTO`, `BASELINE`, `MAIN`, `HIGH`, `HIGH444`, `STEREO`, `SVC_TEMPORAL_SCALABILITY`, `PROGRESSIVE_HIGH`, `CONSTRAINED_HIGH`"),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarPixelStreamingEncoderKeyframeInterval(
		TEXT("PixelStreaming.Encoder.KeyframeInterval"),
		300,
		TEXT("How many frames before a key frame is sent. Default: 300. Values <=0 will disable sending of periodic key frames. Note: NVENC does not support changing this after encoding has started."),
		ECVF_Default);

	TAutoConsoleVariable<FString> CVarPixelStreamingEncoderCodec(
		TEXT("PixelStreaming.Encoder.Codec"),
		TEXT("H264"),
		TEXT("PixelStreaming encoder codec. Supported values are `H264`, `VP8`, `VP9`"),
		ECVF_Default);
	// End Encoder CVars

	// Begin WebRTC CVars
	TAutoConsoleVariable<FString> CVarPixelStreamingDegradationPreference(
		TEXT("PixelStreaming.WebRTC.DegradationPreference"),
		TEXT("MAINTAIN_FRAMERATE"),
		TEXT("PixelStreaming degradation preference. Supported modes are `BALANCED`, `MAINTAIN_FRAMERATE`, `MAINTAIN_RESOLUTION`"),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarPixelStreamingWebRTCFps(
		TEXT("PixelStreaming.WebRTC.Fps"),
		60,
		TEXT("Framerate for WebRTC encoding. Default: 60"),
		ECVF_Default);

	// Note: 1 megabit is the maximum allowed in WebRTC for a start bitrate.
	TAutoConsoleVariable<int32> CVarPixelStreamingWebRTCStartBitrate(
		TEXT("PixelStreaming.WebRTC.StartBitrate"),
		1000000,
		TEXT("Start bitrate (bps) that WebRTC will try begin the stream with. Must be between Min/Max bitrates. Default: 1000000"),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarPixelStreamingWebRTCMinBitrate(
		TEXT("PixelStreaming.WebRTC.MinBitrate"),
		100000,
		TEXT("Min bitrate (bps) that WebRTC will not request below. Careful not to set too high otherwise WebRTC will just drop frames. Default: 100000"),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarPixelStreamingWebRTCMaxBitrate(
		TEXT("PixelStreaming.WebRTC.MaxBitrate"),
		100000000,
		TEXT("Max bitrate (bps) that WebRTC will not request above. Default: 100000000 aka 100 megabits/per second."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int> CVarPixelStreamingWebRTCLowQpThreshold(
		TEXT("PixelStreaming.WebRTC.LowQpThreshold"),
		25,
		TEXT("Only useful when MinQP=-1. Value between 1-51 (default: 25). If WebRTC is getting frames below this QP it will try to increase resolution when not in MAINTAIN_RESOLUTION mode."),
		ECVF_Default);

	TAutoConsoleVariable<int> CVarPixelStreamingWebRTCHighQpThreshold(
		TEXT("PixelStreaming.WebRTC.HighQpThreshold"),
		37,
		TEXT("Only useful when MinQP=-1. Value between 1-51 (default: 37). If WebRTC is getting frames above this QP it will decrease resolution when not in MAINTAIN_RESOLUTION mode."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarPixelStreamingWebRTCDisableReceiveAudio(
		TEXT("PixelStreaming.WebRTC.DisableReceiveAudio"),
		false,
		TEXT("Disables receiving audio from the browser into UE."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarPixelStreamingWebRTCDisableTransmitAudio(
		TEXT("PixelStreaming.WebRTC.DisableTransmitAudio"),
		false,
		TEXT("Disables transmission of UE audio to the browser."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarPixelStreamingWebRTCDisableAudioSync(
		TEXT("PixelStreaming.WebRTC.DisableAudioSync"),
		true,
		TEXT("Disables the synchronization of audio and video tracks in WebRTC. This can be useful in low latency usecases where synchronization is not required."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarPixelStreamingWebRTCUseLegacyAudioDevice(
		TEXT("PixelStreaming.WebRTC.UseLegacyAudioDevice"),
		false,
		TEXT("Whether put audio and video in the same stream (which will make WebRTC try to sync them)."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarPixelStreamingWebRTCDisableStats(
		TEXT("PixelStreaming.WebRTC.DisableStats"),
		false,
		TEXT("Disables the collection of WebRTC stats."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarPixelStreamingWebRTCAudioGain(
		TEXT("PixelStreaming.WebRTC.AudioGain"),
		1.0f,
		TEXT("Sets the amount of gain to apply to audio. Default: 1.0"),
		ECVF_Default);

	// End WebRTC CVars

	// Begin Pixel Streaming Plugin CVars

	TAutoConsoleVariable<bool> CVarPixelStreamingOnScreenStats(
		TEXT("PixelStreaming.HUDStats"),
		false,
		TEXT("Whether to show PixelStreaming stats on the in-game HUD (default: true)."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarPixelStreamingLogStats(
		TEXT("PixelStreaming.LogStats"),
		false,
		TEXT("Whether to show PixelStreaming stats in the log (default: false)."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarPixelStreamingFreezeFrameQuality(
		TEXT("PixelStreaming.FreezeFrameQuality"),
		100,
		TEXT("Compression quality of the freeze frame"),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarSendPlayerIdAsInteger(
		TEXT("PixelStreaming.SendPlayerIdAsInteger"),
		true,
		TEXT("If true transmit the player id as an integer (for backward compatibility) or as a string."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarPixelStreamingDisableLatencyTester(
		TEXT("PixelStreaming.DisableLatencyTester"),
		false,
		TEXT("If true disables latency tester being triggerable."),
		ECVF_Default);

	TAutoConsoleVariable<FString> CVarPixelStreamingKeyFilter(
		TEXT("PixelStreaming.KeyFilter"),
		"",
		TEXT("Comma separated list of keys to ignore from streaming clients."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarPixelStreamingAllowConsoleCommands(
		TEXT("PixelStreaming.AllowPixelStreamingCommands"),
		false,
		TEXT("If true browser can send consoleCommand payloads that execute in UE's console."),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarPixelStreamingVPXUseCompute(
		TEXT("PixelStreaming.VPXUseCompute"),
		false,
		TEXT("If true a compute shader will be used to process RGB -> I420. Otherwise libyuv will be used on the CPU."),
		ECVF_Default);

	TAutoConsoleVariable<FString> CVarPixelStreamingVideoTracks(
		TEXT("PixelStreaming.VideoTracks"),
		TEXT("Backbuffer"),
		TEXT("Comma separated list of video track types to create when a peer joins. Default: Backbuffer"),
		ECVF_Default);

	TAutoConsoleVariable<FString> CVarPixelStreamingInputController(
		TEXT("PixelStreaming.InputController"),
		TEXT("Any"),
		TEXT("Various modes of input control supported by Pixel Streaming, currently: \"Any\"  or \"Host\". Default: Any"),
		ECVF_Default);

	TAutoConsoleVariable<bool> CVarPixelStreamingSuppressICECandidateErrors(
		TEXT("PixelStreaming.SuppressICECandidateErrors"),
		false,
		TEXT("Silences ice candidate errors from the peer connection. This is used for testing and silencing a specific error that is expected but the number of errors is non-deterministic."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarPixelStreamingSignalingReconnectInterval(
		TEXT("PixelStreaming.SignalingReconnectInterval"),
		2.0f,
		TEXT("Changes the number of seconds between attempted reconnects to the signaling server. This is useful for reducing the log spam produced from attempted reconnects. A value <= 0 results in an immediate reconnect. Default: 2.0s"),
		ECVF_Default);

	TArray<FKey> FilteredKeys;
	FString DefaultStreamerID = TEXT("DefaultStreamer");
	FString DefaultSignallingURL = TEXT("ws://127.0.0.1:8888");

	void OnFilteredKeysChanged(IConsoleVariable* Var)
	{
		FString CommaList = Var->GetString();
		TArray<FString> KeyStringArray;
		CommaList.ParseIntoArray(KeyStringArray, TEXT(","), true);
		FilteredKeys.Empty();
		for (auto&& KeyString : KeyStringArray)
		{
			FilteredKeys.Add(FKey(*KeyString));
		}
	}

	void OnHudStatsToggled(IConsoleVariable* Var)
	{
		bool bHudStatsEnabled = Var->GetBool();

		if (!GEngine)
		{
			return;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (WorldContext.WorldType == EWorldType::Game || WorldContext.WorldType == EWorldType::PIE)
			{
				UWorld* World = WorldContext.World();
				UGameViewportClient* ViewportClient = World->GetGameViewport();
				GEngine->SetEngineStat(World, ViewportClient, TEXT("PixelStreaming"), bHudStatsEnabled);
			}
		}
	}

	void OnKeyframeIntervalChanged(IConsoleVariable* Var)
	{
		AsyncTask(ENamedThreads::GameThread, [Var]() {
			IConsoleVariable* CVarNVENCKeyframeInterval = IConsoleManager::Get().FindConsoleVariable(TEXT("NVENC.KeyframeInterval"));
			if (CVarNVENCKeyframeInterval)
			{
				CVarNVENCKeyframeInterval->Set(Var->GetInt(), ECVF_SetByCommandline);
			}

			IConsoleVariable* CVarAMFKeyframeInterval = IConsoleManager::Get().FindConsoleVariable(TEXT("AMF.KeyframeInterval"));
			if (CVarNVENCKeyframeInterval)
			{
				CVarAMFKeyframeInterval->Set(Var->GetInt(), ECVF_SetByCommandline);
			}
		});
	}

	void OnWebRTCFpsChanged(IConsoleVariable* Var)
	{
		IPixelStreamingModule::Get().ForEachStreamer([Var](TSharedPtr<IPixelStreamingStreamer> Streamer) {
			Streamer->SetStreamFPS(Var->GetInt());
		});
	}
	// Ends Pixel Streaming Plugin CVars

	// Begin TextureSource CVars
	TAutoConsoleVariable<float> CVarPixelStreamingFrameScale(
		TEXT("PixelStreaming.FrameScale"),
		1.0,
		TEXT("The texture source frame scale. Default: 1.0"),
		ECVF_Default);
	// End TextureSource CVars

	// Begin utility functions etc.
	std::map<FString, AVEncoder::FVideoEncoder::RateControlMode> const RateControlCVarMap{
		{ "ConstQP", AVEncoder::FVideoEncoder::RateControlMode::CONSTQP },
		{ "VBR", AVEncoder::FVideoEncoder::RateControlMode::VBR },
		{ "CBR", AVEncoder::FVideoEncoder::RateControlMode::CBR },
	};

	std::map<FString, AVEncoder::FVideoEncoder::MultipassMode> const MultipassCVarMap{
		{ "DISABLED", AVEncoder::FVideoEncoder::MultipassMode::DISABLED },
		{ "QUARTER", AVEncoder::FVideoEncoder::MultipassMode::QUARTER },
		{ "FULL", AVEncoder::FVideoEncoder::MultipassMode::FULL },
	};

	std::map<FString, AVEncoder::FVideoEncoder::H264Profile> const H264ProfileMap{
		{ "AUTO", AVEncoder::FVideoEncoder::H264Profile::AUTO },
		{ "BASELINE", AVEncoder::FVideoEncoder::H264Profile::BASELINE },
		{ "MAIN", AVEncoder::FVideoEncoder::H264Profile::MAIN },
		{ "HIGH", AVEncoder::FVideoEncoder::H264Profile::HIGH },
		{ "HIGH444", AVEncoder::FVideoEncoder::H264Profile::HIGH444 },
		{ "STEREO", AVEncoder::FVideoEncoder::H264Profile::STEREO },
		{ "SVC_TEMPORAL_SCALABILITY", AVEncoder::FVideoEncoder::H264Profile::SVC_TEMPORAL_SCALABILITY },
		{ "PROGRESSIVE_HIGH", AVEncoder::FVideoEncoder::H264Profile::PROGRESSIVE_HIGH },
		{ "CONSTRAINED_HIGH", AVEncoder::FVideoEncoder::H264Profile::CONSTRAINED_HIGH },
	};

	AVEncoder::FVideoEncoder::RateControlMode GetRateControlCVar()
	{
		const FString EncoderRateControl = CVarPixelStreamingEncoderRateControl.GetValueOnAnyThread();
		auto const Iter = RateControlCVarMap.find(EncoderRateControl);
		if (Iter == std::end(RateControlCVarMap))
			return AVEncoder::FVideoEncoder::RateControlMode::CBR;
		return Iter->second;
	}

	AVEncoder::FVideoEncoder::MultipassMode GetMultipassCVar()
	{
		const FString EncoderMultipass = CVarPixelStreamingEncoderMultipass.GetValueOnAnyThread();
		auto const Iter = MultipassCVarMap.find(EncoderMultipass);
		if (Iter == std::end(MultipassCVarMap))
			return AVEncoder::FVideoEncoder::MultipassMode::FULL;
		return Iter->second;
	}

	webrtc::DegradationPreference GetDegradationPreference()
	{
		FString DegradationPreference = CVarPixelStreamingDegradationPreference.GetValueOnAnyThread();
		if (DegradationPreference == "MAINTAIN_FRAMERATE")
		{
			return webrtc::DegradationPreference::MAINTAIN_FRAMERATE;
		}
		else if (DegradationPreference == "MAINTAIN_RESOLUTION")
		{
			return webrtc::DegradationPreference::MAINTAIN_RESOLUTION;
		}
		// Everything else, return balanced.
		return webrtc::DegradationPreference::BALANCED;
	}

	AVEncoder::FVideoEncoder::H264Profile GetH264Profile()
	{
		const FString H264Profile = CVarPixelStreamingH264Profile.GetValueOnAnyThread();
		auto const Iter = H264ProfileMap.find(H264Profile);
		if (Iter == std::end(H264ProfileMap))
			return AVEncoder::FVideoEncoder::H264Profile::BASELINE;
		return Iter->second;
	}
	// End utility functions etc.

	FSimulcastParameters SimulcastParameters;

	void ReadSimulcastParameters()
	{
		SimulcastParameters.Layers.Empty();

		FString StringOptions;
		bool bPassedSimulcastParams = FParse::Value(FCommandLine::Get(), TEXT("SimulcastParameters="), StringOptions, false);

		// If no simulcast parameters are passed use some default values
		if (!bPassedSimulcastParams)
		{
			// StringOptions = FString(TEXT("1.0,5000000,20000000,2.0,1000000,5000000,4.0,50000,1000000"));
			StringOptions = FString(TEXT("1.0,5000000,100000000,2.0,1000000,5000000"));
		}

		TArray<FString> ParameterArray;
		StringOptions.ParseIntoArray(ParameterArray, TEXT(","), true);
		const int OptionCount = ParameterArray.Num();
		bool bSuccess = OptionCount % 3 == 0;
		int NextOption = 0;
		while (bSuccess && ((OptionCount - NextOption) >= 3))
		{
			FSimulcastParameters::FLayer Layer;
			bSuccess = FDefaultValueHelper::ParseFloat(ParameterArray[NextOption++], Layer.Scaling);
			bSuccess = FDefaultValueHelper::ParseInt(ParameterArray[NextOption++], Layer.MinBitrate);
			bSuccess = FDefaultValueHelper::ParseInt(ParameterArray[NextOption++], Layer.MaxBitrate);
			SimulcastParameters.Layers.Add(Layer);
		}

		if (!bSuccess)
		{
			// failed parsing the parameters. just ignore the parameters.
			UE_LOG(LogPixelStreaming, Error, TEXT("Simulcast parameters malformed. Expected [Scaling_0, MinBitrate_0, MaxBitrate_0, ... , Scaling_N, MinBitrate_N, MaxBitrate_N] as [float, int, int, ... , float, int, int] etc.]"));
			SimulcastParameters.Layers.Empty();
		}
	}

	void SetCodec(EPixelStreamingCodec Codec)
	{
		switch (Codec)
		{
			default:
			case EPixelStreamingCodec::H264:
				CVarPixelStreamingEncoderCodec.AsVariable()->Set(TEXT("H264"));
				break;
			case EPixelStreamingCodec::VP8:
				CVarPixelStreamingEncoderCodec.AsVariable()->Set(TEXT("VP8"));
				break;
			case EPixelStreamingCodec::VP9:
				CVarPixelStreamingEncoderCodec.AsVariable()->Set(TEXT("VP9"));
				break;
		}
	}

	/*
	 * Selected Codec.
	 */
	EPixelStreamingCodec GetSelectedCodec()
	{
		const FString CodecStr = CVarPixelStreamingEncoderCodec.GetValueOnAnyThread();
		if (CodecStr == TEXT("H264"))
		{
			return EPixelStreamingCodec::H264;
		}
		else if (CodecStr == TEXT("VP8"))
		{
			return EPixelStreamingCodec::VP8;
		}
		else if (CodecStr == TEXT("VP9"))
		{
			return EPixelStreamingCodec::VP9;
		}
		else
		{
			return EPixelStreamingCodec::H264;
		}
	}

	bool IsCodecVPX()
	{
		EPixelStreamingCodec SelectedCodec = GetSelectedCodec();
		return SelectedCodec == EPixelStreamingCodec::VP8 || SelectedCodec == EPixelStreamingCodec::VP9;
	}

	EInputControllerMode GetInputControllerMode()
	{
		// Convert the current value to all lowercase and remove any whitespace.
		const FString InputControllerMode = CVarPixelStreamingInputController.GetValueOnAnyThread().ToLower().TrimStartAndEnd();

		if (InputControllerMode == TEXT("host"))
		{
			return EInputControllerMode::Host;
		}
		else
		{
			return EInputControllerMode::Any;
		}
	}

	FString GetDefaultStreamerID()
	{
		return DefaultStreamerID;
	}

	FString GetDefaultSignallingURL()
	{
		return DefaultSignallingURL;
	}

	/*
	 * Stats logger - as turned on/off by CVarPixelStreamingLogStats
	 */
	void ConsumeStat(FPixelStreamingPlayerId PlayerId, FName StatName, float StatValue)
	{
		UE_LOG(LogPixelStreaming, Log, TEXT("[%s](%s) = %f"), *PlayerId, *StatName.ToString(), StatValue);
	}

	FDelegateHandle LogStatsHandle;

	void OnLogStatsChanged(IConsoleVariable* Var)
	{
		bool bLogStats = Var->GetBool();

		UPixelStreamingDelegates* Delegates = UPixelStreamingDelegates::GetPixelStreamingDelegates();
		if (!Delegates)
		{
			return;
		}

		if (bLogStats)
		{
			LogStatsHandle = Delegates->OnStatChangedNative.AddStatic(&ConsumeStat);
		}
		else
		{
			Delegates->OnStatChangedNative.Remove(LogStatsHandle);
		}
	}

	/*
	 * Settings parsing and initialization.
	 */

	// Some settings need to be set after streamer is initialized
	void OnStreamerReady(IPixelStreamingModule& Module)
	{
		CVarPixelStreamingLogStats.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnLogStatsChanged));
		CommandLineParseOption(TEXT("PixelStreamingLogStats"), CVarPixelStreamingLogStats);
	}

	void InitialiseSettings()
	{
		using namespace UE::PixelStreaming;
		UE_LOG(LogPixelStreaming, Log, TEXT("Initialising Pixel Streaming settings."));

		FString StringOptions;

		FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingID="), DefaultStreamerID, false);

		CVarPixelStreamingOnScreenStats.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnHudStatsToggled));
		CVarPixelStreamingKeyFilter.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnFilteredKeysChanged));
		CVarPixelStreamingWebRTCFps.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnWebRTCFpsChanged));
		CVarPixelStreamingEncoderKeyframeInterval.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnKeyframeIntervalChanged));

		// Values parse from commands line
		CommandLineParseValue(TEXT("PixelStreamingEncoderKeyframeInterval="), CVarPixelStreamingEncoderKeyframeInterval);
		CommandLineParseValue(TEXT("PixelStreamingEncoderTargetBitrate="), CVarPixelStreamingEncoderTargetBitrate);
		CommandLineParseValue(TEXT("PixelStreamingEncoderMaxBitrate="), CVarPixelStreamingEncoderMaxBitrate);
		CommandLineParseValue(TEXT("PixelStreamingEncoderMinQP="), CVarPixelStreamingEncoderMinQP);
		CommandLineParseValue(TEXT("PixelStreamingEncoderMaxQP="), CVarPixelStreamingEncoderMaxQP);
		CommandLineParseValue(TEXT("PixelStreamingEncoderRateControl="), CVarPixelStreamingEncoderRateControl);
		CommandLineParseValue(TEXT("PixelStreamingEncoderMultipass="), CVarPixelStreamingEncoderMultipass);
		CommandLineParseValue(TEXT("PixelStreamingEncoderCodec="), CVarPixelStreamingEncoderCodec);
		CommandLineParseValue(TEXT("PixelStreamingH264Profile="), CVarPixelStreamingH264Profile);
		CommandLineParseValue(TEXT("PixelStreamingDegradationPreference="), CVarPixelStreamingDegradationPreference);
		CommandLineParseValue(TEXT("PixelStreamingWebRTCDegradationPreference="), CVarPixelStreamingDegradationPreference);
		CommandLineParseValue(TEXT("PixelStreamingWebRTCFps="), CVarPixelStreamingWebRTCFps);
		CommandLineParseValue(TEXT("PixelStreamingWebRTCStartBitrate="), CVarPixelStreamingWebRTCStartBitrate);
		CommandLineParseValue(TEXT("PixelStreamingWebRTCMinBitrate="), CVarPixelStreamingWebRTCMinBitrate);
		CommandLineParseValue(TEXT("PixelStreamingWebRTCMaxBitrate="), CVarPixelStreamingWebRTCMaxBitrate);
		CommandLineParseValue(TEXT("PixelStreamingWebRTCLowQpThreshold="), CVarPixelStreamingWebRTCLowQpThreshold);
		CommandLineParseValue(TEXT("PixelStreamingWebRTCHighQpThreshold="), CVarPixelStreamingWebRTCHighQpThreshold);
		CommandLineParseValue(TEXT("PixelStreamingFreezeFrameQuality"), CVarPixelStreamingFreezeFrameQuality);
		CommandLineParseValue(TEXT("PixelStreamingKeyFilter="), CVarPixelStreamingKeyFilter);
		CommandLineParseValue(TEXT("PixelStreamingVideoTracks="), CVarPixelStreamingVideoTracks);
		CommandLineParseValue(TEXT("PixelStreamingFrameScale="), CVarPixelStreamingFrameScale);
		CommandLineParseValue(TEXT("PixelStreamingInputController="), CVarPixelStreamingInputController);
		CommandLineParseValue(TEXT("PixelStreamingSignalingReconnectInterval="), CVarPixelStreamingSignalingReconnectInterval);

		// Options parse (if these exist they are set to true)
		CommandLineParseOption(TEXT("AllowPixelStreamingCommands"), CVarPixelStreamingAllowConsoleCommands);
		CommandLineParseOption(TEXT("PixelStreamingOnScreenStats"), CVarPixelStreamingOnScreenStats);
		CommandLineParseOption(TEXT("PixelStreamingHudStats"), CVarPixelStreamingOnScreenStats);
		CommandLineParseOption(TEXT("PixelStreamingDebugDumpFrame"), CVarPixelStreamingDebugDumpFrame);
		CommandLineParseOption(TEXT("PixelStreamingEnableFillerData"), CVarPixelStreamingEnableFillerData);
		CommandLineParseOption(TEXT("PixelStreamingWebRTCDisableStats"), CVarPixelStreamingWebRTCDisableStats);
		CommandLineParseOption(TEXT("PixelStreamingWebRTCDisableReceiveAudio"), CVarPixelStreamingWebRTCDisableReceiveAudio);
		CommandLineParseOption(TEXT("PixelStreamingWebRTCDisableTransmitAudio"), CVarPixelStreamingWebRTCDisableTransmitAudio);
		CommandLineParseOption(TEXT("PixelStreamingWebRTCDisableAudioSync"), CVarPixelStreamingWebRTCDisableAudioSync);
		CommandLineParseOption(TEXT("PixelStreamingSendPlayerIdAsInteger"), CVarSendPlayerIdAsInteger);
		CommandLineParseOption(TEXT("PixelStreamingWebRTCUseLegacyAudioDevice"), CVarPixelStreamingWebRTCUseLegacyAudioDevice);
		CommandLineParseOption(TEXT("PixelStreamingDisableLatencyTester"), CVarPixelStreamingDisableLatencyTester);
		CommandLineParseOption(TEXT("PixelStreamingVPXUseCompute"), CVarPixelStreamingVPXUseCompute);

		ReadSimulcastParameters();

		IPixelStreamingModule& Module = IPixelStreamingModule::Get();
		Module.OnReady().AddStatic(&OnStreamerReady);

		IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("PixelStreaming.StartStreaming"),
			TEXT("Start all streaming sessions"),
			FConsoleCommandDelegate::CreateLambda([]() {
				IPixelStreamingModule::Get().StartStreaming();
			}));

		IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("PixelStreaming.StopStreaming"),
			TEXT("End any existing streaming sessions."),
			FConsoleCommandDelegate::CreateLambda([]() {
				IPixelStreamingModule::Get().StopStreaming();
			}));
	}

} // namespace UE::PixelStreaming::Settings
