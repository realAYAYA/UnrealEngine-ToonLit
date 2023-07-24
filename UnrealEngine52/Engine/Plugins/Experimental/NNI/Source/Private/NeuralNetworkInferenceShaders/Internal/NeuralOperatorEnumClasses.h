// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EBatchNormalizationMode : uint8
{
	OneDimension,
	NDimensions,
	MAX /** Needed for shader (for SHADER_PERMUTATION_ENUM_CLASS) */
};

enum class EConvMode : uint8
{
	D1, /** 1D convolution */
	D2, /** 2D convolution */
	D3, /** 3D convolution */
	DN, /** nD convolution */
	MAX /** Needed for shader (for SHADER_PERMUTATION_ENUM_CLASS) */
};

enum class EGemmMode : uint8
{
	CTensor,
	CScalar,
	MAX /** Needed for shader (for SHADER_PERMUTATION_ENUM_CLASS) */
};

enum class EElementWiseOperator : uint8
{
	Abs, Acos, Asin, Atan, Ceil, Cos, Cosh, Exp, Floor, LeakyRelu, Log, Neg, Reciprocal, Round, Relu, Sigmoid, Sign, Sin, Sinh, Sqrt, Tan, Tanh,
	MAX /** Needed for shader (for SHADER_PERMUTATION_ENUM_CLASS) */
};

enum class EMultidirectionalBroadcastOperator : uint8
{
	Add,
	Div,
	Mul,
	Pow,
	Sub,
	MAX /** Needed for shader (for SHADER_PERMUTATION_ENUM_CLASS) */
};

enum class EMultidirectionalBroadcastInlinedMode : uint8
{
	A,
	B,
	None,
	MAX /** Needed for shader (for SHADER_PERMUTATION_ENUM_CLASS) */
};

enum class EMultidirectionalBroadcastShapeMode : uint8
{
	ElementWise,
	AScalar,
	BScalar,
	MultidirectionalBroadcast,
	MAX /** Needed for shader (for SHADER_PERMUTATION_ENUM_CLASS) */
};
