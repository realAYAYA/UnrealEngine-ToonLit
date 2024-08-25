// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessDrawDebugContext.h"
#include "DrawDebugHelpers.h"

#include "Components/LineBatchComponent.h"
#include "Engine/World.h"

#if WITH_EDITOR
void FNiagaraStatelessDrawDebugContext::DrawArrow(const FVector& Origin, const FVector& DirectionWithLength, const FColor& Color) const
{
	const float Len = DirectionWithLength.Length();
	if (Len > UE_SMALL_NUMBER)
	{
		DrawDebugDirectionalArrow(
			World,
			Origin,
			Origin + DirectionWithLength,
			Len,
			Color
		);
	}
}

void FNiagaraStatelessDrawDebugContext::DrawBox(const FVector& Center, const FVector& Extent, const FColor& Color, const FQuat& Rotation) const
{
	DrawDebugBox(
		World,
		LocalToWorldTransform.TransformPosition(Center),
		Extent,
		LocalToWorldTransform.TransformRotation(Rotation),
		Color
	);
}

void FNiagaraStatelessDrawDebugContext::DrawCone(const FVector& Origin, const FQuat& Rotation, float Angle, float Length, const FColor& Color) const
{
	DrawDebugCone(
		World,
		Origin,
		Rotation.GetAxisZ(),
		Length,
		FMath::DegreesToRadians(Angle),
		FMath::DegreesToRadians(Angle),
		16,
		Color
	);
}

void FNiagaraStatelessDrawDebugContext::DrawCylinder(float CylinderHeight, float CylinderRadius, float CylinderHeightMidpoint, const FColor& Color) const
{
	const FVector Axis = LocalToWorldTransform.TransformVector(FVector::ZAxisVector);
	const FVector Center = LocalToWorldTransform.TransformPosition(FVector::ZeroVector);
	const float Offset = -CylinderHeightMidpoint * CylinderHeight;

	DrawDebugCylinder(
		World,
		Center + (Axis * Offset),
		Center + (Axis * (Offset + CylinderHeight)),
		CylinderRadius,
		12,
		Color
	);
}

void FNiagaraStatelessDrawDebugContext::DrawCircle(const FVector& InCenter, const float Radius, const FColor& Color) const
{
	if (ULineBatchComponent* LineBatcher = World->LineBatcher)
	{
		int32 Segments = 16;
		const float AngleStep = 2.f * UE_PI / float(Segments);

		const FVector Center = LocalToWorldTransform.TransformVector(InCenter);
		const FVector AxisX = LocalToWorldTransform.TransformVector(FVector::XAxisVector);
		const FVector AxisY = LocalToWorldTransform.TransformVector(FVector::YAxisVector);

		TArray<FBatchedLine> Lines;
		Lines.Empty(Segments);

		float Angle = 0.f;
		while (Segments--)
		{
			const FVector Vertex1 = Center + Radius * (AxisX * FMath::Cos(Angle) + AxisY * FMath::Sin(Angle));
			Angle += AngleStep;
			const FVector Vertex2 = Center + Radius * (AxisX * FMath::Cos(Angle) + AxisY * FMath::Sin(Angle));
			Lines.Emplace(Vertex1, Vertex2, Color, -1.0f, 0.0f, 0);
		}
		LineBatcher->DrawLines(Lines);
	}
}

void FNiagaraStatelessDrawDebugContext::DrawSphere(const FVector& Center, const float Radius, const FColor& Color) const
{
	DrawDebugSphere(
		World,
		LocalToWorldTransform.TransformPosition(Center),
		Radius,
		16,
		Color
	);
}
#endif
