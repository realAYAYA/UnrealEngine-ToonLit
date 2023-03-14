// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraGPUSystemTick.h"
#include "NiagaraSystemGpuComputeProxy.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemSimulation.h"

void FNiagaraGPUSystemTick::Init(FNiagaraSystemInstance* InSystemInstance)
{
	ensure(InSystemInstance != nullptr);
	CA_ASSUME(InSystemInstance != nullptr);
	ensure(!InSystemInstance->IsComplete());
	SystemInstanceID = InSystemInstance->GetId();
	SystemGpuComputeProxy = InSystemInstance->GetSystemGpuComputeProxy();
 
	uint32 DataSizeForGPU = InSystemInstance->GPUDataInterfaceInstanceDataSize;

	if (DataSizeForGPU > 0)
	{
		uint32 AllocationSize = DataSizeForGPU;

		DIInstanceData = new FNiagaraComputeDataInterfaceInstanceData;
		DIInstanceData->PerInstanceDataSize = AllocationSize;
		DIInstanceData->PerInstanceDataForRT = FMemory::Malloc(AllocationSize);
		DIInstanceData->Instances = InSystemInstance->DataInterfaceInstanceDataOffsets.Num();

		uint8* InstanceDataBase = (uint8*) DIInstanceData->PerInstanceDataForRT;
		uint32 RunningOffset = 0;

		DIInstanceData->InterfaceProxiesToOffsets.Reserve(InSystemInstance->GPUDataInterfaces.Num());

		for (const auto& Pair : InSystemInstance->GPUDataInterfaces)
		{
			UNiagaraDataInterface* Interface = Pair.Key.Get();
			if (Interface == nullptr)
			{
				continue;
			}

			FNiagaraDataInterfaceProxy* Proxy = Interface->GetProxy();
			const int32 Offset = Pair.Value;

			const int32 RTDataSize = Align(Interface->PerInstanceDataPassedToRenderThreadSize(), 16);
			ensure(RTDataSize > 0);
			check(Proxy);

			void* PerInstanceData = &InSystemInstance->DataInterfaceInstanceData[Offset];

			Interface->ProvidePerInstanceDataForRenderThread(InstanceDataBase, PerInstanceData, SystemInstanceID);

			// @todo rethink this. So ugly.
			DIInstanceData->InterfaceProxiesToOffsets.Add(Proxy, RunningOffset);

			InstanceDataBase += RTDataSize;
			RunningOffset += RTDataSize;
		}
	}

	check(MAX_uint32 > InSystemInstance->ActiveGPUEmitterCount);

	// Layout our packet.
	const uint32 PackedDispatchesSize = InSystemInstance->ActiveGPUEmitterCount * sizeof(FNiagaraComputeInstanceData);
	// We want the Params after the instance data to be aligned so we can upload to the gpu.
	uint32 PackedDispatchesSizeAligned = Align(PackedDispatchesSize, SHADER_PARAMETER_STRUCT_ALIGNMENT);
	uint32 TotalParamSize = InSystemInstance->TotalGPUParamSize;

	uint32 TotalPackedBufferSize = PackedDispatchesSizeAligned + TotalParamSize;

	InstanceData_ParamData_Packed = (uint8*)FMemory::Malloc(TotalPackedBufferSize);

	FNiagaraComputeInstanceData* Instances = (FNiagaraComputeInstanceData*)(InstanceData_ParamData_Packed);
	uint8* ParamDataBufferPtr = InstanceData_ParamData_Packed + PackedDispatchesSizeAligned;

	// we want to include interpolation parameters (current and previous frame) if any of the emitters in the system
	// require it
	const bool IncludeInterpolationParameters = InSystemInstance->GPUParamIncludeInterpolation;
	const int32 InterpFactor = IncludeInterpolationParameters ? 2 : 1;

	GlobalParamData = ParamDataBufferPtr;
	SystemParamData = GlobalParamData + InterpFactor * sizeof(FNiagaraGlobalParameters);
	OwnerParamData = SystemParamData + InterpFactor * sizeof(FNiagaraSystemParameters);

	// actually copy all of the data over, for the system data we only need to do it once (rather than per-emitter)
	FMemory::Memcpy(GlobalParamData, &InSystemInstance->GetGlobalParameters(), sizeof(FNiagaraGlobalParameters));
	FMemory::Memcpy(SystemParamData, &InSystemInstance->GetSystemParameters(), sizeof(FNiagaraSystemParameters));
	FMemory::Memcpy(OwnerParamData, &InSystemInstance->GetOwnerParameters(), sizeof(FNiagaraOwnerParameters));

	if (IncludeInterpolationParameters)
	{
		FMemory::Memcpy(GlobalParamData + sizeof(FNiagaraGlobalParameters), &InSystemInstance->GetGlobalParameters(true), sizeof(FNiagaraGlobalParameters));
		FMemory::Memcpy(SystemParamData + sizeof(FNiagaraSystemParameters), &InSystemInstance->GetSystemParameters(true), sizeof(FNiagaraSystemParameters));
		FMemory::Memcpy(OwnerParamData + sizeof(FNiagaraOwnerParameters), &InSystemInstance->GetOwnerParameters(true), sizeof(FNiagaraOwnerParameters));
	}

	ParamDataBufferPtr = OwnerParamData + InterpFactor * sizeof(FNiagaraOwnerParameters);

	// Now we will generate instance data for every GPU simulation we want to run on the render thread.
	// This is spawn rate as well as DataInterface per instance data and the ParameterData for the emitter.
	// @todo Ideally we would only update DataInterface and ParameterData bits if they have changed.
	uint32 InstanceIndex = 0;
	bool bStartNewOverlapGroup = false;

	const TConstArrayView<FNiagaraEmitterExecutionIndex> EmitterExecutionOrder = InSystemInstance->GetEmitterExecutionOrder();
	for (const FNiagaraEmitterExecutionIndex& EmiterExecIndex : EmitterExecutionOrder)
	{
		// The dependency resolution code does not consider CPU and GPU emitters separately, so the flag which marks the start of a new overlap group can be set on either
		// a CPU or GPU emitter. We must turn on bStartNewOverlapGroup when we encounter the flag, and reset it when we've actually marked a GPU emitter as starting a new group.
		bStartNewOverlapGroup |= EmiterExecIndex.bStartNewOverlapGroup;

		const uint32 EmitterIdx = EmiterExecIndex.EmitterIndex;
		if (FNiagaraEmitterInstance* EmitterInstance = &InSystemInstance->GetEmitters()[EmitterIdx].Get())
		{
			if (EmitterInstance->IsComplete() )
			{
				continue;
			}

			const FVersionedNiagaraEmitterData* EmitterData = EmitterInstance->GetCachedEmitterData();
			FNiagaraComputeExecutionContext* GPUContext = EmitterInstance->GetGPUContext();

			check(EmitterData);

			if (!EmitterData || !GPUContext || EmitterData->SimTarget != ENiagaraSimTarget::GPUComputeSim)
			{
				continue;
			}

			// Handle edge case where an emitter was set to inactive on the first frame by scalability
			// In which case it will never have ticked so we should not execute a GPU tick for this until it becomes active
			// See FNiagaraSystemInstance::Tick_Concurrent for details
			if (EmitterInstance->HasTicked() == false)
			{
				ensure((EmitterInstance->GetExecutionState() == ENiagaraExecutionState::Inactive) || (EmitterInstance->GetExecutionState() == ENiagaraExecutionState::InactiveClear));
				continue;
			}

			FNiagaraComputeInstanceData* InstanceData = new (&Instances[InstanceIndex]) FNiagaraComputeInstanceData;
			InstanceIndex++;

			InstanceData->Context = GPUContext;
			check(GPUContext->MainDataSet);

			InstanceData->SpawnInfo = GPUContext->GpuSpawnInfo_GT;

			// Consume pending reset
			if ( GPUContext->bResetPending_GT )
			{
				InstanceData->bResetData = GPUContext->bResetPending_GT;
				GPUContext->bResetPending_GT = false;

				++GPUContext->ParticleCountReadFence;
			}
			InstanceData->ParticleCountFence = GPUContext->ParticleCountReadFence;

			InstanceData->EmitterParamData = ParamDataBufferPtr;
			ParamDataBufferPtr += InterpFactor * sizeof(FNiagaraEmitterParameters);

			InstanceData->ExternalParamData = ParamDataBufferPtr;
			InstanceData->ExternalParamDataSize = GPUContext->CombinedParamStore.GetPaddedParameterSizeInBytes();
			ParamDataBufferPtr += InstanceData->ExternalParamDataSize;

			// actually copy all of the data over
			FMemory::Memcpy(InstanceData->EmitterParamData, &InSystemInstance->GetEmitterParameters(EmitterIdx), sizeof(FNiagaraEmitterParameters));
			if (IncludeInterpolationParameters)
			{
				FMemory::Memcpy(InstanceData->EmitterParamData + sizeof(FNiagaraEmitterParameters), &InSystemInstance->GetEmitterParameters(EmitterIdx, true), sizeof(FNiagaraEmitterParameters));
			}

			bHasMultipleStages = InstanceData->bHasMultipleStages;
			bHasInterpolatedParameters |= GPUContext->HasInterpolationParameters;

			GPUContext->CombinedParamStore.CopyParameterDataToPaddedBuffer(InstanceData->ExternalParamData, InstanceData->ExternalParamDataSize);

			// Calling PostTick will push current -> previous parameters this must be done after copying the parameter data
			GPUContext->PostTick();

			InstanceData->bStartNewOverlapGroup = bStartNewOverlapGroup;
			bStartNewOverlapGroup = false;

			// @todo-threadsafety Think of a better way to do this!
			const TArray<UNiagaraDataInterface*>& DataInterfaces = GPUContext->CombinedParamStore.GetDataInterfaces();
			InstanceData->DataInterfaceProxies.Reserve(DataInterfaces.Num());
			InstanceData->IterationDataInterfaceProxies.Reserve(DataInterfaces.Num());

			for (UNiagaraDataInterface* DI : DataInterfaces)
			{
				FNiagaraDataInterfaceProxy* DIProxy = DI->GetProxy();
				check(DIProxy);
				InstanceData->DataInterfaceProxies.Add(DIProxy);

				if ( FNiagaraDataInterfaceProxyRW* RWProxy = DIProxy->AsIterationProxy() )
				{
					InstanceData->IterationDataInterfaceProxies.Add(RWProxy);
				}
			}

			// Gather number of iterations for each stage, and if the stage should run or not
			InstanceData->bHasMultipleStages = false;
			InstanceData->PerStageInfo.Reserve(GPUContext->SimStageInfo.Num());
			for ( FSimulationStageMetaData& SimStageMetaData : GPUContext->SimStageInfo )
			{
				int32 NumIterations = SimStageMetaData.NumIterations;
				FIntVector ElementCountXYZ = FIntVector::NoneValue;
				if (SimStageMetaData.ShouldRunStage(InstanceData->bResetData))
				{
					InstanceData->bHasMultipleStages = true;
					if (!SimStageMetaData.NumIterationsBinding.IsNone())
					{
						FNiagaraParameterStore& BoundParamStore = EmitterInstance->GetRendererBoundVariables();
						if ( const uint8* ParameterData = BoundParamStore.GetParameterData(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), SimStageMetaData.NumIterationsBinding)) )
						{
							NumIterations = *reinterpret_cast<const int32*>(ParameterData);
							NumIterations = FMath::Max(NumIterations, 0);
						}
					}
					if (!SimStageMetaData.EnabledBinding.IsNone())
					{
						FNiagaraParameterStore& BoundParamStore = EmitterInstance->GetRendererBoundVariables();
						if (const uint8* ParameterData = BoundParamStore.GetParameterData(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), SimStageMetaData.EnabledBinding)))
						{
							const FNiagaraBool StageEnabled = *reinterpret_cast<const FNiagaraBool*>(ParameterData);
							NumIterations = StageEnabled.GetValue() ? NumIterations : 0;
						}
					}
					if (SimStageMetaData.bOverrideElementCount)
					{
						if (!SimStageMetaData.ElementCountXBinding.IsNone() && (SimStageMetaData.GpuDispatchType >= ENiagaraGpuDispatchType::OneD))
						{
							FNiagaraParameterStore& BoundParamStore = EmitterInstance->GetRendererBoundVariables();
							if (const uint8* ParameterData = BoundParamStore.GetParameterData(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), SimStageMetaData.ElementCountXBinding)))
							{
								ElementCountXYZ.X = *reinterpret_cast<const int32*>(ParameterData);
							}
						}
						if (!SimStageMetaData.ElementCountYBinding.IsNone() && (SimStageMetaData.GpuDispatchType >= ENiagaraGpuDispatchType::TwoD))
						{
							FNiagaraParameterStore& BoundParamStore = EmitterInstance->GetRendererBoundVariables();
							if (const uint8* ParameterData = BoundParamStore.GetParameterData(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), SimStageMetaData.ElementCountYBinding)))
							{
								ElementCountXYZ.Y = *reinterpret_cast<const int32*>(ParameterData);
							}
						}
						if (!SimStageMetaData.ElementCountZBinding.IsNone() && (SimStageMetaData.GpuDispatchType >= ENiagaraGpuDispatchType::ThreeD))
						{
							FNiagaraParameterStore& BoundParamStore = EmitterInstance->GetRendererBoundVariables();
							if (const uint8* ParameterData = BoundParamStore.GetParameterData(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), SimStageMetaData.ElementCountZBinding)))
							{
								ElementCountXYZ.Z = *reinterpret_cast<const int32*>(ParameterData);
							}
						}

						// make sure we don't have negatives as we use the values directly for dispatch
						ElementCountXYZ.X = FMath::Max(0, ElementCountXYZ.X);
						ElementCountXYZ.Y = FMath::Max(0, ElementCountXYZ.Y);
						ElementCountXYZ.Z = FMath::Max(0, ElementCountXYZ.Z);
					}
				}
				else
				{
					NumIterations = 0;
				}

				InstanceData->TotalDispatches += NumIterations;
				InstanceData->PerStageInfo.Emplace(NumIterations, ElementCountXYZ);
			}
			TotalDispatches += InstanceData->TotalDispatches;
		}
	}

	check(InSystemInstance->ActiveGPUEmitterCount == InstanceIndex);
	InstanceCount = InstanceIndex;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	InstanceDataDebuggingOnly = GetInstances();
#endif
}

void FNiagaraGPUSystemTick::Destroy()
{
	for ( FNiagaraComputeInstanceData& Instance : GetInstances() )
	{
		Instance.Context->ParticleCountWriteFence = Instance.ParticleCountFence;
		Instance.~FNiagaraComputeInstanceData();
	}

	FMemory::Free(InstanceData_ParamData_Packed);
	if (DIInstanceData)
	{
		FMemory::Free(DIInstanceData->PerInstanceDataForRT);
		delete DIInstanceData;
	}
}

void FNiagaraGPUSystemTick::BuildUniformBuffers()
{
	check(ExternalUnformBuffers_RT.Num() == 0);

	const int32 InterpCount = bHasInterpolatedParameters ? 2 : 1;
	ExternalUnformBuffers_RT.AddDefaulted(InstanceCount * InterpCount);

	TArrayView<FNiagaraComputeInstanceData> Instances = GetInstances();
	for (uint32 iInstance=0; iInstance < InstanceCount; ++iInstance)
	{
		const FNiagaraComputeInstanceData& Instance = Instances[iInstance];

		FNiagaraRHIUniformBufferLayout* ExternalCBufferLayout = Instance.Context->ExternalCBufferLayout;
		const bool bExternalLayoutValid = ExternalCBufferLayout && (ExternalCBufferLayout->Resources.Num() || ExternalCBufferLayout->ConstantBufferSize > 0);
		if ( Instance.Context->GPUScript_RT->IsExternalConstantBufferUsed_RenderThread(0) )
		{
			if ( ensure(ExternalCBufferLayout && bExternalLayoutValid) )
			{
				ExternalUnformBuffers_RT[iInstance] = RHICreateUniformBuffer(Instance.ExternalParamData, ExternalCBufferLayout, bHasMultipleStages ? EUniformBufferUsage::UniformBuffer_SingleFrame : EUniformBufferUsage::UniformBuffer_SingleDraw);
			}
		}
		if ( Instance.Context->GPUScript_RT->IsExternalConstantBufferUsed_RenderThread(1) )
		{
			if (ensure(ExternalCBufferLayout && bExternalLayoutValid))
			{
				check(ExternalCBufferLayout->ConstantBufferSize + ExternalCBufferLayout->ConstantBufferSize <= Instance.ExternalParamDataSize);
				ExternalUnformBuffers_RT[InstanceCount + iInstance] = RHICreateUniformBuffer(Instance.ExternalParamData + ExternalCBufferLayout->ConstantBufferSize, ExternalCBufferLayout, bHasMultipleStages ? EUniformBufferUsage::UniformBuffer_SingleFrame : EUniformBufferUsage::UniformBuffer_SingleDraw);
			}
		}
	}
}
