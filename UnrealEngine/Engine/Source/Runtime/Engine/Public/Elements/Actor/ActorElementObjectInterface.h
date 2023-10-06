// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementObjectInterface.h"
#include "ActorElementObjectInterface.generated.h"

UCLASS(MinimalAPI)
class UActorElementObjectInterface : public UObject, public ITypedElementObjectInterface
{
	GENERATED_BODY()

public:
	ENGINE_API virtual UObject* GetObject(const FTypedElementHandle& InElementHandle) override;
};
