// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorFactoryText3D.h"
#include "Text3DActor.h"

#define LOCTEXT_NAMESPACE "ActorFactory"

UActorFactoryText3D::UActorFactoryText3D()
{
	DisplayName = LOCTEXT("Text3D", "Text 3D");
	NewActorClass = AText3DActor::StaticClass();
	SpawnPositionOffset = FVector(0, 0, 0);
	bUseSurfaceOrientation = false;
}

#undef LOCTEXT_NAMESPACE
