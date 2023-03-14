// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactoryText3D.generated.h"

UCLASS(MinimalAPI, config=Editor)
class UActorFactoryText3D : public UActorFactory
{
	GENERATED_BODY()

public:
	UActorFactoryText3D();
};
