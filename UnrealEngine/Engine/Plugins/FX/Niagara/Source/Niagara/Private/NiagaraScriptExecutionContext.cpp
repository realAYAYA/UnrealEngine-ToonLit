// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraScriptExecutionContext.h"
#include "NiagaraSettings.h"
#include "NiagaraStats.h"
#include "NiagaraDataInterface.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraWorldManager.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraGPUInstanceCountManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "HAL/PlatformFileManager.h"

DECLARE_CYCLE_STAT(TEXT("Register Setup"), STAT_NiagaraSimRegisterSetup, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Context Ticking"), STAT_NiagaraScriptExecContextTick, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Rebind DInterface Func Table"), STAT_NiagaraRebindDataInterfaceFunctionTable, STATGROUP_Niagara);
	//Add previous frame values if we're interpolated spawn.
	
	//Internal constants - only needed for non-GPU sim

uint32 FNiagaraScriptExecutionContextBase::TickCounter = 0;

static int32 GbExecVMScripts = 1;
static FAutoConsoleVariableRef CVarNiagaraExecVMScripts(
	TEXT("fx.ExecVMScripts"),
	GbExecVMScripts,
	TEXT("If > 0 VM scripts will be executed, otherwise they won't, useful for looking at the bytecode for a crashing compiled script. \n"),
	ECVF_Default
);

static int32 GbForceExecVMPath = 0;
static FAutoConsoleVariableRef CVarNiagaraForceExecVMPath(
	TEXT("fx.ForceExecVMPath"),
	GbForceExecVMPath,
	TEXT("If < 0, the legacy VM path will be used, if > 0 the experimental version will be used, and the default if 0. \n"),
	ECVF_Default
);

FNiagaraScriptExecutionContextBase::FNiagaraScriptExecutionContextBase()
	: Script(nullptr)
	, VectorVMState(nullptr)
	, ScriptType(ENiagaraSystemSimulationScript::Update)
	, bAllowParallel(true)
#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY
	, bUsingExperimentalVM(true)
#endif
{

}

FNiagaraScriptExecutionContextBase::~FNiagaraScriptExecutionContextBase()
{
#if VECTORVM_SUPPORTS_EXPERIMENTAL
	FreeVectorVMState(VectorVMState);
#endif 
}

bool FNiagaraScriptExecutionContextBase::Init(UNiagaraScript* InScript, ENiagaraSimTarget InTarget)
{
	Script = InScript;

	Parameters.InitFromOwningContext(Script, InTarget, true);

	HasInterpolationParameters = Script && Script->GetComputedVMCompilationId().HasInterpolatedParameters();

#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY
	const bool ProjectSupportsExperimentalVM = GetDefault<UNiagaraSettings>()->bExperimentalVMEnabled;
	const bool ScriptSupportsExperimentalVM = (Script ? Script->GetVMExecutableData().SupportsExperimentalVM() : false);
	bool SystemDisabledExperimentalVM = false;

	if (InScript)
	{
		if (const UNiagaraSystem* OwnerSystem = InScript->GetTypedOuter<UNiagaraSystem>())
		{
			if (OwnerSystem->ShouldDisableExperimentalVM())
			{
				SystemDisabledExperimentalVM = true;
			}
		}
	}

	bUsingExperimentalVM = ProjectSupportsExperimentalVM && ScriptSupportsExperimentalVM && ((GbForceExecVMPath > 0) || (!GbForceExecVMPath && !SystemDisabledExperimentalVM));
#endif

#	if VECTORVM_SUPPORTS_EXPERIMENTAL
	FVectorVMOptimizeContext OptimizeContext = Script->GetVMExecutableData().BuildExperimentalContext();
	VectorVMState = AllocVectorVMState(&OptimizeContext);
#	endif

	return true;
}

void FNiagaraScriptExecutionContextBase::BindData(int32 Index, FNiagaraDataSet& DataSet, int32 StartInstance, bool bUpdateInstanceCounts)
{
	FNiagaraDataBuffer* Input = DataSet.GetCurrentData();
	FNiagaraDataBuffer* Output = DataSet.GetDestinationData();

	DataSetInfo.SetNum(FMath::Max(DataSetInfo.Num(), Index + 1));
	DataSetInfo[Index].Init(&DataSet, Input, Output, StartInstance, bUpdateInstanceCounts);

	//Would be nice to roll this and DataSetInfo into one but currently the VM being in it's own Engine module prevents this. Possibly should move the VM into Niagara itself.
	TArrayView<uint8 const* RESTRICT const> InputRegisters = Input ? Input->GetRegisterTable() : TArrayView<uint8 const* RESTRICT const>();
	TArrayView<uint8 const* RESTRICT const> OutputRegisters = Output ? Output->GetRegisterTable() : TArrayView<uint8 const* RESTRICT const>();

	DataSetMetaTable.SetNum(FMath::Max(DataSetMetaTable.Num(), Index + 1));
	DataSetMetaTable[Index].Init(InputRegisters, OutputRegisters, StartInstance,
		Output ? &Output->GetIDTable() : nullptr, &DataSet.GetFreeIDTable(), &DataSet.GetNumFreeIDs(), &DataSet.NumSpawnedIDs, &DataSet.GetMaxUsedID(), DataSet.GetIDAcquireTag(), &DataSet.GetSpawnedIDsTable());

	if (InputRegisters.Num() > 0)
	{
		static_assert(sizeof(DataSetMetaTable[Index].InputRegisterTypeOffsets) == sizeof(FNiagaraDataBuffer::RegisterTypeOffsetType), "ArraySizes do not match");
		memcpy(DataSetMetaTable[Index].InputRegisterTypeOffsets, Input->GetRegisterTypeOffsets(), sizeof(FNiagaraDataBuffer::RegisterTypeOffsetType));
	}

	if (OutputRegisters.Num() > 0)
	{
		static_assert(sizeof(DataSetMetaTable[Index].OutputRegisterTypeOffsets) == sizeof(FNiagaraDataBuffer::RegisterTypeOffsetType), "ArraySizes do not match");
		memcpy(DataSetMetaTable[Index].OutputRegisterTypeOffsets, Output->GetRegisterTypeOffsets(), sizeof(FNiagaraDataBuffer::RegisterTypeOffsetType));
	}
}

void FNiagaraScriptExecutionContextBase::BindData(int32 Index, FNiagaraDataBuffer* Input, int32 StartInstance, bool bUpdateInstanceCounts)
{
	check(Input && Input->GetOwner());
	DataSetInfo.SetNum(FMath::Max(DataSetInfo.Num(), Index + 1));
	FNiagaraDataSet* DataSet = Input->GetOwner();
	DataSetInfo[Index].Init(DataSet, Input, nullptr, StartInstance, bUpdateInstanceCounts);

	TArrayView<uint8 const* RESTRICT const> InputRegisters = Input->GetRegisterTable();

	DataSetMetaTable.SetNum(FMath::Max(DataSetMetaTable.Num(), Index + 1));
	DataSetMetaTable[Index].Init(InputRegisters, TArrayView<uint8 const* RESTRICT const>(), StartInstance, nullptr, nullptr, &DataSet->GetNumFreeIDs(), &DataSet->NumSpawnedIDs, &DataSet->GetMaxUsedID(), DataSet->GetIDAcquireTag(), &DataSet->GetSpawnedIDsTable());

	if (InputRegisters.Num() > 0)
	{
		static_assert(sizeof(DataSetMetaTable[Index].InputRegisterTypeOffsets) == sizeof(FNiagaraDataBuffer::RegisterTypeOffsetType), "ArraySizes do not match");
		memcpy(DataSetMetaTable[Index].InputRegisterTypeOffsets, Input->GetRegisterTypeOffsets(), sizeof(FNiagaraDataBuffer::RegisterTypeOffsetType));
	}
}

#if STATS
void FNiagaraScriptExecutionContextBase::CreateStatScopeData()
{
	StatScopeData.Empty();
	for (const TStatId& StatId : Script->GetStatScopeIDs())
	{
		StatScopeData.Add(FStatScopeData(StatId));
	}
}

TMap<TStatIdData const*, float> FNiagaraScriptExecutionContextBase::ReportStats()
{
	// Process recorded times
	for (FStatScopeData& ScopeData : StatScopeData)
	{
		uint64 ExecCycles = ScopeData.ExecutionCycleCount.exchange(0);
		if (ExecCycles > 0)
		{
			ExecutionTimings.FindOrAdd(ScopeData.StatId.GetRawPointer()) = ExecCycles;
		}
	}
	return ExecutionTimings;
}
#endif

static void *VVMRealloc(void* Ptr, size_t NumBytes, const char *Filename, int LineNum)
{
	return FMemory::Realloc(Ptr, NumBytes);
}
static void VVMFree(void* Ptr, const char *Filename, int LineNum)
{
	return FMemory::Free(Ptr);
}

bool FNiagaraScriptExecutionContextBase::Execute(uint32 NumInstances, const FScriptExecutionConstantBufferTable& ConstantBufferTable)
{
	if (NumInstances == 0)
	{
		DataSetInfo.Reset();
		return true;
	}

	++TickCounter;//Should this be per execution?

	if (GbExecVMScripts != 0)
	{
#if STATS
		CreateStatScopeData();
#endif	

		bool bSuccess = true;

#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY
		if (bUsingExperimentalVM)
		{
			bSuccess = ExecuteInternal_Experimental(NumInstances, ConstantBufferTable);
		}
		else
		{
			bSuccess = ExecuteInternal_Legacy(NumInstances, ConstantBufferTable);
		}
#elif VECTORVM_SUPPORTS_EXPERIMENTAL
		bSuccess = ExecuteInternal_Experimental(NumInstances, ConstantBufferTable);
#elif VECTORVM_SUPPORTS_LEGACY
		bSuccess = ExecuteInternal_Legacy(NumInstances, ConstantBufferTable);
#else
	#error "Not implemented"
#endif

		// Tell the datasets we wrote how many instances were actually written.
		for (int Idx = 0; Idx < DataSetInfo.Num(); Idx++)
		{
			FNiagaraDataSetExecutionInfo& Info = DataSetInfo[Idx];

#if NIAGARA_NAN_CHECKING
			Info.DataSet->CheckForNaNs();
#endif
			if (Info.bUpdateInstanceCount)
			{
				Info.Output->SetNumInstances(Info.StartInstance + DataSetMetaTable[Idx].DataSetAccessIndex + 1);
			}
		}

		//Can maybe do without resetting here. Just doing it for tidiness.
		for (int32 DataSetIdx = 0; DataSetIdx < DataSetInfo.Num(); ++DataSetIdx)
		{
			DataSetInfo[DataSetIdx].Reset();
			DataSetMetaTable[DataSetIdx].Reset();
		}
	}
	return true;//TODO: Error cases?
}

#if VECTORVM_SUPPORTS_EXPERIMENTAL

bool FNiagaraScriptExecutionContextBase::ExecuteInternal_Experimental(uint32 NumInstances, const FScriptExecutionConstantBufferTable& ConstantBufferTable)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VectorVM_Experimental);
	FVectorVMExecContext ExecCtx;
	ExecCtx.VVMState            = VectorVMState;
	ExecCtx.DataSets            = DataSetMetaTable;
	ExecCtx.ExtFunctionTable    = FunctionTable;
	ExecCtx.UserPtrTable        = UserPtrTable;
	ExecCtx.NumInstances        = NumInstances;
	ExecCtx.ConstantTableData   = ConstantBufferTable.Buffers.GetData();
	ExecCtx.ConstantTableSizes  = ConstantBufferTable.BufferSizes.GetData();
	ExecCtx.ConstantTableCount  = ConstantBufferTable.Buffers.Num();

	if (VectorVMState)
	{
		ExecVectorVMState(&ExecCtx, nullptr, nullptr);
	}
	return true;
}

#endif

#if VECTORVM_SUPPORTS_LEGACY

bool FNiagaraScriptExecutionContextBase::ExecuteInternal_Legacy(uint32 NumInstances, const FScriptExecutionConstantBufferTable& ConstantBufferTable)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VectorVM_Legacy);

	FNiagaraVMExecutableData& ExecData = Script->GetVMExecutableData();

	// If we have an optimization task it must be ready at this point
	// However we will need to lock and test again as multiple threads may be coming in here
	if (ExecData.OptimizationTask.State.IsValid())
	{
		FScopeLock Lock(&ExecData.OptimizationTask.Lock);
		if (ExecData.OptimizationTask.State.IsValid())
		{
			ExecData.ApplyFinishedOptimization(Script->GetVMExecutableDataCompilationId(), ExecData.OptimizationTask.State);
			ExecData.OptimizationTask.State.Reset();
		}
	}

	check((ExecData.ByteCode.HasByteCode() && !ExecData.ByteCode.IsCompressed()) || (ExecData.OptimizedByteCode.HasByteCode() && !ExecData.OptimizedByteCode.IsCompressed()));

	VectorVM::FVectorVMExecArgs ExecArgs;
	ExecArgs.ByteCode = ExecData.ByteCode.GetDataPtr();
	ExecArgs.OptimizedByteCode = ExecData.OptimizedByteCode.HasByteCode() ? ExecData.OptimizedByteCode.GetDataPtr() : nullptr;
	ExecArgs.NumTempRegisters = ExecData.NumTempRegisters;
	ExecArgs.ConstantTableCount = ConstantBufferTable.Buffers.Num();
	ExecArgs.ConstantTable = ConstantBufferTable.Buffers.GetData();
	ExecArgs.ConstantTableSizes = ConstantBufferTable.BufferSizes.GetData();
	ExecArgs.DataSetMetaTable = DataSetMetaTable;
	ExecArgs.ExternalFunctionTable = FunctionTable.GetData();
	ExecArgs.UserPtrTable = UserPtrTable.GetData();
	ExecArgs.NumInstances = NumInstances;
#if STATS
	ExecArgs.StatScopes = MakeArrayView(StatScopeData);
#elif ENABLE_STATNAMEDEVENTS
	ExecArgs.StatNamedEventsScopes = Script->GetStatNamedEvents();
#endif

	ExecArgs.bAllowParallel = bAllowParallel;
	FVectorVMSerializeState UESerializeState = { };
#ifdef VVM_INCLUDE_SERIALIZATION
	static const IConsoleVariable* CVarInstancesPerChunk = IConsoleManager::Get().FindConsoleVariable(TEXT("vm.InstancesPerChunk"));
	int32 NumParallelInstancesPerChunk = CVarInstancesPerChunk ? CVarInstancesPerChunk->GetInt() : 128;

	UESerializeState.ReallocFn        = VVMRealloc;
	UESerializeState.FreeFn           = VVMFree;
	UESerializeState.NumInstances     = NumInstances;
	UESerializeState.NumTempRegisters = ExecData.NumTempRegisters;
	UESerializeState.NumTempRegFlags  = UESerializeState.NumTempRegisters;
		
	UESerializeState.TempRegFlags     = (unsigned char *)VVMRealloc(NULL, UESerializeState.NumTempRegisters, __FILE__, __LINE__);
	FMemory::Memset(UESerializeState.TempRegFlags, 0, UESerializeState.NumTempRegisters);
	UESerializeState.Bytecode         = (unsigned char *)ExecData.ByteCode.GetData().GetData();
	UESerializeState.NumBytecodeBytes = ExecData.ByteCode.GetData().Num();
	SerializeVectorVMInputDataSets(&UESerializeState, DataSetMetaTable,  ConstantBufferTable.Buffers.GetData(), ConstantBufferTable.BufferSizes.GetData(), ConstantBufferTable.Buffers.Num());

	UESerializeState.NumChunks        = (NumInstances + NumParallelInstancesPerChunk - 1) / NumParallelInstancesPerChunk;
	UESerializeState.Chunks           = (FVectorVMSerializeChunk *)VVMRealloc(NULL, sizeof(FVectorVMSerializeChunk) * UESerializeState.NumChunks, __FILE__, __LINE__);

		
	int NumExternalFunctions = FunctionTable.Num();
	if (NumExternalFunctions != 0)
	{
		UESerializeState.ExternalData = (FVectorVMSerializeExternalData *)VVMRealloc(NULL, sizeof(FVectorVMSerializeExternalData) * NumExternalFunctions, __FILE__, __LINE__);
		if (UESerializeState.ExternalData)
		{
			UESerializeState.NumExternalData = NumExternalFunctions;
			const FNiagaraVMExecutableData& ScriptExecutableData = Script->GetVMExecutableData();
			for (int i = 0; i < NumExternalFunctions; ++i)
			{
				FVectorVMSerializeExternalData *ExtData = UESerializeState.ExternalData + i;
				TCHAR buff[128];
				uint32 NameLen = ScriptExecutableData.CalledVMExternalFunctions[i].Name.ToString(buff, sizeof(buff) >> 1);
				if (NameLen > 0)
				{
					ExtData->Name       = (wchar_t *)UESerializeState.ReallocFn(NULL, sizeof(wchar_t) * NameLen, __FILE__, __LINE__);
					FMemory::Memcpy(ExtData->Name, buff, sizeof(TCHAR) * NameLen);
					ExtData->NameLen    = NameLen;
				}
				else
				{
					ExtData->Name       = NULL;
					ExtData->NameLen    = 0;
				}
				ExtData->NumInputs      = ScriptExecutableData.CalledVMExternalFunctions[i].GetNumInputs();
				ExtData->NumOutputs     = ScriptExecutableData.CalledVMExternalFunctions[i].GetNumOutputs();
			}
		}
	}
#endif //VVM_INCLUDE_SERIALIZATION
	VectorVM::Exec(ExecArgs, &UESerializeState);

#ifdef VVM_INCLUDE_SERIALIZATION
		SerializeVectorVMOutputDataSets(&UESerializeState, DataSetMetaTable,  ConstantBufferTable.Buffers.GetData(), ConstantBufferTable.BufferSizes.GetData(), ConstantBufferTable.Buffers.Num());
#endif //VVM_INCLUDE_SERIALIZATION

	for (int32 Idx = 0; Idx < DataSetInfo.Num(); Idx++)
	{
		FNiagaraDataSetExecutionInfo& Info = DataSetInfo[Idx];

		Info.DataSet->NumSpawnedIDs = Info.DataSet->GetSpawnedIDsTable().Num();
	}

#		ifdef VVM_INCLUDE_SERIALIZATION
		{
			//we don't own the bytecode memory so we can't free it
			UESerializeState.Bytecode = NULL;
			UESerializeState.NumBytecodeBytes = 0;
			FreeVectorVMSerializeState(&UESerializeState);
		}
#		endif //VVM_INCLUDE_SERIALIZATION

	return true;
}

#endif // VECTORVM_SUPPORTS_LEGACY

TArrayView<const uint8> FNiagaraScriptExecutionContextBase::GetScriptLiterals() const
{
#if WITH_EDITORONLY_DATA
	if (!Script->IsScriptCooked())
	{
		return Parameters.GetScriptLiterals();
	}
#endif
	return MakeArrayView(Script->GetVMExecutableData().ScriptLiterals);
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraScriptExecutionContextBase::DirtyDataInterfaces()
{
	Parameters.MarkInterfacesDirty();
}

bool FNiagaraScriptExecutionContext::Tick(FNiagaraSystemInstance* ParentSystemInstance, ENiagaraSimTarget SimTarget)
{
	//Bind data interfaces if needed.
	if (Parameters.GetInterfacesDirty())
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraScriptExecContextTick);
		if (Script && Script->IsReadyToRun(ENiagaraSimTarget::CPUSim) && SimTarget == ENiagaraSimTarget::CPUSim)//TODO: Remove. Script can only be null for system instances that currently don't have their script exec context set up correctly.
		{
			const FNiagaraVMExecutableData& ScriptExecutableData = Script->GetVMExecutableData();
			const TArray<UNiagaraDataInterface*>& DataInterfaces = GetDataInterfaces();

			SCOPE_CYCLE_COUNTER(STAT_NiagaraRebindDataInterfaceFunctionTable);
			// UE_LOG(LogNiagara, Log, TEXT("Updating data interfaces for script %s"), *Script->GetFullName());

			// We must make sure that the data interfaces match up between the original script values and our overrides...
			if (ScriptExecutableData.DataInterfaceInfo.Num() != DataInterfaces.Num())
			{
				UE_LOG(LogNiagara, Warning, TEXT("Mismatch between Niagara Exectuion Context data interfaces and those in it's script!"));
				return false;
			}

			const FNiagaraScriptExecutionParameterStore* ScriptParameterStore = Script->GetExecutionReadyParameterStore(SimTarget);
			check(ScriptParameterStore != nullptr);

			//Fill the instance data table.
			if (ParentSystemInstance)
			{
				UserPtrTable.SetNumZeroed(ScriptExecutableData.NumUserPtrs, false);
				for (int32 i = 0; i < DataInterfaces.Num(); i++)
				{
					UNiagaraDataInterface* Interface = DataInterfaces[i];

					int32 UserPtrIdx = ScriptExecutableData.DataInterfaceInfo[i].UserPtrIdx;
					if (UserPtrIdx != INDEX_NONE)
					{
						if (void* InstData = ParentSystemInstance->FindDataInterfaceInstanceData(Interface))
						{
							UserPtrTable[UserPtrIdx] = InstData;
						}
						else
						{
							UE_LOG(LogNiagara, Warning, TEXT("Failed to resolve User Pointer for UserPtrTable[%d] looking for DI: %s for system: %s"),
								UserPtrIdx,
								*Interface->GetName(),
								*ParentSystemInstance->GetSystem()->GetName());
							return false;
						}
					}
				}
			}
			else
			{
				check(ScriptExecutableData.NumUserPtrs == 0);//Can't have user ptrs if we have no parent instance.
			}

			const int32 FunctionCount = ScriptExecutableData.CalledVMExternalFunctions.Num();
			FunctionTable.Reset(FunctionCount);
			FunctionTable.AddZeroed(FunctionCount);
			LocalFunctionTable.Reset();
			TArray<int32> LocalFunctionTableIndices;
			LocalFunctionTableIndices.Reserve(FunctionCount);

			const auto& ScriptDataInterfaces = ScriptParameterStore->GetDataInterfaces();

			bool bSuccessfullyMapped = true;

			for (int32 FunctionIt = 0; FunctionIt < FunctionCount; ++FunctionIt)
			{
				const FVMExternalFunctionBindingInfo& BindingInfo = ScriptExecutableData.CalledVMExternalFunctions[FunctionIt];

				// First check to see if we can pull from the fast path library..
				FVMExternalFunction FuncBind;
				if (UNiagaraFunctionLibrary::GetVectorVMFastPathExternalFunction(BindingInfo, FuncBind) && FuncBind.IsBound())
				{
					LocalFunctionTable.Add(FuncBind);
					LocalFunctionTableIndices.Add(FunctionIt);
					continue;
				}

				for (int32 i = 0; i < ScriptExecutableData.DataInterfaceInfo.Num(); i++)
				{
					const FNiagaraScriptDataInterfaceCompileInfo& ScriptInfo = ScriptExecutableData.DataInterfaceInfo[i];
					UNiagaraDataInterface* ExternalInterface = DataInterfaces[i];
					if (ScriptInfo.Name == BindingInfo.OwnerName)
					{
						// first check to see if we should just use the one from the script
						if (ScriptExecutableData.CalledVMExternalFunctionBindings.IsValidIndex(FunctionIt)
							&& ScriptDataInterfaces.IsValidIndex(i)
							&& ExternalInterface == ScriptDataInterfaces[i])
						{
							const FVMExternalFunction& ScriptFuncBind = ScriptExecutableData.CalledVMExternalFunctionBindings[FunctionIt];
							if (ScriptFuncBind.IsBound())
							{
								FunctionTable[FunctionIt] = &ScriptFuncBind;

								check(ScriptInfo.UserPtrIdx == INDEX_NONE);
								break;
							}
						}

						void* InstData = ScriptInfo.UserPtrIdx == INDEX_NONE ? nullptr : UserPtrTable[ScriptInfo.UserPtrIdx];
						FVMExternalFunction& LocalFunction = LocalFunctionTable.AddDefaulted_GetRef();
						LocalFunctionTableIndices.Add(FunctionIt);

						if (ExternalInterface != nullptr)
						{
							ExternalInterface->GetVMExternalFunction(BindingInfo, InstData, LocalFunction);
						}

						if (!LocalFunction.IsBound())
						{
							UE_LOG(LogNiagara, Error, TEXT("Could not Get VMExternalFunction '%s'.. emitter will not run!"), *BindingInfo.Name.ToString());
							bSuccessfullyMapped = false;
						}
						break;
					}
				}
			}

			const int32 LocalFunctionCount = LocalFunctionTableIndices.Num();
			for (int32 LocalFunctionIt = 0; LocalFunctionIt < LocalFunctionCount; ++LocalFunctionIt)
			{
				FunctionTable[LocalFunctionTableIndices[LocalFunctionIt]] = &LocalFunctionTable[LocalFunctionIt];
			}

			for (int32 i = 0; i < FunctionTable.Num(); i++)
			{
				if (FunctionTable[i] == nullptr)
				{
					UE_LOG(LogNiagara, Warning, TEXT("Invalid Function Table Entry! %s"), *ScriptExecutableData.CalledVMExternalFunctions[i].Name.ToString());
				}
			}

#if WITH_EDITOR	
			// We may now have new errors that we need to broadcast about, so flush the asset parameters delegate..
			if (ParentSystemInstance)
			{
				ParentSystemInstance->RaiseNeedsUIResync();
			}
#endif

			if (!bSuccessfullyMapped)
			{
				UE_LOG(LogNiagara, Warning, TEXT("Error building data interface function table!"));
				FunctionTable.Empty();
				return false;
			}
		}
	}
	if (ParentSystemInstance && Parameters.GetPositionDataDirty())
	{
		Parameters.ResolvePositions(ParentSystemInstance->GetLWCConverter());
	}
	Parameters.Tick();

	return true;
}

void FNiagaraScriptExecutionContextBase::PostTick()
{
	//If we're for interpolated spawn, copy over the previous frame's parameters into the Prev parameters.
	if (HasInterpolationParameters)
	{
		Parameters.CopyCurrToPrev();
	}
}

//////////////////////////////////////////////////////////////////////////
#if VECTORVM_SUPPORTS_EXPERIMENTAL
static void PerInsFn(FVectorVMExternalFunctionContext& ParentContext, FVectorVMExternalFunctionContextExperimental& PerInsFnContext, TArray<FNiagaraSystemInstance*>** SystemInstances, ENiagaraSystemSimulationScript ScriptType, int32 PerInstFunctionIndex, int32 UserPtrIdx)
{
	check(PerInsFnContext.DataSets.Num() > 0);
	check(SystemInstances);
	check(*SystemInstances);

	void* SavedUserPtrData = UserPtrIdx != INDEX_NONE ? PerInsFnContext.UserPtrTable[UserPtrIdx] : NULL;
	int32 InstanceOffset = PerInsFnContext.DataSets[0].InstanceOffset; //Apparently the function table is generated based off the first data set, therefore this is safe. (I don't like this - @shawn mcgrath)	
	int NumInstances = PerInsFnContext.NumInstances;
	PerInsFnContext.NumInstances = 1;
	for (int i = 0; i < NumInstances; ++i)
	{
		PerInsFnContext.RegReadCount = 0;
		PerInsFnContext.PerInstanceFnInstanceIdx = i;

		int32 InstanceIndex = InstanceOffset + PerInsFnContext.StartInstance + i;
		FNiagaraSystemInstance* Instance = (**SystemInstances)[InstanceIndex];
		const FNiagaraPerInstanceDIFuncInfo& FuncInfo = Instance->GetPerInstanceDIFunction(ScriptType, PerInstFunctionIndex);

		if (UserPtrIdx != INDEX_NONE)
		{
			PerInsFnContext.UserPtrTable[UserPtrIdx] = FuncInfo.InstData;
		}
		FuncInfo.Function.Execute(ParentContext);
	}

	if (SavedUserPtrData)
	{
		PerInsFnContext.UserPtrTable[UserPtrIdx] = SavedUserPtrData;
	}
}
#endif

#if VECTORVM_SUPPORTS_LEGACY
void FNiagaraSystemScriptExecutionContext::PerInstanceFunctionHook(FVectorVMExternalFunctionContext& ParentContext, FVectorVMExternalFunctionContextLegacy& Context, int32 PerInstFunctionIndex, int32 UserPtrIndex)
{
	check(SystemInstances);

	//This is a bit of a hack. We grab the base offset into the instance data from the primary dataset.
	//TODO: Find a cleaner way to do this.
	int32 InstanceOffset = Context.VectorVMContext->GetDataSetMeta(0).InstanceOffset;

	//Cache context state.
	int32 CachedContextStartInstance = Context.VectorVMContext->GetStartInstance();
	int32 CachedContextNumInstances = Context.VectorVMContext->GetNumInstances();
	uint8 const* CachedCodeLocation = Context.VectorVMContext->Code;

	//Hack context so we can run the DI calls one by one.
	Context.VectorVMContext->NumInstances = 1;

	for (int32 i = 0; i < CachedContextNumInstances; ++i)
	{
		//Reset the code each iteration.
		Context.VectorVMContext->Code = CachedCodeLocation;
		//Offset buffer I/O to the correct instance's data.
		Context.VectorVMContext->ExternalFunctionInstanceOffset = i;

		int32 InstanceIndex = InstanceOffset + CachedContextStartInstance + i;
		FNiagaraSystemInstance* Instance = (*SystemInstances)[InstanceIndex];
		const FNiagaraPerInstanceDIFuncInfo& FuncInfo = Instance->GetPerInstanceDIFunction(ScriptType, PerInstFunctionIndex);

		//TODO: We can embed the instance data inside the function lambda. No need for the user ptr table at all.
		//Do this way for now to reduce overall complexity of the initial change. Doing this needs extensive boiler plate changes to most DI classes and a script recompile.
		if (UserPtrIndex != INDEX_NONE)
		{
			Context.VectorVMContext->UserPtrTable[UserPtrIndex] = FuncInfo.InstData;
		}

		Context.VectorVMContext->StartInstance = InstanceIndex;

		//TODO: In future for DIs where more perf is needed here we could split the DI func into an args gen and a execution.
		//The this path could gen args from the bytecode once and just run the execution func per instance.
		//I wonder if we could auto generate the args gen in a template func and just pass them into the DI for perf and reduced end user/author complexity.
		FuncInfo.Function.Execute(ParentContext);
	}

	//Restore the context state.
	Context.VectorVMContext->ExternalFunctionInstanceOffset = 0;
	Context.VectorVMContext->StartInstance = CachedContextStartInstance;
	Context.VectorVMContext->NumInstances = CachedContextNumInstances;
}
#endif

bool FNiagaraSystemScriptExecutionContext::Init(UNiagaraScript* InScript, ENiagaraSimTarget InTarget)
{
	//FORT - 314222 - There is a bug currently when system scripts execute in parallel.
	//This is unlikely for these scripts but we're explicitly disallowing it for safety.
	bAllowParallel = false;

	return FNiagaraScriptExecutionContextBase::Init(InScript, InTarget);
}

bool FNiagaraSystemScriptExecutionContext::Tick(class FNiagaraSystemInstance* Instance, ENiagaraSimTarget SimTarget)
{
	//Bind data interfaces if needed.
	if (Parameters.GetInterfacesDirty())
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraScriptExecContextTick);
		if (Script && Script->IsReadyToRun(ENiagaraSimTarget::CPUSim))//TODO: Remove. Script can only be null for system instances that currently don't have their script exec context set up correctly.
		{
			const FNiagaraVMExecutableData& ScriptExecutableData = Script->GetVMExecutableData();
			const TArray<UNiagaraDataInterface*>& DataInterfaces = GetDataInterfaces();

			const int32 FunctionCount = ScriptExecutableData.CalledVMExternalFunctions.Num();
			FunctionTable.Reset();
			FunctionTable.SetNum(FunctionCount);
			ExtFunctionInfo.AddDefaulted(FunctionCount);

			const FNiagaraScriptExecutionParameterStore* ScriptParameterStore = Script->GetExecutionReadyParameterStore(ENiagaraSimTarget::CPUSim);
			check(ScriptParameterStore != nullptr);
			const auto& ScriptDataInterfaces = ScriptParameterStore->GetDataInterfaces();
			int32 NumPerInstanceFunctions = 0;
			for (int32 FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
			{
				const FVMExternalFunctionBindingInfo& BindingInfo = ScriptExecutableData.CalledVMExternalFunctions[FunctionIndex];

				FExternalFuncInfo& FuncInfo = ExtFunctionInfo[FunctionIndex];

				// First check to see if we can pull from the fast path library..		
				if (UNiagaraFunctionLibrary::GetVectorVMFastPathExternalFunction(BindingInfo, FuncInfo.Function) && FuncInfo.Function.IsBound())
				{
					continue;
				}

				//TODO: Remove use of userptr table here and just embed the instance data in the function lambda.
				UserPtrTable.SetNumZeroed(ScriptExecutableData.NumUserPtrs, false);

				//Next check DI functions.
				for (int32 i = 0; i < ScriptExecutableData.DataInterfaceInfo.Num(); i++)
				{
					const FNiagaraScriptDataInterfaceCompileInfo& ScriptDIInfo = ScriptExecutableData.DataInterfaceInfo[i];
					UNiagaraDataInterface* ScriptInterface = ScriptDataInterfaces[i];
					UNiagaraDataInterface* ExternalInterface = GetDataInterfaces()[i];

					if (ScriptDIInfo.Name == BindingInfo.OwnerName)
					{
						//Currently we must assume that any User DI is overridden but maybe we can be less conservative with this in future.
						if (ScriptDIInfo.NeedsPerInstanceBinding())
						{
#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY
							if (bUsingExperimentalVM)
							{
								FuncInfo.Function = FVMExternalFunction::CreateLambda(
									[SystemInstances = &this->SystemInstances, ScriptType = this->ScriptType, NumPerInstanceFunctions, UserPtrIdx = ScriptDIInfo.UserPtrIdx](FVectorVMExternalFunctionContext& ExtFnContext)
									{
										PerInsFn(ExtFnContext, ExtFnContext.Experimental, SystemInstances, ScriptType, NumPerInstanceFunctions, UserPtrIdx);
									}
								);
							}
							else
							{
								FuncInfo.Function = FVMExternalFunction::CreateLambda(
									[ExecContext = this, NumPerInstanceFunctions, UserPtrIndex = ScriptDIInfo.UserPtrIdx](FVectorVMExternalFunctionContext& Context)
									{
										//This DI needs a binding per instance so we just bind to the external function hook which will call the correct binding for each instance.
										ExecContext->PerInstanceFunctionHook(Context, Context.Legacy, NumPerInstanceFunctions, UserPtrIndex);
									}
								);
							}
#elif VECTORVM_SUPPORTS_EXPERIMENTAL
							FuncInfo.Function = FVMExternalFunction::CreateLambda(
								[SystemInstances = &this->SystemInstances, ScriptType = this->ScriptType, NumPerInstanceFunctions, UserPtrIdx = ScriptDIInfo.UserPtrIdx](FVectorVMExternalFunctionContext& ExtFnContext)
							{
								PerInsFn(ExtFnContext, ExtFnContext, SystemInstances, ScriptType, NumPerInstanceFunctions, UserPtrIdx);
							}
							);
#elif VECTORVM_SUPPORTS_LEGACY
							FuncInfo.Function = FVMExternalFunction::CreateLambda(
								[ExecContext = this, NumPerInstanceFunctions, UserPtrIndex = ScriptDIInfo.UserPtrIdx](FVectorVMExternalFunctionContext& Context)
							{
								//This DI needs a binding per instance so we just bind to the external function hook which will call the correct binding for each instance.
								ExecContext->PerInstanceFunctionHook(Context, Context, NumPerInstanceFunctions, UserPtrIndex);
							}
							);
#endif
							++NumPerInstanceFunctions;
						}
						else
						{
							// first check to see if we should just use the one from the script
							if (ScriptExecutableData.CalledVMExternalFunctionBindings.IsValidIndex(FunctionIndex)
								&& ScriptInterface
								&& ExternalInterface == ScriptDataInterfaces[i])
							{
								const FVMExternalFunction& ScriptFuncBind = ScriptExecutableData.CalledVMExternalFunctionBindings[FunctionIndex];
								if (ScriptFuncBind.IsBound())
								{
									FuncInfo.Function = ScriptFuncBind;
									check(ScriptDIInfo.UserPtrIdx == INDEX_NONE);
									break;
								}
							}

							//If we don't need a call per instance we can just bind directly to the DI function call;
							check(ExternalInterface);
							ExternalInterface->GetVMExternalFunction(BindingInfo, nullptr, FuncInfo.Function);
						}
						break;
					}
				}				

				if (FuncInfo.Function.IsBound() == false)
				{
					UE_LOG(LogNiagara, Warning, TEXT("Error building data interface function table for system script!"));
					FunctionTable.Empty();
					return false;
				}
			}

			if (FunctionTable.Num() != ExtFunctionInfo.Num())
			{
				UE_LOG(LogNiagara, Warning, TEXT("Error building data interface function table for system script!"));
				FunctionTable.Empty();
				return false;
			}

			for (int32 FunctionIt = 0; FunctionIt < FunctionTable.Num(); ++FunctionIt)
			{
				FunctionTable[FunctionIt] = &ExtFunctionInfo[FunctionIt].Function;
			}

			for (int32 i = 0; i < FunctionTable.Num(); i++)
			{
				if (FunctionTable[i] == nullptr)
				{
					UE_LOG(LogNiagara, Warning, TEXT("Invalid Function Table Entry! %s"), *ScriptExecutableData.CalledVMExternalFunctions[i].Name.ToString());
				}
			}
		}
	}
	if (Instance && Parameters.GetPositionDataDirty())
	{
		Parameters.ResolvePositions(Instance->GetLWCConverter());
	}
	Parameters.Tick();

	return true;
}

bool FNiagaraSystemScriptExecutionContext::GeneratePerInstanceDIFunctionTable(FNiagaraSystemInstance* Inst, TArray<FNiagaraPerInstanceDIFuncInfo>& OutFunctions)
{
	const FNiagaraScriptExecutionParameterStore* ScriptParameterStore = Script->GetExecutionReadyParameterStore(ENiagaraSimTarget::CPUSim);
	const FNiagaraVMExecutableData& ScriptExecutableData = Script->GetVMExecutableData();

	for (int32 FunctionIndex = 0; FunctionIndex < ScriptExecutableData.CalledVMExternalFunctions.Num(); ++FunctionIndex)
	{
		const FVMExternalFunctionBindingInfo& BindingInfo = ScriptExecutableData.CalledVMExternalFunctions[FunctionIndex];

		for (int32 i = 0; i < ScriptExecutableData.DataInterfaceInfo.Num(); i++)
		{
			const FNiagaraScriptDataInterfaceCompileInfo& ScriptDIInfo = ScriptExecutableData.DataInterfaceInfo[i];
			//UNiagaraDataInterface* ScriptInterface = ScriptDataInterfaces[i];
			UNiagaraDataInterface* ExternalInterface = GetDataInterfaces()[i];

			if (ScriptDIInfo.Name == BindingInfo.OwnerName && ScriptDIInfo.NeedsPerInstanceBinding())
			{
				UNiagaraDataInterface* DIToBind = nullptr;
				FNiagaraPerInstanceDIFuncInfo& NewFuncInfo = OutFunctions.AddDefaulted_GetRef();
				void* InstData = nullptr;

				if (const int32* DIIndex = Inst->GetInstanceParameters().FindParameterOffset(FNiagaraVariable(ScriptDIInfo.Type, ScriptDIInfo.Name)))
				{
					//If this is a User DI we bind to the user DI and find instance data with it.
					if (UNiagaraDataInterface* UserInterface = Inst->GetInstanceParameters().GetDataInterface(*DIIndex))
					{
						DIToBind = UserInterface;
						InstData = Inst->FindDataInterfaceInstanceData(UserInterface);
					}
				}
				else
				{
					//Otherwise we use the script DI and search for instance data with that.
					DIToBind = ExternalInterface;
					InstData = Inst->FindDataInterfaceInstanceData(ExternalInterface);
				}

				if (DIToBind)
				{
					check(ExternalInterface->PerInstanceDataSize() == 0 || InstData);
					DIToBind->GetVMExternalFunction(BindingInfo, InstData, NewFuncInfo.Function);
					NewFuncInfo.InstData = InstData;
				}

				if (NewFuncInfo.Function.IsBound() == false)
				{
					return false;
				}
				break;
			}
		}
	}
	return true;
};

