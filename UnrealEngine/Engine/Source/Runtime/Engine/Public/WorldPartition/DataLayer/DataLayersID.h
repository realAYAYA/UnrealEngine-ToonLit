// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UDataLayerInstance;

#if WITH_EDITOR
class ENGINE_API FDataLayersID
{
public:
	FDataLayersID();
	FDataLayersID(const TArray<const UDataLayerInstance*>& DataLayerInstance);

	bool operator==(const FDataLayersID& Other) const
	{
		return Hash == Other.Hash;
	}

	bool operator!=(const FDataLayersID& Other) const
	{
		return !(*this == Other);
	}

	uint32 GetHash() const
	{
		return Hash;
	}

	friend uint32 GetTypeHash(const FDataLayersID& InDataLayersID)
	{
		return InDataLayersID.Hash;
	}

private:
	uint32 Hash;
};
#endif
