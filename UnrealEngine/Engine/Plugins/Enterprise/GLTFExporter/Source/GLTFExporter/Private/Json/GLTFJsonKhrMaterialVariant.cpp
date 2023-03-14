// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonKhrMaterialVariant.h"
#include "Json/GLTFJsonMaterial.h"

void FGLTFJsonKhrMaterialVariantMapping::WriteObject(IGLTFJsonWriter& Writer) const
{
	Writer.Write(TEXT("material"), Material);
	Writer.Write(TEXT("variants"), Variants);
}

void FGLTFJsonKhrMaterialVariant::WriteObject(IGLTFJsonWriter& Writer) const
{
	Writer.Write(TEXT("name"), Name);
}
