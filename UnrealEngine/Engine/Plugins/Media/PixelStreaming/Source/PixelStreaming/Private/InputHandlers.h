// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IInputDevice.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"

namespace UE::PixelStreaming
{
	// The IInputDevice module only creates a single IInputDevice but we have one per streamer.
	// As such, this wrapper exists to act as the bridge between the single IInputDevice created 
	// by the IInputDeviceModule and the multiple streamers
	class FInputHandlers : public IInputDevice
	{
	public:
		// This wrapper does effectively nothing, and it is up to the individual streamers to manage their input handlers
		FInputHandlers(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler);
		virtual ~FInputHandlers() = default;

		virtual void Tick(float DeltaTime) override;
		virtual void SendControllerEvents() override;
		virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;
		virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
		virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
		virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values) override;

		void ForEachInputHandler(TFunction<void(IInputDevice*)> const& Visitor);
	};
} // namespace UE::PixelStreaming
