// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralOperators/GemmOperator.h"
#include "GemmCS.h"
#include "ModelProto.h"
#include "NeuralNetworkInferenceUtils.h"
#include "NeuralNetworkInferenceUtilsGPU.h"



/* FGemmOperator structors
 *****************************************************************************/

FGemmOperator::FGemmOperator(const FNodeProto* const InNodeProto)
	: FGemmOperator()
{
	// Sanity check
	if (!InNodeProto)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FGemmOperator(): InNodeProto was a nullptr."));
		return;
	}
	if (const FAttributeProto* AlphaAttribute = FModelProto::FindElementInArray(TEXT("Alpha"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		Alpha = AlphaAttribute->F;
	}
	if (const FAttributeProto* BetaAttribute = FModelProto::FindElementInArray(TEXT("Beta"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		Beta = BetaAttribute->F;
	}
	if (const FAttributeProto* TransAAttribute = FModelProto::FindElementInArray(TEXT("TransA"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		bTransA = (TransAAttribute->I != 0);
	}
	if (const FAttributeProto* TransBAttribute = FModelProto::FindElementInArray(TEXT("TransB"), InNodeProto->Attribute, /*bMustValueBeFound*/false))
	{
		bTransB = (TransBAttribute->I != 0);
	}
}

FGemmOperator::FGemmOperator(const float InAlpha, const float InBeta, const bool bInTransA, const bool bInTransB)
	: FNeuralOperator(TEXT("GEMM"), 13, -1)
	, Alpha(InAlpha)
	, Beta(InBeta)
	, bTransA(bInTransA)
	, bTransB(bInTransB)
{
}

FGemmOperator::~FGemmOperator()
{
}



/* FGemmOperator public functions
 *****************************************************************************/

bool FGemmOperator::ConfigureOutputAndInternalVariablesAndSanityChecks()
{
	bIsLoaded = false;
	// Input sanity checks
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	if (!FNeuralNetworkInferenceUtils::SizeSanityChecks(InputTensors, 2, 3, 1, 2))
	{
		return bIsLoaded;
	}
	const int64 CommonK1 = InputTensors[0]->GetSize(bTransA ? 0 : 1);
	const int64 CommonK2 = InputTensors[1]->GetSize(bTransB ? 1 : 0);
	if (CommonK1 != CommonK2)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FGemmOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): CommonK1 == CommonK2 failed: %d != %d"), CommonK1, CommonK2);
		return bIsLoaded;
	}
	if (InlinedTensor >= 0)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FGemmOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): FGemmOperator cannot be inlined (InlinedTensor = %d)."), InlinedTensor);
		return bIsLoaded;
	}
	// Initialize output tensor
	const int64 M = InputTensors[0]->GetSize(bTransA ? 1 : 0);
	const int64 N = InputTensors[1]->GetSize(bTransB ? 0 : 1);
	FNeuralTensor& OutputTensor = GetOutputTensorNoConst();
	OutputTensor.SetNumUninitialized(InputTensors[0]->GetDataType(), TArray<int64>({ M, N }));
	// Output sanity checks
	if (!FNeuralNetworkInferenceUtils::SizeSanityChecks(GetOutputTensorsNoConst(), 1, 1, 2, 3))
	{
		return bIsLoaded;
	}
	// Check bias tensor (C)
	if (InputTensors.Num() == 3)
	{
		const FNeuralTensor& C = *InputTensors[2];
		// If not same volume than OutputTensor (and not an scalar), then C is unidirectional broadcastable to output tensor. I.e., one of the following is true:
		// Source: https://github.com/onnx/onnx/blob/master/docs/Broadcasting.md#unidirectional-broadcasting
		// - Tensor A and B both have exactly the same shape.
		// - Tensor A and B all have the same number of dimensions and the length of each dimensions is either a common length or B's length is 1.
		// - Tensor B has too few dimensions, and B can have its shapes prepended with a dimension of length 1 to satisfy property 2.
		TArray<int64> CSize2D({ 1u, 1u });
		if (OutputTensor.GetNumberDimensions() < C.GetNumberDimensions())
		{
			UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FGemmOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): Dimension mismatch between the output tensor and"
				" the bias tensor (C): %d vs. %d."), OutputTensor.GetNumberDimensions(), C.GetNumberDimensions());
			return bIsLoaded;
		}
		for (int32 CDimension = 0; CDimension < C.GetNumberDimensions(); ++CDimension) // CSizes.Num() might be smaller than OutputSizes.Num()
		{
			const int64 CSize = C.GetSize(CDimension);
			const int32 OutputDimension = CDimension + OutputTensor.GetNumberDimensions() - C.GetNumberDimensions();
			const int64 OutputSize = OutputTensor.GetSize(OutputDimension);
			if (CSize != OutputSize && CSize != 1)
			{
				UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FGemmOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): CPadded.GetSize(%d) == 1 |"
					" OutputTensor.GetSize(%d): %d != %d"), CDimension, OutputDimension, CSize, OutputSize);
				return bIsLoaded;
			}
			// Basically
			//  - if (C.GetNumberDimensions() == 1 && OutputTensor.GetNumberDimensions() > 1) --> CSize2D = [1, C.GetSize(0)]
			//  - if (C.GetNumberDimensions() == 1 && OutputTensor.GetNumberDimensions() == 1) --> CSize2D = [C.GetSizes(0), 1]
			//  - if (C.GetNumberDimensions() == 2) --> CSize2D = C.GetSizes
			CSize2D[OutputDimension] = CSize;
		}
		CSizeX = CSize2D[1];
		CSizeY = CSize2D[0];
	}
	// Set GEMM parameters
	const FNeuralTensor& A = *InputTensors[0];
	const FNeuralTensor& B = *InputTensors[1];
	OutputRows = OutputTensor.GetSize(0);
	OutputColumns = OutputTensor.GetSize(1);
	AColsOrBRows = A.GetSize(bTransA ? 0 : 1);
	if (AColsOrBRows != B.GetSize(bTransB ? 1 : 0))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FGemmOperator::ConfigureOutputAndInternalVariablesAndSanityChecks(): AColsOrBRows == B.GetSize(1): %d vs. %d"),
			AColsOrBRows, B.GetSize(bTransB ? 1 : 0));
		return bIsLoaded;
	}
	AStrideX = (bTransA ? A.GetSize(1) : 1);
	AStrideY = (bTransA ? 1 : A.GetSize(1));
	BStrideX = (bTransB ? B.GetSize(1) : 1);
	BStrideY = (bTransB ? 1 : B.GetSize(1));
	OutputStride = OutputTensor.GetSize(1);
	// Return true
	bIsLoaded = true;
	return bIsLoaded;
}

void FGemmOperator::ForwardCPU()
{
	// Sanity check
	if (!bIsLoaded)
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FGemmOperator::ForwardCPU(): bIsLoaded was false."));
		return;
	}

	// Bias
	FNeuralTensor& OutputTensor = GetOutputTensorNoConst();
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	if (InputTensors.Num() == 3)
	{
		const FNeuralTensor& C = *InputTensors[2];
		// C is scalar
		if (C.Num() == 1)
		{
			const float BetaBias = Beta * C.At<float>(0);
			OutputTensor.SetTo<float>(BetaBias);
		}
		// O and C both have exactly the same shape.
		else if (OutputTensor.GetSizes() == C.GetSizes())
		{
			for (int64 Index = 0; Index < OutputTensor.Num(); ++Index)
			{
				OutputTensor.At<float>(Index) = Beta * C.At<float>(Index);
			}
		}
		// O and C all have the same number of dimensions and the length of each dimensions is either a common length or C's length is 1.
		// C has too few dimensions, and C can have its shapes prepended with a dimension of length 1 to satisfy property 2.
		else
		{
			for (uint32 YOutput = 0; YOutput < OutputRows; ++YOutput)
			{
				for (uint32 XOutput = 0; XOutput < OutputColumns; ++XOutput)
				{
					const uint32 OutputIndex = YOutput * OutputStride + XOutput; // Row-major
					const uint32 CIndex = (YOutput%CSizeY) * CSizeX + (XOutput%CSizeX);
					OutputTensor.At<float>(OutputIndex) = Beta * C.At<float>(CIndex);
				}
			}
		}
	}
	else
	{
		OutputTensor.SetTo<float>(0.f);
	}

	// Alpha x A x B
	//		Output.noalias() += Alpha * (A * B);
	// Column major
	// Loop over the rows of Output
	const FNeuralTensor& A = *InputTensors[0];
	const FNeuralTensor& B = *InputTensors[1];
	for (uint32 YOutput = 0u; YOutput < OutputRows; ++YOutput)
	{
		// Loop over the columns of Output
		for (uint32 XOutput = 0u; XOutput < OutputColumns; ++XOutput)
		{
			const uint32 OutputIndex = YOutput * OutputStride + XOutput; // Row-major
			// Update Output( YOutput,XOutput ) with the inner product of the ith row of A and the jth column of B
			for (uint32 AColOrBRow = 0; AColOrBRow < AColsOrBRows; ++AColOrBRow)
			{
				// C(i,j) += A(i,p) * B(p,j);
				const uint32 AIndex = YOutput * AStrideY + AColOrBRow * AStrideX; // Row-major (AStrideX == 1) or column-major (AStrideY == 1)
				const uint32 BIndex = AColOrBRow * BStrideY + XOutput * BStrideX; // Row-major (BStrideX == 1) or column-major (BStrideY == 1)
				OutputTensor.At<float>(OutputIndex) += Alpha * A.At<float>(AIndex) * B.At<float>(BIndex);
			}
		}
	}
	// Eigen equivalent of Alpha*A*B
	// #include "Eigen/Dense"
	//const Eigen::Map<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> A((float*)InputTensors[0]->GetData(), InputTensors[0]->GetSize(0), InputTensors[0]->GetSize(1));
	//const Eigen::Map<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> B((float*)InputTensors[1]->GetData(), InputTensors[1]->GetSize(0), InputTensors[1]->GetSize(1));
	//Eigen::Map<Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> Output((float*)OutputTensor.GetData(), OutputTensor.GetSize(0), OutputTensor.GetSize(1));
	//if (bTransA)
	//	if (bTransB)
	//		Output.noalias() += Alpha * (A.transpose() * B.transpose());
	//	else
	//		Output.noalias() += Alpha * (A.transpose() * B);
	//else
	//	if (bTransB)
	//		Output.noalias() += Alpha * (A * B.transpose());
	//	else
	//		Output.noalias() += Alpha * (A * B);
}

void FGemmOperator::ForwardGPU_RenderThread(FRDGBuilder* InOutGraphBuilder)
{
	// Sanity checks
	if (!FNeuralNetworkInferenceUtilsGPU::GPUSanityChecks(InOutGraphBuilder, bIsLoaded))
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FGemmOperator::ForwardGPU_RenderThread(): Sanity checks failed."));
		return;
	}
	// Set parameters
	FGemmCS::FParameters* Parameters = InOutGraphBuilder->AllocParameters<FGemmCS::FParameters>();
	// Input variables
	Parameters->Alpha = Alpha;
	Parameters->OutputRows = OutputRows;
	Parameters->OutputColumns = OutputColumns;
	Parameters->AColsOrBRows = AColsOrBRows;
	Parameters->AStrideX = AStrideX;
	Parameters->AStrideY = AStrideY;
	Parameters->BStrideX = BStrideX;
	Parameters->BStrideY = BStrideY;
	Parameters->OutputStride = OutputStride;
	// SRV/UAV variables
	const TArray<FNeuralTensor*>& InputTensors = GetInputTensorsConst();
	Parameters->ASRV = InputTensors[0]->GetBufferSRVRef();
	Parameters->BSRV = InputTensors[1]->GetBufferSRVRef();
	Parameters->OutputUAV = GetOutputTensorNoConst().GetBufferUAVRef();
	const EGemmMode GemmMode = (InputTensors.Num() == 2 || InputTensors[2]->Num() == 1 ? EGemmMode::CScalar :
		InputTensors[2]->Num() > 1 ? EGemmMode::CTensor :
		EGemmMode::MAX);
	// No bias (i.e., 0 bias) || bias is a scalar
	if (GemmMode == EGemmMode::CScalar)
	{
		Parameters->BetaTimesCScalar = (InputTensors.Num() > 2 ? Beta * InputTensors[2]->At<float>(0) : 0.f);
	}
	// If bias is a tensor
	else if (GemmMode == EGemmMode::CTensor)
	{
		Parameters->CSRV = InputTensors[2]->GetBufferSRVRef();
		Parameters->Beta = Beta;
		Parameters->CSizeX = CSizeX;
		Parameters->CSizeY = CSizeY;
	}
	else
	{
		UE_LOG(LogNeuralNetworkInference, Warning, TEXT("FGemmOperator::ForwardGPU_RenderThread(): Unknown GemmMode = %d."), (int32)GemmMode);
		return;
	}
	// Set shader
	FGemmCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FGemmCS::FGemmMode>(GemmMode);
	TShaderMapRef<FGemmCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
	// Run shader
	const uint32 ThreadGroupCountValueX = FMath::DivideAndRoundUp(OutputColumns, FGemmCS::THREADGROUP_SIZE_X);
	const uint32 ThreadGroupCountValueY = FMath::DivideAndRoundUp(OutputRows, FGemmCS::THREADGROUP_SIZE_Y);
	FComputeShaderUtils::AddPass(
		*InOutGraphBuilder,
		RDG_EVENT_NAME("FGemmCS() - Operator: %s, EGemmMode: %d", *Name, GemmMode),
		ComputeShader,
		Parameters,
		FIntVector(ThreadGroupCountValueX, ThreadGroupCountValueY, 1));
}
