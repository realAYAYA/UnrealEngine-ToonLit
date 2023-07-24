// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGElementWiseUnary.h"
#include "NNERuntimeRDGHelperElementWiseUnary.h"
#include "NNEHlslShadersElementWiseUnaryCS.h"
#include "NNERuntimeRDGHlslHelper.h"
#include "NNECoreAttributeMap.h"
#include "NNECoreTypes.h"
#include "NNECoreTensor.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorElementWiseUnary, TEXT("NNE.Operator.Hlsl.ElementWise.Unary"));

	using TElementWiseUnaryCS = typename UE::NNEHlslShaders::Internal::TElementWiseUnaryCS;
	using FElementWiseUnaryConstants = UE::NNEHlslShaders::Internal::FElementWiseUnaryConstants;

	/**
	 * Unary element-wise operator implementation
	 */
	template<NNECore::Internal::EElementWiseUnaryOperatorType OpType>
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

		virtual int PrepareOutputs(TConstArrayView<NNECore::Internal::FTensorRef> InputTensors, TArrayView<NNECore::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);
			OutputTensors[0]->SetShape(InputTensors[0]->GetShape());

			const NNECore::Internal::FTensor& X = *InputTensors[0];

			Internal::CPUHelper::ElementWiseUnary::Apply(OpType, X, Alpha, Beta, Gamma, *OutputTensors[0]);
			
			return 0;
		}

		virtual bool Initialize(TConstArrayView<NNECore::FTensorDesc> InputTensorDescs, TConstArrayView<NNECore::FTensorDesc> OutputTensorDescs, const NNECore::FAttributeMap& Attributes) override
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

	template<> TElementWiseUnary<NNECore::Internal::EElementWiseUnaryOperatorType::Selu>::TElementWiseUnary()
		: Alpha(1.67326319217681884765625f), Beta(0.0f), Gamma(1.05070102214813232421875f)
	{
	}

	template<> TElementWiseUnary<NNECore::Internal::EElementWiseUnaryOperatorType::Elu>::TElementWiseUnary()
		: Alpha(1.0f), Beta(0.0f), Gamma(0.0f) 
	{
	}

	template<> TElementWiseUnary<NNECore::Internal::EElementWiseUnaryOperatorType::HardSigmoid>::TElementWiseUnary()
		: Alpha(0.2f), Beta(0.5f), Gamma(0.0f)
	{
	}

	template<> TElementWiseUnary<NNECore::Internal::EElementWiseUnaryOperatorType::LeakyRelu>::TElementWiseUnary()
		: Alpha(0.01f), Beta(0.0f), Gamma(0.0f)
	{
	}

	template<> bool TElementWiseUnary<NNECore::Internal::EElementWiseUnaryOperatorType::Clip>::Initialize(TConstArrayView<NNECore::FTensorDesc> InputTensorDescs, TConstArrayView<NNECore::FTensorDesc> OutputTensorDescs, const NNECore::FAttributeMap& Attributes)
	{
		check(InputTensorDescs.Num() == 1);
		check(OutputTensorDescs.Num() == 1);

		Alpha = Attributes.GetValueOrDefault(TEXT("min"), -3.402823e+38f);
		Beta = Attributes.GetValueOrDefault(TEXT("max"), 3.402823e+38f);
		return true;
	}

	template<NNECore::Internal::EElementWiseUnaryOperatorType OpType>
	FOperatorHlsl* CreateElementWiseUnaryOperator()
	{
		return new TElementWiseUnary<OpType>();
	}

	template<NNECore::Internal::EElementWiseUnaryOperatorType OpType>
	bool ValidateElementWiseUnaryOperator(const NNECore::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNECore::FSymbolicTensorShape> InputShapes)
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
	bool ValidateElementWiseUnaryOperator<NNECore::Internal::EElementWiseUnaryOperatorType::Selu>(const NNECore::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNECore::FSymbolicTensorShape> InputShapes)
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
	bool ValidateElementWiseUnaryOperator<NNECore::Internal::EElementWiseUnaryOperatorType::Elu>(const NNECore::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNECore::FSymbolicTensorShape> InputShapes)
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
	bool ValidateElementWiseUnaryOperator<NNECore::Internal::EElementWiseUnaryOperatorType::HardSigmoid>(const NNECore::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNECore::FSymbolicTensorShape> InputShapes)
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
	bool ValidateElementWiseUnaryOperator<NNECore::Internal::EElementWiseUnaryOperatorType::LeakyRelu>(const NNECore::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNECore::FSymbolicTensorShape> InputShapes)
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
	bool ValidateElementWiseUnaryOperator<NNECore::Internal::EElementWiseUnaryOperatorType::Clip>(const NNECore::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNECore::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		//version 6 of operator Clip, next version is 11
		//https://github.com/onnx/onnx/blob/main/docs/Changelog.md#Clip-6
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
#define OP(Name) Registry.OpAdd(TEXT(#Name), CreateElementWiseUnaryOperator<NNECore::Internal::EElementWiseUnaryOperatorType::Name>, ValidateElementWiseUnaryOperator<NNECore::Internal::EElementWiseUnaryOperatorType::Name>)
		OP(Abs);
		OP(Acos);
		OP(Acosh);
		OP(Asin);
		OP(Asinh);
		OP(Atan);
		OP(Atanh);
		//OP(BitShift);
		OP(Ceil);
		OP(Clip);
		OP(Cos);
		OP(Cosh);
		OP(Elu);
		OP(Erf);
		OP(Exp);
		OP(Floor);
		OP(IsInf);
		OP(IsNan);
		OP(HardSigmoid);
		OP(HardSwish);
		OP(LeakyRelu);
		OP(Log);
		OP(Neg);
		//OP(Not);
		OP(Reciprocal);
		OP(Relu);
		OP(Round);
		OP(Selu);
		OP(Sigmoid);
		OP(Sign);
		OP(Sin);
		OP(Sinh);
		OP(Softplus);
		OP(Softsign);
		OP(Sqrt);
		OP(Tan);
		OP(Tanh);
#undef OP

		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
