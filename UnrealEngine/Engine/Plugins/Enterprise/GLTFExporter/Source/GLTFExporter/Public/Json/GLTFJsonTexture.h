// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonTexture : IGLTFJsonIndexedObject
{
	FString Name;

	FGLTFJsonSampler* Sampler;

	FGLTFJsonImage* Source;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

protected:

	friend TGLTFJsonIndexedObjectArray<FGLTFJsonTexture, void>;

	FGLTFJsonTexture(int32 Index)
		: IGLTFJsonIndexedObject(Index)
		, Sampler(nullptr)
		, Source(nullptr)
	{
	}
};
