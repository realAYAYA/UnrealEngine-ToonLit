// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNECore.h"
#include "NNECoreRuntimeCPU.h"
#include "NNECoreRuntimeFormat.h"
#include "NNECoreRuntimeGPU.h"
#include "NNECoreRuntimeRDG.h"
#include "NNECoreTypes.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "Templates/UniquePtr.h"

namespace UE::NNEQA::Private 
{

// This class is a wrapper around a NNECore::IModelCPU and NNECore::IModelRDG
// Allowing to run a model on CPU or GPU using IModelCPU interface only.
class FModelQA : NNECore::IModelCPU
{
private:

	TUniquePtr<NNECore::IModelCPU> ModelCPU;
	TUniquePtr<NNECore::IModelGPU> ModelGPU;
	TUniquePtr<NNECore::IModelRDG> ModelRDG;

public:
	struct FReadbackEntry
	{
		TUniquePtr<FRHIGPUBufferReadback> RHI;
		void* CpuMemory;
		size_t	Size;
	};
	TArray<FReadbackEntry> Readbacks;

	FModelQA() = default;
	virtual ~FModelQA() = default;

	virtual TConstArrayView<NNECore::FTensorDesc> GetInputTensorDescs() const;		
	virtual TConstArrayView<NNECore::FTensorDesc> GetOutputTensorDescs() const;
	virtual TConstArrayView<NNECore::FTensorShape> GetInputTensorShapes() const;
	virtual TConstArrayView<NNECore::FTensorShape> GetOutputTensorShapes() const;
	virtual int SetInputTensorShapes(TConstArrayView<NNECore::FTensorShape> InInputShapes);
	
	int EnqueueRDG(FRDGBuilder& RDGBuilder,
		TConstArrayView<NNECore::FTensorBindingCPU> InInputBindings,
		TConstArrayView<NNECore::FTensorBindingCPU> InOutputBindings);

	virtual int RunSync(TConstArrayView<NNECore::FTensorBindingCPU> InInputBindings, TConstArrayView<NNECore::FTensorBindingCPU> InOutputBindings);

	static TUniquePtr<FModelQA> MakeModelQA(const FNNEModelRaw& ONNXModelData, const FString& RuntimeName);
};
	
} // namespace UE::NNEQA::Private