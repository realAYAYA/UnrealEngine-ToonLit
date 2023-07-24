// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "NiagaraScript.h"

#include "NiagaraAsyncCompile.generated.h"

class UNiagaraEmitter;
class UNiagaraSystem;

USTRUCT()
struct FEmitterCompiledScriptPair
{
	GENERATED_USTRUCT_BODY()
	
	bool bResultsReady = false;
	FVersionedNiagaraEmitter VersionedEmitter;
	UNiagaraScript* CompiledScript = nullptr;
	uint32 PendingJobID = INDEX_NONE; // this is the ID for any active shader compiler worker job
	FNiagaraVMExecutableDataId CompileId;
	TSharedPtr<FNiagaraVMExecutableData> CompileResults;
	float CompileTime = 0.0f;
};

UENUM()
enum class ENiagaraCompilationState : uint8
{
	CheckDDC,
	Precompile,
	StartCompileJob,
	AwaitResult,
	OptimizeByteCode,
	ProcessResult,
	PutToDDC,
	Finished,
	Aborted
};

struct FNiagaraLazyPrecompileReference
{
	TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> GetPrecompileData(UNiagaraScript* ForScript);
	TSharedPtr<FNiagaraCompileRequestDuplicateDataBase, ESPMode::ThreadSafe> GetPrecompileDuplicateData(UNiagaraEmitter* OwningEmitter, UNiagaraScript* TargetScript);
	
	UNiagaraSystem* System = nullptr;
	TArray<UNiagaraScript*> Scripts;
	TMap<UNiagaraScript*, int32> EmitterScriptIndex;
	TArray<TObjectPtr<UObject>> CompilationRootObjects;

private:
	TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> SystemPrecompiledData;
	TMap<UNiagaraScript*, TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe>> EmitterMapping;
	TArray<TSharedPtr<FNiagaraCompileRequestDuplicateDataBase, ESPMode::ThreadSafe>> PrecompileDuplicateDatas;
};

class FNiagaraAsyncCompileTask
{
#if WITH_EDITORONLY_DATA
public:
	FString DDCKey;
	TOptional<FString> AlternateDDCKey;
	FString AssetPath;
	FString UniqueEmitterName;
	uint32 TaskHandle = 0;

	double StartTaskTime = 0;
	double DDCFetchTime = 0;
	double StartCompileTime = 0;
	bool bWaitForCompileJob = false;
	bool bUsedShaderCompilerWorker = false;
	bool bFetchedGCObjects = false;

	// in order to coordinate between tasks associated with a system CheckDDC can be handled external to the task.
	// In that case we disable this flag
	bool bCheckDDCEnabled = true;

	UNiagaraSystem* OwningSystem;
	FEmitterCompiledScriptPair ScriptPair;
	TArray<FNiagaraVariable> EncounteredExposedVars;
	TArray<FNiagaraVariable> BakedRapidIterationParameters;

	ENiagaraCompilationState CurrentState;

	TSharedPtr<FNiagaraVMExecutableData> ExeData;
	TArray<uint8> DDCOutData;

	// this data is shared between the ddc thread and the game thread that starts the compilation
	TSharedPtr<FNiagaraLazyPrecompileReference, ESPMode::ThreadSafe> PrecompileReference;
	TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> ComputedPrecompileData;
	TSharedPtr<FNiagaraCompileRequestDuplicateDataBase, ESPMode::ThreadSafe> ComputedPrecompileDuplicateData;

	FNiagaraAsyncCompileTask(UNiagaraSystem* InOwningSystem, FString InAssetPath, const FEmitterCompiledScriptPair& InScriptPair);

	const FEmitterCompiledScriptPair& GetScriptPair() const { return ScriptPair; };
	void WaitAndResolveResult();
	void AbortTask();

	void CheckDDCResult();
	void PutToDDC();
	void ProcessCurrentState();
	void MoveToState(ENiagaraCompilationState NewState);
	bool IsDone() const;

	void PrecompileData();
	void StartCompileJob();
	bool AwaitResult();
	void ProcessResult();
	void OptimizeByteCode();

	void AssignInitialCompilationId(const FNiagaraVMExecutableDataId& InitialCompilationId);
	void UpdateCompilationId(const FNiagaraVMExecutableDataId& UpdatedCompilationId);
	bool CompilationIdMatchesRequest() const;
#endif
};
