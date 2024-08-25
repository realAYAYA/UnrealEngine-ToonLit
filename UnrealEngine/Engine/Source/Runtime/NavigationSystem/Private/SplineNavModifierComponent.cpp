// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineNavModifierComponent.h"

#include "AI/NavigationSystemBase.h"
#include "AI/Navigation/NavigationRelevantData.h"
#include "Components/SplineComponent.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SplineNavModifierComponent)

namespace
{
	const USplineComponent* GetSpline(const AActor* Owner)
	{
		if (!Owner)
		{
			UE_LOG(LogNavigation, Warning, TEXT("USplineNavModifierComponent has no owner, cannot proceed"));
			return nullptr;
		}

		const USplineComponent* Spline = Owner->GetComponentByClass<USplineComponent>();
		UE_CVLOG_UELOG(!Spline, Owner, LogNavigation, Warning, TEXT("USplineNavModifierComponent attached to \"%s\" could not find a spline component, cannot proceed"), *Owner->GetName());

		return Spline;
	}
}

void USplineNavModifierComponent::CalculateBounds() const
{
	if (const USplineComponent* Spline = GetSpline(GetOwner()))
	{
		Bounds = Spline->CalcBounds(GetOwner()->GetTransform()).GetBox();
	}
}

void USplineNavModifierComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	const USplineComponent* Spline = GetSpline(GetOwner());
	if (!Spline)
	{
		return;
	}

	// Square in the YZ plane used to sample the spline at each cross section
	constexpr int32 SampleVertices = 4;
	TStaticArray<FVector, SampleVertices> SampleSquare;
	SampleSquare[0] = FVector(0, -SplineExtent, -SplineExtent);
	SampleSquare[1] = FVector(0,  SplineExtent, -SplineExtent);
	SampleSquare[2] = FVector(0,  SplineExtent,  SplineExtent);
	SampleSquare[3] = FVector(0, -SplineExtent,  SplineExtent);

	// Vertices (in no particular order) of a prism which will enclose each segment of the spline
	// @Note The only reason Tube isn't a TStaticArray is because FAreaNavModifier expects a TArray
	TArray<FVector> Tube;
	Tube.SetNum(SampleSquare.Num() * 2);

	// Always sample at least the start and end points
	const int32 NumPoints = FMath::Max(NumSplineSamples, 2);
	
	const float SplineLength = Spline->GetSplineLength();
	FTransform PreviousTransform;

	for (int32 SampleIndex = 0; SampleIndex < NumPoints; SampleIndex++)
	{
		// Sample a point on the spline at the current distance
		const double Distance = SplineLength * SampleIndex / (NumPoints - 1);
		const FTransform CurrentTransform = Spline->GetTransformAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

		if (SampleIndex > 0)
		{
			// Compute the vertices of the current tube segment
			for (int i = 0; i < SampleVertices; i++)
			{
				Tube[i] = PreviousTransform.TransformPosition(SampleSquare[i]);
				Tube[i + SampleVertices] = CurrentTransform.TransformPosition(SampleSquare[i]);
			}

			// From the tube construct a convex hull whose volume will be used to mark the nav mesh with the selected AreaClass
			const FAreaNavModifier NavModifier(Tube, ENavigationCoordSystem::Type::Unreal, FTransform::Identity, AreaClass);
			Data.Modifiers.Add(NavModifier);
		}
		
		PreviousTransform = CurrentTransform;
	}
}