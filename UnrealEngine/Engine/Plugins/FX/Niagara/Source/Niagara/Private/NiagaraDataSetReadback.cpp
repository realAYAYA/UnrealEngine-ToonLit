// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataSetReadback.h"
#include "NiagaraComputeExecutionContext.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraGpuReadbackManager.h"

#include "Async/Async.h"

class FNiagaraEmitterInstance;

void FNiagaraDataSetReadback::SetReadbackRead(FOnReadbackReady InOnReadbackReady)
{
	check(IsReady());
	OnReadbackReady = InOnReadbackReady;
}

void FNiagaraDataSetReadback::EnqueueReadback(FNiagaraEmitterInstance* EmitterInstance)
{
	check(EmitterInstance);
	check(IsReady());

	SourceName = EmitterInstance->GetEmitterHandle().GetName();
	DataSet.Init(&EmitterInstance->GetData().GetCompiledData());
	if ( FNiagaraComputeExecutionContext* GPUExecContext = EmitterInstance->GetGPUContext() )
	{
		ParameterStore = GPUExecContext->CombinedParamStore;

		FNiagaraSystemInstance* SystemInstance = EmitterInstance->GetParentSystemInstance();

		++PendingReadbacks;
		ENQUEUE_RENDER_COMMAND(NiagaraDataSetReadback)
		(
			[RT_DataSetReadback=AsShared(), RT_ComputeDispatchInterface=SystemInstance->GetComputeDispatchInterface(), RT_GPUExecContext=GPUExecContext](FRHICommandListImmediate& RHICmdList)
			{
				RT_DataSetReadback->GPUReadbackInternal(RHICmdList, RT_ComputeDispatchInterface, RT_GPUExecContext);
			}
		);
	}
	else
	{
		const FNiagaraDataSet& SourceDataSet = EmitterInstance->GetData();
		if (FNiagaraDataBuffer* SourceDataBuffer = SourceDataSet.GetCurrentData())
		{
			EmitterInstance->GetData().CopyTo(DataSet, 0, SourceDataBuffer->GetNumInstances());
		}
		else
		{
			DataSet.CopyFromGPUReadback(nullptr, nullptr, nullptr, 0, 0, 0, 0, 0);
		}
		ParameterStore = EmitterInstance->GetUpdateExecutionContext().Parameters;
	}
}

void FNiagaraDataSetReadback::ImmediateReadback(FNiagaraEmitterInstance* EmitterInstance)
{
	EnqueueReadback(EmitterInstance);
	if ( !IsReady() )
	{
		FNiagaraSystemInstance* SystemInstance = EmitterInstance->GetParentSystemInstance();
		FNiagaraGpuComputeDispatchInterface* DispatchInterface = SystemInstance->GetComputeDispatchInterface();
		ENQUEUE_RENDER_COMMAND(NiagaraFlushReadback)
		(
			[RT_ComputeDispatchInterface=SystemInstance->GetComputeDispatchInterface()](FRHICommandListImmediate& RHICmdList)
			{
				RT_ComputeDispatchInterface->GetGpuReadbackManager()->WaitCompletion(RHICmdList);
			}
		);
		FlushRenderingCommands();
		check(IsReady());
	}
}

void FNiagaraDataSetReadback::ReadbackCompleteInternal()
{
	if ( OnReadbackReady.IsBound() )
	{
		AsyncTask(
			ENamedThreads::GameThread,
			[Readback=this]()
			{
				Readback->PendingReadbacks--;
				Readback->OnReadbackReady.Execute(*Readback);
			}
		);
	}
	else
	{
		PendingReadbacks--;
	}
}

void FNiagaraDataSetReadback::GPUReadbackInternal(FRHICommandListImmediate& RHICmdList, FNiagaraGpuComputeDispatchInterface* DispatchInterface, FNiagaraComputeExecutionContext* GPUContext)
{
	FNiagaraGpuReadbackManager* ReadbackManager = DispatchInterface->GetGpuReadbackManager();
	FNiagaraDataBuffer* CurrentDataBuffer = GPUContext->MainDataSet->GetCurrentData();
	if (CurrentDataBuffer == nullptr || ReadbackManager == nullptr)
	{
		DataSet.CopyFromGPUReadback(nullptr, nullptr, nullptr, 0, 0, 0, 0, 0);
		ReadbackCompleteInternal();
		return;
	}

	const uint32 CountOffset = CurrentDataBuffer->GetGPUInstanceCountBufferOffset();
	if (CountOffset == INDEX_NONE)
	{
		DataSet.CopyFromGPUReadback(nullptr, nullptr, nullptr, 0, 0, 0, 0, 0);
		ReadbackCompleteInternal();
		return;
	}

	const FNiagaraGPUInstanceCountManager& CountManager = DispatchInterface->GetGPUInstanceCounterManager();
	FRWBuffer& FloatBuffer = CurrentDataBuffer->GetGPUBufferFloat();
	FRWBuffer& HalfBuffer = CurrentDataBuffer->GetGPUBufferHalf();
	FRWBuffer& IntBuffer = CurrentDataBuffer->GetGPUBufferInt();
	FRWBuffer& IDtoIndexBuffer = CurrentDataBuffer->GetGPUIDToIndexTable();

	constexpr int32 NumReadbackBuffers = 5;
	TArray<FNiagaraGpuReadbackManager::FBufferRequest, TInlineAllocator<NumReadbackBuffers>> ReadbackBuffers;
	ReadbackBuffers.Emplace(CountManager.GetInstanceCountBuffer().Buffer, CountOffset * sizeof(uint32), sizeof(uint32));
	const int32 FloatBufferIndex = FloatBuffer.NumBytes == 0 ? INDEX_NONE : ReadbackBuffers.Emplace(FloatBuffer.Buffer, 0, FloatBuffer.NumBytes);
	const int32 HalfBufferIndex = HalfBuffer.NumBytes == 0 ? INDEX_NONE : ReadbackBuffers.Emplace(HalfBuffer.Buffer, 0, HalfBuffer.NumBytes);
	const int32 IntBufferIndex = IntBuffer.NumBytes == 0 ? INDEX_NONE : ReadbackBuffers.Emplace(IntBuffer.Buffer, 0, IntBuffer.NumBytes);
	const int32 IDtoIndexBufferIndex = IDtoIndexBuffer.NumBytes == 0 ? INDEX_NONE : ReadbackBuffers.Emplace(IDtoIndexBuffer.Buffer, 0, IDtoIndexBuffer.NumBytes);

	const int32 FloatBufferStride = CurrentDataBuffer->GetFloatStride();
	const int32 HalfBufferStride = CurrentDataBuffer->GetHalfStride();
	const int32 IntBufferStride = CurrentDataBuffer->GetInt32Stride();

	ReadbackManager->EnqueueReadbacks(
		RHICmdList,
		MakeArrayView(ReadbackBuffers),
		[=, Readback=AsShared()](TConstArrayView<TPair<void*, uint32>> BufferData)
		{
			const int32 InstanceCount = *reinterpret_cast<int32*>(BufferData[0].Key);

			// Copy dataset databuffer
			const float* FloatDataBuffer = FloatBufferIndex == INDEX_NONE ? nullptr : reinterpret_cast<float*>(BufferData[FloatBufferIndex].Key);
			const FFloat16* HalfDataBuffer = HalfBufferIndex == INDEX_NONE ? nullptr : reinterpret_cast<FFloat16*>(BufferData[HalfBufferIndex].Key);
			const int32* IntDataBuffer = IntBufferIndex == INDEX_NONE ? nullptr : reinterpret_cast<int32*>(BufferData[IntBufferIndex].Key);
			Readback->DataSet.CopyFromGPUReadback(FloatDataBuffer, IntDataBuffer, HalfDataBuffer, 0, InstanceCount, FloatBufferStride, IntBufferStride, HalfBufferStride);

			// Copy ID to Index table
			const int32* IDtoIndexBuffer = IDtoIndexBufferIndex == INDEX_NONE ? nullptr : reinterpret_cast<int32*>(BufferData[IDtoIndexBufferIndex].Key);
			if (IDtoIndexBufferIndex != INDEX_NONE)
			{
				check(BufferData[IDtoIndexBufferIndex].Value >= InstanceCount * sizeof(int32));
				Readback->IDToIndexTable.Empty();
				Readback->IDToIndexTable.Reset(InstanceCount);
				FMemory::Memcpy(Readback->IDToIndexTable.GetData(), IDtoIndexBuffer, InstanceCount * sizeof(int32));
			}
			else
			{
				Readback->IDToIndexTable.Empty();
			}


			Readback->ReadbackCompleteInternal();
		}
	);
}
