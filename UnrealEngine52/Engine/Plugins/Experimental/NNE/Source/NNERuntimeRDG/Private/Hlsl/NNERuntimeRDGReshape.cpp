// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGReshape.h"
#include "NNECoreTensor.h"
#include "NNECoreTypes.h"
#include "RenderGraphUtils.h"
#include "Helper/NNERuntimeRDGHelperReshape.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorReshape, TEXT("NNE.Operator.Hlsl.Reshape"));

	/**
	 * Reshape operator implementation
	 */
	class FReshape : public FOperatorHlsl
	{
	public:

		FReshape() {}
		virtual ~FReshape() = default;

		bool bAllowZero = false;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNECore::Internal::FTensorRef> InputTensors, TArrayView<NNECore::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() == 2);
			check(OutputTensors.Num() == 1);

			const NNECore::Internal::FTensor& X = *InputTensors[0];
			const NNECore::Internal::FTensor& ShapeTensor = *InputTensors[1];
			TArray<uint32> OutputShapeData;

			check(ShapeTensor.GetDataType() == ENNETensorDataType::Int64);

			if (!ShapeTensor.HasPreparedData())
			{
				UE_LOG(LogNNE, Warning, TEXT("Reshape input 'Shape' (name: %s) should be constant for shape inference to succeed, however it is not."), *ShapeTensor.GetName());
				return -1;
			}

			if (!ShapeHelper::Reshape::ReshapeTensor<int64>(X.GetShape(), ShapeTensor, bAllowZero, OutputShapeData))
			{
				return -1;
			}

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
			check(InputTensorDescs.Num() == 2);
			check(OutputTensorDescs.Num() == 1);

			bAllowZero = (bool) (Attributes.GetValueOrDefault<int32>(TEXT("allowzero"), 0));
			
			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			check(InputTensors.Num() == 2);
			check(OutputTensors.Num() == 1);
			check(InputTensors[0] != nullptr);
			check(OutputTensors[0] != nullptr);

			const FTensorRDG& Data = *InputTensors[0];
			const FTensorRDG& Output = *OutputTensors[0];

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.Reshape");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorReshape);

			AddCopyBufferPass(GraphBuilder, Output.GetBuffer(), Data.GetBuffer());
		}
	};

	bool ValidateReshapeOperator(const NNECore::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNECore::FSymbolicTensorShape> InputReshapes)
	{
		bool bIsValid = true;

		//This match version 14 of the Reshape operator
		//https://github.com/onnx/onnx/blob/main/docs/Operators.md#Reshape
		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("allowzero"), ENNEAttributeDataType::Int32);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.SetTemplateCount(2);
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

		InputValidator.AddSupportedType(ENNETensorDataType::Int64, 1);
		InputValidator.AddRequired(1);

		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	FOperatorHlsl* CreateReshapeOperator()
	{
		return new FReshape();
	}

	bool RegisterReshapeOperator(FOperatorRegistryHlsl& Registry)
	{
		Registry.OpAdd(TEXT("Reshape"), CreateReshapeOperator, ValidateReshapeOperator);
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
