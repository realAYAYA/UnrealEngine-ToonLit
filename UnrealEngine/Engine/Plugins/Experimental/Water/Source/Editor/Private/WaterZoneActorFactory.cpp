// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterZoneActorFactory.h"
#include "WaterZoneActor.h"
#include "WaterMeshComponent.h"
#include "WaterEditorSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaterZoneActorFactory)

#define LOCTEXT_NAMESPACE "WaterZoneActorFactory"

UWaterZoneActorFactory::UWaterZoneActorFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("WaterZoneActorDisplayName", "Water Zone");
	NewActorClass = AWaterZone::StaticClass();
	bUseSurfaceOrientation = true;
}

void UWaterZoneActorFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);
	
	AWaterZone* WaterZoneActor = CastChecked<AWaterZone>(NewActor);

	const FWaterZoneActorDefaults& WaterMeshActorDefaults = GetDefault<UWaterEditorSettings>()->WaterZoneActorDefaults;
	WaterZoneActor->GetWaterMeshComponent()->FarDistanceMaterial = WaterMeshActorDefaults.GetFarDistanceMaterial();
	WaterZoneActor->GetWaterMeshComponent()->FarDistanceMeshExtent = WaterMeshActorDefaults.FarDistanceMeshExtent;
}

#undef LOCTEXT_NAMESPACE
