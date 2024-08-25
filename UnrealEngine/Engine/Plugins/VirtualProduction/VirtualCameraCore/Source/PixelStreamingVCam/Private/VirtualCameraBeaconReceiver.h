// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DiscoveryBeaconReceiver.h"

namespace UE::PixelStreamingVCam::Private
{
	/**
	 * Receives beacon messages from the VCAM app and replies with connection information.
	 * This allows the app to detect compatible Unreal instances on the local network and list them for the user.
	 */
	class FVirtualCameraBeaconReceiver
		: public FDiscoveryBeaconReceiver
	{
	public:
		FVirtualCameraBeaconReceiver();

		/** Set whether streaming should be reported to beacon senders as available. */
		void SetIsStreamingReady(bool bNewValue);

		//~ Begin FDiscoveryBeaconReceiver implementation
		virtual void Startup() override;

	protected:
		virtual bool GetDiscoveryAddress(FIPv4Address& OutAddress) const override;
		virtual int32 GetDiscoveryPort() const override;
		virtual bool MakeBeaconResponse(uint8 BeaconProtocolVersion, FArrayReader& InMessageData, FArrayWriter& OutResponseData) const override;
		//~ End FDiscoveryBeaconReceiver implementation

	private:
		/** Get the name to report to apps searching for the engine. */
		FString GetFriendlyName() const;

		/** The Pixel Streaming port with which to reply. */
		uint32 PixelStreamingPort;

		/** Whether streaming should be reported to beacon senders as available. */
		bool bIsStreamingReady = true;
	};
}