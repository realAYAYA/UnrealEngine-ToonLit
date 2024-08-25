// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// TODO: should we add GLTFEXPORTER_API ?
class FGLTFUVArray : public TArray<FVector2f>
{
public:

	using TArray::TArray;
	using TArray::operator=;

	friend uint32 GetTypeHash(const FGLTFUVArray& UVArray)
	{
		uint32 Hash = GetTypeHash(UVArray.Num());
		for (const FVector2f& UV : UVArray)
		{
			Hash = HashCombine(Hash, GetTypeHash(UV));
		}
		return Hash;
	}
};