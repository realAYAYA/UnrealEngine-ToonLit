// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeformerGroomComponentSource.h"

#include "GroomComponent.h"

#define LOCTEXT_NAMESPACE "DeformersGroomComponentSource"

static const FHairGroupInstance* GetGroomInstance(const UGroomComponent* In)
{
	// HAIR_TODO: apply to all hair group, or only selected one
	if (In && In->GetGroupCount() > 0)
	{
		return In->GetGroupInstance(0);
	}
	return nullptr;
}

FName UOptimusGroomComponentSource::Domains::ControlPoint("ControlPoint");
FName UOptimusGroomComponentSource::Domains::Curve("Curve");

FText UOptimusGroomComponentSource::GetDisplayName() const
{
	return LOCTEXT("GroomComponent", "Groom Component");
}

TSubclassOf<UActorComponent> UOptimusGroomComponentSource::GetComponentClass() const
{
	return UGroomComponent::StaticClass();
}

bool UOptimusGroomComponentSource::IsUsableAsPrimarySource() const
{
	return GetComponentClass()->IsChildOf<UGroomComponent>();
}

TArray<FName> UOptimusGroomComponentSource::GetExecutionDomains() const
{
	return { Domains::ControlPoint, Domains::Curve };
}

int32 UOptimusGroomComponentSource::GetLodIndex(const UActorComponent* InComponent) const
{
	return 0;
}

bool UOptimusGroomComponentSource::GetComponentElementCountsForExecutionDomain(
	FName InDomainName,
	const UActorComponent* InComponent,
	int32 InLodIndex,
	TArray<int32>& OutInvocationElementCounts
) const
{
	const UGroomComponent* GroomComponent = Cast<UGroomComponent>(InComponent);
	if (!GroomComponent)
	{
		return false;
	}

	const FHairGroupInstance* Instance = GetGroomInstance(GroomComponent);
	if (!Instance)
	{
		return 0;
	}

	const int32 NumVertices = Instance->Strands.Data->PointCount;
	const int32 NumCurves = Instance->Strands.Data->CurveCount;

	OutInvocationElementCounts.Reset();

	if (InDomainName == Domains::ControlPoint || InDomainName == Domains::Curve)
	{
		const int32 NumInvocations = 1; //TODO: num hair group?

		OutInvocationElementCounts.Reset();
		OutInvocationElementCounts.Reserve(NumInvocations);
		for (int32 InvocationIndex = 0; InvocationIndex < NumInvocations; ++InvocationIndex)
		{
			const int32 NumThreads = InDomainName == Domains::ControlPoint ? NumVertices : NumCurves;
			OutInvocationElementCounts.Add(NumThreads);
		}

		return true;
	}

	// Unknown execution domain.
	return false;
}
#undef LOCTEXT_NAMESPACE
