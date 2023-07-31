// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAssetIndexer.h"

class FWidgetBlueprintIndexer : public IAssetIndexer
{
	virtual FString GetName() const override { return TEXT("WidgetBlueprint"); }
	virtual int32 GetVersion() const override;
	virtual void IndexAsset(const UObject* InAssetObject, FSearchSerializer& Serializer) const override;
};