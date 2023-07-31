// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonSkin : IGLTFJsonIndexedObject
{
	FString Name;

	FGLTFJsonAccessor* InverseBindMatrices;
	FGLTFJsonNode* Skeleton;

	TArray<FGLTFJsonNode*> Joints;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

protected:

	friend TGLTFJsonIndexedObjectArray<FGLTFJsonSkin, void>;

	FGLTFJsonSkin(int32 Index)
		: IGLTFJsonIndexedObject(Index)
		, InverseBindMatrices(nullptr)
		, Skeleton(nullptr)
	{
	}
};
