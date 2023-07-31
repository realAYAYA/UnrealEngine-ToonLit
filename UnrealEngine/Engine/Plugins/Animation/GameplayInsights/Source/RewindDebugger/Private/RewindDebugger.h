// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IRewindDebugger.h"
#include "RewindDebuggerTrack.h"
#include "BindableProperty.h"
#include "Containers/Ticker.h"
#include "UObject/WeakObjectPtr.h"

class FMenuBuilder;
class USkeletalMeshComponent;

namespace TraceServices
{
	class IAnalysisSession;
}

class SWidget;
class SDockTab;

// Singleton class that handles the logic for the Rewind Debugger
// handles:
//  Playback/Scrubbing state
//  Start/Stop recording
//  Keeping track of the current Debug Target actor, and outputing a list of it's Components for the UI

class FRewindDebugger : public IRewindDebugger
{
public:
	FRewindDebugger();
	virtual ~FRewindDebugger();

	// IRewindDebugger interface
	virtual double CurrentTraceTime() const override { return TraceTime.Get(); }
	virtual const TRange<double>& GetCurrentTraceRange() const override { return CurrentTraceRange; }
	virtual const TRange<double>& GetCurrentViewRange() const override { return CurrentViewRange; }
	virtual const TraceServices::IAnalysisSession* GetAnalysisSession() const override;
	virtual uint64 GetTargetActorId() const override;
	virtual bool GetTargetActorPosition(FVector& OutPosition) const override;
	virtual UWorld* GetWorldToVisualize() const override;
	virtual bool IsRecording() const override { return bRecording; }
	virtual bool IsPIESimulating() const override { return bPIESimulating; }
	virtual double GetRecordingDuration() const override { return RecordingDuration.Get(); }
	virtual TSharedPtr<FDebugObjectInfo> GetSelectedComponent() const override;
	virtual TArray<TSharedPtr<FDebugObjectInfo>>& GetDebugComponents() override;
	
	TArray<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>& GetDebugTracks() { return DebugTracks; }

	// create singleton instance
	static void Initialize();

	// destroy singleton instance
	static void Shutdown();

	// get singleton instance
	static FRewindDebugger* Instance() { return static_cast<FRewindDebugger*>(InternalInstance); }

	// Start a new Recording:  Start tracing Object + Animation data, increment the current recording index, and reset the recording elapsed time to 0
	void StartRecording();

	bool CanStartRecording() const { return !IsRecording() && bPIESimulating; }

	bool ShouldAutoRecordOnPIE() const;
	void SetShouldAutoRecordOnPIE(bool value);

	// Stop recording: Stop tracing Object + Animation Data.
	void StopRecording();
	bool CanStopRecording() const { return IsRecording(); }

	// VCR controls

	bool CanPause() const;
	void Pause();

	bool CanPlay() const;
	void Play();
	bool IsPlaying() const;

	bool CanPlayReverse() const;
	void PlayReverse();

	void ScrubToStart();
	void ScrubToEnd();
	void Step(int32 frames);
	void StepForward();
	void StepBackward();

	bool CanScrub() const;
	void ScrubToTime(double ScrubTime, bool bIsScrubbing);
	double GetScrubTime() { return CurrentScrubTime; }

	// Tick function: While recording, update recording duration.  While paused, and we have recorded data, update skinned mesh poses for the current frame, and handle playback.
	void Tick(float DeltaTime);

	// update the list of tracks for the currently selected debug target
	void RefreshDebugTracks();
	
	void SetCurrentViewRange(const TRange<double>& Range);

	DECLARE_DELEGATE(FOnComponentListChanged)
	void OnComponentListChanged(const FOnComponentListChanged& ComponentListChangedCallback);
	
	void ComponentDoubleClicked(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SelectedObject);
	void ComponentSelectionChanged(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SelectedObject);
	TSharedPtr<SWidget> BuildComponentContextMenu();

	void UpdateDetailsPanel(TSharedRef<SDockTab> DetailsTab);

	DECLARE_DELEGATE_OneParam( FOnTrackCursor, bool)
	void OnTrackCursor(const FOnTrackCursor& TrackCursorCallback);

	TBindableProperty<double>* GetTraceTimeProperty() { return &TraceTime; }
	TBindableProperty<float>* GetRecordingDurationProperty() { return &RecordingDuration; }
	TBindableProperty<FString, BindingType_Out>* GetDebugTargetActorProperty() { return &DebugTargetActor; }

	virtual void OpenDetailsPanel() override;
	void SetIsDetailsPanelOpen(bool bIsOpen) { bIsDetailsPanelOpen = bIsOpen; }
	bool IsDetailsPanelOpen(bool bIsOpen) { return bIsDetailsPanelOpen; }

private:
	void RefreshDebugComponents(TArray<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>& InTracks, TArray<TSharedPtr<FDebugObjectInfo>>& OutComponents);
	
	void OnPIEStarted(bool bSimulating);
	void OnPIEPaused(bool bSimulating);
	void OnPIEResumed(bool bSimulating);
	void OnPIEStopped(bool bSimulating);
	void OnPIESingleStepped(bool bSimulating);

	void SetCurrentScrubTime(double Time);

	TBindableProperty<double> TraceTime;
	TBindableProperty<float> RecordingDuration;
	TBindableProperty<FString, BindingType_Out> DebugTargetActor;

	enum class EControlState : int8
	{
		Play,
		PlayReverse,
		Pause
	};

	EControlState ControlState;

	FOnComponentListChanged ComponentListChangedDelegate;
	FOnTrackCursor TrackCursorDelegate;

	bool bPIEStarted;
	bool bPIESimulating;
	
	bool bRecording;

	float PlaybackRate;
	double PreviousTraceTime;
	double CurrentScrubTime;
	TRange<double> CurrentViewRange;
	TRange<double> CurrentTraceRange;
	uint16 RecordingIndex;

	struct FScrubTimeInformation
	{
		double ProfileTime = 0;  // Profiling/Tracing time
		int64 FrameIndex = 0;    // Scrub Frame Index
	};
	
	FScrubTimeInformation ScrubTimeInformation;
	FScrubTimeInformation LowerBoundViewTimeInformation;
	FScrubTimeInformation UpperBoundViewTimeInformation;

	static void GetScrubTimeInformation(double InDebugTime, FScrubTimeInformation & InOutTimeInformation, uint16 InRecordingIndex, const TraceServices::IAnalysisSession* AnalysisSession);
	
	TArray<TSharedPtr<FDebugObjectInfo>> DebugComponents;
	mutable TSharedPtr<FDebugObjectInfo> SelectedComponent;
	
	TArray<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>> DebugTracks;
	TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SelectedTrack;

	struct FMeshComponentResetData
	{
		TWeakObjectPtr<USkeletalMeshComponent> Component;
		FTransform RelativeTransform;
	};

	TMap<uint64, FMeshComponentResetData> MeshComponentsToReset;

	mutable class IUnrealInsightsModule *UnrealInsightsModule;
	FTSTicker::FDelegateHandle TickerHandle;

	bool bTargetActorPositionValid;
	FVector TargetActorPosition;

	bool bIsDetailsPanelOpen;
};
