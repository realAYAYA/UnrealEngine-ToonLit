// Copyright Epic Games, Inc. All Rights Reserved.

export void Gemm_PRelu(
	uniform float Outputs[],
	const uniform float Inputs[],
	const uniform float Gemm_Weights[],
	const uniform float Gemm_Biases[],
	const uniform float PRelu_Alpha,
	const uniform int NumInputs,
	const uniform int NumOutputs)
{
	foreach (OutputIndex = 0 ... NumOutputs)
	{
		Outputs[OutputIndex] = Gemm_Biases[OutputIndex];
	}

	for (uniform int InputIndex = 0; InputIndex < NumInputs; ++InputIndex)
	{
		uniform float X = Inputs[InputIndex];
		foreach (OutputIndex = 0 ... NumOutputs)
		{
			Outputs[OutputIndex] += X * Gemm_Weights[InputIndex * NumOutputs + OutputIndex];
		}
	}

	foreach (OutputIndex = 0 ... NumOutputs)
	{
		const float Output = Outputs[OutputIndex];
		Outputs[OutputIndex] = select(Output > 0.0f, Output, Output * PRelu_Alpha);
	}
}


export void Gemm(
	uniform float Outputs[],
	const uniform float Inputs[],
	const uniform float Gemm_Weights[],
	const uniform float Gemm_Biases[],
	const uniform int NumInputs,
	const uniform int NumOutputs)
{
	foreach (OutputIndex = 0 ... NumOutputs)
	{
		Outputs[OutputIndex] = Gemm_Biases[OutputIndex];
	}

	for (uniform int InputIndex = 0; InputIndex < NumInputs; ++InputIndex)
	{
		uniform float X = Inputs[InputIndex];
		foreach (OutputIndex = 0 ... NumOutputs)
		{
			Outputs[OutputIndex] += X * Gemm_Weights[InputIndex * NumOutputs + OutputIndex];
		}
	}
}




