// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraDataSet.h"
#include "NiagaraParameterStore.h"

struct FNiagaraComputeExecutionContext;
class FNiagaraEmitterInstance;
class FNiagaraGpuComputeDispatchInterface;

class FNiagaraDataSetReadback : public TSharedFromThis<FNiagaraDataSetReadback, ESPMode::ThreadSafe>
{
public:
	DECLARE_DELEGATE_OneParam(FOnReadbackReady, const FNiagaraDataSetReadback&);

public:
	bool IsReady() const { return PendingReadbacks == 0; }
	void SetReadbackRead(FOnReadbackReady InOnReadbackReady);

	FName GetSourceName() const { return SourceName; }
	//ENiagaraScriptUsage GetSourceScriptUsage() const { return SourceScriptUsage; }
	const FNiagaraDataSet& GetDataSet() const { check(IsReady()); return DataSet; }
	const FNiagaraParameterStore& GetParameterStore() const { check(IsReady()); return ParameterStore; }

	void EnqueueReadback(FNiagaraEmitterInstance* EmitterInstance);
	void ImmediateReadback(FNiagaraEmitterInstance* EmitterInstance);

private:
	void ReadbackCompleteInternal();
	void GPUReadbackInternal(FRHICommandListImmediate& RHICmdList, FNiagaraGpuComputeDispatchInterface* DispatchInterface, FNiagaraComputeExecutionContext* GPUContext);

private:
	std::atomic<int>		PendingReadbacks;

	FName					SourceName;
	//ENiagaraScriptUsage		SourceScriptUsage;
	//FGuid					SourceUsageId;
	FNiagaraDataSet			DataSet;
	FNiagaraParameterStore	ParameterStore;
	TArray<int32>			IDToIndexTable;

	FOnReadbackReady		OnReadbackReady;
};
