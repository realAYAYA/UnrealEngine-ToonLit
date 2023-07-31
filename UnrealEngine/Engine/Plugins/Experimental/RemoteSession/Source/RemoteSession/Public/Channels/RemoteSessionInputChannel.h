// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteSessionChannel.h"
#include "../Private/MessageHandler/RecordingMessageHandler.h"

class FBackChannelOSCMessage;
class FBackChannelOSCDispatch;
class IBackChannelPacket;

class REMOTESESSION_API FRemoteSessionInputChannel : public IRemoteSessionChannel, public IRecordingMessageHandlerWriter
{
public:

	FRemoteSessionInputChannel(ERemoteSessionChannelMode InRole, TSharedPtr<IBackChannelConnection, ESPMode::ThreadSafe> InConnection);

	~FRemoteSessionInputChannel();

	virtual void Tick(const float InDeltaTime) override;

	virtual void RecordMessage(const TCHAR* MsgName, const TArray<uint8>& Data) override;

	void OnRemoteMessage(IBackChannelPacket& Message);

	void SetPlaybackWindow(TWeakPtr<SWindow> InWindow, TWeakPtr<FSceneViewport> InViewport);

	void SetInputRect(const FVector2D& TopLeft, const FVector2D& Extents);

	void TryRouteTouchMessageToWidget(bool bRouteMessageToWidget);

	/**
	 * Delegate that fires when routing a touch message to the widget did not work.
	 * @note Only useful during playback.
	 * @return Null when recording, Pointer to delegate during playback.
	 */
	FOnRouteTouchDownToWidgetFailedDelegate* GetOnRouteTouchDownToWidgetFailedDelegate();

	static const TCHAR* StaticType() { return TEXT("FRemoteSessionInputChannel"); }
	virtual const TCHAR* GetType() const override { return StaticType(); }

protected:

	TSharedPtr<FGenericApplicationMessageHandler> DefaultHandler;

	TSharedPtr<FRecordingMessageHandler> RecordingHandler;

	TSharedPtr<FRecordingMessageHandler> PlaybackHandler;

	TSharedPtr<IBackChannelConnection, ESPMode::ThreadSafe> Connection;

	ERemoteSessionChannelMode Role;

	/** So we can manage callback lifetimes properly */
	FDelegateHandle MessageCallbackHandle;
};

