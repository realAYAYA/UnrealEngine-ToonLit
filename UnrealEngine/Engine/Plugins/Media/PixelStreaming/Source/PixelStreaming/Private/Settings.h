// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "InputCoreTypes.h"
#include "VideoEncoder.h"
#include "WebRTCIncludes.h"
#include "PixelStreamingCodec.h"

namespace UE::PixelStreaming::Settings
{
	extern void InitialiseSettings();

	// Begin Encoder CVars
	extern TAutoConsoleVariable<int32> CVarPixelStreamingEncoderTargetBitrate;
	extern TAutoConsoleVariable<int32> CVarPixelStreamingEncoderMaxBitrate;
	extern TAutoConsoleVariable<bool> CVarPixelStreamingDebugDumpFrame;
	extern TAutoConsoleVariable<int32> CVarPixelStreamingEncoderMinQP;
	extern TAutoConsoleVariable<int32> CVarPixelStreamingEncoderMaxQP;
	extern TAutoConsoleVariable<FString> CVarPixelStreamingEncoderRateControl;
	extern TAutoConsoleVariable<bool> CVarPixelStreamingEnableFillerData;
	extern TAutoConsoleVariable<FString> CVarPixelStreamingEncoderMultipass;
	extern TAutoConsoleVariable<FString> CVarPixelStreamingH264Profile;
	extern TAutoConsoleVariable<int32> CVarPixelStreamingEncoderKeyframeInterval;
	extern TAutoConsoleVariable<FString> CVarPixelStreamingEncoderCodec;
	extern TAutoConsoleVariable<FString> CVarPixelStreamingVideoTracks;
	// End Encoder CVars

	// Begin WebRTC CVars
	extern TAutoConsoleVariable<FString> CVarPixelStreamingDegradationPreference;
	extern TAutoConsoleVariable<int32> CVarPixelStreamingWebRTCFps;
	extern TAutoConsoleVariable<int32> CVarPixelStreamingWebRTCStartBitrate;
	extern TAutoConsoleVariable<int32> CVarPixelStreamingWebRTCMinBitrate;
	extern TAutoConsoleVariable<int32> CVarPixelStreamingWebRTCMaxBitrate;
	extern TAutoConsoleVariable<int> CVarPixelStreamingWebRTCLowQpThreshold;
	extern TAutoConsoleVariable<int> CVarPixelStreamingWebRTCHighQpThreshold;
	extern TAutoConsoleVariable<bool> CVarPixelStreamingWebRTCDisableReceiveAudio;
	extern TAutoConsoleVariable<bool> CVarPixelStreamingWebRTCDisableTransmitAudio;
	extern TAutoConsoleVariable<bool> CVarPixelStreamingWebRTCDisableAudioSync;
	extern TAutoConsoleVariable<bool> CVarPixelStreamingWebRTCUseLegacyAudioDevice;
	extern TAutoConsoleVariable<bool> CVarPixelStreamingWebRTCDisableStats;
	extern TAutoConsoleVariable<float> CVarPixelStreamingWebRTCAudioGain;
	// End WebRTC CVars

	// Begin Pixel Streaming Plugin CVars
	extern TAutoConsoleVariable<bool> CVarPixelStreamingAllowConsoleCommands;
	extern TAutoConsoleVariable<bool> CVarPixelStreamingOnScreenStats;
	extern TAutoConsoleVariable<bool> CVarPixelStreamingLogStats;
	extern TAutoConsoleVariable<int32> CVarPixelStreamingFreezeFrameQuality;
	extern TAutoConsoleVariable<bool> CVarSendPlayerIdAsInteger;
	extern TAutoConsoleVariable<bool> CVarPixelStreamingDisableLatencyTester;
	extern TAutoConsoleVariable<bool> CVarPixelStreamingVPXUseCompute;
	extern TAutoConsoleVariable<FString> CVarPixelStreamingInputController;
	extern TAutoConsoleVariable<bool> CVarPixelStreamingSuppressICECandidateErrors;
	extern TAutoConsoleVariable<float> CVarPixelStreamingSignalingReconnectInterval;

	extern TArray<FKey> FilteredKeys;
	// Ends Pixel Streaming Plugin CVars

	// Begin TextureSource CVars
	extern TAutoConsoleVariable<float> CVarPixelStreamingFrameScale;
	// End TextureSource CVars

	/* Pixel Streaming can limit who can send input (keyboard, mouse, etc). */
	enum EInputControllerMode
	{
		/* Any - Any peer can control input. */
		Any,
		/* Host - Only the "host" peer can control input. */
		Host
	};

	// Begin utility functions etc.
	bool IsCodecVPX();
	void SetCodec(EPixelStreamingCodec Codec);
	EPixelStreamingCodec GetSelectedCodec();
	AVEncoder::FVideoEncoder::RateControlMode GetRateControlCVar();
	AVEncoder::FVideoEncoder::MultipassMode GetMultipassCVar();
	webrtc::DegradationPreference GetDegradationPreference();
	AVEncoder::FVideoEncoder::H264Profile GetH264Profile();
	EInputControllerMode GetInputControllerMode();
	FString GetDefaultStreamerID();
	FString GetDefaultSignallingURL();
	// End utility functions etc.

	struct FSimulcastParameters
	{
		struct FLayer
		{
			float Scaling;
			int MinBitrate;
			int MaxBitrate;
		};

		TArray<FLayer> Layers;
	};

	extern FSimulcastParameters SimulcastParameters;

	// Begin Command line args
	inline bool IsExperimentalAudioInputEnabled()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("PixelStreamingExperimentalAudioInput"));
	}

	inline bool IsPixelStreamingHideCursor()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("PixelStreamingHideCursor"));
	}

	inline bool ShouldNegotiateCodecs()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("PixelStreamingNegotiateCodecs"));
	}

	inline bool IsUsingSafeTextureCopy()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("PixelCaptureUseFence"));
	}

	inline bool CoupleFrameRate()
	{
		return !FParse::Param(FCommandLine::Get(), TEXT("PixelStreamingDecoupleFrameRate"));
	}

	inline bool GetSignallingServerUrl(FString& OutSignallingServerURL)
	{
		return FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingURL="), OutSignallingServerURL);
	}

	inline bool GetSignallingServerIP(FString& OutSignallingServerIP)
	{
		return FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingIP="), OutSignallingServerIP);
	}

	inline bool GetSignallingServerPort(uint16& OutSignallingServerPort)
	{
		return FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingPort="), OutSignallingServerPort);
	}

	inline bool GetControlScheme(FString& OutControlScheme)
	{
		return FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingControlScheme="), OutControlScheme);
	}

	inline bool GetFastPan(float& OutFastPan)
	{
		return FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingFastPan="), OutFastPan);
	}

	// End Command line args
} // namespace UE::PixelStreaming::Settings
