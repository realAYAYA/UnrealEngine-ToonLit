// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGHelperElementWiseUnary.h"
#include "NNECoreTensor.h"
#include "Math/UnrealMathUtility.h"

namespace UE::NNERuntimeRDG::Internal::CPUHelper::ElementWiseUnary
{
	template<NNECore::Internal::EElementWiseUnaryOperatorType OpType> float Apply(float X, float Alpha, float Beta, float Gamma);
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Abs>(float X, float Alpha, float Beta, float Gamma) { return FMath::Abs(X); }

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Acos>(float X, float Alpha, float Beta, float Gamma) { return FMath::Acos(X); }
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Acosh>(float X, float Alpha, float Beta, float Gamma) {
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
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Asin>(float X, float Alpha, float Beta, float Gamma) { return FMath::Asin(X); }
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Asinh>(float X, float Alpha, float Beta, float Gamma) {
		//https://mathworld.wolfram.com/InverseHyperbolicSine.html
		return FMath::Loge(X + FMath::Sqrt(1 + (X * X)));
	}
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Atan>(float X, float Alpha, float Beta, float Gamma) { return FMath::Atan(X); }
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Atanh>(float X, float Alpha, float Beta, float Gamma) {
		//https://mathworld.wolfram.com/InverseHyperbolicTangent.html
		return 0.5f * (FMath::Loge(1 + X) - FMath::Loge(1 - X));
	}

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Ceil>(float X, float Alpha, float Beta, float Gamma) { return FMath::CeilToFloat(X); }

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Clip>(float X, float Alpha, float Beta, float Gamma) { return FMath::Clamp(X, Alpha, Beta); }
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Cos>(float X, float Alpha, float Beta, float Gamma) { return FMath::Cos(X); }
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Cosh>(float X, float Alpha, float Beta, float Gamma) {
		//https://mathworld.wolfram.com/HyperbolicCosine.html
		return 0.5f * (FMath::Exp(X) + FMath::Exp(-X));
	}
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Elu>(float X, float Alpha, float Beta, float Gamma) {
		//https://github.com/onnx/onnx/blob/main/docs/Operators.md#elu
		float yNeg = Alpha * (FMath::Exp(X) - 1.0f);
		float yPosOrZero = X;
		return (X >= 0.0f) ? yPosOrZero : yNeg;
	}
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Exp>(float X, float Alpha, float Beta, float Gamma) { return FMath::Exp(X); }

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Floor>(float X, float Alpha, float Beta, float Gamma) { return FMath::Floor(X); }

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::IsInf>(float X, float Alpha, float Beta, float Gamma) { return !FMath::IsFinite(X); }

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::IsNan>(float X, float Alpha, float Beta, float Gamma) { return FMath::IsNaN(X); }

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::HardSigmoid>(float X, float Alpha, float Beta, float Gamma) {
		//https://github.com/onnx/onnx/blob/main/docs/Operators.md#hardSigmoid
		return FMath::Max(0.0f, FMath::Min(1.0f, Alpha * X + Beta));
	}

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::HardSwish>(float X, float Alpha, float Beta, float Gamma) {
		//https://github.com/onnx/onnx/blob/main/docs/Operators.md#hardSwish
		return Apply<NNECore::Internal::EElementWiseUnaryOperatorType::HardSigmoid>(X, 1.0f / 6.0f, 0.5f, Gamma);
	}

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::LeakyRelu>(float X, float Alpha, float Beta, float Gamma) { return (X >= 0.0f) ? X : Alpha * X; }

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Log>(float X, float Alpha, float Beta, float Gamma) { return FMath::Loge(X); }

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Neg>(float X, float Alpha, float Beta, float Gamma) { return -X; }

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Reciprocal>(float X, float Alpha, float Beta, float Gamma) { return 1.0f/X; }

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Relu>(float X, float Alpha, float Beta, float Gamma) { return FMath::Max(X, 0.0f); }

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Round>(float X, float Alpha, float Beta, float Gamma) { return FMath::RoundToFloat(X); }

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Selu>(float X, float Alpha, float Beta, float Gamma) {
		//https://github.com/onnx/onnx/blob/main/docs/Operators.md#Selu
		float yNegOrZero = Gamma * (Alpha * FMath::Exp(X) - Alpha);
		float yPos = Gamma * X;
		return (X > 0.0f) ? yPos : yNegOrZero;
	}
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Sigmoid>(float X, float Alpha, float Beta, float Gamma) { return 1.0f / (1.0f + FMath::Exp(-X)); }
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Sign>(float X, float Alpha, float Beta, float Gamma) { return FMath::Sign(X); }
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Sin>(float X, float Alpha, float Beta, float Gamma) { return FMath::Sin(X); }
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Sinh>(float X, float Alpha, float Beta, float Gamma) { return FMath::Sinh(X); }
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Softplus>(float X, float Alpha, float Beta, float Gamma) { return FMath::Loge(FMath::Exp(X) + 1.0f); }
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Softsign>(float X, float Alpha, float Beta, float Gamma) { return X / (1.0f + FMath::Abs(X)); }
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Sqrt>(float X, float Alpha, float Beta, float Gamma) { return FMath::Sqrt(X); }
	
	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Tan>(float X, float Alpha, float Beta, float Gamma) { return FMath::Tan(X); }

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Tanh>(float X, float Alpha, float Beta, float Gamma) {
		//https://mathworld.wolfram.com/HyperbolicTangent.html
		float SinhValue = Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Sinh>(X, Alpha, Beta, Gamma);
		float CoshValue = Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Cosh>(X, Alpha, Beta, Gamma);
		return SinhValue / CoshValue;
	}

	template<> float Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Erf>(float X, float Alpha, float Beta, float Gamma) {
		//https://aapt.scitation.org/doi/abs/10.1119/1.15018?journalCode=ajp
		float a = 167.0f / 148.0f;
		float b = 11.0f / 109.0f;
		float x3 = X * X * X;
		return Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Tanh>(a * X + b * x3, Alpha, Beta, Gamma);
	}

	template<NNECore::Internal::EElementWiseUnaryOperatorType OpType> void Apply(const NNECore::Internal::FTensor& Tensor, float Alpha, float Beta, float Gamma, NNECore::Internal::FTensor& OutputTensor)
	{
		//Heuristic to avoid unexpected performance hit. This helper being intended for shape related arithmetic only.
		static constexpr int32 MaxItemInInputTensors = NNECore::FTensorShape::MaxRank * 2;

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

	void Apply(NNECore::Internal::EElementWiseUnaryOperatorType OpType, const NNECore::Internal::FTensor& Tensor, float Alpha, float Beta, float Gamma, NNECore::Internal::FTensor& OutputTensor)
	{
		switch (OpType)
		{
		case NNECore::Internal::EElementWiseUnaryOperatorType::Abs:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Abs>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Acos:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Acos>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Acosh:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Acosh>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Asin:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Asin>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Asinh:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Asinh>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Atan:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Atan>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Atanh:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Atanh>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Ceil:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Ceil>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Clip:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Clip>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Cos:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Cos>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Cosh:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Cosh>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Elu:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Elu>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Erf:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Erf>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Exp:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Exp>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Floor:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Floor>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::IsInf:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::IsInf>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::IsNan:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::IsNan>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::HardSigmoid:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::HardSigmoid>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::HardSwish:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::HardSwish>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::LeakyRelu:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::LeakyRelu>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Log:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Log>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Neg:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Neg>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Reciprocal:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Reciprocal>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Relu:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Relu>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Round:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Round>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Selu:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Selu>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Sigmoid:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Sigmoid>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Sign:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Sign>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Sin:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Sin>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Sinh:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Sinh>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Softplus:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Softplus>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Softsign:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Softsign>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Sqrt:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Sqrt>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Tan:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Tan>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		case NNECore::Internal::EElementWiseUnaryOperatorType::Tanh:
			Apply<NNECore::Internal::EElementWiseUnaryOperatorType::Tanh>(Tensor, Alpha, Beta, Gamma, OutputTensor);
			break;
		default:
			break;
		}
	}
	
} // UE::NNERuntimeRDG::Internal::CPUHelper::ElementWiseUnary
