// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAssetIndexer.h"

struct FMemberReference;

class FMaterialExpressionIndexer : public IAssetIndexer
{
public:
	FMaterialExpressionIndexer(const FString& InName)
		: Name(InName)
	{
	}

	virtual FString GetName() const override { return Name; }
	virtual int32 GetVersion() const override;
	virtual void IndexAsset(const UObject* InAssetObject, FSearchSerializer& Serializer) const override;

private:
	void IndexParameters(const UObject* InAssetObject, FSearchSerializer& Serializer) const;
	const FString Name;
};