// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_STATETREE_DEBUGGER

#include "Debugger/StateTreeDebugger.h"
#include "Debugger/IStateTreeTraceProvider.h"
#include "Debugger/StateTreeTraceProvider.h"
#include "Debugger/StateTreeTraceTypes.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "StateTreeDelegates.h"
#include "StateTreeModule.h"
#include "Trace/StoreClient.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/Frames.h"
#include "Trace/Analyzer.h"
#include "Trace/Analysis.h"
#include "TraceServices/Model/Diagnostics.h"
#include "GenericPlatform/GenericPlatformMisc.h"

#define LOCTEXT_NAMESPACE "StateTreeDebugger"

//----------------------------------------------------------------//
// UE::StateTreeDebugger
//----------------------------------------------------------------//
namespace UE::StateTreeDebugger
{
	struct FDiagnosticsSessionAnalyzer : public UE::Trace::IAnalyzer
	{
		virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override
		{
			auto& Builder = Context.InterfaceBuilder;
			Builder.RouteEvent(RouteId_Session2, "Diagnostics", "Session2");
		}

		virtual bool OnEvent(const uint16 RouteId, EStyle, const FOnEventContext& Context) override
		{
			const FEventData& EventData = Context.EventData;

			switch (RouteId)
			{
			case RouteId_Session2:
				{
					EventData.GetString("Platform", SessionInfo.Platform);
					EventData.GetString("AppName", SessionInfo.AppName);
					EventData.GetString("CommandLine", SessionInfo.CommandLine);
					EventData.GetString("Branch", SessionInfo.Branch);
					EventData.GetString("BuildVersion", SessionInfo.BuildVersion);
					SessionInfo.Changelist = EventData.GetValue<uint32>("Changelist", 0);
					SessionInfo.ConfigurationType = (EBuildConfiguration) EventData.GetValue<uint8>("ConfigurationType");
					SessionInfo.TargetType = (EBuildTargetType) EventData.GetValue<uint8>("TargetType");

					return false;
				}
			default: ;
			}

			return true;
		}

		enum : uint16
		{
			RouteId_Session2,
		};

		TraceServices::FSessionInfo SessionInfo;
	};

} // UE::StateTreeDebugger


//----------------------------------------------------------------//
// FStateTreeDebugger
//----------------------------------------------------------------//
FStateTreeDebugger::FStateTreeDebugger()
	: StateTreeModule(FModuleManager::GetModuleChecked<IStateTreeModule>("StateTreeModule"))
	, ScrubState(EventCollections)
{
	TracingStateChangedHandle = UE::StateTree::Delegates::OnTracingStateChanged.AddLambda([this](const bool bTracesEnabled)
		{
			// StateTree traces got enabled in the current process so let's analyse it if not already analysing something.
			if (bTracesEnabled && !IsAnalysisSessionActive())
			{
				RequestAnalysisOfLatestTrace();
			}
		});
}

FStateTreeDebugger::~FStateTreeDebugger()
{
	UE::StateTree::Delegates::OnTracingStateChanged.Remove(TracingStateChangedHandle);
	TracingStateChangedHandle.Reset();

	StopSessionAnalysis();
}

void FStateTreeDebugger::Tick(const float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FStateTreeDebugger::Tick);

	if (RetryLoadNextLiveSessionTimer > 0.0f)
	{
		// We are still not connected to the last live session.
		// Update polling timer and retry with remaining time; 0 or less will stop retries.
		if (TryStartNewLiveSessionAnalysis(RetryLoadNextLiveSessionTimer - DeltaTime))
		{
			RetryLoadNextLiveSessionTimer = 0.0f;
			LastLiveSessionId = INDEX_NONE;
		}
	}
	
	UpdateInstances();

	if (bSessionAnalysisPaused == false && StateTreeAsset.IsValid())
	{
		SyncToCurrentSessionDuration();
	}
}

void FStateTreeDebugger::StopSessionAnalysis()
{
	if (const TraceServices::IAnalysisSession* Session = GetAnalysisSession())
	{
		Session->Stop(true);
		AnalysisSession.Reset();
	}

	bSessionAnalysisPaused = false;
	HitBreakpoint.Reset();
}

void FStateTreeDebugger::SyncToCurrentSessionDuration()
{
	if (const TraceServices::IAnalysisSession* Session = GetAnalysisSession())
	{
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);
			AnalysisDuration = Session->GetDurationSeconds();
		}
		ReadTrace(AnalysisDuration);
	}
}

const UE::StateTreeDebugger::FInstanceDescriptor* FStateTreeDebugger::GetInstanceDescriptor(const FStateTreeInstanceDebugId InstanceId) const
{
	const UE::StateTreeDebugger::FInstanceDescriptor* FoundDescriptor = InstanceDescs.FindByPredicate(
		[InstanceId](const UE::StateTreeDebugger::FInstanceDescriptor& Descriptor)
		{
			return Descriptor.Id == InstanceId;
		});

	return FoundDescriptor;
}

FText FStateTreeDebugger::GetInstanceName(const FStateTreeInstanceDebugId InstanceId) const
{
	const UE::StateTreeDebugger::FInstanceDescriptor* FoundDescriptor = GetInstanceDescriptor(InstanceId);
	return (FoundDescriptor != nullptr) ? FText::FromString(FoundDescriptor->Name) : LOCTEXT("InstanceNotFound","Instance not found");
}

FText FStateTreeDebugger::GetInstanceDescription(const FStateTreeInstanceDebugId InstanceId) const
{
	const UE::StateTreeDebugger::FInstanceDescriptor* FoundDescriptor = GetInstanceDescriptor(InstanceId);
	return (FoundDescriptor != nullptr) ? DescribeInstance(*FoundDescriptor) : LOCTEXT("InstanceNotFound","Instance not found");
}

void FStateTreeDebugger::SelectInstance(const FStateTreeInstanceDebugId InstanceId)
{
	if (SelectedInstanceId != InstanceId)
	{
		SelectedInstanceId = InstanceId;

		// Notify so listener can cleanup anything related to previous instance
		OnSelectedInstanceCleared.ExecuteIfBound();

		// Update event collection index for newly debugged instance
		SetScrubStateCollectionIndex(InstanceId.IsValid()
			? EventCollections.IndexOfByPredicate([InstanceId = InstanceId](const UE::StateTreeDebugger::FInstanceEventCollection& Entry)
				{
					return Entry.InstanceId == InstanceId;
				})
			: INDEX_NONE);
	}
}

void FStateTreeDebugger::GetSessionInstances(TArray<UE::StateTreeDebugger::FInstanceDescriptor>& OutInstances) const
{
	if (const TraceServices::IAnalysisSession* Session = GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

		if (const IStateTreeTraceProvider* Provider = Session->ReadProvider<IStateTreeTraceProvider>(FStateTreeTraceProvider::ProviderName))
		{
			Provider->GetInstances(OutInstances);
		}
	}
}

void FStateTreeDebugger::UpdateInstances()
{
	if (const TraceServices::IAnalysisSession* Session = GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

		if (const IStateTreeTraceProvider* Provider = Session->ReadProvider<IStateTreeTraceProvider>(FStateTreeTraceProvider::ProviderName))
		{
			Provider->GetInstances(InstanceDescs);
		}
	}
}

bool FStateTreeDebugger::RequestAnalysisOfEditorSession()
{
	// Get snapshot of current trace to help identify the next live one
	TArray<FTraceDescriptor> TraceDescriptors;
	GetLiveTraces(TraceDescriptors);
	LastLiveSessionId = TraceDescriptors.Num() ? TraceDescriptors.Last().TraceId : INDEX_NONE;

	// 0 is the invalid value used for Trace Id
	constexpr int32 InvalidTraceId = 0;
	int32 ActiveTraceId = InvalidTraceId;

	// StartTraces returns true if a new connection was created. In this case we will receive OnTracingStateChanged
	// and we'll try to start an analysis on that new connection as soon as possible.
	// Otherwise it might have been able to use an active connection in which case it was returned in the output parameter.
	if (StateTreeModule.StartTraces(ActiveTraceId))
	{
		return true;
	}

	// Otherwise we start analysis of the already active trace, if any.
	if (ActiveTraceId != InvalidTraceId)
	{
		if (const FTraceDescriptor* Descriptor = TraceDescriptors.FindByPredicate([ActiveTraceId](const FTraceDescriptor& Descriptor)
			{
				return Descriptor.TraceId == ActiveTraceId;
			}))
		{
			return RequestSessionAnalysis(*Descriptor);
		}
	}

	return false;
}

void FStateTreeDebugger::RequestAnalysisOfLatestTrace()
{
	// Invalidate our current active session
	ActiveSessionTraceDescriptor = FTraceDescriptor();

	// Stop current analysis if any
	StopSessionAnalysis();

	// This might not succeed immediately but will schedule next retry if necessary
	TryStartNewLiveSessionAnalysis(1.0f);
}


bool FStateTreeDebugger::TryStartNewLiveSessionAnalysis(const float RetryPollingDuration)
{
	TArray<FTraceDescriptor> Traces;
	GetLiveTraces(Traces);

	if (Traces.Num() && Traces.Last().TraceId != LastLiveSessionId)
	{
		// Intentional call to StartSessionAnalysis instead of RequestSessionAnalysis since we want
		// to set 'bIsAnalyzingNextEditorSession' before calling OnNewSession delegate.
		const bool bStarted = StartSessionAnalysis(Traces.Last());
		if (bStarted)
		{
			UpdateAnalysisTransitionType(EAnalysisSourceType::EditorSession);

			SetScrubStateCollectionIndex(INDEX_NONE);
			OnNewSession.ExecuteIfBound();
		}

		return bStarted;
	}
	
	RetryLoadNextLiveSessionTimer = RetryPollingDuration;
	UE_CLOG(RetryLoadNextLiveSessionTimer > 0, LogStateTree, Log, TEXT("Unable to start analysis for the most recent live session."));

	return false;
}

bool FStateTreeDebugger::StartSessionAnalysis(const FTraceDescriptor& TraceDescriptor)
{
	if (ActiveSessionTraceDescriptor == TraceDescriptor)
	{
		return ActiveSessionTraceDescriptor.IsValid();
	}

	ActiveSessionTraceDescriptor = FTraceDescriptor();

	// Make sure any active analysis is stopped
	StopSessionAnalysis();

	UE::Trace::FStoreClient* StoreClient = GetStoreClient();
	if (StoreClient == nullptr)
	{
		return false;
	}

	// If new trace descriptor is not valid no need to continue
	if (TraceDescriptor.IsValid() == false)
	{
		return false;
	}

	RecordingDuration = 0;
	AnalysisDuration = 0;
	LastTraceReadTime = 0;

	const uint32 TraceId = TraceDescriptor.TraceId;

	// Make sure it is still live
	const UE::Trace::FStoreClient::FSessionInfo* SessionInfo = StoreClient->GetSessionInfoByTraceId(TraceId);
	if (SessionInfo != nullptr)
	{
		UE::Trace::FStoreClient::FTraceData TraceData = StoreClient->ReadTrace(TraceId);
		if (!TraceData)
		{
			return false;
		}

		FString TraceName(StoreClient->GetStatus()->GetStoreDir());
		const UE::Trace::FStoreClient::FTraceInfo* TraceInfo = StoreClient->GetTraceInfoById(TraceId);
		if (TraceInfo != nullptr)
		{
			FString Name(TraceInfo->GetName());
			if (!Name.EndsWith(TEXT(".utrace")))
			{
				Name += TEXT(".utrace");
			}
			TraceName = FPaths::Combine(TraceName, Name);
			FPaths::NormalizeFilename(TraceName);
		}
		
		ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>("TraceServices");
		if (const TSharedPtr<TraceServices::IAnalysisService> TraceAnalysisService = TraceServicesModule.GetAnalysisService())
		{
			checkf(!AnalysisSession.IsValid(), TEXT("Must make sure that current session was properly stopped before starting a new one otherwise it can cause threading issues"));
			AnalysisSession = TraceAnalysisService->StartAnalysis(TraceId, *TraceName, MoveTemp(TraceData));
		}

		if (AnalysisSession.IsValid())
		{
			ActiveSessionTraceDescriptor = TraceDescriptor;
		}
	}

	return ActiveSessionTraceDescriptor.IsValid();
}

void FStateTreeDebugger::SetScrubStateCollectionIndex(const int32 EventCollectionIndex)
{
	ScrubState.SetEventCollectionIndex(EventCollectionIndex);

	OnScrubStateChanged.ExecuteIfBound(ScrubState);

	RefreshActiveStates();
}

void FStateTreeDebugger::GetLiveTraces(TArray<FTraceDescriptor>& OutTraceDescriptors) const
{
	UE::Trace::FStoreClient* StoreClient = GetStoreClient();
	if (StoreClient == nullptr)
	{
		return;
	}

	OutTraceDescriptors.Reset();

	const uint32 SessionCount = StoreClient->GetSessionCount();
	for (uint32 SessionIndex = 0; SessionIndex < SessionCount; ++SessionIndex)
	{
		const UE::Trace::FStoreClient::FSessionInfo* SessionInfo = StoreClient->GetSessionInfo(SessionIndex);
		if (SessionInfo != nullptr)
		{
			const uint32 TraceId = SessionInfo->GetTraceId();
			const UE::Trace::FStoreClient::FTraceInfo* TraceInfo = StoreClient->GetTraceInfoById(TraceId);
			if (TraceInfo != nullptr)
			{
				FTraceDescriptor& Trace = OutTraceDescriptors.AddDefaulted_GetRef();
				Trace.TraceId = TraceId;
				Trace.Name = FString(TraceInfo->GetName());
				UpdateMetadata(Trace);
			}
		}
	}
}

void FStateTreeDebugger::UpdateMetadata(FTraceDescriptor& TraceDescriptor) const
{
	UE::Trace::FStoreClient* StoreClient = GetStoreClient();
	if (StoreClient == nullptr)
	{
		return;
	}

	const UE::Trace::FStoreClient::FTraceData TraceData = StoreClient->ReadTrace(TraceDescriptor.TraceId);
	if (!TraceData)
	{
		return;
	}

	// inspired from FStoreBrowser
	struct FDataStream : public UE::Trace::IInDataStream
	{
		enum class EReadStatus
		{
			Ready = 0,
			StoppedByReadSizeLimit
		};

		virtual int32 Read(void* Data, const uint32 Size) override
		{
			if (BytesRead >= 1024 * 1024)
			{
				Status = EReadStatus::StoppedByReadSizeLimit;
				return 0;
			}
			const int32 InnerBytesRead = Inner->Read(Data, Size);
			BytesRead += InnerBytesRead;

			return InnerBytesRead;
		}

		virtual void Close() override
		{
			Inner->Close();
		}

		IInDataStream* Inner = nullptr;
		int32 BytesRead = 0;
		EReadStatus Status = EReadStatus::Ready;
	};

	FDataStream DataStream;
	DataStream.Inner = TraceData.Get();

	UE::StateTreeDebugger::FDiagnosticsSessionAnalyzer Analyzer;
	UE::Trace::FAnalysisContext Context;
	Context.AddAnalyzer(Analyzer);
	Context.Process(DataStream).Wait();

	TraceDescriptor.SessionInfo = Analyzer.SessionInfo;
}

FText FStateTreeDebugger::GetSelectedTraceDescription() const
{
	if (ActiveSessionTraceDescriptor.IsValid())
	{
		return DescribeTrace(ActiveSessionTraceDescriptor);
	}

	return LOCTEXT("NoSelectedTraceDescriptor", "No trace selected");
}

void FStateTreeDebugger::SetScrubTime(const double ScrubTime)
{
	if (ScrubState.SetScrubTime(ScrubTime))
	{
		OnScrubStateChanged.ExecuteIfBound(ScrubState);

		RefreshActiveStates();
	}
}

bool FStateTreeDebugger::IsActiveInstance(const double Time, const FStateTreeInstanceDebugId InstanceId) const
{
	for (const UE::StateTreeDebugger::FInstanceDescriptor& Desc : InstanceDescs)
	{
		if (Desc.Id == InstanceId && Desc.Lifetime.Contains(Time))
		{
			return true;
		}
	}
	return false;
}

FText FStateTreeDebugger::DescribeTrace(const FTraceDescriptor& TraceDescriptor)
{
	if (TraceDescriptor.IsValid())
	{
		const TraceServices::FSessionInfo& SessionInfo = TraceDescriptor.SessionInfo;

		return FText::FromString(FString::Printf(TEXT("%s-%s-%s-%s-%s"),
			*LexToString(TraceDescriptor.TraceId),
			*SessionInfo.Platform,
			*SessionInfo.AppName,
			LexToString(SessionInfo.ConfigurationType),
			LexToString(SessionInfo.TargetType)));
	}

	return LOCTEXT("InvalidTraceDescriptor", "Invalid");
}

FText FStateTreeDebugger::DescribeInstance(const UE::StateTreeDebugger::FInstanceDescriptor& InstanceDesc)
{
	if (InstanceDesc.IsValid() == false)
	{
		return LOCTEXT("NoSelectedInstanceDescriptor", "No instance selected");
	}
	return FText::FromString(LexToString(InstanceDesc));
}

void FStateTreeDebugger::SetActiveStates(const FStateTreeTraceActiveStates& NewActiveStates)
{
	ActiveStates = NewActiveStates;
	OnActiveStatesChanged.ExecuteIfBound(ActiveStates);
}

void FStateTreeDebugger::RefreshActiveStates()
{
	if (ScrubState.IsPointingToValidActiveStates())
	{
		const UE::StateTreeDebugger::FInstanceEventCollection& EventCollection = EventCollections[ScrubState.GetEventCollectionIndex()];
		const int32 EventIndex = EventCollection.ActiveStatesChanges[ScrubState.GetActiveStatesIndex()].EventIndex;
		SetActiveStates(EventCollection.Events[EventIndex].Get<FStateTreeTraceActiveStatesEvent>().ActiveStates);
	}
	else
	{
		SetActiveStates(FStateTreeTraceActiveStates());	
	}
}

bool FStateTreeDebugger::CanStepBackToPreviousStateWithEvents() const
{
	return ScrubState.HasPreviousFrame();
}

void FStateTreeDebugger::StepBackToPreviousStateWithEvents()
{
	ScrubState.GotoPreviousFrame();
	OnScrubStateChanged.Execute(ScrubState);
	
	RefreshActiveStates();
}

bool FStateTreeDebugger::CanStepForwardToNextStateWithEvents() const
{
	return ScrubState.HasNextFrame();
}

void FStateTreeDebugger::StepForwardToNextStateWithEvents()
{
	ScrubState.GotoNextFrame();
	OnScrubStateChanged.Execute(ScrubState);

	RefreshActiveStates();
}

bool FStateTreeDebugger::CanStepBackToPreviousStateChange() const
{
	return ScrubState.HasPreviousActiveStates();
}

void FStateTreeDebugger::StepBackToPreviousStateChange()
{
	ScrubState.GotoPreviousActiveStates();
	OnScrubStateChanged.Execute(ScrubState);
	
	RefreshActiveStates();
}

bool FStateTreeDebugger::CanStepForwardToNextStateChange() const
{
	return ScrubState.HasNextActiveStates();
}

void FStateTreeDebugger::StepForwardToNextStateChange()
{
	ScrubState.GotoNextActiveStates();
	OnScrubStateChanged.Execute(ScrubState);

	RefreshActiveStates();
}

bool FStateTreeDebugger::HasStateBreakpoint(const FStateTreeStateHandle StateHandle, const EStateTreeBreakpointType BreakpointType) const
{
	return Breakpoints.ContainsByPredicate([StateHandle, BreakpointType](const FStateTreeDebuggerBreakpoint Breakpoint)
		{
			if (Breakpoint.BreakpointType == BreakpointType)
			{
				const FStateTreeStateHandle* BreakpointStateHandle = Breakpoint.ElementIdentifier.TryGet<FStateTreeStateHandle>();
				return (BreakpointStateHandle != nullptr && *BreakpointStateHandle == StateHandle);
			}
			return false; 
		});
}

bool FStateTreeDebugger::HasTaskBreakpoint(const FStateTreeIndex16 Index, const EStateTreeBreakpointType BreakpointType) const
{
	return Breakpoints.ContainsByPredicate([Index, BreakpointType](const FStateTreeDebuggerBreakpoint Breakpoint)
	{
		if (Breakpoint.BreakpointType == BreakpointType)
		{
			const FStateTreeDebuggerBreakpoint::FStateTreeTaskIndex* BreakpointTaskIndex = Breakpoint.ElementIdentifier.TryGet<FStateTreeDebuggerBreakpoint::FStateTreeTaskIndex>();
			return (BreakpointTaskIndex != nullptr && BreakpointTaskIndex->Index == Index);
		}
		return false; 
	});
}

bool FStateTreeDebugger::HasTransitionBreakpoint(const FStateTreeIndex16 Index, const EStateTreeBreakpointType BreakpointType) const
{
	return Breakpoints.ContainsByPredicate([Index, BreakpointType](const FStateTreeDebuggerBreakpoint Breakpoint)
	{
		if (Breakpoint.BreakpointType == BreakpointType)
		{
			const FStateTreeDebuggerBreakpoint::FStateTreeTransitionIndex* BreakpointTransitionIndex = Breakpoint.ElementIdentifier.TryGet<FStateTreeDebuggerBreakpoint::FStateTreeTransitionIndex>();
			return (BreakpointTransitionIndex != nullptr && BreakpointTransitionIndex->Index == Index);
		}
		return false; 
	});
}

void FStateTreeDebugger::SetStateBreakpoint(const FStateTreeStateHandle StateHandle, const EStateTreeBreakpointType BreakpointType)
{
	Breakpoints.Emplace(StateHandle, BreakpointType);
}

void FStateTreeDebugger::SetTransitionBreakpoint(const FStateTreeIndex16 TransitionIndex, const EStateTreeBreakpointType BreakpointType)
{
	Breakpoints.Emplace(FStateTreeDebuggerBreakpoint::FStateTreeTransitionIndex(TransitionIndex), BreakpointType);
}

void FStateTreeDebugger::SetTaskBreakpoint(const FStateTreeIndex16 NodeIndex, const EStateTreeBreakpointType BreakpointType)
{
	Breakpoints.Emplace(FStateTreeDebuggerBreakpoint::FStateTreeTaskIndex(NodeIndex), BreakpointType);
}

void FStateTreeDebugger::ClearBreakpoint(const FStateTreeIndex16 NodeIndex, const EStateTreeBreakpointType BreakpointType)
{
	const int32 Index = Breakpoints.IndexOfByPredicate([NodeIndex, BreakpointType](const FStateTreeDebuggerBreakpoint& Breakpoint)
		{
			const FStateTreeDebuggerBreakpoint::FStateTreeTaskIndex* IndexPtr = Breakpoint.ElementIdentifier.TryGet<FStateTreeDebuggerBreakpoint::FStateTreeTaskIndex>();
			return (IndexPtr != nullptr && IndexPtr->Index == NodeIndex && Breakpoint.BreakpointType == BreakpointType);
		});

	if (Index != INDEX_NONE)
	{
		Breakpoints.RemoveAtSwap(Index);
	}
}

void FStateTreeDebugger::ClearAllBreakpoints()
{
	Breakpoints.Empty();
}

const TraceServices::IAnalysisSession* FStateTreeDebugger::GetAnalysisSession() const
{
	return AnalysisSession.Get();
}

bool FStateTreeDebugger::RequestSessionAnalysis(const FTraceDescriptor& TraceDescriptor)
{
	if (StartSessionAnalysis(TraceDescriptor))
	{
		UpdateAnalysisTransitionType(EAnalysisSourceType::SelectedSession);
		
		SetScrubStateCollectionIndex(INDEX_NONE);
		OnNewSession.ExecuteIfBound();
		return true;
	}
	return false;
}

void FStateTreeDebugger::UpdateAnalysisTransitionType(const EAnalysisSourceType SourceType)
{
	switch (AnalysisTransitionType)
	{
	case EAnalysisTransitionType::Unset:
		AnalysisTransitionType = (SourceType == EAnalysisSourceType::SelectedSession)
				? EAnalysisTransitionType::NoneToSelected
				: EAnalysisTransitionType::NoneToEditor;
		break;

	case EAnalysisTransitionType::NoneToSelected:
	case EAnalysisTransitionType::EditorToSelected:
	case EAnalysisTransitionType::SelectedToSelected:
		AnalysisTransitionType = (SourceType == EAnalysisSourceType::SelectedSession)
				? EAnalysisTransitionType::SelectedToSelected
				: EAnalysisTransitionType::SelectedToEditor;
		break;

	case EAnalysisTransitionType::NoneToEditor:
	case EAnalysisTransitionType::EditorToEditor:
	case EAnalysisTransitionType::SelectedToEditor:
		AnalysisTransitionType = (SourceType == EAnalysisSourceType::SelectedSession)
				? EAnalysisTransitionType::EditorToSelected
				: EAnalysisTransitionType::EditorToEditor;
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled transition type."));
	}
}

UE::Trace::FStoreClient* FStateTreeDebugger::GetStoreClient() const
{
	return StateTreeModule.GetStoreClient();
}

void FStateTreeDebugger::ReadTrace(const uint64 FrameIndex)
{
	if (const TraceServices::IAnalysisSession* Session = GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

		const TraceServices::IFrameProvider& FrameProvider = ReadFrameProvider(*Session);

		if (const TraceServices::FFrame* TargetFrame = FrameProvider.GetFrame(TraceFrameType_Game, FrameIndex))
		{
			ReadTrace(*Session, FrameProvider, *TargetFrame);
		}
	}

	// Notify outside session read scope
	SendNotifications();
}

void FStateTreeDebugger::ReadTrace(const double ScrubTime)
{
	if (const TraceServices::IAnalysisSession* Session = GetAnalysisSession())
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

		const TraceServices::IFrameProvider& FrameProvider = ReadFrameProvider(*Session);

		TraceServices::FFrame TargetFrame;
		if (FrameProvider.GetFrameFromTime(TraceFrameType_Game, ScrubTime, TargetFrame))
		{
			// Process only completed frames 
			if (TargetFrame.EndTime == std::numeric_limits<double>::infinity())
			{
				if (const TraceServices::FFrame* PreviousCompleteFrame = FrameProvider.GetFrame(TraceFrameType_Game, TargetFrame.Index - 1))
				{
					ReadTrace(*Session, FrameProvider, *PreviousCompleteFrame);
				}
			}
			else
			{
				ReadTrace(*Session, FrameProvider, TargetFrame);
			}
		}
	}

	// Notify outside session read scope
	SendNotifications();
}

void FStateTreeDebugger::SendNotifications()
{
	if (NewInstances.Num() > 0)
	{
		for (const FStateTreeInstanceDebugId NewInstanceId : NewInstances)
		{
			OnNewInstance.ExecuteIfBound(NewInstanceId);
		}
		NewInstances.Reset();
	}

	if (HitBreakpoint.IsSet())
	{
		check(HitBreakpoint.InstanceId.IsValid());
		check(Breakpoints.IsValidIndex(HitBreakpoint.Index));

		// Force scrub time to latest simulation time to reflect most recent events.
		// This will notify scrub position changed and active states
		SetScrubTime(HitBreakpoint.Time);

		// Make sure the instance is selected in case the breakpoint was set for any instances 
		if (SelectedInstanceId != HitBreakpoint.InstanceId)
		{
			SelectInstance(HitBreakpoint.InstanceId);
		}

		OnBreakpointHit.ExecuteIfBound(HitBreakpoint.InstanceId, Breakpoints[HitBreakpoint.Index]);

		PauseSessionAnalysis();
	}
}

void FStateTreeDebugger::ReadTrace(
	const TraceServices::IAnalysisSession& Session,
	const TraceServices::IFrameProvider& FrameProvider,
	const TraceServices::FFrame& Frame
	)
{
	TraceServices::FFrame LastReadFrame;
	const bool bValidLastReadFrame = FrameProvider.GetFrameFromTime(TraceFrameType_Game, LastTraceReadTime, LastReadFrame);
	if (LastTraceReadTime == 0 || (bValidLastReadFrame && Frame.Index > LastReadFrame.Index))
	{
		if (const IStateTreeTraceProvider* Provider = Session.ReadProvider<IStateTreeTraceProvider>(FStateTreeTraceProvider::ProviderName))
		{
			AddEvents(LastTraceReadTime, Frame.EndTime, FrameProvider, *Provider);
			LastTraceReadTime = Frame.EndTime;
		}
	}
}

bool FStateTreeDebugger::EvaluateBreakpoints(const FStateTreeInstanceDebugId InstanceId, const FStateTreeTraceEventVariantType& Event)
{
	if (StateTreeAsset == nullptr // asset is required to properly match state handles
		|| HitBreakpoint.IsSet() // Only consider first hit breakpoint in the frame
		|| Breakpoints.IsEmpty()
		|| (SelectedInstanceId.IsValid() && InstanceId != SelectedInstanceId)) // ignore events not for the selected instances		
	{
		return false;
	}

	for (int BreakpointIndex = 0; BreakpointIndex < Breakpoints.Num(); ++BreakpointIndex)
	{
		const FStateTreeDebuggerBreakpoint Breakpoint = Breakpoints[BreakpointIndex];
		if (Breakpoint.IsMatchingEvent(Event))
		{
			HitBreakpoint.Index = BreakpointIndex;
            HitBreakpoint.InstanceId = InstanceId;
            HitBreakpoint.Time = RecordingDuration;
		}
	}

	return HitBreakpoint.IsSet();
}

bool FStateTreeDebugger::ProcessEvent(const FStateTreeInstanceDebugId InstanceId, const TraceServices::FFrame& Frame, const FStateTreeTraceEventVariantType& Event)
{
	UE::StateTreeDebugger::FInstanceEventCollection* ExistingCollection = EventCollections.FindByPredicate(
		[InstanceId](const UE::StateTreeDebugger::FInstanceEventCollection& Entry)
		{
			return Entry.InstanceId == InstanceId;
		});

	// Create missing EventCollection if necessary
	if (ExistingCollection == nullptr)
	{
		// Push deferred notification for new instance Id
		NewInstances.Push(InstanceId);

		ExistingCollection = &EventCollections.Emplace_GetRef(InstanceId);

		// Update the active event collection index when it's newly created for the currently debugged instance.
		// Otherwise (i.e. EventCollection already exists) it is updated when switching instance (i.e. SelectInstance)
		if (SelectedInstanceId == InstanceId && ScrubState.GetEventCollectionIndex() == INDEX_NONE)
		{
			SetScrubStateCollectionIndex(EventCollections.Num()-1);
		}
	}

	check(ExistingCollection);
	TArray<FStateTreeTraceEventVariantType>& Events = ExistingCollection->Events;

	// Add new frame span if none added yet or new frame
	if (ExistingCollection->FrameSpans.IsEmpty() || ExistingCollection->FrameSpans.Last().Frame.Index < Frame.Index)
	{
		double RecordingWorldTime = 0;
		Visit([&RecordingWorldTime](auto& TypedEvent)
			{
				RecordingWorldTime = TypedEvent.RecordingWorldTime;
			}, Event);

		// Update global recording duration
		RecordingDuration = RecordingWorldTime;

		ExistingCollection->FrameSpans.Add(UE::StateTreeDebugger::FFrameSpan(Frame, RecordingWorldTime, Events.Num()));
	}

	// Add activate states change info
	if (Event.IsType<FStateTreeTraceActiveStatesEvent>())
	{
		checkf(ExistingCollection->FrameSpans.Num() > 0, TEXT("Expecting to always be in a frame span at this point."));
		const int32 FrameSpanIndex = ExistingCollection->FrameSpans.Num()-1;

		// Add new entry for the first event or if the last event is for a different frame
		if (ExistingCollection->ActiveStatesChanges.IsEmpty()
			|| ExistingCollection->ActiveStatesChanges.Last().SpanIndex != FrameSpanIndex)
		{
			ExistingCollection->ActiveStatesChanges.Push({FrameSpanIndex, Events.Num()});
		}
		else
		{
			// Multiple events for change of active states in the same frame, keep the last one until we implement scrubbing within a frame
			ExistingCollection->ActiveStatesChanges.Last().EventIndex = Events.Num();
		}
	}

	// Store event in the collection
	Events.Emplace(Event);

	// Process at the end so RecordingDuration is up to date and we can associate it to a hit breakpoint if necessary.
	EvaluateBreakpoints(InstanceId, Event);

	return /*bKeepProcessing*/true;
}

const UE::StateTreeDebugger::FInstanceEventCollection& FStateTreeDebugger::GetEventCollection(FStateTreeInstanceDebugId InstanceId) const\
{
	using namespace UE::StateTreeDebugger;
	const FInstanceEventCollection* ExistingCollection = EventCollections.FindByPredicate([InstanceId](const FInstanceEventCollection& Entry)
	{
		return Entry.InstanceId == InstanceId;
	});

	return ExistingCollection != nullptr ? *ExistingCollection : FInstanceEventCollection::Invalid;
}

void FStateTreeDebugger::ResetEventCollections()
{
	EventCollections.Reset();
	SetScrubStateCollectionIndex(INDEX_NONE);
}

void FStateTreeDebugger::AddEvents(const double StartTime, const double EndTime, const TraceServices::IFrameProvider& FrameProvider, const IStateTreeTraceProvider& StateTreeTraceProvider)
{
	check(StateTreeAsset.IsValid());
	StateTreeTraceProvider.ReadTimelines(*StateTreeAsset,
		[this, StartTime, EndTime, &FrameProvider](const FStateTreeInstanceDebugId InstanceId, const IStateTreeTraceProvider::FEventsTimeline& TimelineData)
		{
			// Keep track of the frames containing events. Starting with an invalid frame.
			TraceServices::FFrame Frame;
			Frame.Index = INDEX_NONE;

			TimelineData.EnumerateEvents(StartTime,	EndTime,
				[this, InstanceId, &FrameProvider, &Frame](const double EventStartTime, const double EventEndTime, uint32 InDepth, const FStateTreeTraceEventVariantType& Event)
				{
					bool bValidFrame = true;

					// Fetch frame when not set yet or if events no longer part of the current one
					if (Frame.Index == INDEX_NONE ||
						(EventEndTime < Frame.StartTime || Frame.EndTime < EventStartTime))
					{
						bValidFrame = FrameProvider.GetFrameFromTime(TraceFrameType_Game, EventStartTime, Frame);

						if (bValidFrame == false)
						{
							// Edge case for events from a missing first complete frame.
							// (i.e. FrameProvider didn't get BeginFrame event but StateTreeEvent were sent in that frame)
							// Doing this will merge our two first frames of state tree events using the same recording world time
							// but this should happen only for late start recording.
							const TraceServices::FFrame* FirstFrame = FrameProvider.GetFrame(TraceFrameType_Game, 0);
							if (FirstFrame != nullptr && EventEndTime < FirstFrame->StartTime)
							{
								Frame = *FirstFrame;
								bValidFrame = true;
							}
						}
					}

					if (bValidFrame)
					{
						const bool bKeepProcessing = ProcessEvent(InstanceId, Frame, Event);
						return bKeepProcessing ? TraceServices::EEventEnumerate::Continue : TraceServices::EEventEnumerate::Stop;
					}

					// Skip events outside of game frames
					return TraceServices::EEventEnumerate::Continue;
				});
		});
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_STATETREE_DEBUGGER
