// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "UObject/WeakObjectPtr.h"
#include "VirtualCameraBeaconReceiver.h"

class UVCamPixelStreamingSession;

namespace UE::PixelStreamingVCam::Private
{
	class FPixelStreamingVCamModule : public IModuleInterface
	{
	public:

		//~ Begin IModuleInterface Interface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
		//~ End IModuleInterface Interface

		/** Indicate that a VCAM pixel streaming session has become active. */
		void AddActiveSession(const TWeakObjectPtr<UVCamPixelStreamingSession>& Session);

		/** Indicate that a VCAM pixel streaming session has become inactive. */
		void RemoveActiveSession(const TWeakObjectPtr<UVCamPixelStreamingSession>& Session);

		static FPixelStreamingVCamModule& Get();

	private:
		/** Configure CVars and session logic for Pixel Streaming. */
		void ConfigurePixelStreaming();

		/** Update the beacon receiver's streaming readiness state based on the number of active sessions. */
		void UpdateBeaconReceiverStreamReadiness();

	private:

		/** Receiver that responds to beacon messages from the VCAM app. */
		FVirtualCameraBeaconReceiver BeaconReceiver;

		/** VCAM Pixel Streaming sessions that are currently active. */
		TSet<TWeakObjectPtr<UVCamPixelStreamingSession>, TWeakObjectPtrSetKeyFuncs<TWeakObjectPtr<UVCamPixelStreamingSession>>> ActiveSessions;
	};
}

