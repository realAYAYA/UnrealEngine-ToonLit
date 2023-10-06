// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChildActorSubobjectData.h"

#include "GameFramework/Actor.h"
#include "UObject/Class.h"

FChildActorSubobjectData::FChildActorSubobjectData(UObject* ContextObject, const FSubobjectDataHandle& ParentHandle, const bool InbIsInheritedSCS)
    : FInheritedSubobjectData(ContextObject, ParentHandle, InbIsInheritedSCS)
{
}

FText FChildActorSubobjectData::GetDisplayName() const
{
	if(const UChildActorComponent* CAC = GetChildActorComponent())
	{
		return CAC->GetClass()->GetDisplayNameText();	
	}
	return FInheritedSubobjectData::GetDisplayName();
}

FText FChildActorSubobjectData::GetActorDisplayText() const
{
	if (const AActor* ChildActor = GetObject<AActor>())
	{
		return ChildActor->GetClass()->GetDisplayNameText();
	}

	return FInheritedSubobjectData::GetActorDisplayText();
}

bool FChildActorSubobjectData::IsChildActor() const
{
	return true;
}