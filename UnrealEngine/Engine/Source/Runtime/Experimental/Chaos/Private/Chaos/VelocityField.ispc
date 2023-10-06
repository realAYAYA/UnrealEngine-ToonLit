// Copyright Epic Games, Inc. All Rights Reserved.

#define EXPLICIT_VECTOR4 1

#include "Math/Vector.isph"

static const float OneThird = 1.0f/3.0f;

static inline float SafeNormalize(FVector3f &Direction)
{
	const float Size = sqrt(VectorSizeSquared(Direction));
	Direction = VectorSelect((Size < FLOAT_KINDA_SMALL_NUMBER), FloatForwardVector, Direction / Size);
	return (Size < FLOAT_KINDA_SMALL_NUMBER) ? 0 : Size;
}

export void UpdateField(uniform FVector3f Forces[],
								const uniform FIntVector Elements[],
								const uniform FVector3f V[],
								const uniform FVector3f X[],
								const uniform FVector3f& Velocity,
								const uniform float QuarterRho,
								const uniform float Cd,
								const uniform float Cl,
								const uniform float Cp,
								const uniform int32 NumElements)
{
	varying FIntVector Element;
	uniform int Offset = 0;

#if HW_GATHER_SUPPORTED == 1
	if(programIndex < NumElements)
	{
		Element = VectorLoad(&Elements[Offset]);
	}
#endif

	foreach(ElementIndex = 0 ... NumElements)
	{
#if HW_GATHER_SUPPORTED == 0
		Element = VectorLoad(&Elements[Offset]);
#endif

		const FVector3f X0 = VectorGather(&X[Element.V[0]]);
		const FVector3f X1 = VectorGather(&X[Element.V[1]]);
		const FVector3f X2 = VectorGather(&X[Element.V[2]]);

		// Calculate the normal and the area of the surface exposed to the flow
		FVector3f N = VectorCross(X1 - X0, X2 - X0);
		const float DoubleArea = SafeNormalize(N);

		const FVector3f V0 = VectorGather(&V[Element.V[0]]);
		const FVector3f V1 = VectorGather(&V[Element.V[1]]);
		const FVector3f V2 = VectorGather(&V[Element.V[2]]);

		// Calculate the direction and the relative velocity of the triangle to the flow
		const FVector3f SurfaceVelocity = OneThird * (V0 + V1 + V2);
		const FVector3f RelVelocity = Velocity - SurfaceVelocity;

		// Set the aerodynamic forces
		const float VDotN = VectorDot(RelVelocity, N);
		const float VSquare = VectorDot(RelVelocity, RelVelocity);

#if HW_GATHER_SUPPORTED == 1
		if(ElementIndex + programCount < NumElements)
		{
			Element = VectorLoad(&Elements[Offset + programCount]);
		}
#endif

		const FVector3f EvenForce = (Cd - Cl) * VDotN * RelVelocity + Cl * VSquare * N;
		const FVector3f OddForce = (Cl - Cd) * VDotN * RelVelocity - Cl * VSquare * N;

		const FVector3f Force = QuarterRho * DoubleArea * VectorSelect(VDotN >= 0, EvenForce, OddForce) - DoubleArea * 0.5 * Cp * N; // N points in the opposite direction of the actual mesh normals

		VectorStore(&Forces[Offset], Force);

		Offset += programCount;
	}
}

export void UpdateFieldWithWeightMaps(uniform FVector3f Forces[],
								const uniform FIntVector Elements[],
								const uniform FVector3f V[],
								const uniform FVector3f X[],
								const uniform FVector3f Multipliers[],
								const uniform FVector3f& Velocity,
								const uniform float QuarterRho,
								const uniform float DragBase,
								const uniform float DragRange,
								const uniform float LiftBase,
								const uniform float LiftRange,
								const uniform float PressureBase,
								const uniform float PressureRange,
								const uniform int32 NumElements)
{
	varying FIntVector Element;
	uniform int Offset = 0;

#if HW_GATHER_SUPPORTED == 1
	if(programIndex < NumElements)
	{
		Element = VectorLoad(&Elements[Offset]);
	}
#endif

	foreach(ElementIndex = 0 ... NumElements)
	{
#if HW_GATHER_SUPPORTED == 0
		Element = VectorLoad(&Elements[Offset]);
#endif
		const FVector3f X0 = VectorGather(&X[Element.V[0]]);
		const FVector3f X1 = VectorGather(&X[Element.V[1]]);
		const FVector3f X2 = VectorGather(&X[Element.V[2]]);

		const FVector3f Multiplier = VectorGather(&Multipliers[ElementIndex]);

		// Calculate the normal and the area of the surface exposed to the flow
		FVector3f N = VectorCross(X1 - X0, X2 - X0);
		const float DoubleArea = SafeNormalize(N);

		const FVector3f V0 = VectorGather(&V[Element.V[0]]);
		const FVector3f V1 = VectorGather(&V[Element.V[1]]);
		const FVector3f V2 = VectorGather(&V[Element.V[2]]);

		// Calculate the direction and the relative velocity of the triangle to the flow
		const FVector3f SurfaceVelocity = OneThird * (V0 + V1 + V2);
		const FVector3f RelVelocity = Velocity - SurfaceVelocity;

		// Set the aerodynamic forces
		const float VDotN = VectorDot(RelVelocity, N);
		const float VSquare = VectorDot(RelVelocity, RelVelocity);

		const float Cd = DragBase + DragRange * Multiplier.V[0];
		const float Cl = LiftBase + LiftRange * Multiplier.V[1];
		const float Cp = PressureBase + PressureRange * Multiplier.V[2];

#if HW_GATHER_SUPPORTED == 1
		if(ElementIndex + programCount < NumElements)
		{
			Element = VectorLoad(&Elements[Offset + programCount]);
		}
#endif

		const FVector3f EvenForce = (Cd - Cl) * VDotN * RelVelocity + Cl * VSquare * N;
		const FVector3f OddForce = (Cl - Cd) * VDotN * RelVelocity - Cl * VSquare * N;

		const FVector3f Force = QuarterRho * DoubleArea * VectorSelect(VDotN >= 0, EvenForce, OddForce) - DoubleArea * 0.5 * Cp * N; // N points in the opposite direction of the actual mesh normals

		VectorStore(&Forces[Offset], Force);

		Offset += programCount;
	}
}
