// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementObjectInterface.h"
#include "ObjectElementObjectInterface.generated.h"

UCLASS()
class ENGINE_API UObjectElementObjectInterface : public UObject, public ITypedElementObjectInterface
{
	GENERATED_BODY()

public:
	virtual UObject* GetObject(const FTypedElementHandle& InElementHandle) override;
};
