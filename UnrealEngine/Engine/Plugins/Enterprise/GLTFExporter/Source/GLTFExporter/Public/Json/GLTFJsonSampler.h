// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonSampler : IGLTFJsonIndexedObject
{
	FString Name;

	EGLTFJsonTextureFilter MinFilter;
	EGLTFJsonTextureFilter MagFilter;

	EGLTFJsonTextureWrap WrapS;
	EGLTFJsonTextureWrap WrapT;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

protected:

	friend TGLTFJsonIndexedObjectArray<FGLTFJsonSampler, void>;

	FGLTFJsonSampler(int32 Index)
		: IGLTFJsonIndexedObject(Index)
		, MinFilter(EGLTFJsonTextureFilter::None)
		, MagFilter(EGLTFJsonTextureFilter::None)
		, WrapS(EGLTFJsonTextureWrap::Repeat)
		, WrapT(EGLTFJsonTextureWrap::Repeat)
	{
	}
};
