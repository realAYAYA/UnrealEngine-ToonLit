// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_STATETREE_DEBUGGER

#include "Delegates/IDelegateInstance.h"
#include "IStateTreeTraceProvider.h"
#include "StateTree.h"
#include "StateTreeDebuggerTypes.h"
#include "StateTreeTypes.h"
#include "Tickable.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/Diagnostics.h"
#include "TraceServices/Model/Frames.h"

enum class EStateTreeBreakpointType : uint8;

namespace UE::Trace
{
	class FStoreClient;
}

class IStateTreeModule;
class UStateTreeState;
class UStateTree;

DECLARE_DELEGATE_OneParam(FOnStateTreeDebuggerScrubStateChanged, const UE::StateTreeDebugger::FScrubState& ScrubState);
DECLARE_DELEGATE_TwoParams(FOnStateTreeDebuggerBreakpointHit, FStateTreeInstanceDebugId InstanceId, const FStateTreeDebuggerBreakpoint Breakpoint);
DECLARE_DELEGATE_OneParam(FOnStateTreeDebuggerActiveStatesChanges, const FStateTreeTraceActiveStates& ActiveStates);
DECLARE_DELEGATE_OneParam(FOnStateTreeDebuggerNewInstance, FStateTreeInstanceDebugId InstanceId);
DECLARE_DELEGATE(FOnStateTreeDebuggerNewSession);
DECLARE_DELEGATE(FOnStateTreeDebuggerDebuggedInstanceSet);

struct STATETREEMODULE_API FStateTreeDebugger : FTickableGameObject
{
	struct FTraceDescriptor
	{
		FTraceDescriptor() = default;
		FTraceDescriptor(const FString& Name, const uint32 Id) : Name(Name), TraceId(Id) {}
		
		bool operator==(const FTraceDescriptor& Other) const { return Other.TraceId == TraceId; }
		bool operator!=(const FTraceDescriptor& Other) const { return !(Other == *this); }
		bool IsValid() const { return TraceId != INDEX_NONE; }

		FString Name;
		uint32 TraceId = INDEX_NONE;

		TraceServices::FSessionInfo SessionInfo;
	};

	struct FHitBreakpoint
	{
		bool IsSet() const { return Index != INDEX_NONE; }
		void Reset()
		{
			InstanceId = FStateTreeInstanceDebugId::Invalid;
			Time = 0;
			Index = INDEX_NONE;
		}

		/** Indicates the instance for which the breakpoint has been hit */
		FStateTreeInstanceDebugId InstanceId = FStateTreeInstanceDebugId::Invalid;

		/**
		 * Store the time at which the breakpoint was hit since we might have process more events before
		 * sending the notifications.
		 */
		double Time = 0;

		/** Indicates the index of the breakpoint that has been hit */
		int32 Index = INDEX_NONE;
	};

	FStateTreeDebugger();
	virtual ~FStateTreeDebugger() override;

	const UStateTree* GetAsset() const { return StateTreeAsset.Get(); }
	void SetAsset(const UStateTree* InStateTreeAsset) { StateTreeAsset = InStateTreeAsset; }
	
	/** Forces a single refresh to latest state. Useful when simulation is paused. */
	void SyncToCurrentSessionDuration();
	
	bool CanStepBackToPreviousStateWithEvents() const;
	void StepBackToPreviousStateWithEvents();

	bool CanStepForwardToNextStateWithEvents() const;
	void StepForwardToNextStateWithEvents();

	bool CanStepBackToPreviousStateChange() const;
	void StepBackToPreviousStateChange();

	bool CanStepForwardToNextStateChange() const;
	void StepForwardToNextStateChange();

	bool IsActiveInstance(double Time, FStateTreeInstanceDebugId InstanceId) const;
	FText GetInstanceName(FStateTreeInstanceDebugId InstanceId) const;
	FText GetInstanceDescription(FStateTreeInstanceDebugId InstanceId) const;
	void SelectInstance(const FStateTreeInstanceDebugId InstanceId);
	void ClearSelection() { SelectInstance({}); }
	FStateTreeInstanceDebugId GetSelectedInstanceId() const { return SelectedInstanceId; }
	const UE::StateTreeDebugger::FInstanceDescriptor* GetInstanceDescriptor(const FStateTreeInstanceDebugId InstanceId) const ;
	const UE::StateTreeDebugger::FInstanceDescriptor* GetSelectedInstanceDescriptor() const { return GetInstanceDescriptor(SelectedInstanceId); }

	bool HasStateBreakpoint(FStateTreeStateHandle StateHandle, EStateTreeBreakpointType BreakpointType) const;
	bool HasTaskBreakpoint(FStateTreeIndex16 Index, EStateTreeBreakpointType BreakpointType) const;
	bool HasTransitionBreakpoint(FStateTreeIndex16 Index, EStateTreeBreakpointType BreakpointType) const;
	void SetStateBreakpoint(FStateTreeStateHandle StateHandle, EStateTreeBreakpointType BreakpointType);
	void SetTransitionBreakpoint(FStateTreeIndex16 SubIndex, EStateTreeBreakpointType BreakpointType);
	void SetTaskBreakpoint(FStateTreeIndex16 NodeIndex, EStateTreeBreakpointType BreakpointType);
	void ClearBreakpoint(FStateTreeIndex16 NodeIndex, EStateTreeBreakpointType BreakpointType);
	void ClearAllBreakpoints();
	int32 NumBreakpoints() const { return Breakpoints.Num(); }
	bool HasHitBreakpoint() const { return HitBreakpoint.IsSet(); }

	static FText DescribeTrace(const FTraceDescriptor& TraceDescriptor);
	static FText DescribeInstance(const UE::StateTreeDebugger::FInstanceDescriptor& StateTreeInstanceDesc);

	/**
	 * Finds and returns the event collection associated to a given instance Id.
	 * An invalid empty collection is returned if not found (IsValid needs to be called).
	 * @param InstanceId Id of the instance for which the event collection is returned. 
	 * @return Event collection associated to the provided Id or an invalid one if not found.
	 */
	const UE::StateTreeDebugger::FInstanceEventCollection& GetEventCollection(FStateTreeInstanceDebugId InstanceId) const;

	/** Clears events from all instances. */
	void ResetEventCollections();

	/** Returns the recording duration in world recorded time. */
	double GetRecordingDuration() const { return RecordingDuration; }

	/** Returns the duration of the analysis session. This is not related to the world simulation time. */
	double GetAnalysisDuration() const { return AnalysisDuration; }

	/** Returns the time (based on the recording duration) associated to the selected frame. */
	double GetScrubTime() const	{ return ScrubState.GetScrubTime(); }
	void SetScrubTime(double ScrubTime);

	void GetLiveTraces(TArray<FTraceDescriptor>& OutTraceDescriptors) const;

	/**
	 * Queue a request to auto start an analysis session on the next available live trace.
	 * @return True if connection was successfully requested or was able to use active trace, false otherwise.
	 */
	bool RequestAnalysisOfEditorSession();
	bool IsAnalyzingEditorSession() const
	{
		return AnalysisTransitionType == EAnalysisTransitionType::NoneToEditor
			|| AnalysisTransitionType == EAnalysisTransitionType::EditorToEditor
			|| AnalysisTransitionType == EAnalysisTransitionType::SelectedToEditor;
	}

	bool WasAnalyzingEditorSession() const
	{
		return AnalysisTransitionType == EAnalysisTransitionType::EditorToSelected
			|| AnalysisTransitionType == EAnalysisTransitionType::EditorToEditor;
	}

	bool IsAnalysisSessionActive() const { return GetAnalysisSession() != nullptr; }
	bool IsAnalysisSessionPaused() const { return bSessionAnalysisPaused; }
	const TraceServices::IAnalysisSession* GetAnalysisSession() const;

	/**
	 * Tries to start an analysis for a given trace descriptor.
	 * On success this method will execute the OnNewSession delegate.
	 * @param TraceDescriptor Descriptor of the trace that needs to be analyzed
	 * @return True if analysis was successfully started, false otherwise.
	 */
	bool RequestSessionAnalysis(const FTraceDescriptor& TraceDescriptor);
	void PauseSessionAnalysis()  { bSessionAnalysisPaused = true; }
	void ResumeSessionAnalysis()
	{
		bSessionAnalysisPaused = false;
		HitBreakpoint.Reset();
	}
	void StopSessionAnalysis();
	FTraceDescriptor GetSelectedTraceDescriptor() const { return ActiveSessionTraceDescriptor; }
	FText GetSelectedTraceDescription() const;

	void GetSessionInstances(TArray<UE::StateTreeDebugger::FInstanceDescriptor>& OutInstances) const;

	FOnStateTreeDebuggerNewSession OnNewSession;
	FOnStateTreeDebuggerNewInstance OnNewInstance;
	FOnStateTreeDebuggerDebuggedInstanceSet OnSelectedInstanceCleared;
	FOnStateTreeDebuggerScrubStateChanged OnScrubStateChanged;
	FOnStateTreeDebuggerBreakpointHit OnBreakpointHit;
	FOnStateTreeDebuggerActiveStatesChanges OnActiveStatesChanged;

protected:
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return true; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FStateTreeDebugger, STATGROUP_Tickables); }
	
private:
	enum class EAnalysisSourceType : uint8
	{
		// Analysis selected from available sessions
		SelectedSession,

		// Analysis automatically started from Editor new recording
		EditorSession
	};

	enum class EAnalysisTransitionType : uint8
	{
		Unset,
		NoneToSelected,
		NoneToEditor,
		EditorToSelected,
		EditorToEditor,
		SelectedToSelected,
		SelectedToEditor,
	};

	void UpdateAnalysisTransitionType(EAnalysisSourceType SourceType);
	
	void ReadTrace(double ScrubTime);
	void ReadTrace(uint64 FrameIndex);
	void ReadTrace(
		const TraceServices::IAnalysisSession& Session,
		const TraceServices::IFrameProvider& FrameProvider,
		const TraceServices::FFrame& Frame
		);

	/**
	 * Tests event for given instance id against breakpoints.
	 * @param InstanceId Id of the statetree instance that produces the event.
	 * @param Event The event received from the instance.
	 * @return True if a breakpoint has been it, false otherwise.
	 */
	bool EvaluateBreakpoints(FStateTreeInstanceDebugId InstanceId, const FStateTreeTraceEventVariantType& Event);

	void SendNotifications();

	void SetActiveStates(const FStateTreeTraceActiveStates& NewActiveStates);

	/**
	 * Request an analysis session on the latest next available live trace.
	 * This will replace the current analysis session if any.
	 */
	void RequestAnalysisOfLatestTrace();

	/**
	 * Looks for a new live traces to start an analysis session.
	 * On failure, if 'RetryPollingDuration is > 0', will retry connecting every frame for 'RetryPollingDuration' seconds 
	 * @param RetryPollingDuration - On failure, how many seconds to retry connecting during FStateTreeDebugger::Tick
	 * @return True if the analysis was successfully started; false otherwise.
	 */
	bool TryStartNewLiveSessionAnalysis(float RetryPollingDuration);

	bool StartSessionAnalysis(const FTraceDescriptor& TraceDescriptor);

	void SetScrubStateCollectionIndex(int32 EventCollectionIndex);

	/**
	 * Recompute index of the span that contains the active states change event and update the active states.
	 * This method handles unselected instances in which case it will reset the active states and set the span index to INDEX_NONE
	 * */
	void RefreshActiveStates();

	UE::Trace::FStoreClient* GetStoreClient() const;

	void UpdateInstances();

	bool ProcessEvent(const FStateTreeInstanceDebugId InstanceId, const TraceServices::FFrame& Frame, const FStateTreeTraceEventVariantType& Event);
	void AddEvents(double StartTime, double EndTime, const TraceServices::IFrameProvider& FrameProvider, const IStateTreeTraceProvider& StateTreeTraceProvider);
	void UpdateMetadata(FTraceDescriptor& TraceDescriptor) const;

	/** Module used to access the store client and analysis sessions .*/
	IStateTreeModule& StateTreeModule;

	/** The StateTree asset associated to this debugger. All instances will be using this asset. */ 
	TWeakObjectPtr<const UStateTree> StateTreeAsset;
	
	/** The trace analysis session. */
	TSharedPtr<const TraceServices::IAnalysisSession> AnalysisSession;

	/** Descriptor of the currently selected session */
	FTraceDescriptor ActiveSessionTraceDescriptor;

	/** Descriptors for all instances of the StateTree asset that have traces in the analysis session and still active. */
	TArray<UE::StateTreeDebugger::FInstanceDescriptor> InstanceDescs;

	/** Processed events for each instance. */
	TArray<UE::StateTreeDebugger::FInstanceEventCollection> EventCollections;

	/** Specific instance selected for more details */
	FStateTreeInstanceDebugId SelectedInstanceId;

	/** List of breakpoints set. This is per asset and not specific to an instance. */
	TArray<FStateTreeDebuggerBreakpoint> Breakpoints;

	/** List of currently active states in the selected instance */
	FStateTreeTraceActiveStates ActiveStates;

	/**
	 * When auto-connecting on next live session it is possible that a few frames are required for the tracing session to be accessible and connected to.
	 * This is to keep track of the previous last live session id so we can detect when the new one is available.
	 */
	int32 LastLiveSessionId = INDEX_NONE;
	
	/**
	 * When auto-connecting on next live session it is possible that a few frames are required for the tracing session to be accessible and connected to.
	 * This is to keep track of the time window where we will retry.
	 */
	float RetryLoadNextLiveSessionTimer = 0.0f;
	
	/** Recording duration of the analysis session in world recorded time. */
	double RecordingDuration = 0;

	/** Duration of the analysis session. This is not related to the world simulation time. */
	double AnalysisDuration = 0;

	/** Last time in the recording that we use to fetch events and we will use for the next read. */
	double LastTraceReadTime = 0;

	/** Combined information regarding current scrub time (e.g. frame index, event collection index, etc.) */ 
	UE::StateTreeDebugger::FScrubState ScrubState;

	/** Information stored when a breakpoint is hit while processing events and used to send notifications. */
	FHitBreakpoint HitBreakpoint;

	/** List of new instances discovered by processing event in the analysis session. */
	TArray<FStateTreeInstanceDebugId> NewInstances;

	/**
	 * Indicates that the debugger no longer process new events from the analysis session until it gets resumed.
	 * This can be an external explicit request or after hitting a breakpoint.
	 */
	bool bSessionAnalysisPaused = false;

	/**
	 * Indicates the last transition type between two consecutive analyses to manage track cleanup properly.
	 */
	EAnalysisTransitionType AnalysisTransitionType = EAnalysisTransitionType::Unset;

	/**
	 * Delegate Handle bound to UE::StateTree::Delegates::OnTracingStateChanged
	 */
	FDelegateHandle TracingStateChangedHandle;
};

#endif // WITH_STATETREE_DEBUGGER
