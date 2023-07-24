// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SWidget.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDisplayRangeUpdated, const TRange<float> /* New Display Range */);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPlayheadScrubUpdate, const uint32 /* New Playhead Position in Samples */, const uint32 /* Waveform Length In Samples */, const bool /*Playhead is Moving*/);

class FWaveformEditorRenderData;

enum class EReceivedInteractionType
{
	MouseButtonUp, 
	MouseButtonDown, 
	MouseMove
};

class WAVEFORMEDITORWIDGETS_API FWaveformEditorTransportCoordinator : public TSharedFromThis<FWaveformEditorTransportCoordinator>
{
public:
	explicit FWaveformEditorTransportCoordinator(TSharedRef<FWaveformEditorRenderData> InRenderData);
	virtual ~FWaveformEditorTransportCoordinator() = default;

	/** Called when the playhead is scrubbed */
	FOnPlayheadScrubUpdate OnPlayheadScrubUpdate;

	/** Called when the display range is updated */
	FOnDisplayRangeUpdated OnDisplayRangeUpdated;

	/* Intercept widget interactions */
	FReply ReceiveMouseButtonDown(SWidget& WidgetOwner, const FGeometry& Geometry, const FPointerEvent& MouseEvent);
	FReply ReceiveMouseButtonUp(SWidget& WidgetOwner, const FGeometry& Geometry, const FPointerEvent& MouseEvent);
	FReply ReceiveMouseMove(SWidget& WidgetOwner, const FGeometry& Geometry, const FPointerEvent& MouseEvent);

	void ScrubPlayhead(const float InPlayHeadPosition, const bool bIsMoving);
	const bool IsScrubbing() const;

	void HandleRenderDataUpdate();

	const float GetPlayheadPosition() const;
	void ReceivePlayBackRatio(const float NewRatio);
	void OnZoomLevelChanged(const float NewLevel);

	float ConvertAbsoluteRatioToZoomed(const float InAbsoluteRatio) const;
	float ConvertZoomedRatioToAbsolute(const float InZoomedRatio) const;

	TRange<float> GetDisplayRange() const;

	void Stop();

private:
	FORCEINLINE void MovePlayhead(const float InPlayheadPosition);
	void UpdateZoomRatioAndDisplayRange(const float NewZoomRatio);

	void UpdateDisplayRange(const float MinValue, const float MaxValue);
	bool IsRatioWithinDisplayRange(const float Ratio) const;

	uint32 GetSampleFromPlayheadPosition(const float InPlayheadPosition) const;

	FReply HandleWaveformViewerOverlayInteraction(const FPointerEvent& MouseEvent, const FGeometry& Geometry);
	FReply HandleTimeRulerInteraction(const EReceivedInteractionType MouseInteractionType, const FPointerEvent& MouseEvent, SWidget& TimeRulerWidget, const FGeometry& Geometry);
	
	float CurrentPlaybackRatio = 0.f;
	float PlayheadLockPosition = 0.95f;
	float PlayheadPosition = 0.f;
	float ZoomRatio = 1.f;
	
	/* The currently displayed render data range */
	TRange<float> DisplayRange = TRange<float>::Inclusive(0.f, 1.f);

	/* Playback range to scale the incoming ratio from the audiocomponent with*/
	TRange<float> PlaybackRange = TRange<float>::Inclusive(0.f, 1.f);
	
	TSharedPtr<FWaveformEditorRenderData> RenderData = nullptr;

	bool bIsScrubbing = false;
};