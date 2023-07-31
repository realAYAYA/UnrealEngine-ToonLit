// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"

#include "GeometryCacheAbcFileActor.generated.h"

class UGeometryCacheAbcFileComponent;

/** GeometryCacheAbcFile actor serves as a placeable actor for GeometryCache loading from an Alembic file */
UCLASS(ComponentWrapperClass)
class AGeometryCacheAbcFileActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category = GeometryCacheAbcFileActor, VisibleAnywhere)
	TObjectPtr<UGeometryCacheAbcFileComponent> GeometryCacheAbcFileComponent;
};
