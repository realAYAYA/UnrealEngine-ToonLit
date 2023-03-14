// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraComputeExecutionContext.h"
#include "NiagaraStats.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraGpuComputeDispatch.h"
#include "NiagaraDataInterface.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemGpuComputeProxy.h"
#include "NiagaraWorldManager.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraGPUInstanceCountManager.h"
#include "NiagaraGPUSystemTick.h"
#include "NiagaraDataInterfaceRW.h"

FNiagaraComputeExecutionContext::FNiagaraComputeExecutionContext()
	: MainDataSet(nullptr)
	, GPUScript(nullptr)
	, GPUScript_RT(nullptr)
{
}

FNiagaraComputeExecutionContext::~FNiagaraComputeExecutionContext()
{
	// EmitterInstanceReadback.GPUCountOffset should be INDEX_NONE at this point to ensure the index is reused.
	// When the ComputeDispatchInterface is being destroyed though, we don't free the index, but this would not be leaking.
	// check(EmitterInstanceReadback.GPUCountOffset == INDEX_NONE);
	SetDataToRender(nullptr);

	ExternalCBufferLayout = nullptr;
}

void FNiagaraComputeExecutionContext::Reset(FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface)
{
	FNiagaraGpuComputeDispatchInterface* RT_ComputeDispatchInterface = ComputeDispatchInterface && !ComputeDispatchInterface->IsPendingKill() ? ComputeDispatchInterface : nullptr;
	ENQUEUE_RENDER_COMMAND(ResetRT)(
		[RT_ComputeDispatchInterface, RT_Context=this](FRHICommandListImmediate& RHICmdList)
		{
			RT_Context->ResetInternal(RT_ComputeDispatchInterface);
		}
	);
}

void FNiagaraComputeExecutionContext::InitParams(UNiagaraScript* InGPUComputeScript, ENiagaraSimTarget InSimTarget)
{
	GPUScript = InGPUComputeScript;
	CombinedParamStore.InitFromOwningContext(InGPUComputeScript, InSimTarget, true);
	
	HasInterpolationParameters = GPUScript && GPUScript->GetComputedVMCompilationId().HasInterpolatedParameters();

	if (InGPUComputeScript)
	{
		FNiagaraVMExecutableData& VMData = InGPUComputeScript->GetVMExecutableData();
		if ( VMData.IsValid() )
		{
			SimStageInfo = VMData.SimulationStageMetaData;
		}
	}

#if DO_CHECK
	// DI Parameters are the same between all shader permutations so we can just get the first one
	FNiagaraShaderRef Shader = InGPUComputeScript->GetRenderThreadScript()->GetShaderGameThread(0);
	if (Shader.IsValid())
	{
		DIClassNames.Empty(Shader->GetDIParameters().Num());
		for (const FNiagaraDataInterfaceParamRef& DIParams : Shader->GetDIParameters())
		{
			DIClassNames.Add(DIParams.DIType.Get(Shader.GetPointerTable().DITypes)->GetClass()->GetName());
		}
	}
	else
	{
		TSharedRef<FNiagaraShaderScriptParametersMetadata> ScriptParametersMetadata = InGPUComputeScript->GetRenderThreadScript()->GetScriptParametersMetadata();
		DIClassNames.Empty(ScriptParametersMetadata->DataInterfaceParamInfo.Num());
		for (const FNiagaraDataInterfaceGPUParamInfo& DIParams : ScriptParametersMetadata->DataInterfaceParamInfo)
		{
			DIClassNames.Add(DIParams.DIClassName);
		}
	}
#endif
}

bool FNiagaraComputeExecutionContext::IsOutputStage(FNiagaraDataInterfaceProxy* DIProxy, uint32 SimulationStageIndex) const
{
	if (DIProxy && !DIProxy->SourceDIName.IsNone())
	{		
		return SimStageInfo[SimulationStageIndex].OutputDestinations.Contains(DIProxy->SourceDIName);
	}
	return false;
}

bool FNiagaraComputeExecutionContext::IsInputStage(FNiagaraDataInterfaceProxy* DIProxy, uint32 SimulationStageIndex) const
{
	if (DIProxy && !DIProxy->SourceDIName.IsNone())
	{
		return SimStageInfo[SimulationStageIndex].InputDataInterfaces.Contains(DIProxy->SourceDIName);
	}
	return false;
}

bool FNiagaraComputeExecutionContext::IsIterationStage(FNiagaraDataInterfaceProxy* DIProxy, uint32 SimulationStageIndex) const
{
	if (DIProxy && !DIProxy->SourceDIName.IsNone())
	{
		return !SimStageInfo[SimulationStageIndex].IterationSource.IsNone() && (SimStageInfo[SimulationStageIndex].IterationSource == DIProxy->SourceDIName);
	}
	return false;
}

FNiagaraDataInterfaceProxyRW* FNiagaraComputeExecutionContext::FindIterationInterface(const TArray<FNiagaraDataInterfaceProxyRW*>& InProxies, uint32 SimulationStageIndex) const
{
	// Particle stage
	if ( SimStageInfo[SimulationStageIndex].IterationSource.IsNone() )
	{
		return nullptr;
	}

	for (FNiagaraDataInterfaceProxyRW* Proxy : InProxies)
	{
		if (Proxy->SourceDIName == SimStageInfo[SimulationStageIndex].IterationSource)
		{
			return Proxy;
		}
	}

	UE_LOG(LogNiagara, Verbose, TEXT("FNiagaraComputeExecutionContext::FindIterationInterface could not find IterationInterface %s"), *SimStageInfo[SimulationStageIndex].IterationSource.ToString());

	return nullptr;
}

void FNiagaraComputeExecutionContext::DirtyDataInterfaces()
{
	CombinedParamStore.MarkInterfacesDirty();
}

bool FNiagaraComputeExecutionContext::Tick(FNiagaraSystemInstance* ParentSystemInstance)
{
	check(ParentSystemInstance);
	if (CombinedParamStore.GetInterfacesDirty())
	{
#if DO_CHECK
		const TArray<UNiagaraDataInterface*> &DataInterfaces = CombinedParamStore.GetDataInterfaces();
		// We must make sure that the data interfaces match up between the original script values and our overrides...
		if (DIClassNames.Num() != DataInterfaces.Num())
		{
			UE_LOG(LogNiagara, Warning, TEXT("Mismatch between Niagara GPU Execution Context data interfaces and those in its script!"));
			return false;
		}

		for (int32 i = 0; i < DIClassNames.Num(); ++i)
		{
			FString UsedClassName = DataInterfaces[i]->GetClass()->GetName();
			if (DIClassNames[i] != UsedClassName)
			{
				UE_LOG(LogNiagara, Warning, TEXT("Mismatched class between Niagara GPU Execution Context data interfaces and those in its script!\nIndex:%d\nShader:%s\nScript:%s")
					, i, *DIClassNames[i], *UsedClassName);
			}
		}
#endif
		if (CombinedParamStore.GetPositionDataDirty())
		{
			CombinedParamStore.ResolvePositions(ParentSystemInstance->GetLWCConverter());
		}
		CombinedParamStore.Tick();
	}

	return true;
}

bool FNiagaraComputeExecutionContext::OptionalContexInit(FNiagaraSystemInstance* ParentSystemInstance)
{
	if (GPUScript)
	{
		FNiagaraVMExecutableData& VMData = GPUScript->GetVMExecutableData();

		if (VMData.IsValid() && VMData.bNeedsGPUContextInit)
		{
			const TArray<UNiagaraDataInterface*>& DataInterfaces = CombinedParamStore.GetDataInterfaces();
			for (int32 i = 0; i < DataInterfaces.Num(); i++)
			{
				UNiagaraDataInterface* Interface = DataInterfaces[i];

				int32 UserPtrIdx = VMData.DataInterfaceInfo[i].UserPtrIdx;
				if (UserPtrIdx != INDEX_NONE)
				{
					void* InstData = ParentSystemInstance->FindDataInterfaceInstanceData(Interface);
					if (Interface->NeedsGPUContextInit())
					{
						Interface->GPUContextInit(VMData.DataInterfaceInfo[i], InstData, ParentSystemInstance);
					}
				}
			}
		}
	}
	return true;
}

void FNiagaraComputeExecutionContext::PostTick()
{
	//If we're for interpolated spawn, copy over the previous frame's parameters into the Prev parameters.
	if (HasInterpolationParameters)
	{
		CombinedParamStore.CopyCurrToPrev();
	}
}

void FNiagaraComputeExecutionContext::ResetInternal(FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface)
{
	checkf(IsInRenderingThread(), TEXT("Can only reset the gpu context from the render thread"));

	if (ComputeDispatchInterface)
	{
		static_cast<FNiagaraGpuComputeDispatch*>(ComputeDispatchInterface)->GetGPUInstanceCounterManager().FreeEntry(CountOffset_RT);
	}

	CurrentNumInstances_RT = 0;
	CountOffset_RT = INDEX_NONE;
	EmitterInstanceReadback.GPUCountOffset = INDEX_NONE;

	SetDataToRender(nullptr);
}

void FNiagaraComputeExecutionContext::SetDataToRender(FNiagaraDataBuffer* InDataToRender)
{
	if (DataToRender)
	{
		DataToRender->ReleaseReadRef();
	}

	DataToRender = InDataToRender;

	if (DataToRender)
	{
		DataToRender->AddReadRef();
	}

	// This call the DataToRender should be equal to the TranslucentDataToRender so we can release the read ref
	if (TranslucentDataToRender)
	{
		ensure((DataToRender == nullptr) || (DataToRender == TranslucentDataToRender));
		TranslucentDataToRender->ReleaseReadRef();
		TranslucentDataToRender = nullptr;
	}
}

void FNiagaraComputeExecutionContext::SetTranslucentDataToRender(FNiagaraDataBuffer* InTranslucentDataToRender)
{
	if (TranslucentDataToRender)
	{
		TranslucentDataToRender->ReleaseReadRef();
	}

	TranslucentDataToRender = InTranslucentDataToRender;

	if (TranslucentDataToRender)
	{
		TranslucentDataToRender->AddReadRef();
	}
}

bool FNiagaraComputeInstanceData::IsOutputStage(FNiagaraDataInterfaceProxy* DIProxy, uint32 SimulationStageIndex) const
{
	return Context->IsOutputStage(DIProxy, SimulationStageIndex);
}

bool FNiagaraComputeInstanceData::IsInputStage(FNiagaraDataInterfaceProxy* DIProxy, uint32 SimulationStageIndex) const
{
	return Context->IsInputStage(DIProxy, SimulationStageIndex);
}

bool FNiagaraComputeInstanceData::IsIterationStage(FNiagaraDataInterfaceProxy* DIProxy, uint32 SimulationStageIndex) const
{
	return Context->IsIterationStage(DIProxy, SimulationStageIndex);
}

FNiagaraDataInterfaceProxyRW* FNiagaraComputeInstanceData::FindIterationInterface(uint32 SimulationStageIndex) const
{
	return Context->FindIterationInterface(IterationDataInterfaceProxies, SimulationStageIndex);
}

