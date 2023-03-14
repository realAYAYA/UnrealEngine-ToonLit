// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonEpicVariantMaterial : IGLTFJsonObject
{
	FGLTFJsonMaterial* Material;
	int32 Index;

	FGLTFJsonEpicVariantMaterial()
		: Material(nullptr)
		, Index(INDEX_NONE)
	{
	}

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;
};

struct GLTFEXPORTER_API FGLTFJsonEpicVariantNodeProperties : IGLTFJsonObject
{
	FGLTFJsonNode* Node;
	TOptional<bool> bIsVisible;

	TOptional<FGLTFJsonMesh*> Mesh;
	TArray<FGLTFJsonEpicVariantMaterial> Materials;

	FGLTFJsonEpicVariantNodeProperties()
		: Node(nullptr)
	{
	}

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;
};

struct GLTFEXPORTER_API FGLTFJsonEpicVariant : IGLTFJsonObject
{
	FString Name;
	bool bIsActive;

	FGLTFJsonTexture* Thumbnail;
	TMap<FGLTFJsonNode*, FGLTFJsonEpicVariantNodeProperties> Nodes;

	FGLTFJsonEpicVariant()
		: bIsActive(false)
		, Thumbnail(nullptr)
	{
	}

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;
};

struct GLTFEXPORTER_API FGLTFJsonEpicVariantSet : IGLTFJsonObject
{
	FString Name;

	TArray<FGLTFJsonEpicVariant> Variants;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;
};

struct GLTFEXPORTER_API FGLTFJsonEpicLevelVariantSets : IGLTFJsonIndexedObject
{
	FString Name;

	TArray<FGLTFJsonEpicVariantSet> VariantSets;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

protected:

	friend TGLTFJsonIndexedObjectArray<FGLTFJsonEpicLevelVariantSets, void>;

	FGLTFJsonEpicLevelVariantSets(int32 Index)
		: IGLTFJsonIndexedObject(Index)
	{
	}
};
