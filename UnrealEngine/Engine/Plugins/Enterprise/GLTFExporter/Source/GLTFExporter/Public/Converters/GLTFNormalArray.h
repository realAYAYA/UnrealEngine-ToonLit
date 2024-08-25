// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// TODO: should we add GLTFEXPORTER_API ?
class FGLTFNormalArray : public TArray<FVector3f>
{
public:

	using TArray::TArray;
	using TArray::operator=;

	friend uint32 GetTypeHash(const FGLTFNormalArray& NormalArray)
	{
		uint32 Hash = GetTypeHash(NormalArray.Num());
		for (const FVector3f& Normal : NormalArray)
		{
			Hash = HashCombine(Hash, GetTypeHash(Normal));
		}
		return Hash;
	}
};