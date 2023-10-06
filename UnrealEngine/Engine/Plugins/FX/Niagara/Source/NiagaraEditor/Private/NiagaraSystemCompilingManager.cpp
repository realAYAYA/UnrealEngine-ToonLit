// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemCompilingManager.h"

#include "Algo/RemoveIf.h"
#include "NiagaraCompilationTasks.h"
#include "NiagaraEditorModule.h"


#include "Misc/ScopeRWLock.h"

#define LOCTEXT_NAMESPACE "NiagaraCompilationManager"

static int GNiagaraCompilationMaxActiveTaskCount = 4;
static FAutoConsoleVariableRef CVarNiagaraCompilationMaxActiveTaskCount(
	TEXT("fx.Niagara.Compilation.MaxActiveTaskCount"),
	GNiagaraCompilationMaxActiveTaskCount,
	TEXT("The maximum number of active Niagara system compilations that can be going concurrantly."),
	ECVF_Default
);

FNiagaraSystemCompilingManager& FNiagaraSystemCompilingManager::Get()
{
	static FNiagaraSystemCompilingManager Singleton;
	return Singleton;
}

FName FNiagaraSystemCompilingManager::GetAssetTypeName() const
{
	return TEXT("UE-NiagaraSystem");
}

FTextFormat FNiagaraSystemCompilingManager::GetAssetNameFormat() const
{
	return LOCTEXT("NiagaraSystemAssetFormat", "{0}|plural(one=Niagara System,other=Niagara Systems)");
}

TArrayView<FName> FNiagaraSystemCompilingManager::GetDependentTypeNames() const
{
	return {};
}

int32 FNiagaraSystemCompilingManager::GetNumRemainingAssets() const
{
	int32 RemainingAssetCount = 0;

	{
		FReadScopeLock Read(QueueLock);
		RemainingAssetCount = QueuedRequests.Num() + ActiveTasks.Num();
	}

	return RemainingAssetCount;
}

void FNiagaraSystemCompilingManager::FinishCompilationForObjects(TArrayView<UObject* const> InObjects)
{
}

void FNiagaraSystemCompilingManager::FinishAllCompilation()
{
}

void FNiagaraSystemCompilingManager::Shutdown()
{
}

void FNiagaraSystemCompilingManager::ProcessAsyncTasks(bool bLimitExecutionTime)
{
	{
		FReadScopeLock ReadScope(QueueLock);
		if (ActiveTasks.IsEmpty())
		{
			return;
		}

		for (FNiagaraCompilationTaskHandle TaskHandle : ActiveTasks)
		{
			FTaskPtr TaskPtr = SystemRequestMap.FindRef(TaskHandle);
			if (TaskPtr.IsValid())
			{
				TaskPtr->Tick();
			}
		}
	}

	{
		FWriteScopeLock WriteScope(QueueLock);

		// find the list of tasks that we can remove
		TArray<FNiagaraCompilationTaskHandle> TasksToRemove;
		for (TArray<FNiagaraCompilationTaskHandle>::TIterator TaskIt(ActiveTasks); TaskIt; ++TaskIt)
		{
			FTaskPtr TaskPtr = SystemRequestMap.FindRef(*TaskIt);
			if (!TaskPtr.IsValid() || TaskPtr->CanRemove())
			{
				TasksToRemove.Add(*TaskIt);
				TaskIt.RemoveCurrent();
			}
		}

		// remove our found tasks
		for (FNiagaraCompilationTaskHandle TaskToRemove : TasksToRemove)
		{
			QueuedRequests.RemoveSingle(TaskToRemove);
			SystemRequestMap.Remove(TaskToRemove);
		}
	}


	// queue up as many tasks as we can
	while (ConditionalLaunchTask())
	{
		// just keep launching tasks until we run out of room
	}
}

FNiagaraCompilationTaskHandle FNiagaraSystemCompilingManager::AddSystem(UNiagaraSystem* System, FCompileOptions CompileOptions)
{
	check(IsInGameThread());

	struct FCompilableScriptInfo
	{
		FCompilableScriptInfo(bool& bHasCompilation, UNiagaraScript* InScript, bool bForced, int32 InEmitterIndex = INDEX_NONE)
			: Script(InScript)
			, EmitterIndex(InEmitterIndex)
		{
			bRequiresCompilation = Script && Script->IsCompilable() && (bForced || !Script->AreScriptAndSourceSynchronized());
			bHasCompilation = bHasCompilation || bRequiresCompilation;
		}

		UNiagaraScript* Script;
		int32 EmitterIndex = INDEX_NONE;
		bool bRequiresCompilation = false;
	};

	TArray<UNiagaraScript*> AllScripts;
	TArray<FCompilableScriptInfo> ScriptsToCompile;
	bool bHasScriptToCompile = false;

	// ensure that all necessary graphs have been digested
	for (UNiagaraScript* SystemScript : { System->GetSystemSpawnScript(), System->GetSystemUpdateScript() })
	{
		AllScripts.Add(SystemScript);
		ScriptsToCompile.Emplace(bHasScriptToCompile, SystemScript, CompileOptions.bForced);
	}

	const TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();
	const int32 EmitterCount = EmitterHandles.Num();
	for (int32 EmitterIt = 0; EmitterIt < EmitterCount; ++EmitterIt)
	{
		const FNiagaraEmitterHandle& Handle = EmitterHandles[EmitterIt];
		if (!Handle.GetIsEnabled())
		{
			continue;
		}

		if (const FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData())
		{
			constexpr bool bCompilableOnly = false; // we want to include emitter scripts for parameter store processing
			constexpr bool bEnabledOnly = true; // skip disabled stages

			TArray<UNiagaraScript*> EmitterScripts;
			EmitterData->GetScripts(EmitterScripts, bCompilableOnly, bEnabledOnly);
			for (UNiagaraScript* EmitterScript : EmitterScripts)
			{
				AllScripts.Add(EmitterScript);
				ScriptsToCompile.Emplace(bHasScriptToCompile, EmitterScript, CompileOptions.bForced, EmitterIt);
			}
		}
	}

	if (!bHasScriptToCompile)
	{
		return INDEX_NONE;
	}

	FNiagaraCompilationTaskHandle RequestHandle = NextTaskHandle++;

	// do we really need to care about wrapping?
	if (RequestHandle == INDEX_NONE)
	{
		RequestHandle = NextTaskHandle++;
	}

	FTaskPtr CompilationTask;

	{
		FWriteScopeLock WriteLock(QueueLock);

		QueuedRequests.Add(RequestHandle);
		CompilationTask = SystemRequestMap.Add(
			RequestHandle,
			MakeShared<FNiagaraSystemCompilationTask, ESPMode::ThreadSafe>(RequestHandle, System));
	}

	CompilationTask->PrepareStartTime = FPlatformTime::Seconds();

	// we're going to have to compile something so let's digest all the collections and build our compilation task
	CompilationTask->DigestParameterCollections(CompileOptions.ParameterCollections);
	CompilationTask->DigestSystemInfo();

	CompilationTask->bForced = CompileOptions.bForced;

	for (const FCompilableScriptInfo& ScriptToCompile : ScriptsToCompile)
	{
		CompilationTask->AddScript(ScriptToCompile.EmitterIndex, ScriptToCompile.Script, ScriptToCompile.bRequiresCompilation);
	}

	CompilationTask->QueueStartTime = FPlatformTime::Seconds();

	ConditionalLaunchTask();

	return RequestHandle;
}

bool FNiagaraSystemCompilingManager::PollSystemCompile(FNiagaraCompilationTaskHandle TaskHandle, bool bPeek, bool bWait, FNiagaraSystemAsyncCompileResults& Results)
{
	FTaskPtr TaskPtr;

	{
		FReadScopeLock ReadLock(QueueLock);

		TaskPtr = SystemRequestMap.FindRef(TaskHandle);
	}

	if (TaskPtr.IsValid())
	{
		if (bWait)
		{
			TaskPtr->WaitTillCompileCompletion();
		}

		if (TaskPtr->Poll(Results))
		{
			if (!bPeek)
			{
				TaskPtr->ResultsRetrieved = true;
			}

			return true;
		}
	}

	return false;
}

void FNiagaraSystemCompilingManager::AbortSystemCompile(FNiagaraCompilationTaskHandle TaskHandle)
{
	FTaskPtr TaskPtr;

	{
		FReadScopeLock ReadLock(QueueLock);

		TaskPtr = SystemRequestMap.FindRef(TaskHandle);
	}

	if (TaskPtr.IsValid())
	{
		TaskPtr->Abort();
	}
}

bool FNiagaraSystemCompilingManager::ConditionalLaunchTask()
{
	{
		FReadScopeLock Read(QueueLock);

		if (QueuedRequests.IsEmpty() || ActiveTasks.Num() >= GNiagaraCompilationMaxActiveTaskCount)
		{
			return false;
		}
	}

	{
		FWriteScopeLock Write(QueueLock);

		if (!QueuedRequests.IsEmpty() && ActiveTasks.Num() < GNiagaraCompilationMaxActiveTaskCount)
		{
			FNiagaraCompilationTaskHandle TaskHandle = QueuedRequests[0];
			QueuedRequests.RemoveAt(0);

			FTaskPtr CompileTask = SystemRequestMap.FindRef(TaskHandle);
			if (CompileTask.IsValid())
			{
				CompileTask->LaunchStartTime = FPlatformTime::Seconds();
				CompileTask->BeginTasks();
				ActiveTasks.Add(TaskHandle);

				return true;
			}
		}
	}

	return false;
}

FNiagaraCompilationTaskHandle FNiagaraEditorModule::RequestCompileSystem(UNiagaraSystem* System, bool bForced)
{
	FNiagaraSystemCompilingManager::FCompileOptions CompileOptions;
	CompileOptions.bForced = bForced;

	ParameterCollectionAssetCache.RefreshCache(!FUObjectThreadContext::Get().IsRoutingPostLoad /*bAllowLoading*/);
	const TArray<TWeakObjectPtr<UNiagaraParameterCollection>>& Collections = ParameterCollectionAssetCache.Get();
	CompileOptions.ParameterCollections = ParameterCollectionAssetCache.Get();

	return FNiagaraSystemCompilingManager::Get().AddSystem(System, CompileOptions);
}

bool FNiagaraEditorModule::PollSystemCompile(FNiagaraCompilationTaskHandle TaskHandle, FNiagaraSystemAsyncCompileResults& Results, bool bWait, bool bPeek)
{
	return FNiagaraSystemCompilingManager::Get().PollSystemCompile(TaskHandle, bPeek, bWait, Results);
}

void FNiagaraEditorModule::AbortSystemCompile(FNiagaraCompilationTaskHandle TaskHandle)
{
	return FNiagaraSystemCompilingManager::Get().AbortSystemCompile(TaskHandle);
}

#undef LOCTEXT_NAMESPACE