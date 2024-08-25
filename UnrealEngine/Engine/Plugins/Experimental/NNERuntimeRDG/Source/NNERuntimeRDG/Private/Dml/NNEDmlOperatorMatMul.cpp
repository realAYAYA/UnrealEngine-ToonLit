// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlMatMul : public FOperatorDml
{
	static constexpr uint32 NumAllowedInputTensors = 2, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 2, MaxTensorRank = TNumericLimits<int32>::Max();
public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlMatMul();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("MatMul");

		if(InputShapes.Num() != NumAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid number of input tensors. %d provided, it should be %d."), *OpName, InputShapes.Num(), NumAllowedInputTensors);
			return false;
		}
		
		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], 
			{ 	ENNETensorDataType::Float, ENNETensorDataType::Half
			},
			MinTensorRank, MaxTensorRank
		  	))
		{
			return false;
		}

		if (!CheckGenericTensor(OpName, InputTypes[1], InputShapes[1], 
			{ 	ENNETensorDataType::Float, ENNETensorDataType::Half
			},
			MinTensorRank, MaxTensorRank
		  	))
		{
			return false;
		}

		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() == NumAllowedInputTensors);
		check(Outputs.Num() == NumAllowedOutputTensors);

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		check(InputTensors.Num() == NumAllowedInputTensors);
		check(OutputTensors.Num() == NumAllowedOutputTensors);

		const NNE::FTensorShape& InputA = InputTensors[0]->GetShape();
		const NNE::FTensorShape& InputB = InputTensors[1]->GetShape();

		if (InputA.Rank() < 2)
		{
			UE_LOG(LogNNE, Warning, TEXT("Matmul first input should be at least of rank 2"));
			return -1;
		}
		
		if (InputB.Rank() < 2)
		{
			UE_LOG(LogNNE, Warning, TEXT("Matmul second input should be at least of rank 2"));
			return -1;
		}

		if (InputA.GetData()[InputA.Rank() - 1] != InputB.GetData()[InputB.Rank() - 2])
		{
			UE_LOG(LogNNE, Warning, TEXT("Matmul first input last dimension should be equal to second input last dimension"));
			return -1;
		}

		const int32 OutputRank = FMath::Max(InputA.Rank(), InputB.Rank());
		TArray<uint32> OutputShape;
		OutputShape.SetNumUninitialized(OutputRank);

		//Broadcast
		for (int32 i = 0; i < OutputRank; ++i)
		{
			int32 AIndex = InputA.Rank() - 1 - i;
			int32 BIndex = InputB.Rank() - 1 - i;
			int32 AValue = AIndex >= 0 ? InputA.GetData()[AIndex] : 1;
			int32 BValue = BIndex >= 0 ? InputB.GetData()[BIndex] : 1;
			int32 OutputValue = FMath::Max(AValue, BValue);
			OutputShape[OutputRank - 1 - i] = OutputValue;
		}

		//2D Mat
		OutputShape[OutputRank - 2] = InputA.GetData()[InputA.Rank() - 2];
		OutputShape[OutputRank - 1] = InputB.GetData()[InputB.Rank() - 1];

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));
		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& ATensor = *InputTensors[0];
		const NNE::Internal::FTensor& BTensor = *InputTensors[1];
		const NNE::Internal::FTensor& OutTensor = *OutputTensors[0];

		auto TrimMatrixStackLeadingOnes = [](TConstArrayView<uint32>&& Input) -> TConstArrayView<uint32>
		{
			int Start = 0;
			for (; Start < Input.Num() - 2; ++Start)
			{
				if (Input[Start] != 1)
				{
					break;
				}
			}
			return Input.RightChop(Start);
		};

		Util::FSmallUIntArray ActualShapeA (TrimMatrixStackLeadingOnes(ATensor.GetShape().GetData()));
		Util::FSmallUIntArray ActualShapeB (TrimMatrixStackLeadingOnes(BTensor.GetShape().GetData()));
		Util::FSmallUIntArray ActualShapeOut (TrimMatrixStackLeadingOnes(OutTensor.GetShape().GetData()));

		check(ActualShapeA.Num() > 0);
		check(ActualShapeB.Num() > 0);
		check(ActualShapeOut.Num() > 0);

		if (ActualShapeA.Num() == 1)
		{
			check(ActualShapeOut.Num() == 1);
			ActualShapeA = {1, ActualShapeA[0]};
		}

		if (ActualShapeB.Num() == 1)
		{
			check(ActualShapeOut.Num() == 1);
			ActualShapeB = {ActualShapeB[0], 1};
		}

		// Since DML requires a 4D tensor as shape, where first two sizes are BatchCount and ChannelCount, we cannot represent general
		// broadcasting of both A and B.
		// Example: [2,1,2,5,4] x [1,3,2,4,3] => [2,3,2,5,3]
		// Furthermore, for simplicity, we have chosen to only use the channel dimension to represent the stack index and put the batch size to 1.
		// This implies that we support only the cases in which only one between A and B are broadcasted.
		// This constraint could be relaxed by also smartly employing the batch dimension, but general broadcasting of both A and B is still not possible.

		// Test compatibility (see also comments below)
		bool bARequiresBroadcasting = false;
		bool bBRequiresBroadcasting = false;
		{
			int32 MaxRank = FMath::Max(ActualShapeA.Num(), ActualShapeB.Num());
			for (int Dim = MaxRank - 3; Dim >= 0; Dim--)
			{
				int AIdx = Dim - MaxRank + ActualShapeA.Num();
				int BIdx = Dim - MaxRank + ActualShapeB.Num();

				if (bARequiresBroadcasting)
				{
					if (AIdx >= 0)
					{
						check(ActualShapeA[AIdx] == 1);
					}
				}

				if (bBRequiresBroadcasting)
				{
					if (BIdx >= 0)
					{
						check(ActualShapeB[BIdx] == 1);
					}
				}

				if (AIdx >= 0 && BIdx >= 0)
				{
					if (ActualShapeA[AIdx] > ActualShapeB[BIdx])
					{
						check(!bARequiresBroadcasting);
						check(ActualShapeA[AIdx] % ActualShapeB[BIdx] == 0);
						bBRequiresBroadcasting = true;
					}

					if (ActualShapeB[BIdx] > ActualShapeA[AIdx])
					{
						check(!bBRequiresBroadcasting);
						check(ActualShapeB[BIdx] % ActualShapeA[AIdx] == 0);
						bARequiresBroadcasting = true;
					}
				}

				if (AIdx < 0 && BIdx >= 0 && ActualShapeB[BIdx] != 1)
				{
					bARequiresBroadcasting = true;
				}

				if (BIdx < 0 && AIdx >= 0 && ActualShapeA[AIdx] != 1)
				{
					bBRequiresBroadcasting = true;
				}
			}
		}

		Util::FSmallUIntArray BroadcastShape(
			bARequiresBroadcasting ? 
			TConstArrayView<uint32>(ActualShapeB).LeftChop(2) : 
			TConstArrayView<uint32>(ActualShapeA).LeftChop(2)
			);

		check(BroadcastShape == TConstArrayView<uint32>(ActualShapeOut).LeftChop(2));

		auto To4DTensor = [](TConstArrayView<uint32> BroadcastShape, TConstArrayView<uint32> MatrixShape) -> Util::FSmallUIntArray
		{
			check(MatrixShape.Num() == 2);

			uint32 NumMatrices = 1;
			for(uint32 Elem : BroadcastShape)
			{
				NumMatrices *= Elem;
			}
			return Util::FSmallUIntArray({1, NumMatrices, MatrixShape[0], MatrixShape[1]});
		};

		Util::FSmallUIntArray DmlShapeA = To4DTensor(TConstArrayView<uint32>(ActualShapeA).LeftChop(2), TConstArrayView<uint32>(ActualShapeA).Right(2));
		Util::FSmallUIntArray BroadcastShapeA = To4DTensor(BroadcastShape, TConstArrayView<uint32>(ActualShapeA).Right(2));
		Util::FSmallUIntArray DmlShapeB = To4DTensor(TConstArrayView<uint32>(ActualShapeB).LeftChop(2), TConstArrayView<uint32>(ActualShapeB).Right(2));
		Util::FSmallUIntArray BroadcastShapeB = To4DTensor(BroadcastShape, TConstArrayView<uint32>(ActualShapeB).Right(2));
		Util::FSmallUIntArray DmlShapeOut = To4DTensor(BroadcastShape, TConstArrayView<uint32>(ActualShapeOut).Right(2));

		check(DmlShapeA[3] == DmlShapeB[2]); // K
		check(DmlShapeOut[2] == DmlShapeA[2]); // M
		check(DmlShapeOut[3] == DmlShapeB[3]); // N

		// Another broadcast issue!
		// The current implementation of Util::FTensorDesc::InitFromTensor() is not correct with respect to broadcast as it sets
		// wrong sizes and strides when a dimension to be broadcasted is not of size 1.
		// Example:
		// [1, 2, 5, 4] ->(broadcast) [1, 12, 5, 4]
		// would require
		// sizes:   [ 1,   6,   2,   5,   4 ],
		// strides: [ 40,  0,  20,   4,   1 ].
		// This is in general possible only if 
		// - one is free to have arbitrary sized arrays for sizes and strides, but DirectML constrains them to 4D or 5D,
		// - the broadcast dimension size is a multiple of the broadcasted one.
		// In our specific case here we can use both the batch and index dimensions to represent non-1 broadcasting, but
		// we are still limited to cases where the broadcast dimension size is a multiple of the broadcasted one.
		{
			auto FixBroadcast = [](Util::FSmallUIntArray& DmlShape, Util::FSmallUIntArray& BroadcastArray, uint32 BroadcastFactor)
			{
				if(DmlShape == BroadcastArray)
				{
					DmlShape[0] = BroadcastFactor;
					DmlShape[1] /= BroadcastFactor;
				}

				BroadcastArray[0] = BroadcastFactor;
				BroadcastArray[1] /= BroadcastFactor;
			};
			check(DmlShapeB == BroadcastShapeB ? (BroadcastShapeA[1] % DmlShapeA[1] == 0) : (BroadcastShapeB[1] % DmlShapeB[1] == 0));
			uint32 BroadcastFactor = DmlShapeB == BroadcastShapeB ? BroadcastShapeA[1] / DmlShapeA[1] : BroadcastShapeB[1] / DmlShapeB[1];

			FixBroadcast(DmlShapeA, BroadcastShapeA, BroadcastFactor);
			FixBroadcast(DmlShapeB, BroadcastShapeB, BroadcastFactor);
			DmlShapeOut[0] = BroadcastFactor;
			DmlShapeOut[1] /= BroadcastFactor;
		}
		
		FTensorDescDml	DmlInputATensorDesc;
		FTensorDescDml	DmlInputBTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputATensorDesc
				.SetTensorRank(2, 4)
				.SetFromTensor(ATensor)
				.SetShape(DmlShapeA, BroadcastShapeA)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize MatMul's tensor A for DML inference"));
			return false;
		}

		if (!DmlInputBTensorDesc
				.SetTensorRank(2, 4)
				.SetFromTensor(BTensor)
				.SetShape(DmlShapeB, BroadcastShapeB)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize MatMul's tensor B for DML inference"));
			return false;
		}

		if (!DmlOutputTensorDesc
				.SetTensorRank(2, 4)
				.SetFromTensor(OutTensor)
				.SetShape(DmlShapeOut)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize MatMul's output tensor for DML inference"));
			return false;
		}

		DML_GEMM_OPERATOR_DESC	DmlGemmOpDesc{};

		DmlGemmOpDesc.ATensor = DmlInputATensorDesc.GetDmlDesc();
		DmlGemmOpDesc.BTensor = DmlInputBTensorDesc.GetDmlDesc();
		DmlGemmOpDesc.CTensor = nullptr;
		DmlGemmOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		DmlGemmOpDesc.Alpha = 1.0f;
		DmlGemmOpDesc.Beta = 0.0f;
		DmlGemmOpDesc.TransA = DML_MATRIX_TRANSFORM_NONE;
		DmlGemmOpDesc.TransB = DML_MATRIX_TRANSFORM_NONE;
		DmlGemmOpDesc.FusedActivation = nullptr;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_GEMM, &DmlGemmOpDesc} );
	}
};

NNE_DML_REGISTER_OP_VERSION(MatMul, 1)
NNE_DML_REGISTER_OP_VERSION(MatMul, 9)
NNE_DML_REGISTER_OP_VERSION(MatMul, 13)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
