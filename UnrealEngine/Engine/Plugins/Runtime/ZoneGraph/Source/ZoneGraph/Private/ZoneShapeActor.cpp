// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZoneShapeActor.h"
#include "UObject/ConstructorHelpers.h"
#include "ZoneShapeComponent.h"

AZoneShape::AZoneShape(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ShapeComponent = CreateDefaultSubobject<UZoneShapeComponent>(TEXT("ShapeComp"));

	RootComponent = ShapeComponent;

	SetHidden(true);
	SetCanBeDamaged(false);

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}
