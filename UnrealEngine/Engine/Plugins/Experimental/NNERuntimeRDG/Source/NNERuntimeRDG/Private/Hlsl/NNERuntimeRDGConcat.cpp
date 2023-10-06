// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGConcat.h"
#include "NNETensor.h"
#include "NNETypes.h"
#include "NNERuntimeRDGHelperConcat.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	/**
	 * Concat operator implementation
	 */
	class FConcat : public FOperatorHlsl
	{
	public:

		FConcat() {}
		virtual ~FConcat() = default;

		int32 Axis;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() >= 1);
			check(OutputTensors.Num() == 1);

			TArray<uint32> OutputShapeData(InputTensors[0]->GetShape().GetData());
			int32 AxisIndex = Axis >= 0 ? Axis : InputTensors[0]->GetShape().Rank() - Axis;
			
			for (int32 i = 1; i < InputTensors.Num(); ++i)
			{
				OutputShapeData[AxisIndex] += InputTensors[i]->GetShape().GetData()[AxisIndex];
				
				for (int32 r = 0; r < OutputShapeData.Num(); ++r)
				{
					if (r != AxisIndex && (OutputShapeData[r] != InputTensors[i]->GetShape().GetData()[r]))
					{
						UE_LOG(LogNNE, Warning, TEXT("Concat: all input tensors should have the same shape except on the concatenation axis"));
						return false;
					}
				}
			}

			NNE::FTensorShape OutputShape = NNE::FTensorShape::Make(OutputShapeData);
			
			OutputTensors[0]->SetShape(OutputShape);

			Internal::CPUHelper::Concat::Apply(InputTensors, *OutputTensors[0], Axis);
			
			if (!OutputTensors[0]->HasPreparedData())
			{
				UE_LOG(LogNNE, Warning, TEXT("Concat: Output could not be computed as a constant tensor, however Concat is not implemented on GPU at the moment."));
				return -1;
			}
			
			return 0;
		};

		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() >= 1);
			check(OutputTensorDescs.Num() == 1);

			Axis = Attributes.GetValue<int32>(TEXT("axis"));
			
			int32 InputsRank = InputTensorDescs[0].GetShape().Rank();
			
			for (int32 i = 1; i < InputTensorDescs.Num(); ++i)
			{
				if (InputsRank != InputTensorDescs[i].GetShape().Rank())
				{
					UE_LOG(LogNNE, Warning, TEXT("Concat: all input tensors should have the same rank"));
					return false;
				}
			}
			
			if (Axis < -InputsRank || Axis >(InputsRank - 1))
			{
				UE_LOG(LogNNE, Warning, TEXT("Axis should be in range [-r,r-1] however it is %d while inputs have rank %d."), Axis, InputsRank);
				return false;
			}
			
			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			UE_LOG(LogNNE, Warning, TEXT("Concat: Output should be constant and already uploaded to GPU memory. Dispatch should not need to be called."));
		}
	};

	bool ValidateConcatOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputConcats)
	{
		bool bIsValid = true;

		//This match version 11 of the Concat operator
		//https://github.com/onnx/onnx/blob/main/docs/Operators.md#Concat
		FAttributeValidator AttributeValidator;
		AttributeValidator.AddRequired(TEXT("axis"), ENNEAttributeDataType::Int32);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		if (InputTypes.Num() == 0)
		{
			UE_LOG(LogNNE, Error, TEXT("Concat operator requires at least 1 input"));
			bIsValid = false;
		}
		for (int32 i = 0; i < InputTypes.Num(); ++i)
		{
			if (InputTypes[i] != ENNETensorDataType::Float)
			{
				UE_LOG(LogNNE, Warning, TEXT("Concat operator input '%d' of type '%d' is not supported, should be float at the moment."), i, int(InputTypes[i]));
				bIsValid = false;
			}
		}

		return bIsValid;
	}

	FOperatorHlsl* CreateConcatOperator()
	{
		return new FConcat();
	}

	bool RegisterConcatOperator(FOperatorRegistryHlsl& Registry)
	{
		Registry.OpAdd(TEXT("Concat"), CreateConcatOperator, ValidateConcatOperator);
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
