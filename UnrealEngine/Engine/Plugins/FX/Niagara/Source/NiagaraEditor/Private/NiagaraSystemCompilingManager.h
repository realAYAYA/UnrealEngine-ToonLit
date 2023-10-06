// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetCompilingManager.h"

#include "NiagaraCompilationPrivate.h"
#include "NiagaraCompilationTypes.h"
#include "NiagaraSystem.h"

struct FNiagaraSystemCompilationTask;

class FNiagaraSystemCompilingManager : public IAssetCompilingManager
{
public:
	struct FCompileOptions
	{
		TArray<TWeakObjectPtr<UNiagaraParameterCollection>> ParameterCollections;
		bool bForced = false;
	};

	static FNiagaraSystemCompilingManager& Get();

	FNiagaraCompilationTaskHandle AddSystem(UNiagaraSystem* System, FCompileOptions CompileOptions);
	bool PollSystemCompile(FNiagaraCompilationTaskHandle TaskHandle, bool bPeek, bool bWait, FNiagaraSystemAsyncCompileResults& Results);
	void AbortSystemCompile(FNiagaraCompilationTaskHandle TaskHandle);

protected:
	// Begin - IAssetCompilingManager
	virtual FName GetAssetTypeName() const override;
	virtual FTextFormat GetAssetNameFormat() const override;
	virtual TArrayView<FName> GetDependentTypeNames() const override;
	virtual int32 GetNumRemainingAssets() const override;
	virtual void FinishCompilationForObjects(TArrayView<UObject* const> InObjects) override;
	virtual void FinishAllCompilation() override;
	virtual void Shutdown() override;
	virtual void ProcessAsyncTasks(bool bLimitExecutionTime = false) override;
	// End - IAssetCompilingManager

	bool ConditionalLaunchTask();

	using FTaskPtr = TSharedPtr<FNiagaraSystemCompilationTask, ESPMode::ThreadSafe>;

	mutable FRWLock QueueLock;
	TArray<FNiagaraCompilationTaskHandle> QueuedRequests;
	TMap<FNiagaraCompilationTaskHandle, FTaskPtr> SystemRequestMap;

	std::atomic<FNiagaraCompilationTaskHandle> NextTaskHandle = { 0 };

	TArray<FNiagaraCompilationTaskHandle> ActiveTasks;
};
