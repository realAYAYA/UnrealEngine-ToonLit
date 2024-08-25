// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/Framework/AvaTestDynamicMeshActor.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Object.h"

AAvaTestDynamicMeshActor::AAvaTestDynamicMeshActor()
{
	ShapeDynamicMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DynamicMeshComponent"));
	SetRootComponent(ShapeDynamicMeshComponent);
}
