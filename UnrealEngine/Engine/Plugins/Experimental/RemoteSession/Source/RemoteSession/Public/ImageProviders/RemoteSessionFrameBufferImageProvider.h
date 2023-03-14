// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/RemoteSessionImageChannel.h"


class FFrameGrabber;
class FSceneViewport;
class SWindow;

/**
 *	Use the FrameGrabber on the host to provide an image to the image channel.
 */
class REMOTESESSION_API FRemoteSessionFrameBufferImageProvider : public IRemoteSessionImageProvider
{
public:

	FRemoteSessionFrameBufferImageProvider(TSharedPtr<FRemoteSessionImageChannel::FImageSender, ESPMode::ThreadSafe> ImageSender);
	~FRemoteSessionFrameBufferImageProvider();

	/** Specifies which viewport to capture */
	void SetCaptureViewport(TSharedRef<FSceneViewport> Viewport);

	/** Specifies the framerate at */
	void SetCaptureFrameRate(int32 InFramerate);

	/** Tick this channel */
	virtual void Tick(const float InDeltaTime) override;

	/** Signals that the viewport was resized */
	void OnViewportResized(FVector2D NewSize);

protected:

	/** Release the FrameGrabber */
	void ReleaseFrameGrabber();

	/** When the window is destroyed */
	void OnWindowClosedEvent(const TSharedRef<SWindow>&);

	/** Safely create the frame grabber */
	void CreateFrameGrabber(TSharedRef<FSceneViewport> Viewport);

	TWeakPtr<FRemoteSessionImageChannel::FImageSender, ESPMode::ThreadSafe> ImageSender;

	TSharedPtr<FFrameGrabber> FrameGrabber;

	TSharedPtr<FThreadSafeCounter, ESPMode::ThreadSafe> NumDecodingTasks;

	/** Time we last sent an image */
	double LastSentImageTime;

	/** Shows that the viewport was just resized */
	bool ViewportResized;

	/** Holds a reference to the scene viewport */
	TWeakPtr<FSceneViewport> SceneViewport;

	/** Holds a reference to the SceneViewport SWindow */
	TWeakPtr<SWindow> SceneViewportWindow;

	FRemoteSesstionImageCaptureStats CaptureStats;
};


