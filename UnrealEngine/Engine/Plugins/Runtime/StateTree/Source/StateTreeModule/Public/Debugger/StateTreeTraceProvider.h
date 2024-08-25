// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_STATETREE_DEBUGGER

#include "IStateTreeTraceProvider.h"
#include "Model/PointTimeline.h"
#include "StateTreeIndexTypes.h"

namespace TraceServices { class IAnalysisSession; }
class UStateTree;
struct FStateTreeInstanceDebugId;
namespace UE::StateTreeDebugger { struct FInstanceDescriptor; }

class FStateTreeTraceProvider : public IStateTreeTraceProvider
{
public:
	static FName ProviderName;

	explicit FStateTreeTraceProvider(TraceServices::IAnalysisSession& InSession);
	
	void AppendEvent(FStateTreeInstanceDebugId InInstanceId, double InTime, const FStateTreeTraceEventVariantType& InEvent);
	void AppendInstanceEvent(
		const FStateTreeIndex16 AssetDebugId,
		const FStateTreeInstanceDebugId InInstanceId,
		const TCHAR* InInstanceName,
		double InTime,
		double InWorldRecordingTime,
		EStateTreeTraceEventType InEventType);

	void AppendAssetDebugId(const UStateTree* InStateTree, const FStateTreeIndex16 AssetDebugId);
	bool GetAssetFromDebugId(const FStateTreeIndex16 AssetDebugId, TWeakObjectPtr<const UStateTree>& WeakStateTree) const;

protected:
	/** IStateTreeDebuggerProvider interface */
	virtual void GetInstances(TArray<UE::StateTreeDebugger::FInstanceDescriptor>& OutInstances) const override;
	virtual bool ReadTimelines(const FStateTreeInstanceDebugId InstanceId, TFunctionRef<void(const FStateTreeInstanceDebugId ProcessedInstanceId, const FEventsTimeline&)> Callback) const override;
	virtual bool ReadTimelines(const UStateTree& StateTree, TFunctionRef<void(const FStateTreeInstanceDebugId ProcessedInstanceId, const FEventsTimeline&)> Callback) const override;

private:
	TraceServices::IAnalysisSession& Session;

	TMap<FStateTreeInstanceDebugId, uint32> InstanceIdToDebuggerEntryTimelines;
	TArray<UE::StateTreeDebugger::FInstanceDescriptor> Descriptors;
	TArray<TSharedRef<TraceServices::TPointTimeline<FStateTreeTraceEventVariantType>>> EventsTimelines;

	struct FStateTreeDebugIdPair
	{
		TWeakObjectPtr<const UStateTree> WeakStateTree;
		FStateTreeIndex16 Id;
	};

	TArray<FStateTreeDebugIdPair> StateTreeAssets;
};
#endif // WITH_STATETREE_DEBUGGER