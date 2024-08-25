// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/PropertyAnimatorTextGroup.h"

#include "Properties/PropertyAnimatorCoreContext.h"
#include "Properties/PropertyAnimatorCoreResolver.h"
#include "Text3DComponent.h"

void UPropertyAnimatorTextGroup::SetRangeStart(float InRangeStart)
{
	RangeStart = FMath::Clamp(InRangeStart, 0, 1);
}

void UPropertyAnimatorTextGroup::SetRangeEnd(float InRangeEnd)
{
	RangeEnd = FMath::Clamp(InRangeEnd, 0, 1);
}

void UPropertyAnimatorTextGroup::SetRangeOffset(float InRangeOffset)
{
	RangeOffset = InRangeOffset;
}

void UPropertyAnimatorTextGroup::ManageProperties(const UPropertyAnimatorCoreContext* InContext, TArray<FPropertyAnimatorCoreData>& InOutProperties)
{
	if (InOutProperties.IsEmpty())
	{
		return;
	}

	const int32 MaxIndex = InOutProperties.Num();
	const int32 BeginIndex = RangeStart * MaxIndex + RangeOffset * MaxIndex;
	const int32 EndIndex = RangeEnd * MaxIndex + RangeOffset * MaxIndex;

	if (EndIndex < 0 || BeginIndex > EndIndex || BeginIndex == EndIndex || BeginIndex > MaxIndex)
	{
		InOutProperties.Empty();
		return;
	}

	if (EndIndex < MaxIndex)
	{
		InOutProperties.RemoveAt(EndIndex, MaxIndex - EndIndex);
	}

	if (BeginIndex > 0 && BeginIndex <= MaxIndex)
	{
		InOutProperties.RemoveAt(0, BeginIndex);
	}
}

bool UPropertyAnimatorTextGroup::IsPropertySupported(const UPropertyAnimatorCoreContext* InContext) const
{
	const UActorComponent* ActorComponent = InContext->GetAnimatedProperty().GetOwningComponent();
	const UPropertyAnimatorCoreResolver* PropertyResolver = InContext->GetAnimatedProperty().GetPropertyResolver();

	if (PropertyResolver
		&& PropertyResolver->IsRange()
		&& ActorComponent
		&& (ActorComponent->IsA<UText3DComponent>() || ActorComponent->GetTypedOuter<UText3DComponent>()))
	{
		return true;
	}

	return Super::IsPropertySupported(InContext);
}
