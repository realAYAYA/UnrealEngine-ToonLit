// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDenoiserModelInstanceRDG.h"
#include "NNE.h"
#include "NNEDenoiserLog.h"
#include "NNEDenoiserUtils.h"
#include "NNEModelData.h"
#include "NNERuntimeRDG.h"
#include "PathTracingDenoiser.h"
#include "RenderGraphBuilder.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "RHITypes.h"
#include "UObject/UObjectGlobals.h"

namespace UE::NNEDenoiser::Private
{

	static const FString DefaultRuntimeRDGName = TEXT("NNERuntimeRDGHlsl");

	TUniquePtr<FModelInstanceRDG> FModelInstanceRDG::Make(UNNEModelData& ModelData, const FString& RuntimeNameOverride)
	{
		FString RuntimeRDGName = RuntimeNameOverride.IsEmpty() ? DefaultRuntimeRDGName : RuntimeNameOverride;
		
		// Create the model
		TWeakInterfacePtr<INNERuntimeRDG> RuntimeRDG = UE::NNE::GetRuntime<INNERuntimeRDG>(RuntimeRDGName);
		if (!RuntimeRDG.IsValid())
		{
			UE_LOG(LogNNEDenoiser, Error, TEXT("No RDG runtime '%s' found. Valid RDG runtimes are: "), *RuntimeRDGName);
			for (const FString& RuntimeName : UE::NNE::GetAllRuntimeNames<INNERuntimeRDG>())
			{
				UE_LOG(LogNNEDenoiser, Error, TEXT("- %s"), *RuntimeName);
			}
			return {};
		}

		TSharedPtr<UE::NNE::IModelRDG> Model = RuntimeRDG->CreateModelRDG(ModelData);
		if (!Model.IsValid())
		{
			UE_LOG(LogNNEDenoiser, Error, TEXT("Could not create model using %s"), *RuntimeRDGName);
			return {};
		}

		TSharedPtr<UE::NNE::IModelInstanceRDG> ModelInstance = Model->CreateModelInstanceRDG();
		if (!ModelInstance.IsValid())
		{
			UE_LOG(LogNNEDenoiser, Error, TEXT("Could not create model instance using %s"), *RuntimeRDGName);
			return {};
		}

		UE_LOG(LogNNEDenoiser, Log, TEXT("NNEDenoiserRDG: Creaded model instance from %s using %s"), *ModelData.GetFileId().ToString(), *RuntimeRDGName);

		return MakeUnique<FModelInstanceRDG>(ModelInstance.ToSharedRef());
	}

	FModelInstanceRDG::FModelInstanceRDG(TSharedRef<UE::NNE::IModelInstanceRDG> ModelInstance) :
		ModelInstance(ModelInstance)
	{

	}

	FModelInstanceRDG::~FModelInstanceRDG()
	{
		
	}

	TConstArrayView<NNE::FTensorDesc> FModelInstanceRDG::GetInputTensorDescs() const
	{
		return ModelInstance->GetInputTensorDescs();
	}

	TConstArrayView<NNE::FTensorDesc> FModelInstanceRDG::GetOutputTensorDescs() const
	{
		return ModelInstance->GetOutputTensorDescs();
	}

	TConstArrayView<NNE::FTensorShape> FModelInstanceRDG::GetInputTensorShapes() const
	{
		return ModelInstance->GetInputTensorShapes();
	}

	TConstArrayView<NNE::FTensorShape> FModelInstanceRDG::GetOutputTensorShapes() const
	{
		return ModelInstance->GetOutputTensorShapes();
	}

	FModelInstanceRDG::ESetInputTensorShapesStatus FModelInstanceRDG::SetInputTensorShapes(TConstArrayView<NNE::FTensorShape> InInputShapes)
	{
		return ModelInstance->SetInputTensorShapes(InInputShapes);
	}

	FModelInstanceRDG::EEnqueueRDGStatus FModelInstanceRDG::EnqueueRDG(FRDGBuilder &GraphBuilder, TConstArrayView<NNE::FTensorBindingRDG> Inputs, TConstArrayView<NNE::FTensorBindingRDG> Outputs)
	{
		return ModelInstance->EnqueueRDG(GraphBuilder, Inputs, Outputs);
	}

}