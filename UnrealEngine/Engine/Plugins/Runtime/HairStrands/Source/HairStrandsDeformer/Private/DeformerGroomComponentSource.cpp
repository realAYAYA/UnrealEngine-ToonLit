// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeformerGroomComponentSource.h"

#include "GroomComponent.h"

#define LOCTEXT_NAMESPACE "DeformersGroomComponentSource"

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

uint32 UOptimusGroomComponentSource::GetDefaultNumInvocations(
	const UActorComponent* InComponent,
	int32 InLod
	) const
{
	const UGroomComponent* GroomComponent = Cast<UGroomComponent>(InComponent);
	if (!GroomComponent)
	{
		return 0;
	}

	const uint32 GroupCount = GroomComponent->GetGroupCount();

	return GroupCount;
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

	// Ensure all groups are valid
	const uint32 GroupCount = GroomComponent->GetGroupCount();
	for (uint32 GroupIt=0;GroupIt<GroupCount;++GroupIt)
	{
		if (!GroomComponent->GetGroupInstance(GroupIt))
		{
			return 0;
		}
	}

	OutInvocationElementCounts.Reset();

	if (InDomainName == Domains::ControlPoint || InDomainName == Domains::Curve)
	{
		const int32 NumInvocations = GroupCount;
		OutInvocationElementCounts.Reserve(NumInvocations);
		for (int32 InvocationIndex = 0; InvocationIndex < NumInvocations; ++InvocationIndex)
		{
			if (const FHairGroupInstance* Instance = GroomComponent->GetGroupInstance(InvocationIndex))
			{
				const int32 NumControlPoints = Instance->Strands.GetData().GetNumPoints();
				const int32 NumCurves = Instance->Strands.GetData().GetNumCurves();
				const int32 NumThreads = InDomainName == Domains::ControlPoint ? NumControlPoints : NumCurves;
				OutInvocationElementCounts.Add(NumThreads);
			}
		}

		return true;
	}

	// Unknown execution domain.
	return false;
}
#undef LOCTEXT_NAMESPACE
