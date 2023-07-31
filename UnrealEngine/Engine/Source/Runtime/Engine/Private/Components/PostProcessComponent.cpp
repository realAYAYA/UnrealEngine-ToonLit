// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/PostProcessComponent.h"
#include "Components/SphereComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PostProcessComponent)

UPostProcessComponent::UPostProcessComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bEnabled = true;
	BlendRadius = 100.0f;
	BlendWeight = 1.0f;
	Priority = 0;
	bUnbound = 1;
}

bool UPostProcessComponent::EncompassesPoint(FVector Point, float SphereRadius/*=0.f*/, float* OutDistanceToPoint)
{
	UShapeComponent* ParentShape = Cast<UShapeComponent>(GetAttachParent());
	if (ParentShape != nullptr)
	{
		float Distance = -1.f;

		FVector ClosestPoint;
		float DistanceSq = -1.f;

		if (ParentShape->GetSquaredDistanceToCollision(Point, DistanceSq, ClosestPoint))
		{
			Distance = FMath::Sqrt(DistanceSq);
		}
		else
		{
			FBoxSphereBounds SphereBounds = ParentShape->CalcBounds(ParentShape->GetComponentTransform());	
			if (ParentShape->IsA<USphereComponent>())
			{
				const FSphere& Sphere = SphereBounds.GetSphere();
				const FVector& Dist = Sphere.Center - Point;
				Distance = FMath::Max(0.0f, Dist.Size() - Sphere.W);
			}
			else // UBox or UCapsule shape (approx).
			{
				Distance = FMath::Sqrt(SphereBounds.GetBox().ComputeSquaredDistanceToPoint(Point));
			}
		}

		if (OutDistanceToPoint)
		{
			*OutDistanceToPoint = Distance;
		}

		return Distance >= 0.f && Distance <= SphereRadius;
	}
	if (OutDistanceToPoint != nullptr)
	{
		*OutDistanceToPoint = 0;
	}
	return true;
}

