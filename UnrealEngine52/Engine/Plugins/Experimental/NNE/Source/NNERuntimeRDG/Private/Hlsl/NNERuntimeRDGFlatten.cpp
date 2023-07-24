// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGFlatten.h"
#include "NNECoreTensor.h"
#include "NNECoreTypes.h"
#include "RenderGraphUtils.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorFlatten, TEXT("NNE.Operator.Hlsl.Flatten"));

	/**
	 * Flatten operator implementation
	 */
	class FFlatten : public FOperatorHlsl
	{
	public:

		FFlatten() {}
		virtual ~FFlatten() = default;

		int32 Axis = 1;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNECore::Internal::FTensorRef> InputTensors, TArrayView<NNECore::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);

			const NNECore::Internal::FTensor& X = *InputTensors[0];
			TArray<uint32> OutputShapeData;
			uint32 InnerDimSize = 1;

			for (int32 i = 0; i < Axis; ++i)
			{
				InnerDimSize *= X.GetShape().GetData()[i];
			}
			OutputShapeData.Add(InnerDimSize);
			OutputShapeData.Add(X.GetShape().Volume() / InnerDimSize);

			NNECore::FTensorShape OutputShape = NNECore::FTensorShape::Make(OutputShapeData);

			OutputTensors[0]->SetShape(OutputShape);
			if (X.HasPreparedData())
			{
				OutputTensors[0]->SetPreparedData<uint8>(X.GetPreparedData<uint8>());
			}
			
			return 0;
		};

		virtual bool Initialize(TConstArrayView<NNECore::FTensorDesc> InputTensorDescs, TConstArrayView<NNECore::FTensorDesc> OutputTensorDescs, const NNECore::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() == 1);
			check(OutputTensorDescs.Num() == 1);

			int32 InputRank = InputTensorDescs[0].GetShape().Rank();
			Axis = Attributes.GetValueOrDefault<int32>(TEXT("axis"), 1);

			if (Axis > InputRank || Axis < -InputRank)
			{
				UE_LOG(LogNNE, Warning, TEXT("Flatten 'Axis' attribute should be in the range [-r,r] with r being the rank of the input (name: %s) however axis is %d while rank is %d."), *InputTensorDescs[0].GetName(), Axis, InputRank);
				return false;
			}
			if (Axis < 0)
			{
				Axis = InputRank + Axis;
			}
			
			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);
			check(InputTensors[0] != nullptr);
			check(OutputTensors[0] != nullptr);

			const FTensorRDG& Data = *InputTensors[0];
			const FTensorRDG& Output = *OutputTensors[0];

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.Flatten");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorFlatten);

			AddCopyBufferPass(GraphBuilder, Output.GetBuffer(), Data.GetBuffer());
		}
	};

	bool ValidateFlattenOperator(const NNECore::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNECore::FSymbolicTensorShape> InputFlattens)
	{
		bool bIsValid = true;

		//This match version 13 of the Flatten operator
		//https://github.com/onnx/onnx/blob/main/docs/Operators.md#Flatten
		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("axis"), ENNEAttributeDataType::Int32);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Half);
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddSupportedType(ENNETensorDataType::Double);
		InputValidator.AddSupportedType(ENNETensorDataType::Int8);
		InputValidator.AddSupportedType(ENNETensorDataType::Int16);
		InputValidator.AddSupportedType(ENNETensorDataType::Int32);
		InputValidator.AddSupportedType(ENNETensorDataType::Int64);
		InputValidator.AddSupportedType(ENNETensorDataType::UInt8);
		InputValidator.AddSupportedType(ENNETensorDataType::UInt16);
		InputValidator.AddSupportedType(ENNETensorDataType::UInt32);
		InputValidator.AddSupportedType(ENNETensorDataType::UInt64);
		InputValidator.AddRequired();

		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	FOperatorHlsl* CreateFlattenOperator()
	{
		return new FFlatten();
	}

	bool RegisterFlattenOperator(FOperatorRegistryHlsl& Registry)
	{
		Registry.OpAdd(TEXT("Flatten"), CreateFlattenOperator, ValidateFlattenOperator);
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
