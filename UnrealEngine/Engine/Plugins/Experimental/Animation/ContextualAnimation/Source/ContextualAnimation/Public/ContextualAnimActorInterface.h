// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ContextualAnimActorInterface.generated.h"

UINTERFACE()
class UContextualAnimActorInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IContextualAnimActorInterface : public IInterface
{
	GENERATED_IINTERFACE_BODY()

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Contextual Animation System")
	class USkeletalMeshComponent* GetMesh() const;
};