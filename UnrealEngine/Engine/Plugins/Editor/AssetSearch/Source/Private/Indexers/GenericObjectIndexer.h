// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAssetIndexer.h"

class FGenericObjectIndexer : public IAssetIndexer
{
public:
	FGenericObjectIndexer(const FString& InName)
		: Name(InName)
	{
	}

	virtual FString GetName() const override { return Name; }
	virtual int32 GetVersion() const override;
	virtual void IndexAsset(const UObject* InAssetObject, FSearchSerializer& Serializer) const override;

protected:
	const FString Name;
};