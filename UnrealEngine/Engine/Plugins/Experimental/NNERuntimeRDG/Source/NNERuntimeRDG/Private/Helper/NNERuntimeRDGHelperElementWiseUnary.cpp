// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGHelperElementWiseUnary.h"
#include "NNETensor.h"
#include "Math/UnrealMathUtility.h"

namespace UE::NNERuntimeRDG::Internal::CPUHelper::ElementWiseUnary
{
	template<NNE::Internal::EElementWiseUnaryOperatorType OpType> float Apply(float X, float Alpha, float Beta, float Gamma);
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Abs>(float X, float Alpha, float Beta, float Gamma) { return FMath::Abs(X); }

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Acos>(float X, float Alpha, float Beta, float Gamma) { return FMath::Acos(X); }
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Acosh>(float X, float Alpha, float Beta, float Gamma) {
		//https://mathworld.wolfram.com/InverseHyperbolicCosine.html
		float FloatNan = FMath::Sqrt(-1.0f);
		float yAboveOne = FMath::Loge(X + FMath::Sqrt(X + 1.0f) * FMath::Sqrt(X - 1.0f));
		if (X == 1.0f)
		{
			return 0.0f;
		}
		else
		{
			return (X >= 1.0f) ? yAboveOne : FloatNan;
		}
	}
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Asin>(float X, float Alpha, float Beta, float Gamma) { return FMath::Asin(X); }
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Asinh>(float X, float Alpha, float Beta, float Gamma) {
		//https://mathworld.wolfram.com/InverseHyperbolicSine.html
		return FMath::Loge(X + FMath::Sqrt(1 + (X * X)));
	}
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Atan>(float X, float Alpha, float Beta, float Gamma) { return FMath::Atan(X); }
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Atanh>(float X, float Alpha, float Beta, float Gamma) {
		//https://mathworld.wolfram.com/InverseHyperbolicTangent.html
		return 0.5f * (FMath::Loge(1 + X) - FMath::Loge(1 - X));
	}

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Ceil>(float X, float Alpha, float Beta, float Gamma) { return FMath::CeilToFloat(X); }

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Clip>(float X, float Alpha, float Beta, float Gamma) { return FMath::Clamp(X, Alpha, Beta); }
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Cos>(float X, float Alpha, float Beta, float Gamma) { return FMath::Cos(X); }
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Cosh>(float X, float Alpha, float Beta, float Gamma) {
		//https://mathworld.wolfram.com/HyperbolicCosine.html
		return 0.5f * (FMath::Exp(X) + FMath::Exp(-X));
	}
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Elu>(float X, float Alpha, float Beta, float Gamma) {
		//https://github.com/onnx/onnx/blob/main/docs/Operators.md#elu
		float yNeg = Alpha * (FMath::Exp(X) - 1.0f);
		float yPosOrZero = X;
		return (X >= 0.0f) ? yPosOrZero : yNeg;
	}
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Exp>(float X, float Alpha, float Beta, float Gamma) { return FMath::Exp(X); }

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Floor>(float X, float Alpha, float Beta, float Gamma) { return FMath::Floor(X); }

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::IsInf>(float X, float Alpha, float Beta, float Gamma) { return !FMath::IsFinite(X); }

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::IsNan>(float X, float Alpha, float Beta, float Gamma) { return FMath::IsNaN(X); }

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::HardSigmoid>(float X, float Alpha, float Beta, float Gamma) {
		//https://github.com/onnx/onnx/blob/main/docs/Operators.md#hardSigmoid
		return FMath::Max(0.0f, FMath::Min(1.0f, Alpha * X + Beta));
	}

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::HardSwish>(float X, float Alpha, float Beta, float Gamma) {
		//https://github.com/onnx/onnx/blob/main/docs/Operators.md#hardSwish
		return Apply<NNE::Internal::EElementWiseUnaryOperatorType::HardSigmoid>(X, 1.0f / 6.0f, 0.5f, Gamma);
	}

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::LeakyRelu>(float X, float Alpha, float Beta, float Gamma) { return (X >= 0.0f) ? X : Alpha * X; }

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Log>(float X, float Alpha, float Beta, float Gamma) { return FMath::Loge(X); }

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Neg>(float X, float Alpha, float Beta, float Gamma) { return -X; }

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Reciprocal>(float X, float Alpha, float Beta, float Gamma) { return 1.0f/X; }

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Relu>(float X, float Alpha, float Beta, float Gamma) { return FMath::Max(X, 0.0f); }

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Round>(float X, float Alpha, float Beta, float Gamma) { return FMath::RoundToFloat(X); }

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Selu>(float X, float Alpha, float Beta, float Gamma) {
		//https://github.com/onnx/onnx/blob/main/docs/Operators.md#Selu
		float yNegOrZero = Gamma * (Alpha * FMath::Exp(X) - Alpha);
		float yPos = Gamma * X;
		return (X > 0.0f) ? yPos : yNegOrZero;
	}
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Sigmoid>(float X, float Alpha, float Beta, float Gamma) { return 1.0f / (1.0f + FMath::Exp(-X)); }
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Sign>(float X, float Alpha, float Beta, float Gamma) { return FMath::Sign(X); }
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Sin>(float X, float Alpha, float Beta, float Gamma) { return FMath::Sin(X); }
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Sinh>(float X, float Alpha, float Beta, float Gamma) { return FMath::Sinh(X); }
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Softplus>(float X, float Alpha, float Beta, float Gamma) { return FMath::Loge(FMath::Exp(X) + 1.0f); }
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Softsign>(float X, float Alpha, float Beta, float Gamma) { return X / (1.0f + FMath::Abs(X)); }
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Sqrt>(float X, float Alpha, float Beta, float Gamma) { return FMath::Sqrt(X); }
	
	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Tan>(float X, float Alpha, float Beta, float Gamma) { return FMath::Tan(X); }

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Tanh>(float X, float Alpha, float Beta, float Gamma) {
		//https://mathworld.wolfram.com/HyperbolicTangent.html
		float SinhValue = Apply<NNE::Internal::EElementWiseUnaryOperatorType::Sinh>(X, Alpha, Beta, Gamma);
		float CoshValue = Apply<NNE::Internal::EElementWiseUnaryOperatorType::Cosh>(X, Alpha, Beta, Gamma);
		return SinhValue / CoshValue;
	}

	template<> float Apply<NNE::Internal::EElementWiseUnaryOperatorType::Erf>(float X, float Alpha, float Beta, float Gamma) {
		//https://aapt.scitation.org/doi/abs/10.1119/1.15018?journalCode=ajp
		float a = 167.0f / 148.0f;
		float b = 11.0f / 109.0f;
		float x3 = X * X * X;
		return Apply<NNE::Internal::EElementWiseUnaryOperatorType::Tanh>(a * X + b * x3, Alpha, Beta, Gamma);
	}

	template<NNE::Internal::EElementWiseUnaryOperatorType OpType> void Apply(const NNE::Internal::FTensor& Tensor, float Alpha, float Beta, float Gamma, NNE::Internal::FTensor& OutputTensor)
	{
		//Heuristic to avoid unexpected performance hit. This helper being intended for shape related arithmetic only.
		static constexpr int32 MaxItemInInputTensors = NNE::FTensorShape::MaxRank * 2;

		if (Tensor.HasPreparedData() && (Tensor.GetVolume() <= MaxItemInInputTensors))
		{
			TConstArrayView<float> TensorData = Tensor.GetPreparedData<float>();
			TArray<float> OutputData;
			OutputData.Reserve(TensorData.Num());
			for (float elem : TensorData)
			{
				OutputData.Add(Apply<OpType>(elem, Alpha, Beta, Gamma));
			}
			OutputTensor.SetPreparedData<float>(OutputData);
		}
	}

	void Apply(NNE::Internal::EElementWiseUnaryOperatorType OpType, const NNE::Internal::FTensor& Tensor, float Alpha, float Beta, float Gamma, NNE::Internal::FTensor& OutputTensor)
	{
		switch (OpType)
		{
		case NNE::Internal::EElementWiseUnaryOperatorType::Abs:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Abs>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Acos:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Acos>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Acosh:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Acosh>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Asin:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Asin>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Asinh:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Asinh>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Atan:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Atan>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Atanh:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Atanh>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Ceil:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Ceil>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Clip:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Clip>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Cos:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Cos>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Cosh:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Cosh>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Elu:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Elu>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Erf:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Erf>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Exp:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Exp>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Floor:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Floor>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::IsInf:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::IsInf>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::IsNan:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::IsNan>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::HardSigmoid:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::HardSigmoid>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::HardSwish:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::HardSwish>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::LeakyRelu:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::LeakyRelu>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Log:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Log>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Neg:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Neg>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Reciprocal:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Reciprocal>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Relu:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Relu>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Round:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Round>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Selu:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Selu>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Sigmoid:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Sigmoid>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Sign:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Sign>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Sin:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Sin>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Sinh:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Sinh>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Softplus:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Softplus>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Softsign:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Softsign>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Sqrt:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Sqrt>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Tan:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Tan>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNE::Internal::EElementWiseUnaryOperatorType::Tanh:
			Apply<NNE::Internal::EElementWiseUnaryOperatorType::Tanh>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		default:
			break;
		}
	}
	
} // UE::NNERuntimeRDG::Internal::CPUHelper::ElementWiseUnary
