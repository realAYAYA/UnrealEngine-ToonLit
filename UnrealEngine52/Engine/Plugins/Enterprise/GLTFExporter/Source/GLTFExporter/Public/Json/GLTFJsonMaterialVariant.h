// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonMaterialVariantMapping : IGLTFJsonObject
{
	FGLTFJsonMaterial* Material;
	TArray<FGLTFJsonMaterialVariant*> Variants;

	FGLTFJsonMaterialVariantMapping()
		: Material(nullptr)
	{
	}

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;
};

struct GLTFEXPORTER_API FGLTFJsonMaterialVariant : IGLTFJsonIndexedObject
{
	FString Name;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

protected:

	friend TGLTFJsonIndexedObjectArray<FGLTFJsonMaterialVariant, void>;

	FGLTFJsonMaterialVariant(int32 Index)
		: IGLTFJsonIndexedObject(Index)
	{
	}
};
