// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "ActorForWorldTransforms.generated.h"

//Description of an actor selected parts we can find world transforms on 
USTRUCT(BlueprintType)
struct  MOVIESCENE_API FActorForWorldTransforms
{
	GENERATED_BODY()

	FActorForWorldTransforms() : Actor(nullptr), Component(nullptr), SocketName(NAME_None) {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Actor")
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Actor")	
	TWeakObjectPtr<USceneComponent> Component;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Actor")
	FName SocketName;
};