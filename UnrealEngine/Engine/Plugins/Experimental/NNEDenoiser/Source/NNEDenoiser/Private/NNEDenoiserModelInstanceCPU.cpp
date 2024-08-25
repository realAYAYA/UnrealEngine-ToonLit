// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDenoiserModelInstanceCPU.h"
#include "NNE.h"
#include "NNEDenoiserLog.h"
#include "NNEDenoiserUtils.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "RenderGraphBuilder.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "RHITypes.h"
#include "UObject/UObjectGlobals.h"

BEGIN_SHADER_PARAMETER_STRUCT(FNNEDenoiserModelInstanceCPUTextureParameters, )
	RDG_BUFFER_ACCESS_ARRAY(InputBuffers)
	RDG_BUFFER_ACCESS_ARRAY(OutputBuffers)
END_SHADER_PARAMETER_STRUCT()

namespace UE::NNEDenoiser::Private
{

	static const FString DefaultRuntimeCPUName = TEXT("NNERuntimeORTCpu");

	TUniquePtr<FModelInstanceCPU> FModelInstanceCPU::Make(UNNEModelData& ModelData, const FString &RuntimeNameOverride)
	{
		const FString RuntimeCPUName = RuntimeNameOverride.IsEmpty() ? DefaultRuntimeCPUName : RuntimeNameOverride;

		// Create the model
		TWeakInterfacePtr<INNERuntimeCPU> RuntimeCPU = UE::NNE::GetRuntime<INNERuntimeCPU>(RuntimeCPUName);
		if (!RuntimeCPU.IsValid())
		{
			UE_LOG(LogNNEDenoiser, Error, TEXT("No CPU runtime '%s' found. Valid CPU runtimes are: "), *RuntimeCPUName);
			for (const FString& RuntimeName : UE::NNE::GetAllRuntimeNames<INNERuntimeCPU>())
			{
				UE_LOG(LogNNEDenoiser, Error, TEXT("- %s"), *RuntimeName);
			}
			return {};
		}

		TSharedPtr<UE::NNE::IModelCPU> Model = RuntimeCPU->CreateModelCPU(ModelData);
		if (!Model.IsValid())
		{
			UE_LOG(LogNNEDenoiser, Error, TEXT("Could not create model using %s"), *RuntimeCPUName);
			return {};
		}

		TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstance = Model->CreateModelInstanceCPU();
		if (!ModelInstance.IsValid())
		{
			UE_LOG(LogNNEDenoiser, Error, TEXT("Could not create model instance using %s"), *RuntimeCPUName);
			return {};
		}

		UE_LOG(LogNNEDenoiser, Log, TEXT("NNEDenoiserCPU: Creaded model instance from %s using %s"), *ModelData.GetFileId().ToString(), *RuntimeCPUName);

		return MakeUnique<FModelInstanceCPU>(ModelInstance.ToSharedRef());
	}

	FModelInstanceCPU::FModelInstanceCPU(TSharedRef<UE::NNE::IModelInstanceCPU> ModelInstance) :
			ModelInstance(ModelInstance)
	{

	}

	FModelInstanceCPU::~FModelInstanceCPU()
	{
		
	}

	TConstArrayView<NNE::FTensorDesc> FModelInstanceCPU::GetInputTensorDescs() const
	{
		return ModelInstance->GetInputTensorDescs();
	}

	TConstArrayView<NNE::FTensorDesc> FModelInstanceCPU::GetOutputTensorDescs() const
	{
		return ModelInstance->GetOutputTensorDescs();
	}

	TConstArrayView<NNE::FTensorShape> FModelInstanceCPU::GetInputTensorShapes() const
	{
		return ModelInstance->GetInputTensorShapes();
	}

	TConstArrayView<NNE::FTensorShape> FModelInstanceCPU::GetOutputTensorShapes() const
	{
		return ModelInstance->GetOutputTensorShapes();
	}

	FModelInstanceCPU::ESetInputTensorShapesStatus FModelInstanceCPU::SetInputTensorShapes(TConstArrayView<NNE::FTensorShape> InInputShapes)
	{
		NNE::IModelInstanceCPU::ESetInputTensorShapesStatus Status = ModelInstance->SetInputTensorShapes(InInputShapes);

		return Status == NNE::IModelInstanceCPU::ESetInputTensorShapesStatus::Ok ? ESetInputTensorShapesStatus::Ok : ESetInputTensorShapesStatus::Fail;
	}

	FModelInstanceCPU::EEnqueueRDGStatus FModelInstanceCPU::EnqueueRDG(FRDGBuilder &GraphBuilder, TConstArrayView<NNE::FTensorBindingRDG> Inputs, TConstArrayView<NNE::FTensorBindingRDG> Outputs)
	{
		FNNEDenoiserModelInstanceCPUTextureParameters* DenoiserParameters = GraphBuilder.AllocParameters<FNNEDenoiserModelInstanceCPUTextureParameters>();
		for (const NNE::FTensorBindingRDG& Binding : Inputs)
		{
			DenoiserParameters->InputBuffers.Emplace(Binding.Buffer, ERHIAccess::CopySrc);
		}
		for (const NNE::FTensorBindingRDG& Binding : Outputs)
		{
			DenoiserParameters->OutputBuffers.Emplace(Binding.Buffer, ERHIAccess::CopySrc);
		}
		
		GraphBuilder.AddPass(RDG_EVENT_NAME("NNEDenoiser.DenoiseCPU"), DenoiserParameters, ERDGPassFlags::Readback,
		[this, DenoiserParameters](FRHICommandListImmediate& RHICmdList)
		{
			SCOPED_NAMED_EVENT_TEXT("FModelInstanceCPU::DenoisePass", FColor::Magenta);
			
#if WITH_EDITOR
			// NOTE: the time will include the transfer from GPU to CPU which will include waiting for the GPU pipeline to complete
			uint64 FilterExecuteTime = 0;
			FilterExecuteTime -= FPlatformTime::Cycles64();
#endif

			ScratchInputBuffers.SetNum(DenoiserParameters->InputBuffers.Num());
			TArray<NNE::FTensorBindingCPU> InputBindings;
			for (int32 Idx = 0; Idx < DenoiserParameters->InputBuffers.Num(); Idx++)
			{
				ScratchInputBuffers[Idx].SetNumUninitialized(DenoiserParameters->InputBuffers[Idx].GetBuffer()->GetRHI()->GetSize());

				InputBindings.Emplace(NNE::FTensorBindingCPU{(void *)ScratchInputBuffers[Idx].GetData(), (uint64)ScratchInputBuffers[Idx].Num()});
			}

			ScratchOutputBuffers.SetNum(DenoiserParameters->OutputBuffers.Num());
			TArray<NNE::FTensorBindingCPU> OutputBindings;
			for (int32 Idx = 0; Idx < DenoiserParameters->OutputBuffers.Num(); Idx++)
			{
				ScratchOutputBuffers[Idx].SetNumUninitialized(DenoiserParameters->OutputBuffers[Idx].GetBuffer()->GetRHI()->GetSize());

				OutputBindings.Emplace(NNE::FTensorBindingCPU{(void *)ScratchOutputBuffers[Idx].GetData(), (uint64)ScratchOutputBuffers[Idx].Num()});
			}

			for (int32 Idx = 0; Idx < DenoiserParameters->InputBuffers.Num(); Idx++)
			{
				FRHIBuffer* Buffer = DenoiserParameters->InputBuffers[Idx].GetBuffer()->GetRHI();
				CopyBufferFromGPUToCPU(RHICmdList, Buffer, Buffer->GetSize(), ScratchInputBuffers[Idx]);
			}

			NNE::IModelInstanceCPU::ERunSyncStatus Status = ModelInstance->RunSync(InputBindings, OutputBindings);
			checkf(Status == NNE::IModelInstanceCPU::ERunSyncStatus::Ok, TEXT("RunSync failed with status %d"), static_cast<int>(Status))

			for (int32 Idx = 0; Idx < DenoiserParameters->OutputBuffers.Num(); Idx++)
			{
				FRHIBuffer* Buffer = DenoiserParameters->OutputBuffers[Idx].GetBuffer()->GetRHI();
				CopyBufferFromCPUToGPU(RHICmdList, ScratchOutputBuffers[Idx], Buffer->GetSize(), Buffer);
			}

#if WITH_EDITOR
		FilterExecuteTime += FPlatformTime::Cycles64();
		const double FilterExecuteTimeMS = 1000.0 * FPlatformTime::ToSeconds64(FilterExecuteTime);
		UE_LOG(LogNNEDenoiser, Log, TEXT("Denoised on CPU in %.2f ms"), FilterExecuteTimeMS);
#endif
		});

		return EEnqueueRDGStatus::Ok;
	}

}