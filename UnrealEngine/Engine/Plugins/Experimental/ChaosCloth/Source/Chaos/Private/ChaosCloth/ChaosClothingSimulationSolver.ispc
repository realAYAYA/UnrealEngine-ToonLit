// Copyright Epic Games, Inc. All Rights Reserved.

#define EXPLICIT_VECTOR4 1

#include "Math/Vector.isph"
#include "Math/Transform.isph"

export void ApplyPreSimulationTransforms(uniform FVector4f ParticlesPandInvM[],
										uniform FVector3f ParticlesV[],
										uniform FVector3f ParticlesX[],
										uniform FVector3f OldAnimationPositions[],
										const uniform float InvM[],
										const uniform uint32 ParticleGroupIds[],
										const uniform FTransform3f PreSimulationTransforms[],
										const uniform FVector3f &DeltaLocalSpaceLocation,
										const uniform int32 Offset,
										const uniform int32 Range)
{
	foreach(i = Offset ... Range)
	{
		const varying uint32 ParticleGroupId = ParticleGroupIds[i];
		#pragma ignore warning(perf)
		const varying FTransform3f GroupSpaceTransform = PreSimulationTransforms[ParticleGroupId];

		const uniform int32 Index = extract(i, 0);

		const varying FVector3f ParticleX = VectorLoad(&ParticlesX[Index]);
		const varying FVector3f PX = TransformPosition(GroupSpaceTransform, ParticleX) - DeltaLocalSpaceLocation;

		VectorStore(&ParticlesPandInvM[Index], SetVector4(PX, InvM[i]));
		VectorStore(&ParticlesX[Index], PX);

		const varying FVector3f ParticleV = VectorLoad(&ParticlesV[Index]);
		const varying FVector3f V = TransformVector(GroupSpaceTransform, ParticleV);
		VectorStore(&ParticlesV[Index], V);

		const varying FVector3f OldAnimationPosition = VectorLoad(&OldAnimationPositions[Index]);
		const varying FVector3f AP = TransformPosition(GroupSpaceTransform, OldAnimationPosition) - DeltaLocalSpaceLocation;
		VectorStore(&OldAnimationPositions[Index], AP);
	}
}

export void CalculateBounds(uniform FVector3f &MMin,
							uniform FVector3f &MMax,
							const uniform FVector3f ParticlesX[],
							const uniform int32 Offset,
							const uniform int32 Range)
{
	varying FVector3f Min = MMin;
	varying FVector3f Max = MMax;

	foreach(i = Offset ... Range)
	{
		const uniform int32 Index = extract(i, 0);
		const varying FVector3f X = VectorLoad(&ParticlesX[Index]);

		Min = VectorMin(Min, X);
		Max = VectorMax(Max, X);
	}

	MMin = VectorReduceMin(Min);
	MMax = VectorReduceMax(Max);
}

export void CalculateSquaredRadius(uniform float &SquaredRadius,
									const uniform FVector3f &Center,
									const uniform FVector3f ParticlesX[],
									const uniform int32 Offset,
									const uniform int32 Range)
{
	varying float vSquaredRadius = SquaredRadius;

	foreach(i = Offset ... Range)
	{
		const uniform int32 Index = extract(i, 0);
		const varying FVector3f X = VectorLoad(&ParticlesX[Index]);

		vSquaredRadius = max(vSquaredRadius, VectorSizeSquared(X - Center));
	}

	SquaredRadius = reduce_max(vSquaredRadius);
}
