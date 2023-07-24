// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementAssetDataInterface.h"

#include "SMInstanceElementAssetDataInterface.generated.h"

UCLASS()
class ENGINE_API USMInstanceElementAssetDataInterface : public UObject, public ITypedElementAssetDataInterface
{
	GENERATED_BODY()

public:
	virtual FAssetData GetAssetData(const FTypedElementHandle& InElementHandle) override;
};
