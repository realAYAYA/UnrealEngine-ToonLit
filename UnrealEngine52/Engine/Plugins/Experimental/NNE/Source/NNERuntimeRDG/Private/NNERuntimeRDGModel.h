// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNECoreModelBase.h"
#include "NNECoreRuntimeRDG.h"
#include "NNECoreTensor.h"
#include "NNECoreTypes.h"
#include "NNERuntimeRDG.h"

namespace UE::NNERuntimeRDG::Private
{
	class FModelRDG : public NNECore::Internal::FModelBase<NNECore::IModelRDG>
	{
	public:
		FModelRDG() {};
		virtual ~FModelRDG() = default;

		virtual int SetInputTensorShapes(TConstArrayView<NNECore::FTensorShape> InInputShapes) override;
		virtual int EnqueueRDG(FRDGBuilder& RDGBuilder, TConstArrayView<NNECore::FTensorBindingRDG> InInputBindings, TConstArrayView<NNECore::FTensorBindingRDG> InOutputBindings) override;
		
	protected:
		bool LoadModel(TConstArrayView<uint8> ModelData, FNNERuntimeFormat& Format, int32 GuidAndVersionSize);
		int SetTensors(FRDGBuilder& GraphBuilder, FTensorRDGArray& InTensorRDGs, TConstArrayView<NNECore::FTensorBindingRDG> InBindings);

		virtual int PrepareTensorShapesAndData() = 0;
		virtual bool PrepareModelRDG(FRDGBuilder& RDGBuilder) { return false; }
		virtual void AddDispatchOps_RenderThread(FRDGBuilder& GraphBuilder) = 0;

		//Tensor descriptor
		TArray<NNECore::FTensorDesc>	AllSymbolicTensorDescs;

		//Tensor indices for models
		TArray<int32>				IntermediateTensorIndices;
		TArray<int32>				WeightTensorIndices;
		TArray<int32>				InputTensorIndices;
		TArray<int32>				OutputTensorIndices;

		//Tensor indices by operator
		TArray<TArray<uint32>>		OperatorInputTensorIndices;
		TArray<TArray<uint32>>		OperatorOutputTensorIndices;

		//RDG Tensors
		FTensorRDGRefArray			AllTensorRDGRefs;
		FTensorRDGArray				InputTensorRDGs;
		FTensorRDGArray				OutputTensorRDGs;
		FTensorRDGArray				IntermediateTensorRDGs;
		FTensorRDGArray				WeightTensorRDGs;
	};
	
} // UE::NNERuntimeRDG::Private