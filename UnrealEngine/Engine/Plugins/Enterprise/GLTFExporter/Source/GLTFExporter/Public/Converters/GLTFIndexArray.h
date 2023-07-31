// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// TODO: should we add GLTFEXPORTER_API ?
class FGLTFIndexArray : public TArray<int32>
{
public:

	using TArray::TArray;
	using TArray::operator=;

	friend uint32 GetTypeHash(const FGLTFIndexArray& IndexArray)
	{
		uint32 Hash = GetTypeHash(IndexArray.Num());
		for (const int32 Index : IndexArray)
		{
			Hash = HashCombine(Hash, GetTypeHash(Index));
		}
		return Hash;
	}
};
