// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonEpicLevelVariantSets.h"
#include "Json/GLTFJsonMaterial.h"
#include "Json/GLTFJsonTexture.h"
#include "Json/GLTFJsonMesh.h"
#include "Json/GLTFJsonNode.h"

void FGLTFJsonEpicVariantMaterial::WriteObject(IGLTFJsonWriter& Writer) const
{
	Writer.Write(TEXT("material"), Material);

	if (Index != INDEX_NONE)
	{
		Writer.Write(TEXT("index"), Index);
	}
}

void FGLTFJsonEpicVariantNodeProperties::WriteObject(IGLTFJsonWriter& Writer) const
{
	Writer.Write(TEXT("node"), Node);

	Writer.StartObject(TEXT("properties"));

	if (bIsVisible.IsSet())
	{
		Writer.Write(TEXT("visible"), bIsVisible.GetValue());
	}

	if (Mesh.IsSet())
	{
		Writer.Write(TEXT("mesh"), Mesh.GetValue());
	}

	if (Materials.Num() > 0)
	{
		Writer.Write(TEXT("materials"), Materials);
	}

	Writer.EndObject();
}

void FGLTFJsonEpicVariant::WriteObject(IGLTFJsonWriter& Writer) const
{
	Writer.Write(TEXT("name"), Name);
	Writer.Write(TEXT("active"), bIsActive);

	if (Thumbnail != nullptr)
	{
		Writer.Write(TEXT("thumbnail"), Thumbnail);
	}

	TArray<FGLTFJsonEpicVariantNodeProperties> NodeValues;
	Nodes.GenerateValueArray(NodeValues);
	Writer.Write(TEXT("nodes"), NodeValues);
}

void FGLTFJsonEpicVariantSet::WriteObject(IGLTFJsonWriter& Writer) const
{
	if (!Name.IsEmpty())
	{
		Writer.Write(TEXT("name"), Name);
	}

	Writer.Write(TEXT("variants"), Variants);
}

void FGLTFJsonEpicLevelVariantSets::WriteObject(IGLTFJsonWriter& Writer) const
{
	if (!Name.IsEmpty())
	{
		Writer.Write(TEXT("name"), Name);
	}

	Writer.Write(TEXT("variantSets"), VariantSets);
}
