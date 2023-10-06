// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGElementWiseBinary.h"
#include "NNERuntimeRDGHelperElementWiseBinary.h"
#include "NNEHlslShadersElementWiseBinaryCS.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "NNETensor.h"
#include "NNETypes.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorElementWiseBinary, TEXT("NNE.Operator.Hlsl.ElementWise.Binary"));

	using TElementWiseBinaryCS = typename UE::NNEHlslShaders::Internal::TElementWiseBinaryCS;
	using FElementWiseBinaryConstants = UE::NNEHlslShaders::Internal::FElementWiseBinaryConstants;

	/**
	 * Binary element-wise operator implementation
	 */
	template<NNE::Internal::EElementWiseBinaryOperatorType OpType>
	class TElementWiseBinary : public FOperatorHlsl
	{
	public:

		TElementWiseBinary() {}
		virtual ~TElementWiseBinary() = default;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() == 2);
			check(OutputTensors.Num() == 1);
			
			const NNE::FTensorShape& LHSInput = InputTensors[0]->GetShape();
			const NNE::FTensorShape& RHSInput = InputTensors[1]->GetShape();
			const int32 OutputRank = FMath::Max(LHSInput.Rank(), RHSInput.Rank());
			TArray<uint32> OutputShapeData;
			
			OutputShapeData.SetNumUninitialized(OutputRank);
			
			for (int32 i = 0; i < OutputRank; ++i)
			{
				int32 LHSIndex = LHSInput.Rank() - 1 - i;
				int32 RHSIndex = RHSInput.Rank() - 1 - i;
				int32 LHSValue = LHSIndex >= 0 ? LHSInput.GetData()[LHSIndex] : 1;
				int32 RHSValue = RHSIndex >= 0 ? RHSInput.GetData()[RHSIndex] : 1;
				if (LHSValue != RHSValue && LHSValue != 1 && RHSValue != 1)
				{
					UE_LOG(LogNNE, Warning, TEXT("Error while computing shape for element wise binary op, input shapes are not compatible"));
					return -1;
				}
				int32 OutputValue = FMath::Max(LHSValue, RHSValue);
				OutputShapeData[OutputRank - 1 - i] = OutputValue;
			}

			NNE::FTensorShape OutputShape = NNE::FTensorShape::Make(OutputShapeData);
			
			OutputTensors[0]->SetShape(OutputShape);

			Internal::CPUHelper::ElementWiseBinary::Apply(OpType, *InputTensors[0], *InputTensors[1], *OutputTensors[0]);

			if (OutputTensors[0]->GetDataType() != ENNETensorDataType::Float && !OutputTensors[0]->HasPreparedData())
			{
				UE_LOG(LogNNE, Warning, TEXT("Error: binary element wise op output tensor could not be made constant nor it was of float type. Only floats are supported at the moment on the HLSL compute path."));
				return -1;
			}
			
			return 0;
		}
		
		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() == 2);
			check(OutputTensorDescs.Num() == 1);
			check(InputTensorDescs[0].GetDataType() == InputTensorDescs[1].GetDataType());
			check(InputTensorDescs[0].GetDataType() == OutputTensorDescs[0].GetDataType());
		
			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			check(InputTensors.Num() == 2);
			check(OutputTensors.Num() == 1);
			check(InputTensors[0] != nullptr);
			check(InputTensors[1] != nullptr);
			check(OutputTensors[0] != nullptr);
			const FTensorRDG& LHSInput = *InputTensors[0];
			const FTensorRDG& RHSInput = *InputTensors[1];
			const FTensorRDG& Output = *OutputTensors[0];

			FRDGBufferSRVRef LHSInputSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(LHSInput.GetBuffer(), PF_R32_FLOAT));
			FRDGBufferSRVRef RHSInputSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(RHSInput.GetBuffer(), PF_R32_FLOAT));
			FRDGBufferUAVRef OutputUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Output.GetBuffer(), PF_R32_FLOAT));

			FIntVector ThreadGroupCount = ComputeElementWiseThreadGroups(Output.GetVolume(), FElementWiseBinaryConstants::NUM_GROUP_THREADS);

			// Set parameters
			TElementWiseBinaryCS::FParameters* Params = GraphBuilder.AllocParameters<TElementWiseBinaryCS::FParameters>();
			Params->LHSInput = LHSInputSRV;
			Params->RHSInput = RHSInputSRV;
			Params->Output = OutputUAV;
			FillTensorStrideForBroadcastShaderParameters(LHSInput, Output.GetShape().Rank(), Params->TensorInfo, 0);
			FillTensorStrideForBroadcastShaderParameters(RHSInput, Output.GetShape().Rank(), Params->TensorInfo, 1);
			FillTensorStrideShaderParameters(Output, Params->TensorInfo, 2);
			Params->Num = Output.GetVolume();
			Params->ThreadCountX = ThreadGroupCount.X * FElementWiseBinaryConstants::NUM_GROUP_THREADS;

			TElementWiseBinaryCS::FPermutationDomain PermutationVector;

			PermutationVector.Set<TElementWiseBinaryCS::FOperatorType>(OpType);
			PermutationVector.Set<TElementWiseBinaryCS::FBinaryNumDimensions>(Output.GetShape().Rank());

			TShaderMapRef<TElementWiseBinaryCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
		
			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.ElementWise.Binary");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorElementWiseBinary);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NNE.Operator.Hlsl.ElementWise.Binary.Dispatch"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				Params,
				ThreadGroupCount);
		}
	};

	bool ValidateElementWiseBinaryOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddSupportedType(ENNETensorDataType::Int32);
		InputValidator.AddSupportedType(ENNETensorDataType::Int64);
		InputValidator.AddRequired();
		InputValidator.AddRequired();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	template<NNE::Internal::EElementWiseBinaryOperatorType OpType>
	FOperatorHlsl* CreateElementWiseBinaryOperator()
	{
		return new TElementWiseBinary<OpType>();
	}

	bool RegisterElementWiseBinaryOperators(FOperatorRegistryHlsl& Registry)
	{
#define OP(Name) Registry.OpAdd(TEXT(#Name), CreateElementWiseBinaryOperator<NNE::Internal::EElementWiseBinaryOperatorType::Name>, ValidateElementWiseBinaryOperator)
		OP(Add);
		//OP(And);
		OP(Div);
		//OP(Equal);
		//OP(Greater);
		//OP(GreaterOrEqual);
		//OP(Less);
		//OP(LessOrEqual);
		OP(Mod);
		OP(Mul);
		//OP(Or);
		OP(Prelu);
		OP(Pow);
		OP(Sub);
		//OP(Or);
#undef OP

		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
