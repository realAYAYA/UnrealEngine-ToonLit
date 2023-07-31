// Copyright Epic Games, Inc. All Rights Reserved.


#include "NiagaraAsyncCompile.h"

#include "Modules/ModuleManager.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraModule.h"
#include "NiagaraPrecompileContainer.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraStats.h"
#include "VectorVM.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraAsyncCompile)

#define LOCTEXT_NAMESPACE "NiagaraAsyncCompile"

#if WITH_EDITOR
#include "DerivedDataCacheInterface.h"
#endif

DECLARE_CYCLE_STAT(TEXT("Niagara - System - Precompile"), STAT_Niagara_System_Precompile, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Niagara - System - CompileScriptTaskGT"), STAT_Niagara_System_CompileScriptTaskGT, STATGROUP_Niagara);

static float GNiagaraCompileDDCWaitTimeout = 10.0f;
static FAutoConsoleVariableRef CVarNiagaraCompileDDCWaitTimeout(
	TEXT("fx.Niagara.CompileDDCWaitTimeout"),
	GNiagaraCompileDDCWaitTimeout,
	TEXT("During script compilation, how long do we wait for the ddc to answer in seconds before starting shader compilation?"),
	ECVF_Default
);

//////////////////////////////////////////////////////////////////////////

#if WITH_EDITORONLY_DATA
TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> FNiagaraLazyPrecompileReference::GetPrecompileData(UNiagaraScript* ForScript)
{
	if (SystemPrecompiledData == nullptr)
	{
		UNiagaraPrecompileContainer* Container = NewObject<UNiagaraPrecompileContainer>(GetTransientPackage());
		Container->System = System;
		Container->Scripts = Scripts;
		INiagaraModule& NiagaraModule = FModuleManager::Get().LoadModuleChecked<INiagaraModule>(TEXT("Niagara"));
		SystemPrecompiledData = NiagaraModule.Precompile(Container, FGuid());

		const TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();
		for (int32 i = 0; i < EmitterHandles.Num(); i++)
		{
			const FNiagaraEmitterHandle& Handle = EmitterHandles[i];
			if (Handle.GetEmitterData() && Handle.GetIsEnabled())
			{
				TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe> EmitterPrecompiledData = SystemPrecompiledData->GetDependentRequest(i);
				TArray<UNiagaraScript*> EmitterScripts;
				Handle.GetEmitterData()->GetScripts(EmitterScripts, false, true);
				check(EmitterScripts.Num() > 0);
				for (UNiagaraScript* EmitterScript : EmitterScripts)
				{
					EmitterMapping.Add(EmitterScript, EmitterPrecompiledData);
				}
			}
		}
	}
	if (SystemPrecompiledData.IsValid() == false)
	{
		UE_LOG(LogNiagara, Error, TEXT("Failed to precompile %s.  This is due to unexpected invalid or broken data.  Additional details should be in the log."), *System->GetPathName());
		return nullptr;
	}

	if (ForScript->IsSystemSpawnScript() || ForScript->IsSystemUpdateScript())
	{
		return SystemPrecompiledData;
	}
	TSharedPtr<FNiagaraCompileRequestDataBase, ESPMode::ThreadSafe>* EmitterPrecompileData = EmitterMapping.Find(ForScript);
	if (EmitterPrecompileData == nullptr)
	{
		UE_LOG(LogNiagara, Log, TEXT("Failed to precompile %s. This is probably because the system was modified between the original compile request and now."), *System->GetPathName());
		return nullptr;
	}
	return *EmitterPrecompileData;
}

TSharedPtr<FNiagaraCompileRequestDuplicateDataBase, ESPMode::ThreadSafe> FNiagaraLazyPrecompileReference::GetPrecompileDuplicateData(UNiagaraEmitter* OwningEmitter, UNiagaraScript* TargetScript)
{
	TSharedPtr<FNiagaraCompileRequestDuplicateDataBase, ESPMode::ThreadSafe> PrecompileDuplicateData;
	for (TSharedPtr<FNiagaraCompileRequestDuplicateDataBase, ESPMode::ThreadSafe> ExistingPrecompileDuplicateData : PrecompileDuplicateDatas)
	{
		if (ExistingPrecompileDuplicateData->IsDuplicateDataFor(System, OwningEmitter, TargetScript))
		{
			PrecompileDuplicateData = ExistingPrecompileDuplicateData;
		}
	}

	if (PrecompileDuplicateData.IsValid() == false)
	{
		INiagaraModule& NiagaraModule = FModuleManager::Get().LoadModuleChecked<INiagaraModule>(TEXT("Niagara"));

		PrecompileDuplicateData = NiagaraModule.PrecompileDuplicate(SystemPrecompiledData.Get(), System, OwningEmitter, TargetScript, FGuid());
		PrecompileDuplicateDatas.Add(PrecompileDuplicateData);

		PrecompileDuplicateData->GetDuplicatedObjects(CompilationRootObjects);
		for (int32 i = 0; i < PrecompileDuplicateData->GetDependentRequestCount(); i++)
		{
			PrecompileDuplicateData->GetDependentRequest(i)->GetDuplicatedObjects(CompilationRootObjects);
		}
	}

	return PrecompileDuplicateData;
}

FNiagaraAsyncCompileTask::FNiagaraAsyncCompileTask(UNiagaraSystem* InOwningSystem, FString InAssetPath, const FEmitterCompiledScriptPair& InScriptPair)
{
	OwningSystem = InOwningSystem;
	AssetPath = InAssetPath;
	ScriptPair = InScriptPair;

	CurrentState = ENiagaraCompilationState::CheckDDC;

	bExperimentalVMDisabled = InScriptPair.CompileId.AdditionalDefines.Contains(FNiagaraCompileOptions::ExperimentalVMDisabled);
}

void FNiagaraAsyncCompileTask::ProcessCurrentState()
{
	SCOPE_CYCLE_COUNTER(STAT_Niagara_System_CompileScriptTaskGT);
	if (IsDone())
	{
		return;
	}

	if (CurrentState == ENiagaraCompilationState::CheckDDC)
	{
		// check if the DDC data is ready
		CheckDDCResult();
	}
	else if (CurrentState == ENiagaraCompilationState::Precompile)
	{
		// gather precompile data
		StartCompileTime = FPlatformTime::Seconds();
		PrecompileData();
		MoveToState(ENiagaraCompilationState::StartCompileJob);
	}
	else if (CurrentState == ENiagaraCompilationState::StartCompileJob)
	{
		// start the async compile job
		StartCompileJob();
		MoveToState(ENiagaraCompilationState::AwaitResult);
	}
	else if (CurrentState == ENiagaraCompilationState::AwaitResult)
	{
		if (AwaitResult())
		{
			MoveToState(ENiagaraCompilationState::OptimizeByteCode);
		}
	}
	else if (CurrentState == ENiagaraCompilationState::OptimizeByteCode)
	{
		OptimizeByteCode();
		MoveToState(ENiagaraCompilationState::ProcessResult);
	}
	else if (CurrentState == ENiagaraCompilationState::ProcessResult)
	{
		// save the result from the compile job
		ProcessResult();
		MoveToState(ENiagaraCompilationState::PutToDDC);
	}
	else if (CurrentState == ENiagaraCompilationState::PutToDDC)
	{
		// put the result from the compile job into the ddc
		PutToDDC();
		MoveToState(ENiagaraCompilationState::Finished);
	}
	else
	{
		check(false);
	}
}

void FNiagaraAsyncCompileTask::MoveToState(ENiagaraCompilationState NewState)
{
	if (CurrentState == ENiagaraCompilationState::Finished || CurrentState == ENiagaraCompilationState::Aborted)
	{
		// we are already in an end state, so nothing to do
		return;
	}

	// check state machine integrity
	check(NewState != ENiagaraCompilationState::CheckDDC);
	if (NewState == ENiagaraCompilationState::Precompile)
	{
		check(CurrentState == ENiagaraCompilationState::CheckDDC);
	}
	if (NewState == ENiagaraCompilationState::StartCompileJob)
	{
		check(CurrentState == ENiagaraCompilationState::Precompile);
	}
	if (NewState == ENiagaraCompilationState::AwaitResult)
	{
		check(CurrentState == ENiagaraCompilationState::StartCompileJob);
	}
	if (NewState == ENiagaraCompilationState::OptimizeByteCode)
	{
		check(CurrentState == ENiagaraCompilationState::AwaitResult);
	}
	if (NewState == ENiagaraCompilationState::ProcessResult)
	{
		check(CurrentState == ENiagaraCompilationState::OptimizeByteCode);
	}
	if (NewState == ENiagaraCompilationState::PutToDDC)
	{
		check(CurrentState == ENiagaraCompilationState::ProcessResult);
	}
	if (NewState == ENiagaraCompilationState::Finished)
	{
		check(CurrentState == ENiagaraCompilationState::CheckDDC || CurrentState == ENiagaraCompilationState::PutToDDC);
	}

	UE_LOG(LogNiagara, Verbose, TEXT("Changing state %i -> %i for for %s!"), CurrentState, NewState, *AssetPath);
	CurrentState = NewState;
}

bool FNiagaraAsyncCompileTask::IsDone() const
{
	return CurrentState == ENiagaraCompilationState::Finished || CurrentState == ENiagaraCompilationState::Aborted;
}

void LogVMID(const FNiagaraVMExecutableDataId& ID, FString Name)
{
	// some debug logging to find out why the ddc key changed
	UE_LOG(LogNiagara, Display, TEXT("FNiagaraVMExecutableDataId %s:"), *Name);
	UE_LOG(LogNiagara, Display, TEXT("  ScriptUsageType: %i"), ID.ScriptUsageType);
	UE_LOG(LogNiagara, Display, TEXT("  ScriptUsageTypeID: %s"), *ID.ScriptUsageTypeID.ToString());
	UE_LOG(LogNiagara, Display, TEXT("  CompilerVersionID: %s"), *ID.CompilerVersionID.ToString());
	UE_LOG(LogNiagara, Display, TEXT("  BaseScriptCompileHash: %s"), *ID.BaseScriptCompileHash.ToString());
	UE_LOG(LogNiagara, Display, TEXT("  bUsesRapidIterationParams: %i"), ID.bUsesRapidIterationParams);
	UE_LOG(LogNiagara, Display, TEXT("  AdditionalDefines:"));
	for (int32 Idx = 0; Idx < ID.AdditionalDefines.Num(); Idx++)
	{
		UE_LOG(LogNiagara, Display, TEXT("     %s"), *ID.AdditionalDefines[Idx]);
	}
	UE_LOG(LogNiagara, Display, TEXT("  AdditionalVariables:"));
	for (int32 Idx = 0; Idx < ID.AdditionalVariables.Num(); Idx++)
	{
		UE_LOG(LogNiagara, Display, TEXT("     %s / %s"), *ID.AdditionalVariables[Idx].GetName().ToString(), *ID.AdditionalVariables[Idx].GetType().GetName());
	}
	UE_LOG(LogNiagara, Display, TEXT("  ReferencedCompileHashes:"));
	for (int32 HashIndex = 0; HashIndex < ID.ReferencedCompileHashes.Num(); HashIndex++)
	{
		UE_LOG(LogNiagara, Display, TEXT("     %s"), *ID.ReferencedCompileHashes[HashIndex].ToString());
	}
}

void FNiagaraAsyncCompileTask::PrecompileData()
{
	SCOPE_CYCLE_COUNTER(STAT_Niagara_System_Precompile);
	UE_LOG(LogNiagara, Verbose, TEXT("Getting precompile data for %s!"), *AssetPath);

	// we compute the vmID here again to check if the script changed between the start of the task and now. If it was changed we need to update the ddc key,
	// because otherwise we would save the compiled data under the wrong ddc key
	FNiagaraVMExecutableDataId NewID;
	ScriptPair.CompiledScript->ComputeVMCompilationId(NewID, FGuid());
	FString CurrentDDCKey = UNiagaraScript::BuildNiagaraDDCKeyString(NewID);
	if (DDCKey != CurrentDDCKey && LogNiagara.GetVerbosity() >= ELogVerbosity::Verbose)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Compile ID for %s changed during the compilation task."), *AssetPath);
		FNiagaraVMExecutableDataId OldID = ScriptPair.CompileId;
		LogVMID(OldID, "OldID");
		LogVMID(NewID, "NewID");
	}
	DDCKey = CurrentDDCKey;
	
	ComputedPrecompileData = PrecompileReference->GetPrecompileData(ScriptPair.CompiledScript);
	TSharedPtr<FNiagaraCompileRequestDuplicateDataBase, ESPMode::ThreadSafe> SystemPrecompileDuplicateData = PrecompileReference->GetPrecompileDuplicateData(ScriptPair.VersionedEmitter.Emitter, ScriptPair.CompiledScript);

	if (ComputedPrecompileData == nullptr || SystemPrecompileDuplicateData == nullptr)
	{
		AbortTask();
		return;
	}

	// Grab the list of user variables that were actually encountered so that we can add to them later.
	ComputedPrecompileData->GatherPreCompiledVariables(TEXT("User"), EncounteredExposedVars);
	for(int32 i = 0; i < ComputedPrecompileData->GetDependentRequestCount(); i++)
	{
		ComputedPrecompileData->GetDependentRequest(i)->GatherPreCompiledVariables(TEXT("User"), EncounteredExposedVars);
	}

	// Assign the correct duplicate data for this request.
	if(ScriptPair.CompiledScript->GetUsage() == ENiagaraScriptUsage::SystemSpawnScript || ScriptPair.CompiledScript->GetUsage() == ENiagaraScriptUsage::SystemUpdateScript)
	{
		ComputedPrecompileDuplicateData = SystemPrecompileDuplicateData;
	}
	else
	{
		UNiagaraEmitter* OwningEmitter = ScriptPair.VersionedEmitter.Emitter;
		int32 EmitterIndex = OwningSystem->GetEmitterHandles().IndexOfByPredicate([OwningEmitter](const FNiagaraEmitterHandle& EmitterHandle) { return EmitterHandle.GetInstance().Emitter == OwningEmitter; });
		if(EmitterIndex != INDEX_NONE)
		{
			ComputedPrecompileDuplicateData = SystemPrecompileDuplicateData->GetDependentRequest(EmitterIndex);
		}
	}
}

void FNiagaraAsyncCompileTask::StartCompileJob()
{
	UE_LOG(LogNiagara, Verbose, TEXT("Starting compilation for %s!"), *AssetPath);
	
	// Fire off the compile job
	if (ScriptPair.CompiledScript->RequestExternallyManagedAsyncCompile(ComputedPrecompileData, ComputedPrecompileDuplicateData, ScriptPair.CompileId, ScriptPair.PendingJobID))
	{
		UE_LOG(LogNiagara, Verbose, TEXT("Designated compilation ID %i for compilation of %s!"), ScriptPair.PendingJobID, *AssetPath);
		bUsedShaderCompilerWorker = true;
	}
	else
	{
		UE_LOG(LogNiagara, Warning, TEXT("For some reason we are reporting that %s is in sync even though AreScriptAndSourceSynchronized returned false!"), *ScriptPair.CompiledScript->GetPathName())
		AbortTask();
	}
}

bool FNiagaraAsyncCompileTask::AwaitResult()
{
	UE_LOG(LogNiagara, Verbose, TEXT("Waiting on compilation to finish for %s!"), *AssetPath);

	INiagaraModule& NiagaraModule = FModuleManager::Get().LoadModuleChecked<INiagaraModule>(TEXT("Niagara"));
	ExeData = NiagaraModule.GetCompileJobResult(ScriptPair.PendingJobID, bWaitForCompileJob);
	return ExeData.IsValid();
}

void FNiagaraAsyncCompileTask::OptimizeByteCode()
{
	check(ExeData);

#if VECTORVM_SUPPORTS_LEGACY
	if (bExperimentalVMDisabled)
	{
		return;
	}
#endif

#if VECTORVM_SUPPORTS_EXPERIMENTAL
	if (const uint8* ByteCode = ExeData->ByteCode.GetDataPtr())
	{
		//this is just necessary because VectorVM doesn't know about FVMExternalFunctionBindingInfo
		const int32 NumExtFns = ExeData->CalledVMExternalFunctions.Num();

		TArray<FVectorVMExtFunctionData> ExtFnTable;
		ExtFnTable.SetNumZeroed(NumExtFns);
		for (int i = 0; i < NumExtFns; ++i)
		{
			ExtFnTable[i].NumInputs = ExeData->CalledVMExternalFunctions[i].GetNumInputs();
			ExtFnTable[i].NumOutputs = ExeData->CalledVMExternalFunctions[i].GetNumOutputs();
		}

		FVectorVMOptimizeContext OptimizeContext;
		FMemory::Memzero(&OptimizeContext, sizeof(OptimizeContext));
		OptimizeVectorVMScript(ByteCode, ExeData->ByteCode.GetLength(), ExtFnTable.GetData(), ExtFnTable.Num(), &OptimizeContext, VVMFlag_OptOmitStats);

		// freeze the OptimizeContext
		FreezeVectorVMOptimizeContext(OptimizeContext, ExeData->ExperimentalContextData);

		FreeVectorVMOptimizeContext(&OptimizeContext);
	}
#endif // VECTORVM_SUPPORTS_EXPERIMENTAL
}

void FNiagaraAsyncCompileTask::ProcessResult()
{
	check(ExeData);
	UE_LOG(LogNiagara, Verbose, TEXT("Processing compilation results for %s!"), *AssetPath);

	// convert results to be saved in the ddc
	ScriptPair.CompileResults = ExeData;
	ScriptPair.bResultsReady = true;
	ExeData->CompileTime = FPlatformTime::Seconds() - StartCompileTime;
	ScriptPair.CompiledScript->ExecToBinaryData(ScriptPair.CompiledScript, DDCOutData, *ExeData);
	UE_LOG(LogNiagara, Verbose, TEXT("Got %i bytes in ddc data for %s"), DDCOutData.Num(), *AssetPath);
}

void FNiagaraAsyncCompileTask::WaitAndResolveResult()
{
	check(IsInGameThread());
	
	bWaitForCompileJob = true;
	while (!IsDone())
	{
		ProcessCurrentState();
	}
}

void FNiagaraAsyncCompileTask::AbortTask()
{
	MoveToState(ENiagaraCompilationState::Aborted);
}

void FNiagaraAsyncCompileTask::CheckDDCResult()
{
	FDerivedDataCacheInterface* DDC = GetDerivedDataCache();
	if (bWaitForCompileJob)
	{
		DDC->WaitAsynchronousCompletion(TaskHandle);
	}
	if (DDC->PollAsynchronousCompletion(TaskHandle))
	{
		bool bHasResults = DDC->GetAsynchronousResults(TaskHandle, DDCOutData);
		if (bHasResults && DDCOutData.Num() > 0)
		{
			ExeData = MakeShared<FNiagaraVMExecutableData>();
			if (ScriptPair.CompiledScript->BinaryToExecData(ScriptPair.CompiledScript, DDCOutData, *ExeData))
			{
				ScriptPair.CompileResults = ExeData;
				ScriptPair.bResultsReady = true;
				MoveToState(ENiagaraCompilationState::Finished);
				UE_LOG(LogNiagara, Verbose, TEXT("Compilation data for %s could be pulled from the ddc."), *AssetPath);
			}
			else
			{
				UE_LOG(LogNiagara, Warning, TEXT("Unable to create exec data from ddc data for script %s, going to recompile it from scratch. DDC might be corrupted or there is a problem with script serialization."), *AssetPath);
				ExeData.Reset();
				MoveToState(ENiagaraCompilationState::Precompile);
			}
		}
		else
		{
			MoveToState(ENiagaraCompilationState::Precompile);
			UE_LOG(LogNiagara, Verbose, TEXT("No compilation data for %s found in the ddc."), *AssetPath);
		}
		DDCFetchTime = FPlatformTime::Seconds() - StartTaskTime;
		UE_LOG(LogNiagara, Verbose, TEXT("Compile task for %s took %f seconds to fetch data from ddc."), *AssetPath, DDCFetchTime);
	}
	else if ((FPlatformTime::Seconds() - StartTaskTime) > GNiagaraCompileDDCWaitTimeout)
	{
		MoveToState(ENiagaraCompilationState::Precompile);
		UE_LOG(LogNiagara, Log, TEXT("DDC timed out after %i seconds, starting compilation for %s."), (int)GNiagaraCompileDDCWaitTimeout, *AssetPath);
	}
}

void FNiagaraAsyncCompileTask::PutToDDC()
{
	if (DDCOutData.Num() > 0)
	{
		UE_LOG(LogNiagara, Verbose, TEXT("Writing data for %s to the ddc"), *AssetPath);
		GetDerivedDataCache()->Put(*DDCKey, DDCOutData, *AssetPath, true);
	}
}

#endif

#undef LOCTEXT_NAMESPACE // NiagaraAsyncCompile

