// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGElementWiseUnary.h"
#include "NNERuntimeRDGHelperElementWiseUnary.h"
#include "NNEHlslShadersElementWiseUnaryCS.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "NNEAttributeMap.h"
#include "NNETypes.h"
#include "NNETensor.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorElementWiseUnary, TEXT("NNE.Operator.Hlsl.ElementWise.Unary"));

	using TElementWiseUnaryCS = typename UE::NNEHlslShaders::Internal::TElementWiseUnaryCS;
	using FElementWiseUnaryConstants = UE::NNEHlslShaders::Internal::FElementWiseUnaryConstants;

	/**
	 * Unary element-wise operator implementation
	 */
	template<NNE::Internal::EElementWiseUnaryOperatorType OpType>
	class TElementWiseUnary : public FOperatorHlsl
	{
	public:

		TElementWiseUnary() {}
		virtual ~TElementWiseUnary() = default;

	private:

		float Alpha = 0.0f;
		float Beta = 0.0f;
		float Gamma = 0.0f;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
		{
			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);
			OutputTensors[0]->SetShape(InputTensors[0]->GetShape());

			const NNE::Internal::FTensor& X = *InputTensors[0];

			Internal::CPUHelper::ElementWiseUnary::Apply(OpType, X, Alpha, Beta, Gamma, *OutputTensors[0]);
			
			return 0;
		}

		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() == 1);
			check(OutputTensorDescs.Num() == 1);

			Alpha = Attributes.GetValueOrDefault(TEXT("alpha"), Alpha);
			Beta = Attributes.GetValueOrDefault(TEXT("beta"), Beta);
			Gamma = Attributes.GetValueOrDefault(TEXT("gamma"), Gamma);
			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InInputTensors, TConstArrayView<FTensorRDGRef> InOutputTensors) override
		{
			check(InInputTensors[0] != nullptr);
			check(InOutputTensors[0] != nullptr);
			FRDGBufferSRVRef InputSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(InInputTensors[0]->GetBuffer(), PF_R32_FLOAT));
			FRDGBufferUAVRef OutputUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(InOutputTensors[0]->GetBuffer(), PF_R32_FLOAT));
		
			int32 NumElements = InOutputTensors[0]->GetVolume();
			FIntVector ThreadGroupCount = ComputeElementWiseThreadGroups(NumElements, FElementWiseUnaryConstants::NUM_GROUP_THREADS);

			// Set parameters
			TElementWiseUnaryCS::FParameters* Params = GraphBuilder.AllocParameters<TElementWiseUnaryCS::FParameters>();
			Params->Input = InputSRV;
			Params->Output = OutputUAV;
			Params->Alpha = Alpha;
			Params->Beta = Beta;
			Params->Gamma = Gamma;
			Params->Num = NumElements;
			Params->ThreadCountX = ThreadGroupCount.X * FElementWiseUnaryConstants::NUM_GROUP_THREADS;

			TElementWiseUnaryCS::FPermutationDomain PermutationVector;

			PermutationVector.Set<TElementWiseUnaryCS::FOperatorType>(OpType);

			TShaderMapRef<TElementWiseUnaryCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.ElementWise.Unary");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorElementWiseUnary);
		
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NNE.Operator.Hlsl.ElementWise.Unary.Dispatch"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				Params,
				ThreadGroupCount);
		}
	};

	template<> TElementWiseUnary<NNE::Internal::EElementWiseUnaryOperatorType::Selu>::TElementWiseUnary()
		: Alpha(1.67326319217681884765625f), Beta(0.0f), Gamma(1.05070102214813232421875f)
	{
	}

	template<> TElementWiseUnary<NNE::Internal::EElementWiseUnaryOperatorType::Elu>::TElementWiseUnary()
		: Alpha(1.0f), Beta(0.0f), Gamma(0.0f) 
	{
	}

	template<> TElementWiseUnary<NNE::Internal::EElementWiseUnaryOperatorType::HardSigmoid>::TElementWiseUnary()
		: Alpha(0.2f), Beta(0.5f), Gamma(0.0f)
	{
	}

	template<> TElementWiseUnary<NNE::Internal::EElementWiseUnaryOperatorType::LeakyRelu>::TElementWiseUnary()
		: Alpha(0.01f), Beta(0.0f), Gamma(0.0f)
	{
	}

	template<> bool TElementWiseUnary<NNE::Internal::EElementWiseUnaryOperatorType::Clip>::Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes)
	{
		check(InputTensorDescs.Num() == 1);
		check(OutputTensorDescs.Num() == 1);

		Alpha = Attributes.GetValueOrDefault(TEXT("min"), -3.402823e+38f);
		Beta = Attributes.GetValueOrDefault(TEXT("max"), 3.402823e+38f);
		return true;
	}

	template<NNE::Internal::EElementWiseUnaryOperatorType OpType>
	FOperatorHlsl* CreateElementWiseUnaryOperator()
	{
		return new TElementWiseUnary<OpType>();
	}

	template<NNE::Internal::EElementWiseUnaryOperatorType OpType>
	bool ValidateElementWiseUnaryOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	template<>
	bool ValidateElementWiseUnaryOperator<NNE::Internal::EElementWiseUnaryOperatorType::Selu>(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("alpha"), ENNEAttributeDataType::Float);
		AttributeValidator.AddOptional(TEXT("gamma"), ENNEAttributeDataType::Float);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	template<>
	bool ValidateElementWiseUnaryOperator<NNE::Internal::EElementWiseUnaryOperatorType::Elu>(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("alpha"), ENNEAttributeDataType::Float);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	template<>
	bool ValidateElementWiseUnaryOperator<NNE::Internal::EElementWiseUnaryOperatorType::HardSigmoid>(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("alpha"), ENNEAttributeDataType::Float);
		AttributeValidator.AddOptional(TEXT("beta"), ENNEAttributeDataType::Float);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	template<>
	bool ValidateElementWiseUnaryOperator<NNE::Internal::EElementWiseUnaryOperatorType::LeakyRelu>(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("alpha"), ENNEAttributeDataType::Float);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	template<>
	bool ValidateElementWiseUnaryOperator<NNE::Internal::EElementWiseUnaryOperatorType::Clip>(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("min"), ENNEAttributeDataType::Float);
		AttributeValidator.AddOptional(TEXT("max"), ENNEAttributeDataType::Float);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}
	
	bool RegisterElementWiseUnaryOperators(FOperatorRegistryHlsl& Registry)
	{
		// Note: support of a particular version is partial with respect to tensor data types (only the most typical ones are usually supported).
#define OP(Name, Version) Registry.OpAdd({{TEXT(#Name), TEXT("Onnx")}, Version}, CreateElementWiseUnaryOperator<NNE::Internal::EElementWiseUnaryOperatorType::Name>, ValidateElementWiseUnaryOperator<NNE::Internal::EElementWiseUnaryOperatorType::Name>);
		OP(Abs, 6)
		OP(Abs, 13)
		OP(Acos, 7)
		OP(Acosh, 9)
		OP(Asin, 7)
		OP(Asinh, 9)
		OP(Atan, 7)
		OP(Atanh, 9)
		//OP(BitShift, 11)
		OP(Ceil, 6)
		OP(Ceil, 13)
		OP(Clip, 6)
		OP(Cos, 7)
		OP(Cosh, 9)
		OP(Elu, 6)
		OP(Erf, 9)
		OP(Erf, 13)
		OP(Exp, 6)
		OP(Exp, 13)
		OP(Floor, 6)
		OP(Floor, 13)
		OP(IsInf, 10)
		OP(IsInf, 20)
		OP(IsNan, 9)
		OP(IsNan, 13)
		OP(IsNan, 20)
		OP(HardSigmoid, 6)
		OP(HardSwish, 14)
		OP(LeakyRelu, 6)
		OP(LeakyRelu, 16)
		OP(Log, 6)
		OP(Log, 13)
		OP(Neg, 6)
		OP(Neg, 13)
		//OP(Not, 1)
		OP(Reciprocal, 6)
		OP(Reciprocal, 13)
		OP(Relu, 6)
		OP(Relu, 13)
		OP(Relu, 14)
		OP(Round, 11)
		OP(Selu, 6)
		OP(Sigmoid, 6)
		OP(Sigmoid, 13)
		OP(Sign, 9)
		OP(Sign, 13)
		OP(Sin, 7)
		OP(Sinh, 9)
		OP(Softplus, 1)
		OP(Softsign, 1)
		OP(Sqrt, 6)
		OP(Sqrt, 13)
		OP(Tan, 7)
		OP(Tanh, 6)
		OP(Tanh, 13)
#undef OP

		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
