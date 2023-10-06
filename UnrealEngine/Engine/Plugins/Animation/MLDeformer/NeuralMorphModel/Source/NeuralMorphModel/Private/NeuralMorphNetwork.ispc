// Copyright Epic Games, Inc. All Rights Reserved.

// An approximation to Exp(-X).
float InvExpApprox(const float X)
{
	return 1 / (1 + 1.00746054f * X + 0.45053901f * X * X + 0.25724632f * X * X * X);		
}

// Process an MLP layer by multiplying inputs by the weights and adding bias values and passing that
// through an ELU activation function.
export void MorphNeuralNetwork_LayerForward(
	uniform float Outputs[],
	const uniform float Inputs[],
	const uniform float Weights[],
	const uniform float Biases[],
	const uniform int NumRows,
	const uniform int NumCols)
{
	foreach (Index = 0 ... NumCols)
	{
		Outputs[Index] = Biases[Index];
	}

	for (uniform int Row = 0; Row < NumRows; Row++)
	{
		uniform float X = Inputs[Row];
		foreach (Col = 0 ... NumCols)
		{
			Outputs[Col] += X * Weights[Row * NumCols + Col];
		}
	}

	foreach (Index = 0 ... NumCols)
	{
		const float X = Outputs[Index];
		Outputs[Index] = select(X > 0.0f, X, InvExpApprox(-X) - 1.0f);
	}
}

// Perform an ELU activation function on every element.
export void MorphNeuralNetwork_Activation_ELU(uniform float InputsOutputs[], const uniform int Num)
{
	foreach (Index = 0 ... Num)
	{
		const float X = InputsOutputs[Index];
		InputsOutputs[Index] = select(X > 0.0f, X, InvExpApprox(-X) - 1.0f);
	}
}
