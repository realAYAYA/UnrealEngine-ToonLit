// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraEmitterInstance.h: Niagara emitter simulation class
==============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "NiagaraDataSet.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraScriptExecutionParameterStore.h"
#include "NiagaraTypes.h"

struct FNiagaraDataInterfaceProxy;

/** All scripts that will use the system script execution context. */
enum class ENiagaraSystemSimulationScript : uint8
{
	Spawn,
	Update,
	Num,
	//TODO: Maybe add emitter spawn and update here if we split those scripts out.
};

/** Container for data needed to process event data. */
struct FNiagaraEventHandlingInfo
{
	FNiagaraEventHandlingInfo()
		: TotalSpawnCount(0)
		, EventData(nullptr)
		, SourceEmitterName(NAME_None)
	{
	}

	~FNiagaraEventHandlingInfo()
	{
		SetEventData(nullptr);
	}

	void SetEventData(FNiagaraDataBuffer* InEventData)
	{
		if (EventData)
		{
			EventData->ReleaseReadRef();
		}
		EventData = InEventData;
		if (EventData)
		{
			EventData->AddReadRef();
		}
	}

	TArray<int32, TInlineAllocator<16>> SpawnCounts;
	int32 TotalSpawnCount;
	FNiagaraDataBuffer* EventData;
	FGuid SourceEmitterGuid;
	FName SourceEmitterName;
};


struct FNiagaraDataSetExecutionInfo
{
	FNiagaraDataSetExecutionInfo()
		: DataSet(nullptr)
		, Input(nullptr)
		, Output(nullptr)
		, StartInstance(0)
		, bUpdateInstanceCount(false)
	{
		Reset();
	}


	FORCEINLINE void Init(FNiagaraDataSet* InDataSet, FNiagaraDataBuffer* InInput, FNiagaraDataBuffer* InOutput, int32 InStartInstance, bool bInUpdateInstanceCount)
	{
		if (Input)
		{
			Input->ReleaseReadRef();
		}

		DataSet = InDataSet;
		Input = InInput;
		Output = InOutput;
		StartInstance = InStartInstance;
		bUpdateInstanceCount = bInUpdateInstanceCount;

		check(DataSet);
		check(Input == nullptr || DataSet == Input->GetOwner());
		check(Output == nullptr || DataSet == Output->GetOwner());

		if (Input)
		{
			Input->AddReadRef();
		}
		check(Output == nullptr || Output->IsBeingWritten());
	}
	
	~FNiagaraDataSetExecutionInfo()
	{
		Reset();
	}

	FORCEINLINE void Reset()
	{
		if (Input)
		{
			Input->ReleaseReadRef();
		}

		DataSet = nullptr;
		Input = nullptr;
		Output = nullptr;
		StartInstance = INDEX_NONE;
		bUpdateInstanceCount = false;
	}

	FNiagaraDataSet* DataSet;
	FNiagaraDataBuffer* Input;
	FNiagaraDataBuffer* Output;
	int32 StartInstance;
	bool bUpdateInstanceCount;
};

struct FScriptExecutionConstantBufferTable
{
	static constexpr uint32 MaxBufferCount = 12;

	TArray<const uint8*, TInlineAllocator<MaxBufferCount>> Buffers;
	TArray<int32, TInlineAllocator<MaxBufferCount>> BufferSizes;

	void Reset(int32 ResetSize)
	{
		Buffers.Reset(ResetSize);
		BufferSizes.Reset(ResetSize);
	}

	template<typename T>
	void AddTypedBuffer(const T& Buffer)
	{
		Buffers.Add(reinterpret_cast<const uint8*>(&Buffer));
		BufferSizes.Add(sizeof(T));
	}

	void AddRawBuffer(const uint8* BufferData, int32 BufferSize)
	{
		Buffers.Add(BufferData);
		BufferSizes.Add(BufferSize);
	}
};

struct FNiagaraScriptExecutionContextBase
{
	UNiagaraScript* Script;

	FVectorVMState *VectorVMState;
	
	/** Table of external function delegate handles called from the VM. */
	TArray<const FVMExternalFunction*> FunctionTable;

	/**
	Table of user ptrs to pass to the VM.
	*/
	TArray<void*> UserPtrTable;

	/** Parameter store. Contains all data interfaces and a parameter buffer that can be used directly by the VM or GPU. */
	FNiagaraScriptInstanceParameterStore Parameters;

	TArray<FDataSetMeta, TInlineAllocator<2>> DataSetMetaTable;

	TArray<FNiagaraDataSetExecutionInfo, TInlineAllocator<2>> DataSetInfo;

	static uint32 TickCounter;

	/** The script type this context is for. Allows us to access the correct per instance function table on the system instance. */
	ENiagaraSystemSimulationScript ScriptType;

	int32 HasInterpolationParameters : 1;
	int32 bAllowParallel : 1;
#if STATS
	TArray<FStatScopeData> StatScopeData;
	TMap<TStatIdData const*, float> ExecutionTimings;
	void CreateStatScopeData();
	TMap<TStatIdData const*, float> ReportStats();
#endif

#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY
	bool bUsingExperimentalVM;
#endif

	FNiagaraScriptExecutionContextBase();
	virtual ~FNiagaraScriptExecutionContextBase();

	virtual bool Init(UNiagaraScript* InScript, ENiagaraSimTarget InTarget);
	virtual bool Tick(class FNiagaraSystemInstance* Instance, ENiagaraSimTarget SimTarget) = 0;

	void BindData(int32 Index, FNiagaraDataSet& DataSet, int32 StartInstance, bool bUpdateInstanceCounts);
	void BindData(int32 Index, FNiagaraDataBuffer* Input, int32 StartInstance, bool bUpdateInstanceCounts);
	bool Execute(uint32 NumInstances, const FScriptExecutionConstantBufferTable& ConstantBufferTable);

	const TArray<UNiagaraDataInterface*>& GetDataInterfaces()const { return Parameters.GetDataInterfaces(); }

	TArrayView<const uint8> GetScriptLiterals() const;

	void DirtyDataInterfaces();
	void PostTick();

	//Unused. These are only useful in the new SystemScript context.
	virtual void BindSystemInstances(TArray<FNiagaraSystemInstance*>& InSystemInstances) {}
	virtual bool GeneratePerInstanceDIFunctionTable(FNiagaraSystemInstance* Inst, TArray<struct FNiagaraPerInstanceDIFuncInfo>& OutFunctions) {return true;}

private:
#if VECTORVM_SUPPORTS_EXPERIMENTAL
	bool ExecuteInternal_Experimental(uint32 NumInstances, const FScriptExecutionConstantBufferTable& ConstantBufferTable);
#endif // VECTORVM_SUPPORTS_EXPERIMENTAL
#if VECTORVM_SUPPORTS_LEGACY
	bool ExecuteInternal_Legacy(uint32 NumInstances, const FScriptExecutionConstantBufferTable& ConstantBufferTable);
#endif // VECTORVM_SUPPORTS_LEGACY
};

struct FNiagaraScriptExecutionContext : public FNiagaraScriptExecutionContextBase
{
protected:
	/**
	Table of external function delegates unique to the instance.
	*/
	TArray<FVMExternalFunction> LocalFunctionTable;

public:
	virtual bool Tick(class FNiagaraSystemInstance* Instance, ENiagaraSimTarget SimTarget)override;
};

/**
For function calls from system scripts on User DIs or those with per instance data, we build a per instance binding table that is called from a helper function in the exec context.
TODO: We can embed the instance data in the lambda capture for reduced complexity here. No need for the user ptr table.
We have to rebind if the instance data is recreated anyway.
*/
struct FNiagaraPerInstanceDIFuncInfo
{
	FVMExternalFunction Function;
	void* InstData;
};

/** Specialized exec context for system scripts. Allows us to better handle the added complication of Data Interfaces across different system instances. */
struct FNiagaraSystemScriptExecutionContext : public FNiagaraScriptExecutionContextBase
{
protected:

	struct FExternalFuncInfo
	{
		FVMExternalFunction Function;
	};

	TArray<FExternalFuncInfo> ExtFunctionInfo;

	/**
	Array of system instances the context is currently operating on.
	We need this to allow us to call into per instance DI functions.
	*/
	TArray<FNiagaraSystemInstance*>* SystemInstances;

#if VECTORVM_SUPPORTS_LEGACY
	/** Helper function that handles calling into per instance DI calls and massages the VM context appropriately. */
	void PerInstanceFunctionHook(FVectorVMExternalFunctionContext& ParentContext, FVectorVMExternalFunctionContextLegacy& Context, int32 PerInstFunctionIndex, int32 UserPtrIndex);
#endif

public:
	FNiagaraSystemScriptExecutionContext(ENiagaraSystemSimulationScript InScriptType) : SystemInstances(nullptr) { ScriptType = InScriptType; }
	
	virtual bool Init(UNiagaraScript* InScript, ENiagaraSimTarget InTarget)override;
	virtual bool Tick(class FNiagaraSystemInstance* Instance, ENiagaraSimTarget SimTarget);

	void BindSystemInstances(TArray<FNiagaraSystemInstance*>& InSystemInstances) { SystemInstances = &InSystemInstances; }

	/** Generates a table of DI calls unique to the passed system instance. These are then accesss inside the PerInstanceFunctionHook. */
	virtual bool GeneratePerInstanceDIFunctionTable(FNiagaraSystemInstance* Inst, TArray<FNiagaraPerInstanceDIFuncInfo>& OutFunctions);
};
