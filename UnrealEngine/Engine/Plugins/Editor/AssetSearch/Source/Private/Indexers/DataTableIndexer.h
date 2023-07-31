// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAssetIndexer.h"

class FDataTableIndexer : public IAssetIndexer
{
	virtual FString GetName() const override { return TEXT("DataTable"); }
	virtual int32 GetVersion() const override;
	virtual void IndexAsset(const UObject* InAssetObject, FSearchSerializer& Serializer) const override;
};