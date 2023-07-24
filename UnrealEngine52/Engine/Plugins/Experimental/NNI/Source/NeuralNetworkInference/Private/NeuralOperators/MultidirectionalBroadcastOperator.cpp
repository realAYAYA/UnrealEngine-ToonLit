// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/MultidirectionalBroadcastOperator.h"
#include "MultidirectionalBroadcastCS.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralNetworkInferenceUtilsGPU.h"
#include "NeuralOperatorEnumClasses.h"
#include "RHI.h"
#include "Shader.h" // TShaderRef



/* FPrivateMultidirectionalBroadcastOperator static functions
 *****************************************************************************/

class FPrivateMultidirectionalBroadcastOperator
{
public:
	/**
	 * In handles multidirectional broadcastable operators. E.g., Add, Pow, etc.
	 */
	static void RunMultidirectionalBroadcastElementWiseOperation(FNeuralTensor& InOutOutputTensor, const FNeuralTensor& InTensorA, const FNeuralTensor& InTensorB,
		const TArray<uint32>& InShapeOutput, const TArray<uint32>& InShapeA, const TArray<uint32>& InShapeB, float OperationFunction(const float, const float));

	/**
	 * Eg to fill inputs.
	 */
	static void FillShapeMultidirectionalBroadcast(FNeuralInt64ArrayUInt32Buffer& OutShapes, const FNeuralTensor& InTensor, const uint32 InOutputNumberDimensions);

	/**
	 * Eg to fill outputs given the input shapes.
	 */
	static bool FillShapeMultidirectionalBroadcast(TArray<uint32>& OutShapes, const FNeuralInt64ArrayUInt32Buffer& InShapesX, const FNeuralInt64ArrayUInt32Buffer& InShapesY, const uint32 InOutputNumberDimensions);

	static EMultidirectionalBroadcastShapeMode GetMultidirectionalBroadcastShapeMode(const FNeuralTensor& InX, const FNeuralTensor& InY);
};

void FPrivateMultidirectionalBroadcastOperator::RunMultidirectionalBroadcastElementWiseOperation(FNeuralTensor& InOutOutputTensor, const FNeuralTensor& InTensorA, const FNeuralTensor& InTensorB,
	const TArray<uint32>& InShapeOutput, const TArray<uint32>& InShapeA, const TArray<uint32>& InShapeB, float OperationFunction(const float, const float))
{
	// 1-pass operation - This is a faster version that only works when the shapes match
	if (InTensorA.Num() == InOutOutputTensor.Num() && InTensorA.Num() == InTensorB.Num())
	{
		for (uint32 IndexOutput1D = 0; IndexOutput1D < InTensorA.Num(); ++IndexOutput1D)
		{
			InOutOutputTensor.At<float>(IndexOutput1D) = OperationFunction(InTensorA.At<float>(IndexOutput1D), InTensorB.At<float>(IndexOutput1D));
		}
	}
	// B is scalar
	else if (InTensorB.Num() == 1)
	{
		const float BValue = InTensorB.At<float>(0);
		for (uint32 IndexOutput1D = 0; IndexOutput1D < InOutOutputTensor.Num(); ++IndexOutput1D)
		{
			InOutOutputTensor.At<float>(IndexOutput1D) = OperationFunction(InTensorA.At<float>(IndexOutput1D), BValue);
		}
	}
	// A is scalar
	else if (InTensorA.Num() == 1)
	{
		const float AValue = InTensorA.At<float>(0);
		for (uint32 IndexOutput1D = 0; IndexOutput1D < InOutOutputTensor.Num(); ++IndexOutput1D)
		{
			InOutOutputTensor.At<float>(IndexOutput1D) = OperationFunction(AValue, InTensorB.At<float>(IndexOutput1D));
		}
	}
	// InTensorA and/or InTensorB and/or Output does not match on shape. Slower version that would work for any case.
	else
	{
		if (InShapeOutput.Num() != InShapeA.Num() || InShapeOutput.Num() != InShapeB.Num())
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IMultidirectionalBroadcastOperator::RunMultidirectionalBroadcastElementWiseOperation():"
				" InShapeOutput.Num() == InShapeA.Num() == InShapeB.Num() failed: %d vs. %d vs. %d."), InShapeOutput.Num(), InShapeA.Num(), InShapeB.Num());
			return;
		}
		TArray<uint32> CurrentIndexArray;
		CurrentIndexArray.Init(0, InShapeOutput.Num());
		for (uint32 IndexOutput1D = 0; IndexOutput1D < InOutOutputTensor.Num(); ++IndexOutput1D)
		{
			// Estimate right 1-D indexes
			int32 IndexA1D = 0;
			int32 IndexB1D = 0;
			for (int32 ShapeIndex = 0; ShapeIndex < InShapeOutput.Num(); ++ShapeIndex)
			{
				//IndexOutput1D = IndexOutput1D * InShapeOutput[ShapeIndex] + CurrentIndexArray[ShapeIndex]; // IndexOutput1D = [[[[...]P_3M + P3]P_2M + P_2]P_1M + P_1]P_0M + P0
				IndexA1D = IndexA1D * InShapeA[ShapeIndex] + (CurrentIndexArray[ShapeIndex] % InShapeA[ShapeIndex]); // IndexA1D = [[[[...]P_3M + P3']P_2M + P_2']P_1M + P_1']P_0M + P0', P_i' = P_i%P_iM
				IndexB1D = IndexB1D * InShapeB[ShapeIndex] + (CurrentIndexArray[ShapeIndex] % InShapeB[ShapeIndex]); // IndexB1D = [[[[...]P_3M + P3']P_2M + P_2']P_1M + P_1']P_0M + P0', P_i' = P_i%P_iM
			}
			// Perform operation on the right indexes
			InOutOutputTensor.At<float>(IndexOutput1D) = OperationFunction(InTensorA.At<float>(IndexA1D), InTensorB.At<float>(IndexB1D));
			// Update CurrentIndexArray (for ShapesOutput = [5 2 3]):
			//    [0 0 0] -> [0 0 1] -> [0 0 2] -> [0 1 0] -> [0 1 1] -> [0 1 2] -> [1 0 0] -> [1 0 1] -> [1 1 2] -> [2 0 0] -> ...
			for (int32 CurrentIndexArrayIndex = CurrentIndexArray.Num() - 1; CurrentIndexArrayIndex > -1; --CurrentIndexArrayIndex)
			{
				if (CurrentIndexArray[CurrentIndexArrayIndex] + 1 == InShapeOutput[CurrentIndexArrayIndex])
				{
					CurrentIndexArray[CurrentIndexArrayIndex] = 0;
				}
				else
				{
					++CurrentIndexArray[CurrentIndexArrayIndex];
					break;
				}
			}
		}
	}
}

void FPrivateMultidirectionalBroadcastOperator::FillShapeMultidirectionalBroadcast(FNeuralInt64ArrayUInt32Buffer& OutShapes, const FNeuralTensor& InTensor, const uint32 InOutputNumberDimensions)
{
	TArray<uint32> ShapeArray;
	ShapeArray.SetNumUninitialized(InOutputNumberDimensions);
	for (int32 DimensionIndex = 0; DimensionIndex < ShapeArray.Num(); ++DimensionIndex)
	{
		// 1s at the beginning
		if (InTensor.GetNumberDimensions() < ShapeArray.Num() - DimensionIndex)
		{
			ShapeArray[DimensionIndex] = 1;
		}
		// Rest
		else
		{
			ShapeArray[DimensionIndex] = InTensor.GetSize(DimensionIndex + InTensor.GetNumberDimensions() - ShapeArray.Num());
		}
	}
	OutShapes.Move(ShapeArray, /*bShouldCreate64BitVersion*/false);
}

bool FPrivateMultidirectionalBroadcastOperator::FillShapeMultidirectionalBroadcast(TArray<uint32>& OutShapes, const FNeuralInt64ArrayUInt32Buffer& InShapesX, const FNeuralInt64ArrayUInt32Buffer& InShapesY,
	const uint32 InOutputNumberDimensions)
{
	// Sanity checks - All shapes must have same size
	if (InShapesX.Num() != InOutputNumberDimensions || InShapesY.Num() != InOutputNumberDimensions)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IMultidirectionalBroadcastOperator::FillShapeMultidirectionalBroadcast():"
			" InShapesX.Num() == InShapesY.Num() == InOutputNumberDimensions failed: %d vs. %d vs. %d"), InShapesX.Num(), InShapesY.Num(), InOutputNumberDimensions);
		return false;
	}
	// Fill OutShapes
	OutShapes.SetNumUninitialized(InOutputNumberDimensions);
	for (int32 DimensionIndex = 0; DimensionIndex < OutShapes.Num(); ++DimensionIndex)
	{
		OutShapes[DimensionIndex] = FMath::Max(InShapesX.GetUInt32()[DimensionIndex], InShapesY.GetUInt32()[DimensionIndex]);
	}
	return true;
}

EMultidirectionalBroadcastShapeMode FPrivateMultidirectionalBroadcastOperator::GetMultidirectionalBroadcastShapeMode(const FNeuralTensor& InX, const FNeuralTensor& InY)
{
	// 1-pass operation - This is a faster version that only works when the shapes match
	if (InX.GetSizes() == InY.GetSizes()) // This is different than InX.Num() == InY.Num(), eg if X={1,3} and Y={3,1}
	{
		return EMultidirectionalBroadcastShapeMode::ElementWise;
	}
	else if (InX.Num() == 1)
	{
		return EMultidirectionalBroadcastShapeMode::AScalar;
	}
	else if (InY.Num() == 1)
	{
		return EMultidirectionalBroadcastShapeMode::BScalar;
	}
	// InTensorA and/or InTensorB and/or Output does not match on shape. Slower version that would work for any case
	else
	{
		return EMultidirectionalBroadcastShapeMode::MultidirectionalBroadcast;
	}
}



/* IMultidirectionalBroadcastOperator structors
 *****************************************************************************/

IMultidirectionalBroadcastOperator::IMultidirectionalBroadcastOperator(const FString& InName, const int32 InVersion,
	const TSharedPtr<EMultidirectionalBroadcastOperator>& InMultidirectionalBroadcastOperator, const TSet<uint32>& InPotentialInlinedTensors)
	: FNeuralOperator(InName, InVersion, InPotentialInlinedTensors)
	, MultidirectionalBroadcastOperator(InMultidirectionalBroadcastOperator)
	, MultidirectionalBroadcastShapeMode(MakeShared<EMultidirectionalBroadcastShapeMode>(EMultidirectionalBroadcastShapeMode::MAX))
{
}

IMultidirectionalBroadcastOperator::~IMultidirectionalBroadcastOperator()
{
}



/* IMultidirectionalBroadcastOperator public functions
 *****************************************************************************/

bool IMultidirectionalBroadcastOperator::ConfigureOutputAndInternalVariablesAndSanityChecks()
{
	bIsLoaded = false;
	// Input sanity checks
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	if (!FNeuralNetworkInferenceUtils::SizeSanityChecks(InputTensors, 2))
	{
		return bIsLoaded;
	}
	if (ShapesOutput.Num() != ShapesX.Num() || ShapesOutput.Num() != ShapesY.Num())
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IMultidirectionalBroadcastOperator::ConfigureOutputAndInternalVariablesAndSanityChecks():"
			" ShapesOutput.Num() == ShapesX.Num() == ShapesY.Num() failed: %d vs. %d vs. %d."), ShapesOutput.Num(), ShapesX.Num(), ShapesY.Num());
		return bIsLoaded;
	}
	if (InlinedTensor < -1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IMultidirectionalBroadcastOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): Invalid InlinedTensor = %d."), InlinedTensor);
		return bIsLoaded;
	}
	// Output (if not inlined)
	if (InlinedTensor < 0)
	{
		TArray<FNeuralTensor*>& OutputTensors = GetOutputTensorsNoConst();
		// Sanity checks
		if (!FNeuralNetworkInferenceUtils::SizeSanityChecks(OutputTensors, 1))
		{
			return bIsLoaded;
		}
		// Set and configure output tensor
		FNeuralTensor& OutputTensor = *OutputTensors.Last();
		OutputTensor.SetNumUninitialized(InputTensors[0]->GetDataType(), ShapesOutput.GetInt64());
	}
	// ShapesOutput < MAX_NUMBER_DIMENSIONS
	if ((uint32)ShapesOutput.Num() > FMultidirectionalBroadcastCS::MAX_NUMBER_DIMENSIONS)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IMultidirectionalBroadcastOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): Only implemented for"
			" ShapesOutput.Num() <= FMultidirectionalBroadcastCS::MAX_NUMBER_DIMENSIONS: %d vs. %d. Contact us if you need more dimensions."),
			ShapesOutput.Num(), FMultidirectionalBroadcastCS::MAX_NUMBER_DIMENSIONS);
		return bIsLoaded;
	}
	// Return true
	bIsLoaded = true;
	return bIsLoaded;
}

void IMultidirectionalBroadcastOperator::ToGPU_RenderThread()
{
	// Sanity check
	if (ShapesX.IsReadBufferValid() || ShapesY.IsReadBufferValid() || ShapesOutput.IsReadBufferValid())
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IMultidirectionalBroadcastOperator::ToGPU_RenderThread():"
			" ShapesX, ShapesY and/or ShapesOutput FReadBuffers were not nullptr."));
	}
	// Set MultidirectionalBroadcastShapeMode
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	*MultidirectionalBroadcastShapeMode = FPrivateMultidirectionalBroadcastOperator::GetMultidirectionalBroadcastShapeMode(*InputTensors[0], *InputTensors[1]);
	// Memory to GPU
	if (*MultidirectionalBroadcastShapeMode == EMultidirectionalBroadcastShapeMode::MultidirectionalBroadcast)
	{
		ShapesX.CreateAndLoadSRVBuffer(TEXT("MultidirectionalBroadcast_ShapesX"));
		ShapesY.CreateAndLoadSRVBuffer(TEXT("MultidirectionalBroadcast_ShapesY"));
		ShapesOutput.CreateAndLoadSRVBuffer(TEXT("MultidirectionalBroadcast_ShapesOutput"));
	}
}

void IMultidirectionalBroadcastOperator::ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
	// Sanity checks
	if (!FNeuralNetworkInferenceUtilsGPU::GPUSanityChecks(InOutGraphBuilder, bIsLoaded))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IMultidirectionalBroadcastOperator::ForwardGPU_RenderThread():"
			" IsInRenderingThread() = %d != true || InOutGraphBuilder is nullptr."), IsInRenderingThread());
		return;
	}
	// Set parameters
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	FMultidirectionalBroadcastCS::FParameters* Parameters = InOutGraphBuilder->AllocParameters<FMultidirectionalBroadcastCS::FParameters>();
	const uint32 Volume = (uint32)(InlinedTensor < 0 ? GetOutputTensorConst().Num() : InputTensors[InlinedTensor]->Num());
	Parameters->TensorSize = Volume;
	// Set MultidirectionalBroadcastInlinedMode
	EMultidirectionalBroadcastInlinedMode MultidirectionalBroadcastInlinedMode;
	const FNeuralTensor& X = *InputTensors[0];
	const FNeuralTensor& Y = *InputTensors[1];
	// Not inlined
	if (InlinedTensor < 0)
	{
		MultidirectionalBroadcastInlinedMode = EMultidirectionalBroadcastInlinedMode::None;
		Parameters->ASRV = X.GetBufferSRVRef();
		Parameters->BSRV = Y.GetBufferSRVRef();
		Parameters->OutputUAV = GetOutputTensorNoConst().GetBufferUAVRef();
		if (*MultidirectionalBroadcastShapeMode == EMultidirectionalBroadcastShapeMode::MultidirectionalBroadcast)
		{
			Parameters->ShapeDimensions = ShapesOutput.Num();
			Parameters->ShapeOutput = ShapesOutput.GetUInt32SRV();
			Parameters->ShapeA = ShapesX.GetUInt32SRV();
			Parameters->ShapeB = ShapesY.GetUInt32SRV();
		}
	}
	// Inlined
	else
	{
		TArray<FNeuralTensor*>& InputTensorsNotConst = GetInputTensorsNoConst();
		// X Inlined
		if (InlinedTensor == 0)
		{
			MultidirectionalBroadcastInlinedMode = EMultidirectionalBroadcastInlinedMode::A;
			Parameters->BSRV = Y.GetBufferSRVRef();
			Parameters->OutputUAV = InputTensorsNotConst[0]->GetBufferUAVRef();
			if (*MultidirectionalBroadcastShapeMode == EMultidirectionalBroadcastShapeMode::MultidirectionalBroadcast)
			{
				Parameters->ShapeDimensions = ShapesOutput.Num();
				Parameters->ShapeOutput = ShapesOutput.GetUInt32SRV();
				Parameters->ShapeB = ShapesY.GetUInt32SRV();
			}
		}
		// Y Inlined
		else if (InlinedTensor == 1)
		{
			MultidirectionalBroadcastInlinedMode = EMultidirectionalBroadcastInlinedMode::B;
			Parameters->ASRV = X.GetBufferSRVRef();
			Parameters->OutputUAV = InputTensorsNotConst[1]->GetBufferUAVRef();
			if (*MultidirectionalBroadcastShapeMode == EMultidirectionalBroadcastShapeMode::MultidirectionalBroadcast)
			{
				Parameters->ShapeDimensions = ShapesOutput.Num();
				Parameters->ShapeOutput = ShapesOutput.GetUInt32SRV();
				Parameters->ShapeA = ShapesX.GetUInt32SRV();
			}
		}
		// Unknown case
		else
		{
			MultidirectionalBroadcastInlinedMode = EMultidirectionalBroadcastInlinedMode::None; // To avoid unset warning
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IMultidirectionalBroadcastOperator::ForwardGPU_RenderThread(): Unknown InlinedTensor = %d."), InlinedTensor);
			return;
		}
	}
	// Set shader
	FMultidirectionalBroadcastCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FMultidirectionalBroadcastCS::FShaderType>(*MultidirectionalBroadcastOperator);
	PermutationVector.Set<FMultidirectionalBroadcastCS::FInlinedMode>(MultidirectionalBroadcastInlinedMode);
	PermutationVector.Set<FMultidirectionalBroadcastCS::FShapeMode>(*MultidirectionalBroadcastShapeMode);
	TShaderMapRef<FMultidirectionalBroadcastCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
	// Run shader
	const uint32 ThreadGroupCountValueX = FMath::DivideAndRoundUp(Volume, FMultidirectionalBroadcastCS::THREADGROUP_SIZE_X);
	FComputeShaderUtils::AddPass(
		*InOutGraphBuilder,
		RDG_EVENT_NAME("FMultidirectionalBroadcastCS() - Operator: %s, InlinedTensor: %d", *Name, InlinedTensor),
		ComputeShader,
		Parameters,
		FIntVector(ThreadGroupCountValueX, 1, 1));
}



/* IMultidirectionalBroadcastOperator protected functions
 *****************************************************************************/

void IMultidirectionalBroadcastOperator::ForwardCPUWithFunction(float InOperatorFunction(const float, const float))
{
	// Sanity checks
	if (!bIsLoaded)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IMultidirectionalBroadcastOperator::ForwardCPUWithFunction(): bIsLoaded was false."));
		return;
	}
	if (InlinedTensor < -1)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IMultidirectionalBroadcastOperator::ForwardCPUWithFunction(): Invalid InlinedTensor = %d."), InlinedTensor);
		return;
	}
	// Not inlined layer (output is different than inputs)
	if (InlinedTensor < 0)
	{
		const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
		// Running operator in a multidirectional broadcastable way
		FPrivateMultidirectionalBroadcastOperator::RunMultidirectionalBroadcastElementWiseOperation(GetOutputTensorNoConst(),
			*InputTensors[0], *InputTensors[1], ShapesOutput.GetUInt32(), ShapesX.GetUInt32(), ShapesY.GetUInt32(), InOperatorFunction);
	}
	// Inlined layer
	else
	{
		TArray<FNeuralTensor*>& InputTensors = GetInputTensorsNoConst();
		FNeuralTensor& X = *InputTensors[0];
		FNeuralTensor& Y = *InputTensors[1];
		// Running operator in a multidirectional broadcastable way
		FPrivateMultidirectionalBroadcastOperator::RunMultidirectionalBroadcastElementWiseOperation((InlinedTensor == 0 ? X : Y),
			X, Y, ShapesOutput.GetUInt32(), ShapesX.GetUInt32(), ShapesY.GetUInt32(), InOperatorFunction);
	}
}

bool IMultidirectionalBroadcastOperator::EstimateInlinedTensorFromPotentialOnes()
{
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	if (InputTensors.Num() != 2)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IMultidirectionalBroadcastOperator::EstimateInlinedTensorFromPotentialOnes(): InputTensors.Num() = %d != 2."), InputTensors.Num());
		return false;
	}

	// Create ShapesX, ShapesY, and ShapesOutput GetInt64()/GetUInt32()
	const FNeuralTensor& X = *InputTensors[0];
	const FNeuralTensor& Y = *InputTensors[1];
	const uint32 OutputNumberDimensions = FMath::Max(X.GetNumberDimensions(), Y.GetNumberDimensions());
	FPrivateMultidirectionalBroadcastOperator::FillShapeMultidirectionalBroadcast(ShapesX, X, OutputNumberDimensions);
	FPrivateMultidirectionalBroadcastOperator::FillShapeMultidirectionalBroadcast(ShapesY, Y, OutputNumberDimensions);
	TArray<uint32> OutputShapeTemporary;
	if (!FPrivateMultidirectionalBroadcastOperator::FillShapeMultidirectionalBroadcast(OutputShapeTemporary, ShapesX, ShapesY, OutputNumberDimensions))
	{
		return false;
	}
	ShapesOutput.Move(OutputShapeTemporary, /*bShouldCreate64BitVersion*/ true);

	// Can layer be inlined?
	if (PotentiallyInlinedTensors.Num() > 0)
	{
		// Size(X) == Size(Output)
		if (X.GetSizes() == ShapesOutput.GetInt64() && PotentiallyInlinedTensors.Contains(0))
		{
			InlinedTensor = 0;
		}
		// Size(Y) == Size(Output)
		else if (Y.GetSizes() == ShapesOutput.GetInt64() && PotentiallyInlinedTensors.Contains(1))
		{
			InlinedTensor = 1;
		}
		// No same size than output
		else
		{
			InlinedTensor = -1;
		}
	}

	// Reset shape buffers
	if (ShapesOutput.IsReadBufferValid())
	{
		ShapesX.ReleaseReadBuffer();
		ShapesY.ReleaseReadBuffer();
		ShapesOutput.ReleaseReadBuffer();
	}
	ShapesX.ResetReadBuffer();
	ShapesY.ResetReadBuffer();
	ShapesOutput.ResetReadBuffer();
	return true;
}

void IMultidirectionalBroadcastOperator::ShapesToGPU()
{
	// Sanity check
	if (ShapesX.IsReadBufferValid() || ShapesY.IsReadBufferValid() || ShapesOutput.IsReadBufferValid())
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("IMultidirectionalBroadcastOperator::ShapesToGPU():"
			" ShapesX, ShapesY and/or ShapesOutput FReadBuffers were not nullptr."));
	}
	// Create GPU memory for it
	ShapesX.CreateAndLoadSRVBuffer(TEXT("MultidirectionalBroadcast_ShapesX"));
	ShapesY.CreateAndLoadSRVBuffer(TEXT("MultidirectionalBroadcast_ShapesY"));
	ShapesOutput.CreateAndLoadSRVBuffer(TEXT("MultidirectionalBroadcast_ShapesOutput"));
}
