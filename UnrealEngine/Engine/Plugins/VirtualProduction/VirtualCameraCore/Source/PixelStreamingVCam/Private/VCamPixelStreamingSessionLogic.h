// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "IOutputProviderLogic.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UVCamPixelStreamingSession;
class UPixelStreamingMediaIOCapture;
class UPixelStreamingMediaOutput;
class IPixelStreamingStreamer;

namespace UE::PixelStreamingVCam::Private
{
	/** Implements logic for UVCamPixelStreamingSession so it can be loaded on all platforms. */
	class FVCamPixelStreamingSessionLogic : public DecoupledOutputProvider::IOutputProviderLogic
	{
	public:

		//~ Begin IOutputProviderLogic Interface
		virtual void OnDeinitialize(DecoupledOutputProvider::IOutputProviderEvent& Args) override;
		virtual void OnActivate(DecoupledOutputProvider::IOutputProviderEvent& Args) override;
		virtual void OnDeactivate(DecoupledOutputProvider::IOutputProviderEvent& Args) override;
		virtual VCamCore::EViewportChangeReply PreReapplyViewport(DecoupledOutputProvider::IOutputProviderEvent& Args) override;
		virtual void PostReapplyViewport(DecoupledOutputProvider::IOutputProviderEvent& Args) override;
		virtual void OnAddReferencedObjects(DecoupledOutputProvider::IOutputProviderEvent& Args, FReferenceCollector& Collector) override;
#if WITH_EDITOR
		virtual void OnPostEditChangeProperty(DecoupledOutputProvider::IOutputProviderEvent& Args, FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
		//~ End IOutputProviderLogic Interface

	private:

		/** Used to generate unique streamer IDs */
		static int NextDefaultStreamerId;

		/** Last time viewport was touched. Updated every tick. */
		FHitResult 	LastViewportTouchResult;
		/** Whether we overwrote the widget class with the empty widget class; remember: PS needs a widget. */
		bool bUsingDummyUMG = false;
		/** Cached setting from settings object. */
		bool bOldThrottleCPUWhenNotForeground;

		TObjectPtr<UPixelStreamingMediaOutput> MediaOutput = nullptr;
		TObjectPtr<UPixelStreamingMediaIOCapture> MediaCapture = nullptr;

		void SetupSignallingServer();
		void StopSignallingServer();

		void SetupCapture(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr);
		void StartCapture(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr);
		void StopCapture();

		void OnPreStreaming(IPixelStreamingStreamer* PreConnectionStreamer, TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr);
		void StopStreaming();
		void OnStreamingStarted(IPixelStreamingStreamer* StartedStreamer, TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr);
		void OnStreamingStopped(IPixelStreamingStreamer* StoppedStreamer);
		void StopEverything();

		void SetupCustomInputHandling(UVCamPixelStreamingSession* This);

		void OnCaptureStateChanged(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr);
		void OnRemoteResolutionChanged(const FIntPoint& RemoteResolution, TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr);

		/** Sets the owning VCam's live link subject to this the subject created by this session, if this behaviour is enabled. */
		void ConditionallySetLiveLinkSubjectToThis(UVCamPixelStreamingSession* This) const;

		void SetupARKitResponseTimer(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisUObjectPtr);
		void StopARKitResponseTimer();

private:
		/** Handle for ARKit stats timer */
		FTimerHandle ARKitResponseTimer; 
		size_t NumARKitEvents = 0;
	};
}

