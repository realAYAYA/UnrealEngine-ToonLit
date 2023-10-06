// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementAssetDataInterface.h"

#include "ObjectElementAssetDataInterface.generated.h"

UCLASS(MinimalAPI)
class UObjectElementAssetDataInterface : public UObject, public ITypedElementAssetDataInterface
{
	GENERATED_BODY()

public:
	ENGINE_API virtual FAssetData GetAssetData(const FTypedElementHandle& InElementHandle) override;
};
