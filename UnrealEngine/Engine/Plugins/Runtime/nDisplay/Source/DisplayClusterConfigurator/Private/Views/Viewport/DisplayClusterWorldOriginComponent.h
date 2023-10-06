// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "DisplayClusterWorldOriginComponent.generated.h"


/**
 * Display cluster world origin component (for in-Editor visualization)
 */
UCLASS(ClassGroup = (DisplayCluster))
class UDisplayClusterWorldOriginComponent
	: public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UDisplayClusterWorldOriginComponent();
};
