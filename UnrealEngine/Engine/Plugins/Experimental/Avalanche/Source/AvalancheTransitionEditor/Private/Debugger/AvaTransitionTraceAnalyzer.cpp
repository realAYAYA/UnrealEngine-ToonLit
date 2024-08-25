// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_STATETREE_DEBUGGER

#include "AvaTransitionTraceAnalyzer.h"
#include "AvaTransitionDebugger.h"
#include "AvaTransitionEditorLog.h"
#include "Containers/Ticker.h"
#include "Debugger/StateTreeDebuggerTypes.h"
#include "StateTree.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeModule.h"
#include "TraceServices/Model/AnalysisSession.h"

namespace UE::AvaTransitionEditor::Private
{
	EStateTreeTraceEventType GetEventType(const Trace::IAnalyzer::FOnEventContext& InContext)
	{
		return InContext.EventData.GetValue<EStateTreeTraceEventType>("EventType");
	}

	FStateTreeIndex16 GetAssetDebugId(const Trace::IAnalyzer::FOnEventContext& InContext)
	{
		return FStateTreeIndex16(InContext.EventData.GetValue<uint16>("AssetDebugId"));
	}

	FStateTreeInstanceDebugId GetInstanceDebugId(const Trace::IAnalyzer::FOnEventContext& InContext)
	{
		return FStateTreeInstanceDebugId(InContext.EventData.GetValue<uint32>("InstanceId"), InContext.EventData.GetValue<uint32>("InstanceSerial"));
	}
}

TArray<FAvaTransitionDebugger*> FAvaTransitionTraceAnalyzer::ActiveDebuggers;

const TArray<FAvaTransitionTraceAnalyzer::FEvent> FAvaTransitionTraceAnalyzer::Events =
{
	{ "AssetDebugIdEvent", &FAvaTransitionTraceAnalyzer::OnAssetDebugIdEvent },
	{ "InstanceEvent"    , &FAvaTransitionTraceAnalyzer::OnInstanceEvent },
	{ "StateEvent"       , &FAvaTransitionTraceAnalyzer::OnStateEvent },
};

void FAvaTransitionTraceAnalyzer::RegisterDebugger(FAvaTransitionDebugger* InDebugger)
{
	ActiveDebuggers.Add(InDebugger);

	if (IStateTreeModule::IsAvailable())
	{
		int32 TraceId = 0;
        IStateTreeModule::Get().StartTraces(TraceId);
	}
}

void FAvaTransitionTraceAnalyzer::UnregisterDebugger(FAvaTransitionDebugger* InDebugger)
{
	bool bRemovedDebugger = ActiveDebuggers.Remove(InDebugger) > 0;

	// Stop Traces only if the passed in Debugger was the last of the Active Debuggers
	if (bRemovedDebugger && ActiveDebuggers.IsEmpty() && IStateTreeModule::IsAvailable())
	{
		IStateTreeModule::Get().StopTraces();
	}
}

void FAvaTransitionTraceAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& InContext)
{
	for (int32 RouteId = 0; RouteId < Events.Num(); ++RouteId)
	{
		InContext.InterfaceBuilder.RouteEvent(RouteId, "StateTreeDebugger", Events[RouteId].Name);
	}
}

bool FAvaTransitionTraceAnalyzer::OnEvent(uint16 InRouteId, EStyle InStyle, const FOnEventContext& InContext)
{
	if (Events.IsValidIndex(InRouteId))
	{
		::Invoke(Events[InRouteId].Handler, this, InContext);
	}
	return true;
}

void FAvaTransitionTraceAnalyzer::ForEachDebugger(const TCHAR* InDebugName, TFunction<void(FAvaTransitionDebugger&)>&& InCallable)
{
	ExecuteOnGameThread(InDebugName,
		[Callable = MoveTemp(InCallable)]
		{
			for (FAvaTransitionDebugger* Debugger : ActiveDebuggers)
			{
				check(Debugger);
				Callable(*Debugger);
			}
		});
}

UStateTree* FAvaTransitionTraceAnalyzer::FindStateTree(const FStateTreeInstanceDebugId& InDebugInstanceId) const
{
	const FStateTreeIndex16* AssetDebugId = DebugInstances.Find(InDebugInstanceId);
	if (!AssetDebugId)
	{
		return nullptr;
	}

	const TWeakObjectPtr<UStateTree>* DebugAsset = DebugAssets.Find(*AssetDebugId);
	if (!DebugAsset)
	{
		return nullptr;
	}

	return DebugAsset->Get();
}

void FAvaTransitionTraceAnalyzer::OnAssetDebugIdEvent(const FOnEventContext& InContext)
{
	FString TreeName, TreePathName;
	InContext.EventData.GetString("TreeName", TreeName);
	InContext.EventData.GetString("TreePath", TreePathName);

	UE_LOG(LogAvaEditorTransition, Verbose, TEXT("OnAssetDebugIdEvent --- Attempting to find tree %s in path %s"), *TreeName, *TreePathName);

	UStateTree* StateTree;
	{
		FGCScopeGuard Guard;
		StateTree = FindObject<UStateTree>(nullptr, *TreePathName);
	}

	const uint32 CompiledDataHash = InContext.EventData.GetValue<uint32>("CompiledDataHash");
	if (StateTree && StateTree->LastCompiledEditorDataHash == CompiledDataHash)
	{
		FStateTreeIndex16 AssetDebugId = UE::AvaTransitionEditor::Private::GetAssetDebugId(InContext);
		DebugAssets.Add(AssetDebugId, StateTree);
	}
	else
	{
		UE_LOG(LogAvaEditorTransition, Warning, TEXT("Unable to find StateTree asset: %s : %s"), *TreePathName, *TreeName);
	}
}

void FAvaTransitionTraceAnalyzer::OnInstanceEvent(const FOnEventContext& InContext)
{
	EStateTreeTraceEventType EventType = UE::AvaTransitionEditor::Private::GetEventType(InContext);

	switch (EventType)
	{
	case EStateTreeTraceEventType::Push:
		OnTreeInstanceStarted(InContext);
		break;
	case EStateTreeTraceEventType::Pop:
		OnTreeInstanceStopped(InContext);
		break;
	}
}

void FAvaTransitionTraceAnalyzer::OnTreeInstanceStarted(const FOnEventContext& InContext)
{
	using namespace UE::AvaTransitionEditor;

	FStateTreeInstanceDebugId InstanceId = UE::AvaTransitionEditor::Private::GetInstanceDebugId(InContext);
	FStateTreeIndex16 AssetId = UE::AvaTransitionEditor::Private::GetAssetDebugId(InContext);

	DebugInstances.Add(InstanceId, AssetId);

	FString InstanceName;
	InContext.EventData.GetString("InstanceName", InstanceName);

	if (TWeakObjectPtr<UStateTree>* DebugAsset = DebugAssets.Find(AssetId))
	{
		ForEachDebugger(UE_SOURCE_LOCATION,
			[StateTreeWeak = *DebugAsset, InstanceId, InstanceName](FAvaTransitionDebugger& InDebugger)
			{
				if (UStateTree* StateTree = StateTreeWeak.Get())
				{
					InDebugger.OnTreeInstanceStarted(*StateTree, InstanceId, InstanceName);
				}
			});
	}
}

void FAvaTransitionTraceAnalyzer::OnTreeInstanceStopped(const FOnEventContext& InContext)
{
	FStateTreeInstanceDebugId InstanceDebugId = UE::AvaTransitionEditor::Private::GetInstanceDebugId(InContext);

	DebugInstances.Remove(InstanceDebugId);

	ForEachDebugger(UE_SOURCE_LOCATION,
		[InstanceDebugId](FAvaTransitionDebugger& InDebugger)
		{
			InDebugger.OnTreeInstanceStopped(InstanceDebugId);
		});
}

void FAvaTransitionTraceAnalyzer::OnStateEvent(const FOnEventContext& InContext)
{
	EStateTreeTraceEventType EventType = UE::AvaTransitionEditor::Private::GetEventType(InContext);

	// Ignore other events for now. Only concerned with enter/exit
	if (EventType != EStateTreeTraceEventType::OnEntered && EventType != EStateTreeTraceEventType::OnExited)
	{
		return;
	}

	FStateTreeInstanceDebugId InstanceDebugId = UE::AvaTransitionEditor::Private::GetInstanceDebugId(InContext);
	if (!InstanceDebugId.IsValid())
	{
		return;
	}

	UStateTree* StateTree = FindStateTree(InstanceDebugId);
	if (!StateTree)
	{
		return;
	}

	FStateTreeStateHandle StateHandle(InContext.EventData.GetValue<uint16>("StateIndex"));

	FGuid StateId = StateTree->GetStateIdFromHandle(StateHandle);
	if (!StateId.IsValid())
	{
		UE_LOG(LogAvaEditorTransition, Warning, TEXT("Could not find id of state with index %d"), StateHandle.Index);
		return;
	}

	ForEachDebugger(UE_SOURCE_LOCATION,
		[EventType, StateId, InstanceDebugId](FAvaTransitionDebugger& InDebugger)
		{
			switch (EventType)
			{
			case EStateTreeTraceEventType::OnEntered:
				InDebugger.OnNodeEntered(StateId, InstanceDebugId);
				break;

			case EStateTreeTraceEventType::OnExited:
				InDebugger.OnNodeExited(StateId, InstanceDebugId);
				break;
			}
		});
}

#endif // WITH_STATETREE_DEBUGGER
