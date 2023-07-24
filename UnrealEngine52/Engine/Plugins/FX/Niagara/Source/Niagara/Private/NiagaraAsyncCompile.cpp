// Copyright Epic Games, Inc. All Rights Reserved.


#include "NiagaraAsyncCompile.h"

#include "Modules/ModuleManager.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraModule.h"
#include "NiagaraPrecompileContainer.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraStats.h"
#include "UObject/Package.h"
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
		// if FNiagaraSystemCompileRequest is going to handle evaluating the CheckDDC state, then skip doing anything here
		if (bCheckDDCEnabled)
		{
			// check if the DDC data is ready
			CheckDDCResult();
		}
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

	// we do one more check here to see if the source data has been changed out from under us, if it has then we abort the compilation attempt
	if (!CompilationIdMatchesRequest())
	{
		AbortTask();
		return;
	}

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
	if (ScriptPair.CompileId.AdditionalDefines.Contains(FNiagaraCompileOptions::ExperimentalVMDisabled))
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
	ScriptPair.CompileTime = float(FPlatformTime::Seconds() - StartCompileTime);
	ExeData->BakedRapidIterationParameters = BakedRapidIterationParameters;
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

		if (AlternateDDCKey.IsSet())
		{
			GetDerivedDataCache()->Put(*(*AlternateDDCKey), DDCOutData, *AssetPath, true);
		}
	}
}

void FNiagaraAsyncCompileTask::AssignInitialCompilationId(const FNiagaraVMExecutableDataId& InitialCompilationId)
{
	ScriptPair.CompileId = InitialCompilationId;
}

void FNiagaraAsyncCompileTask::UpdateCompilationId(const FNiagaraVMExecutableDataId& UpdatedCompilationId)
{
	// we need to update our CompileId and add a new DDC key where we'll push the data to
	ScriptPair.CompileId = UpdatedCompilationId;
	AlternateDDCKey = UNiagaraScript::BuildNiagaraDDCKeyString(UpdatedCompilationId, AssetPath);
}

bool FNiagaraAsyncCompileTask::CompilationIdMatchesRequest() const
{
	FNiagaraVMExecutableDataId CompilationId;
	ScriptPair.CompiledScript->ComputeVMCompilationId(CompilationId, FGuid());

	const bool IdMatches = CompilationId == ScriptPair.CompileId;
	if (!IdMatches)
	{
		if (LogNiagara.GetVerbosity() >= ELogVerbosity::Verbose)
		{
			UE_LOG(LogNiagara, Warning, TEXT("Compile ID for %s changed during the compilation task."), *AssetPath);
			FNiagaraVMExecutableDataId OldID = ScriptPair.CompileId;
			LogVMID(ScriptPair.CompileId, "RequestedID");
			LogVMID(ScriptPair.CompiledScript->GetComputedVMCompilationId(), "CurrentID");
		}
	}

	return IdMatches;
}

void FNiagaraSystemCompileRequest::Abort()
{
	for (auto& AsyncTask : DDCTasks)
	{
		AsyncTask->AbortTask();
	}
}

FNiagaraSystemCompileRequest::FAsyncTaskPtr* FNiagaraSystemCompileRequest::FindTask(const UNiagaraScript* Script)
{
	return DDCTasks.FindByPredicate([Script](const FAsyncTaskPtr& TaskPtr) -> bool
	{
		return TaskPtr->GetScriptPair().CompiledScript == Script;
	});
}

bool FNiagaraSystemCompileRequest::QueryCompileComplete(UNiagaraSystem* OwningSystem, bool bWait, const double& MaxDuration)
{
	const double QueryStartTime = FPlatformTime::Seconds();

	if (bWait)
	{
		for (FAsyncTaskPtr& AsyncTask : DDCTasks)
		{
			AsyncTask->bWaitForCompileJob = true;
		}
	}

	if (bEvaluateParametersPending)
	{
		if (!bDDCGetCompleted)
		{
			bDDCGetCompleted = true;
			bool bAllTasksFinalized = true;
			{
				for (FAsyncTaskPtr& AsyncTask : DDCTasks)
				{
					if (AsyncTask->CurrentState == ENiagaraCompilationState::CheckDDC)
					{
						AsyncTask->CheckDDCResult();
						if (AsyncTask->CurrentState == ENiagaraCompilationState::CheckDDC)
						{
							bDDCGetCompleted = false;
						}
					}

					if (AsyncTask->CurrentState != ENiagaraCompilationState::Finished)
					{
						bAllTasksFinalized = false;
					}
				}
			}

			if (bDDCGetCompleted && bEvaluateParametersPending && !bAllTasksFinalized)
			{
				// run through the active scripts and make sure that they are still up to date before we try to do
				// any additional processing on them
				for (FAsyncTaskPtr& AsyncTask : DDCTasks)
				{
					if (AsyncTask->CurrentState == ENiagaraCompilationState::Finished)
					{
						continue;
					}

					if (!AsyncTask->CompilationIdMatchesRequest())
					{
						AsyncTask->AbortTask();
					}
				}

				// note - by having this deferred (after we've checked DDC) we do risk that the prepared RI parameters could be
				// out of sync with what was there at the time the compile was requested.  So, we'll only take this path if we're
				// not in edit mode.  In edit mode we'll prepare the RI parameters during UNiagaraSystem::RequestCompile()
				OwningSystem->PrepareRapidIterationParametersForCompilation();

				// go through our tasks and evaluate the rapid iteration parameters.  We'll need to store them in the ExeData
				// so that subsequent builds can take advantage of the work done in PrepareRapidIterationParamtersForCompilation
				// by storing it in the ExeData.  Additionally if this evaluation has changed the rapid iteration parameters the
				// DDC key will be different, and we'll want to push the data both to the old key and to the new key.
				const int32 TaskCount = DDCTasks.Num();
				for (int32 TaskIt = 0; TaskIt < TaskCount; ++TaskIt)
				{
					FAsyncTaskPtr& AsyncTask = DDCTasks[TaskIt];
					if (AsyncTask->CurrentState == ENiagaraCompilationState::Finished)
					{
						continue;
					}

					const FEmitterCompiledScriptPair& ScriptPair = AsyncTask->GetScriptPair();
					if (UNiagaraScript* CompiledScript = ScriptPair.CompiledScript)
					{
						TConstArrayView<FNiagaraVariableWithOffset> RapidIterationParameters = CompiledScript->RapidIterationParameters.ReadParameterVariables();

						AsyncTask->BakedRapidIterationParameters.Reserve(RapidIterationParameters.Num());
						for (const FNiagaraVariableWithOffset& ParamWithOffset : RapidIterationParameters)
						{
							FNiagaraVariable& Parameter = AsyncTask->BakedRapidIterationParameters.Add_GetRef(ParamWithOffset);
							Parameter.SetData(CompiledScript->RapidIterationParameters.GetParameterData(ParamWithOffset.Offset));
						}

						// if preparing the rapid iteration parameters has adjusted the compile id, then we want to be sure
						// to populate both entries in the DDC with the results (so subsequent loads don't need to go through
						// this same code path, as the serialized compileID will find the DDC results)
						FNiagaraVMExecutableDataId PostRapidIterationParameterID;
						CompiledScript->ComputeVMCompilationId(PostRapidIterationParameterID, FGuid());

						if (PostRapidIterationParameterID != ScriptPair.CompileId)
						{
							AsyncTask->UpdateCompilationId(PostRapidIterationParameterID);
						}
					}
				}
			}
		}

		if (!bDDCGetCompleted)
		{
			return false;
		}
	}

	bool bCompileComplete = true;

	if (bWait)
	{
		for (FAsyncTaskPtr& AsyncTask : DDCTasks)
		{
			// before we start to wait for the compile results, we start the compilation of all remaining tasks
			while (!AsyncTask->IsDone() && AsyncTask->CurrentState < ENiagaraCompilationState::AwaitResult)
			{
				AsyncTask->ProcessCurrentState();
			}
		}
	}

	// Check to see if ALL of the sub-requests have resolved.
	{
		for (auto& AsyncTask : DDCTasks)
		{
			// if a task is very expensive we bail and continue on the next frame  
			if (FPlatformTime::Seconds() - QueryStartTime < MaxDuration)
			{
				AsyncTask->ProcessCurrentState();
			}

			if (AsyncTask->IsDone())
			{
				continue;
			}

			if (!AsyncTask->bFetchedGCObjects && AsyncTask->CurrentState > ENiagaraCompilationState::Precompile && AsyncTask->CurrentState <= ENiagaraCompilationState::ProcessResult)
			{
				RootObjects.Append(AsyncTask->PrecompileReference->CompilationRootObjects);
				AsyncTask->bFetchedGCObjects = true;
			}

			if (bWait)
			{
				AsyncTask->WaitAndResolveResult();
			}
			else
			{
				bCompileComplete = false;
			}
		}
	}

	return bCompileComplete;
}

bool FNiagaraSystemCompileRequest::Resolve(UNiagaraSystem* OwningSystem, FNiagaraParameterStore& ExposedParameters)
{
	bool bHasCompiledJobs = false;

	for (FAsyncTaskPtr& AsyncTask : DDCTasks)
	{
		bHasCompiledJobs |= AsyncTask->bUsedShaderCompilerWorker;

		FEmitterCompiledScriptPair& EmitterCompiledScriptPair = AsyncTask->ScriptPair;
		if (EmitterCompiledScriptPair.bResultsReady)
		{
			TSharedPtr<FNiagaraVMExecutableData> ExeData = EmitterCompiledScriptPair.CompileResults;
			CombinedCompileTime += EmitterCompiledScriptPair.CompileTime;
			UNiagaraScript* CompiledScript = EmitterCompiledScriptPair.CompiledScript;

			// generate the ObjectNameMap from the source (from the duplicated data if available).
			TMap<FName, UNiagaraDataInterface*> ObjectNameMap;
			if (AsyncTask->ComputedPrecompileDuplicateData.IsValid())
			{
				if (const UNiagaraScriptSourceBase* ScriptSource = AsyncTask->ComputedPrecompileDuplicateData->GetScriptSource())
				{
					ObjectNameMap = ScriptSource->ComputeObjectNameMap(*OwningSystem, CompiledScript->GetUsage(), CompiledScript->GetUsageId(), AsyncTask->UniqueEmitterName);
				}
				else
				{
					checkf(false, TEXT("Failed to get ScriptSource from ComputedPrecompileDuplicateData"));
				}

				// we need to replace the DI in the above generated map with the duplicates that we've created as a part of the duplication process
				TMap<FName, UNiagaraDataInterface*> DuplicatedObjectNameMap = AsyncTask->ComputedPrecompileDuplicateData->GetObjectNameMap();
				for (auto ObjectMapIt = ObjectNameMap.CreateIterator(); ObjectMapIt; ++ObjectMapIt)
				{
					if (UNiagaraDataInterface** Replacement = DuplicatedObjectNameMap.Find(ObjectMapIt.Key()))
					{
						ObjectMapIt.Value() = *Replacement;
					}
					else
					{
						ObjectMapIt.RemoveCurrent();
					}
				}
			}
			else
			{
				const UNiagaraScriptSourceBase* ScriptSource = CompiledScript->GetLatestSource();
				ObjectNameMap = ScriptSource->ComputeObjectNameMap(*OwningSystem, CompiledScript->GetUsage(), CompiledScript->GetUsageId(), AsyncTask->UniqueEmitterName);
			}

			EmitterCompiledScriptPair.CompiledScript->SetVMCompilationResults(EmitterCompiledScriptPair.CompileId, *ExeData, AsyncTask->UniqueEmitterName, ObjectNameMap, bEvaluateParametersPending);

			// Synchronize the variables that we actually encountered during precompile so that we can expose them to the end user.
			TArray<FNiagaraVariable> OriginalExposedParams;
			ExposedParameters.GetParameters(OriginalExposedParams);
			TArray<FNiagaraVariable>& EncounteredExposedVars = AsyncTask->EncounteredExposedVars;
			for (int32 i = 0; i < EncounteredExposedVars.Num(); i++)
			{
				if (OriginalExposedParams.Contains(EncounteredExposedVars[i]) == false)
				{
					// Just in case it wasn't added previously..
					ExposedParameters.AddParameter(EncounteredExposedVars[i], true, false);
				}
			}
		}
	}

	return bHasCompiledJobs;
}

void FNiagaraSystemCompileRequest::Reset()
{
	// clean up the precompile data
	for (FAsyncTaskPtr& AsyncTask : DDCTasks)
	{
		AsyncTask->ComputedPrecompileData.Reset();
		if (AsyncTask->ComputedPrecompileDuplicateData.IsValid())
		{
			AsyncTask->ComputedPrecompileDuplicateData->ReleaseCompilationCopies();
			AsyncTask->ComputedPrecompileDuplicateData.Reset();
		}
	}
}

void FNiagaraSystemCompileRequest::UpdateSpawnDataInterfaces()
{
	// HACK: This is a temporary hack to fix an issue where data interfaces used by modules and dynamic inputs in the
	// particle update script aren't being shared by the interpolated spawn script when accessed directly.  This works
	// properly if the data interface is assigned to a named particle parameter and then linked to an input.
	// TODO: Bind these data interfaces the same way parameter data interfaces are bound.
	for (auto& AsyncTask : DDCTasks)
	{
		FEmitterCompiledScriptPair& EmitterCompiledScriptPair = AsyncTask->ScriptPair;
		FVersionedNiagaraEmitter Emitter = EmitterCompiledScriptPair.VersionedEmitter;
		UNiagaraScript* CompiledScript = EmitterCompiledScriptPair.CompiledScript;

		if (UNiagaraScript::IsEquivalentUsage(CompiledScript->GetUsage(), ENiagaraScriptUsage::ParticleUpdateScript))
		{
			UNiagaraScript* SpawnScript = Emitter.GetEmitterData()->SpawnScriptProps.Script;
			for (const FNiagaraScriptDataInterfaceInfo& UpdateDataInterfaceInfo : CompiledScript->GetCachedDefaultDataInterfaces())
			{
				// If the data interface isn't being written to a parameter map then it won't be bound properly so we
				// assign the update scripts copy of the data interface to the spawn scripts copy by pointer so that they will share
				// the data interface at runtime and will both be updated in the editor.
				for (FNiagaraScriptDataInterfaceInfo& SpawnDataInterfaceInfo : SpawnScript->GetCachedDefaultDataInterfaces())
				{
					if (SpawnDataInterfaceInfo.RegisteredParameterMapWrite == NAME_None && UpdateDataInterfaceInfo.RegisteredParameterMapWrite == NAME_None && UpdateDataInterfaceInfo.Name == SpawnDataInterfaceInfo.Name)
					{
						SpawnDataInterfaceInfo.DataInterface = UpdateDataInterfaceInfo.DataInterface;
						break;
					}
				}
			}
		}
	}
}

void FNiagaraSystemCompileRequest::Launch(UNiagaraSystem* OwningSystem, TConstArrayView<UNiagaraScript*> ScriptsNeedingCompile, TConstArrayView<FAsyncTaskPtr> Tasks)
{
	bEvaluateParametersPending = !OwningSystem->ShouldUseRapidIterationParameters();
	DDCTasks = Tasks;

	// prepare data for any precompile the ddc tasks need to do
	TSharedPtr<FNiagaraLazyPrecompileReference, ESPMode::ThreadSafe> PrecompileReference = MakeShared<FNiagaraLazyPrecompileReference, ESPMode::ThreadSafe>();
	PrecompileReference->System = OwningSystem;
	PrecompileReference->Scripts = ScriptsNeedingCompile;

	const TArray<FNiagaraEmitterHandle>& EmitterHandles = OwningSystem->GetEmitterHandles();
	for (int32 i = 0; i < EmitterHandles.Num(); i++)
	{
		FNiagaraEmitterHandle Handle = EmitterHandles[i];
		if (Handle.GetIsEnabled() && Handle.GetEmitterData())
		{
			TArray<UNiagaraScript*> EmitterScripts;
			Handle.GetEmitterData()->GetScripts(EmitterScripts, false, true);
			check(EmitterScripts.Num() > 0);
			for (UNiagaraScript* EmitterScript : EmitterScripts)
			{
				PrecompileReference->EmitterScriptIndex.Add(EmitterScript, i);
			}
		}
	}

	for (UNiagaraScript* Script : ScriptsNeedingCompile)
	{
		if (FAsyncTaskPtr* AsyncTaskPtr = FindTask(Script))
		{
			if (ensure(*AsyncTaskPtr))
			{
				FNiagaraAsyncCompileTask& CompileTask = *(*AsyncTaskPtr);
				CompileTask.DDCKey = UNiagaraScript::BuildNiagaraDDCKeyString(CompileTask.GetScriptPair().CompileId, CompileTask.AssetPath);
				CompileTask.PrecompileReference = PrecompileReference;
				CompileTask.StartTaskTime = FPlatformTime::Seconds();
				CompileTask.bCheckDDCEnabled = !bEvaluateParametersPending;

				// fire off all the ddc tasks, which will trigger the compilation if the data is not in the ddc
				UE_LOG(LogNiagara, Verbose, TEXT("Scheduling async get task for %s"), *CompileTask.AssetPath);
				CompileTask.TaskHandle = GetDerivedDataCacheRef().GetAsynchronous(*CompileTask.DDCKey, CompileTask.AssetPath);
			}
		}
	}
}


#endif // WITH_EDITORONLY_DATA

#undef LOCTEXT_NAMESPACE // NiagaraAsyncCompile

