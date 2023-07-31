// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAssetIndexer.h"

class FLevelIndexer : public IAssetIndexer
{
	virtual FString GetName() const override { return TEXT("Level"); }
	virtual int32 GetVersion() const override;
	virtual void IndexAsset(const UObject* InAssetObject, FSearchSerializer& Serializer) const override;
	virtual void GetNestedAssetTypes(TArray<UClass*>& OutTypes) const override;
};