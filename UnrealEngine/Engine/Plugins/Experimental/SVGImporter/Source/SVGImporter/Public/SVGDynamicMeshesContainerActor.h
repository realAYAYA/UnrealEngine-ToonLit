// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "SVGDynamicMeshesContainerActor.generated.h"

class UDynamicMeshComponent;

UCLASS(MinimalAPI, Abstract)
class ASVGDynamicMeshesContainerActor : public AActor
{
	GENERATED_BODY()

public:
	virtual TArray<UDynamicMeshComponent*> GetSVGDynamicMeshes() PURE_VIRTUAL(ASVGDynamicMeshesContainerActor::GetSVGDynamicMeshes, return {};)
};
