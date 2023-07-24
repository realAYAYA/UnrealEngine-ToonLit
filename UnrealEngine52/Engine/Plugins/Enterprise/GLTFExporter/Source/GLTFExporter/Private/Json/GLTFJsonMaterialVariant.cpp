// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonMaterialVariant.h"
#include "Json/GLTFJsonMaterial.h"

void FGLTFJsonMaterialVariantMapping::WriteObject(IGLTFJsonWriter& Writer) const
{
	Writer.Write(TEXT("material"), Material);
	Writer.Write(TEXT("variants"), Variants);
}

void FGLTFJsonMaterialVariant::WriteObject(IGLTFJsonWriter& Writer) const
{
	Writer.Write(TEXT("name"), Name);
}
