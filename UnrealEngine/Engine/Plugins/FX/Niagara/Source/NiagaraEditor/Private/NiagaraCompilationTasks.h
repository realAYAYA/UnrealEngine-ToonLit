// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DerivedDataCache.h"
#include "Tasks/Task.h"
#include "DerivedDataRequestOwner.h"
#include "NiagaraCompiler.h"
#include "NiagaraCompilationPrivate.h"

struct FNiagaraSystemCompilationTask
{
	struct FDispatchAndProcessDataCacheGetRequests
	{
		void Launch(FNiagaraSystemCompilationTask* SystemCompileTask);

		UE::Tasks::FTaskEvent CompletionEvent = UE::Tasks::FTaskEvent(UE_SOURCE_LOCATION);
		std::atomic<int32> PendingGetRequestCount = 0;
	};

	struct FDispatchDataCachePutRequests
	{
		void Launch(FNiagaraSystemCompilationTask* SystemCompileTask);

		UE::Tasks::FTaskEvent CompletionEvent = UE::Tasks::FTaskEvent(UE_SOURCE_LOCATION);
		std::atomic<int32> PendingPutRequestCount = 0;
	};

	FNiagaraSystemCompilationTask(FNiagaraCompilationTaskHandle InTaskHandle, UNiagaraSystem* InSystem);

	void Abort();

	void DigestSystemInfo();
	void DigestParameterCollections(TConstArrayView<TWeakObjectPtr<UNiagaraParameterCollection>> Collections);
	void AddScript(int32 EmitterIndex, UNiagaraScript* ScriptToCompile, bool bRequiresCompilation);
	UE::Tasks::FTask BeginTasks();

	UE::Tasks::FTask BuildRapidIterationParametersAsync();
	void IssueCompilationTasks();
	bool HasOutstandingCompileTasks() const;

	double PrepareStartTime = 0.0;
	double QueueStartTime = 0.0;
	double LaunchStartTime = 0.0;

	bool bForced = false;

	enum class EState : uint8
	{
		Invalid = 0,
		WaitingForProcessing,
		ResultsProcessed,
		Completed,
		Aborted,
	};

	std::atomic<EState> CompilationState = EState::Invalid;
	std::atomic<bool> ResultsRetrieved = false;

	struct FEmitterInfo
	{
		FString UniqueEmitterName;
		FString UniqueInstanceName;
		FNiagaraDigestedGraphPtr SourceGraph;
		TArray<FNiagaraVariable> InitialStaticVariables;
		TArray<FNiagaraVariable> StaticVariableResults;
		TArray<FNiagaraSimulationStageInfo> SimStages;
		FNiagaraFixedConstantResolver ConstantResolver;
		TArray<TObjectKey<UNiagaraScript>> OwnedScriptKeys;

		int32 EmitterIndex;
		bool Enabled;
	};

	struct FSystemInfo
	{
		TArray<FEmitterInfo> EmitterInfo;

		FString SystemName;
		FName SystemPackageName;
		TArray<FNiagaraVariable> InitialStaticVariables;
		TArray<FNiagaraVariable> StaticVariableResults;
		TArray<FNiagaraVariable> OriginalExposedParams;
		FNiagaraDigestedGraphPtr SystemSourceGraph;
		FNiagaraFixedConstantResolver ConstantResolver;
		TArray<TObjectKey<UNiagaraScript>> OwnedScriptKeys;

		bool bUseRapidIterationParams;
		bool bDisableDebugSwitches;
	};

	struct FCompileGroupInfo
	{
		FCompileGroupInfo() = delete;
		FCompileGroupInfo(int32 InEmitterIndex);

		bool HasOutstandingCompileTasks(const FNiagaraSystemCompilationTask& ParentTask) const;
		void InstantiateCompileGraph(const FNiagaraSystemCompilationTask& ParentTask);

		const int32 EmitterIndex;
		TArray<int32> CompileTaskIndices;
		TArray<ENiagaraScriptUsage> ValidUsages;
		TSharedPtr<FNiagaraCompilationCopyData> CompilationCopy;
	};

	struct FScriptInfo
	{
		ENiagaraScriptUsage Usage = ENiagaraScriptUsage::Function;
		FGuid UsageId;
		FNiagaraParameterStore RapidIterationParameters;
		TArray<TObjectKey<UNiagaraScript>> DependentScripts;
		int32 EmitterIndex = INDEX_NONE;
	};

	struct FCompileTaskInfo
	{
		FNiagaraCompileOptions CompileOptions;
		FNiagaraTranslateResults TranslateResults;
		FNiagaraTranslatorOutput TranslateOutput;
		FString TranslatedHlsl;
		TSharedPtr<FHlslNiagaraCompiler> Compiler;
		TSharedPtr<FNiagaraVMExecutableData> ExeData;
		TWeakObjectPtr<UNiagaraScript> SourceScript;
		TObjectKey<UNiagaraScript> ScriptKey;
		FString AssetPath;
		FNiagaraVMExecutableDataId CompileId;
		int32 CompilationJobId = INDEX_NONE;
		UE::DerivedData::FCacheKey DataCacheGetKey;
		TArray<UE::DerivedData::FCacheKey> DataCachePutKeys;
		TUniquePtr<UE::Tasks::FTaskEvent> CompileResultsReadyEvent;
		TUniquePtr<UE::Tasks::FTaskEvent> CompileResultsProcessedEvent;
		TMap<FName, UNiagaraDataInterface*> NamedDataInterfaces;
		TArray<FNiagaraVariable> BakedRapidIterationParameters; // todo - remove and have the data pulled directly from the precompile data
		bool bFromDerivedDataCache = false;
		double TaskStartTime = 0;
		float TranslationTime = 0.0f;
		float CompilerWallTime = 0.0f;
		float CompilerWorkerTime = 0.0f;
		float CompilerPreprocessTime = 0.0f;
		float ByteCodeOptimizeTime = 0.0f;

		bool IsOutstanding() const;
		void CollectNamedDataInterfaces(FNiagaraSystemCompilationTask* SystemCompileTask, const FCompileGroupInfo& GroupInfo);
		void TranslateAndIssueCompile(FNiagaraSystemCompilationTask* SystemCompileTask, const FCompileGroupInfo& GroupInfo);
		void ProcessCompilation(FNiagaraSystemCompilationTask* SystemCompileTask, const FCompileGroupInfo& GroupInfo);
		bool RetrieveCompilationResult(bool bWait);
	};

	struct FCollectStaticVariablesTaskBuilder;
	struct FBuildRapidIterationTaskBuilder;

	FCompileGroupInfo* GetCompileGroupInfo(int32 EmitterIndex);
	const FScriptInfo* GetScriptInfo(const TObjectKey<UNiagaraScript>& ScriptKey) const;

	TSharedPtr<FNiagaraPrecompileData> SystemPrecompileData;
	TSet<FNiagaraVariable> SystemExposedVariables;

	TArray<FCompileGroupInfo> CompileGroups;
	TArray<FCompileTaskInfo> CompileTasks;

	const FNiagaraCompilationTaskHandle TaskHandle;

	int32 TasksAwaitingDDCGetResults = 0;

	void Tick();
	bool Poll(FNiagaraSystemAsyncCompileResults& Results) const;
	bool CanRemove() const;

	void WaitTillCompileCompletion();
	void WaitTillCachePutCompletion();
	void Precompile();

	void GetAvailableCollections(TArray<FNiagaraCompilationNPCHandle>& OutCollections) const;

	UE::DerivedData::FRequestOwner DDCRequestOwner;

private:
	FNiagaraSystemCompilationTask() = delete;

	bool GetStageName(int32 EmitterIndex, const FNiagaraCompilationNodeOutput* OutputNode, FName& OutStageName) const;
	TArray<FNiagaraVariable> CollectStaticVariables(const FScriptInfo& ScriptInfo, const FNiagaraCompilationGraph& ScriptGraph, TConstArrayView<FNiagaraVariable> InitialStaticVariables) const;
	void CollectStaticVariables();

	TWeakObjectPtr<UNiagaraSystem> System_GT;

	FSystemInfo SystemInfo;

	FDispatchAndProcessDataCacheGetRequests InitialGetRequestHelper;
	TUniquePtr<FDispatchAndProcessDataCacheGetRequests> PostRIParameterGetRequestHelper;
	TUniquePtr<FDispatchDataCachePutRequests> PutRequestHelper;

	TMap<TObjectKey<UNiagaraScript>, FScriptInfo> DigestedScriptInfo;
	TSet<FNiagaraDigestedGraphPtr> DigestedGraphs;

	TMap<TObjectKey<UNiagaraParameterCollection>, FNiagaraCompilationNPCHandle> DigestedParameterCollections;

	UE::Tasks::FTaskEvent CompileCompletionEvent = UE::Tasks::FTaskEvent(UE_SOURCE_LOCATION);

	bool bAborting = false;
};
