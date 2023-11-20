// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingVCamModule.h"

#include "BuiltinProviders/VCamPixelStreamingSession.h"
#include "IDecoupledOutputProviderModule.h"
#include "VCamPixelStreamingSessionLogic.h"

#include "Modules/ModuleManager.h"

namespace UE::PixelStreamingVCam::Private
{
	void FPixelStreamingVCamModule::StartupModule()
	{
		using namespace DecoupledOutputProvider;
		IDecoupledOutputProviderModule& DecouplingModule = IDecoupledOutputProviderModule::Get();
		DecouplingModule.RegisterLogicFactory(
			UVCamPixelStreamingSession::StaticClass(),
			FOutputProviderLogicFactoryDelegate::CreateLambda([](const FOutputProviderLogicCreationArgs& Args)
			{
				return MakeShared<FVCamPixelStreamingSessionLogic>();
			})
		);
		
		// Set the some VCam specific CVars in Pixel Streaming
		// TODO UE-198465: consider how to best expose these settings to the user? Or which ones we should expose?
		
		// Decouple engine's render rate from streaming rate
		if(IConsoleVariable* DecoupleFramerateCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.DecoupleFramerate")))
		{
			DecoupleFramerateCVar->Set(true);
		}
		// Set the rate at which we will stream
		if(IConsoleVariable* StreamFPS = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.WebRTC.Fps")))
		{
			StreamFPS->Set(60);
		}
		// Set capture use fence to true, as we require this for decoupling to work.. even though it is irrelevant to VCam (hack)
		if(IConsoleVariable* CaptureUseFence = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.CaptureUseFence")))
		{
			CaptureUseFence->Set(true);
		}
		// Disable keyframes interval
		if(IConsoleVariable* KeyframeInterval = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.Encoder.KeyframeInterval")))
		{
			KeyframeInterval->Set(0);
		}
		// Set a fixed target bitrate (this way quality and network transmission should be bounded).
		// We want this network transmission to be as consistent as possible so that the jitter buffer on the recv side
		// stays well bounded and shrinks to its optimal value and stays there.
		if(IConsoleVariable* TargetBitrate = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.Encoder.TargetBitrate")))
		{
			TargetBitrate->Set(20000000);
		}
		// Enable filler data in the encoding (while wasteful this ensure stable bitrate which helps with latency estimations)
		// In short this a tradeoff between increased bitrate for better latency.
		if(IConsoleVariable* EnableFillerData = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.Encoder.EnableFillerData")))
		{
			EnableFillerData->Set(true);
		}
		// Disable the frame dropper so we can stream as very consistent FPS.
		// This is required for when we are setting a target bitrate (ignoring WebRTC) and feeding in a fixed FPS.
		// Without this we get frames dropped due overshooting bitrate targets (which is fine on LAN).
		if(IConsoleVariable* DisableFrameDropper = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.WebRTC.DisableFrameDropper")))
		{
			DisableFrameDropper->Set(true);
		}
		// When bitrate is set high AND frame dropper is disabled this means the pacer will end up with a lot of packets in it.
		// We need to set a lenient value in the pacer so it too doesn't start dropping frames in fear of congestion control.
		// Again, this is fine to do on LAN where we know the link won't become congested at the bitrates and FPS we are targeting.
		if(IConsoleVariable* VideoPacingFactor = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.WebRTC.VideoPacing.Factor")))
		{
			VideoPacingFactor->Set(100.0f);
		}
		
		// Disable audio transmission as audio sync and extra media stream can incur latency
		if(IConsoleVariable* TransmitAudio = IConsoleManager::Get().FindConsoleVariable(TEXT("PixelStreaming.WebRTC.DisableTransmitAudio")))
		{
			TransmitAudio->Set(false);
		}
	}

	void FPixelStreamingVCamModule::ShutdownModule()
	{
		// DecoupledOutputProvider will also be destroyed in a moment (part of same plugin) so we do not have to call IDecoupledOutputProviderModule::UnregisterLogicFactory.
		// In fact doing so would not even work because UVCamPixelStreamingSession::StaticClass() would return garbage.
	}
}

IMPLEMENT_MODULE(UE::PixelStreamingVCam::Private::FPixelStreamingVCamModule, PixelStreamingVCam);
