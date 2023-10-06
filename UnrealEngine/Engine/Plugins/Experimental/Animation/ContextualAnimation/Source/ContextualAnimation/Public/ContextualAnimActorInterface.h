// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
