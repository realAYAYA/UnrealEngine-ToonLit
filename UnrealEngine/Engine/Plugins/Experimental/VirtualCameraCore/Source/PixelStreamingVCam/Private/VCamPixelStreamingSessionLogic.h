// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/HitResult.h"
#include "IOutputProviderLogic.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UVCamPixelStreamingSession;
class UPixelStreamingMediaCapture;
class UPixelStreamingMediaOutput;

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
		TObjectPtr<UPixelStreamingMediaCapture> MediaCapture = nullptr;
		
		void SetupSignallingServer();
		void StopSignallingServer();
	
		void SetupCapture(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisPtr);
		void StartCapture(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisPtr);
		void SetupCustomInputHandling(UVCamPixelStreamingSession* This);
	
		void OnCaptureStateChanged(TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisPtr);
		void OnRemoteResolutionChanged(const FIntPoint& RemoteResolution, TWeakObjectPtr<UVCamPixelStreamingSession> WeakThisPtr);

		/** Sets the owning VCam's live link subject to this the subject created by this session, if this behaviour is enabled. */
		void ConditionallySetLiveLinkSubjectToThis(UVCamPixelStreamingSession* This) const;
	};
}

