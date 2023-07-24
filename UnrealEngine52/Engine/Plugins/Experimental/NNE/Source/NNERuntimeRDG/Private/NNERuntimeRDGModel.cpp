// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGModel.h"

#include "NNECoreRuntimeFormat.h"
#include "NNEUtilsModelOptimizer.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"

#include "Serialization/MemoryReader.h"

namespace UE::NNERuntimeRDG::Private
{

bool FModelRDG::LoadModel(TConstArrayView<uint8> ModelData, FNNERuntimeFormat& Format, int32 GuidAndVersionSize)
{
	TConstArrayView<uint8> ModelBuffer = { &(ModelData.GetData()[GuidAndVersionSize]), ModelData.Num() - GuidAndVersionSize };

	FMemoryReaderView Reader(ModelBuffer);

	FNNERuntimeFormat::StaticStruct()->SerializeBin(Reader, &Format);

	// Data for base class
	InputSymbolicTensors.Empty();
	OutputSymbolicTensors.Empty();

	// Data for RDG
	AllSymbolicTensorDescs.Empty();
	IntermediateTensorIndices.Empty();
	WeightTensorIndices.Empty();
	InputTensorIndices.Empty();
	OutputTensorIndices.Empty();
	OperatorInputTensorIndices.Empty();
	OperatorOutputTensorIndices.Empty();

	// Add tensors
	for (int32 Idx = 0; Idx < Format.Tensors.Num(); ++Idx)
	{
		const FNNEFormatTensorDesc& FormatTensorDesc = Format.Tensors[Idx];

		const NNECore::FSymbolicTensorShape SymbolicShape = NNECore::FSymbolicTensorShape::Make(FormatTensorDesc.Shape);
		const NNECore::FTensorDesc SymbolicTensor = NNECore::FTensorDesc::Make(FormatTensorDesc.Name, SymbolicShape, FormatTensorDesc.DataType);

		AllSymbolicTensorDescs.Emplace(SymbolicTensor);

		if (FormatTensorDesc.Type == ENNEFormatTensorType::Input)
		{
			InputTensorIndices.Emplace(Idx);
			InputSymbolicTensors.Emplace(SymbolicTensor);
		}
		else if (FormatTensorDesc.Type == ENNEFormatTensorType::Output)
		{
			OutputTensorIndices.Emplace(Idx);
			OutputSymbolicTensors.Emplace(SymbolicTensor);
		}
		else if (FormatTensorDesc.Type == ENNEFormatTensorType::Intermediate)
		{
			IntermediateTensorIndices.Emplace(Idx);
		}
		else if (FormatTensorDesc.Type == ENNEFormatTensorType::Initializer)
		{
			WeightTensorIndices.Emplace(Idx);
			if (!SymbolicTensor.GetShape().IsConcrete())
			{
				UE_LOG(LogNNE, Error, TEXT("Weight tensor %s should have a concrete shape"), *SymbolicTensor.GetName());
				return false;
			}

			const NNECore::FTensorShape TensorShape = NNECore::FTensorShape::MakeFromSymbolic(SymbolicTensor.GetShape());
			FTensorRDG& WeightRDG = WeightTensorRDGs.Emplace_GetRef(FTensorRDG::Make(SymbolicTensor, TensorShape, nullptr));

			if (WeightRDG.GetDataSize() != FormatTensorDesc.DataSize)
			{
				UE_LOG(LogNNE, Error, TEXT("Weight %s has incorrect size. Expected %d bytes, got %d"), *SymbolicTensor.GetName(), FormatTensorDesc.DataSize, WeightRDG.GetDataSize());
				return false;
			}

			const uint8* DataPtr = Format.TensorData.GetData() + FormatTensorDesc.DataOffset;
			TConstArrayView<uint8> DataView = MakeArrayView(DataPtr, FormatTensorDesc.DataSize);

			WeightRDG.SetPreparedData(DataView);
		}
		checkf(FormatTensorDesc.Type != ENNEFormatTensorType::None, TEXT("Unsupported tensor type None"));
	}

	// Loop over all operators in the model and store tensor indices for input/output
	for (int32 Idx = 0; Idx < Format.Operators.Num(); ++Idx)
	{
		OperatorInputTensorIndices.Emplace(Format.Operators[Idx].InTensors);
		OperatorOutputTensorIndices.Emplace(Format.Operators[Idx].OutTensors);
	}

	return true;
}



int FModelRDG::SetInputTensorShapes(TConstArrayView<NNECore::FTensorShape> InInputShapes)
{
	OutputTensorShapes.Reset(OutputTensorIndices.Num());

	//Verify input shape are valid for the model and set InputTensorShapes
	if (FModelBase<NNECore::IModelRDG>::SetInputTensorShapes(InInputShapes) != 0)
	{
		return -1;
	}

	//Allocate and prime all AllTensorRDGRefs with concrete shapes defaulting variables dimension to 1 if needed
	AllTensorRDGRefs.Init(nullptr, AllSymbolicTensorDescs.Num());

	InputTensorRDGs.Reset(InputTensorIndices.Num());
	for (int32 i = 0; i < InputTensorIndices.Num(); ++i)
	{
		const int32 Idx = InputTensorIndices[i];
		const NNECore::FTensorDesc& TensorDesc = InputSymbolicTensors[i];
		const NNECore::FTensorShape& TensorShape = InputTensorShapes[i];

		InputTensorRDGs.Emplace(FTensorRDG::Make(TensorDesc, TensorShape, nullptr));
		AllTensorRDGRefs[Idx] = &InputTensorRDGs[i];
	}

	for (int32 i = 0; i < WeightTensorIndices.Num(); ++i)
	{
		const int32 Idx = WeightTensorIndices[i];

		AllTensorRDGRefs[Idx] = &WeightTensorRDGs[i];
	}

	IntermediateTensorRDGs.Reset(IntermediateTensorIndices.Num());
	for (int32 i = 0; i < IntermediateTensorIndices.Num(); ++i)
	{
		const int32 Idx = IntermediateTensorIndices[i];
		const NNECore::FTensorDesc& TensorDesc = AllSymbolicTensorDescs[Idx];
		const NNECore::FTensorShape TensorShape = NNECore::FTensorShape::MakeFromSymbolic(TensorDesc.GetShape());

		IntermediateTensorRDGs.Emplace(FTensorRDG::Make(TensorDesc, TensorShape, nullptr));
		AllTensorRDGRefs[Idx] = &IntermediateTensorRDGs[i];
	}

	OutputTensorRDGs.Reset(OutputTensorIndices.Num());
	for (int32 i = 0; i < OutputTensorIndices.Num(); ++i)
	{
		const int32 Idx = OutputTensorIndices[i];
		const NNECore::FTensorDesc& TensorDesc = OutputSymbolicTensors[i];
		const NNECore::FTensorShape TensorShape = NNECore::FTensorShape::MakeFromSymbolic(TensorDesc.GetShape());

		OutputTensorRDGs.Emplace(FTensorRDG::Make(TensorDesc, TensorShape, nullptr));
		AllTensorRDGRefs[Idx] = &OutputTensorRDGs[i];
	}

	checkCode(
		for (int i = 0; i < AllTensorRDGRefs.Num(); ++i)
		{
			checkf(AllTensorRDGRefs[i] != nullptr, TEXT("Tensor at index %d, was not allocated for model preparation."), i);
		};
	);

	//Allow the specific runtime to run shape inference if supported
	if (PrepareTensorShapesAndData() != 0)
	{
		return -1;
	}

	checkCode(
		for (int i = 0; i < AllTensorRDGRefs.Num(); ++i)
		{
			checkf(AllTensorRDGRefs[i] != nullptr, TEXT("Tensor at index %d, was not allocated after model preparation."), i);
			checkf(AllTensorRDGRefs[i]->GetShape().IsCompatibleWith(AllSymbolicTensorDescs[i].GetShape()), TEXT("Tensor at index %d have a shape incompatible with model definition."), i);
		};
	);

	//Set OutputTensorShapes for the model from preparation result
	for (int32 OutputIndices : OutputTensorIndices)
	{
		OutputTensorShapes.Emplace(AllTensorRDGRefs[OutputIndices]->GetShape());
	}

	check(InputTensorIndices.Num() + OutputTensorIndices.Num() + WeightTensorIndices.Num() + IntermediateTensorIndices.Num() == AllTensorRDGRefs.Num());
	check(InputTensorShapes.Num() == InputSymbolicTensors.Num());
	check(OutputTensorShapes.Num() == OutputSymbolicTensors.Num());
	check(WeightTensorIndices.Num() == WeightTensorRDGs.Num());
	check(AllTensorRDGRefs.Num() == AllSymbolicTensorDescs.Num());
	
	return 0;
}

FRDGBufferDesc CreateRDGBufferDescForTensorRDG(const FTensorRDG& Tensor)
{
	// FIXME: CreateStructuredDesc() creates a crash on VulkanRHI
	//FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(Tensor.GetElemByteSize(), Tensor.GetVolume());
	FRDGBufferDesc Desc = FRDGBufferDesc::CreateBufferDesc(Tensor.GetElemByteSize(), Tensor.GetVolume());

	return Desc;
}

/**
 * Enqueue operators to RDG, the caller will run the GraphBuilder.Execute()
 */
int FModelRDG::EnqueueRDG(FRDGBuilder& RDGBuilder, TConstArrayView<NNECore::FTensorBindingRDG> InInputBindings, TConstArrayView<NNECore::FTensorBindingRDG> InOutputBindings)
{
	check(IsInRenderingThread());

	int Res;

	// Verify the model inputs were prepared
	if (InputTensorShapes.Num() == 0)
	{
		UE_LOG(LogNNE, Error, TEXT("EnqueueRDG(): Input shapes are not set, please call SetInputTensorShapes."));
		return -1;
	}

	Res = SetTensors(RDGBuilder, InputTensorRDGs, InInputBindings);
	if (Res != -1)
	{
		UE_LOG(LogNNE, Warning, TEXT("Invalid buffer (was nullptr) for input tensor binding at index %d"), Res);
		return -1;
	}

	Res = SetTensors(RDGBuilder, OutputTensorRDGs, InOutputBindings);
	if (Res != -1)
	{
		UE_LOG(LogNNE, Warning, TEXT("Invalid buffer (was nullptr) for output tensor binding at index %d"), Res);
		return -1;
	}

	
	//Register constant and weights tensors resources to RDG graph, uploading constant tensors if needed
	bool bBuffersUploadedAndRegisteredToRDGGraph = PrepareModelRDG(RDGBuilder);

	//Create temporary buffers for NOT const intermediate tensors
	for (FTensorRDG& TensorRDG : IntermediateTensorRDGs)
	{
		if (!TensorRDG.HasPreparedData())
		{
			const FRDGBufferDesc BufferDesc = CreateRDGBufferDescForTensorRDG(TensorRDG);
			const FRDGBufferRef TensorBuffer = RDGBuilder.CreateBuffer(BufferDesc, *TensorRDG.GetName(), ERDGBufferFlags::None);
			check(!bBuffersUploadedAndRegisteredToRDGGraph || TensorRDG.GetBuffer() == nullptr);
			TensorRDG.SetBuffer(TensorBuffer);
		}
	}

	//Note: DirectML uses RHI buffers instead of RDG buffers
	//For now weights tensors are not uploaded to GPU thus GetBuffer will return nullptr for them.
	if (bBuffersUploadedAndRegisteredToRDGGraph)
	{
		checkCode(for (const FTensorRDG* TensorRDG : AllTensorRDGRefs) { if (TensorRDG != nullptr) { check(TensorRDG->GetBuffer() != nullptr); } });
	}

	// We can now dispatch operators
	AddDispatchOps_RenderThread(RDGBuilder);

	return 0;
}

int FModelRDG::SetTensors(FRDGBuilder& GraphBuilder, FTensorRDGArray& InTensorRDGs, TConstArrayView<NNECore::FTensorBindingRDG> InBindings)
{
	check(InBindings.Num() == InTensorRDGs.Num());
	
	for (int32 Idx = 0; Idx < InBindings.Num(); ++Idx)
	{
		FTensorRDG& TensorRDG = InTensorRDGs[Idx];
		const NNECore::FTensorBindingRDG& Binding = InBindings[Idx];
		if (Binding.Buffer == nullptr)
		{
			return Idx;
		}
		TensorRDG.SetBuffer(Binding.Buffer);
	}

	return -1;
}

}