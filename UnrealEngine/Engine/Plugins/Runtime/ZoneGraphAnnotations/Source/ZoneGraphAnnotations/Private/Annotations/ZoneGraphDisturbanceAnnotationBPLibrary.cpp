// Copyright Epic Games, Inc. All Rights Reserved.

#include "Annotations/ZoneGraphDisturbanceAnnotationBPLibrary.h"
#include "Annotations/ZoneGraphDisturbanceAnnotation.h"
#include "ZoneGraphAnnotationSubsystem.h"
#include "ZoneGraphTypes.h"

void UZoneGraphDisturbanceAnnotationBPLibrary::TriggerDanger(UObject* WorldContextObject, const AActor* Instigator, const FVector Position, const float Radius, const float Duration)
{
	if (!WorldContextObject)
	{
		UE_LOG(LogZoneGraphAnnotations, Error, TEXT("%s: WorldContextObject not set."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}
	
	UZoneGraphAnnotationSubsystem* ZoneGraphAnnotation = UWorld::GetSubsystem<UZoneGraphAnnotationSubsystem>(WorldContextObject->GetWorld());
	if (!ZoneGraphAnnotation)
	{
		UE_LOG(LogZoneGraphAnnotations, Error, TEXT("%s: Expecting ZoneGraphAnnotationSubsystem to be present."), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	// This is just for testing, will add later ability to add multiple tests ala ZoneGraphTesting actor.
	FZoneGraphDisturbanceArea Danger;
	Danger.Position = Position;
	Danger.Radius = Radius;
	Danger.Duration = Duration;
	Danger.InstigatorID = Instigator != nullptr ? PointerHash(Instigator) : 0;
	
	ZoneGraphAnnotation->SendEvent(Danger);
}
