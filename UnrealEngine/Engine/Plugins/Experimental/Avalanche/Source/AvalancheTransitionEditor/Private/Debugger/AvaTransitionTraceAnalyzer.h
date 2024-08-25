// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_STATETREE_DEBUGGER

#include "StateTreeExecutionTypes.h"
#include "StateTreeIndexTypes.h"
#include "Trace/Analyzer.h"
#include "UObject/WeakObjectPtr.h"

class FAvaTransitionDebugger;
class UStateTree;

class FAvaTransitionTraceAnalyzer : public UE::Trace::IAnalyzer
{
	struct FEvent
	{
		const ANSICHAR* Name;
		void(FAvaTransitionTraceAnalyzer::*Handler)(const FOnEventContext&);
	};

public:
	static void RegisterDebugger(FAvaTransitionDebugger* InDebugger);
	static void UnregisterDebugger(FAvaTransitionDebugger* InDebugger);

	//~ Begin IAnalyzer
	virtual void OnAnalysisBegin(const FOnAnalysisContext& InContext) override;
	virtual bool OnEvent(uint16 InRouteId, EStyle InStyle, const FOnEventContext& InContext) override;
	//~ End IAnalyzer

private:
	static TArray<FAvaTransitionDebugger*> ActiveDebuggers;

	static const TArray<FEvent> Events;

	void ForEachDebugger(const TCHAR* InDebugName, TFunction<void(FAvaTransitionDebugger&)>&& InCallable);

	UStateTree* FindStateTree(const FStateTreeInstanceDebugId& InDebugInstanceId) const;

	void OnAssetDebugIdEvent(const FOnEventContext& InContext);

	void OnInstanceEvent(const FOnEventContext& InContext);

	void OnTreeInstanceStarted(const FOnEventContext& InContext);

	void OnTreeInstanceStopped(const FOnEventContext& InContext);

	void OnStateEvent(const FOnEventContext& InContext);

	/** Mapping of the Asset Debug Id its State Tree */
	TMap<FStateTreeIndex16, TWeakObjectPtr<UStateTree>> DebugAssets;

	/** Mapping of the Instance Debug Id its Asset Debug Id */
	TMap<FStateTreeInstanceDebugId, FStateTreeIndex16> DebugInstances;
};

#endif //WITH_STATETREE_DEBUGGER
