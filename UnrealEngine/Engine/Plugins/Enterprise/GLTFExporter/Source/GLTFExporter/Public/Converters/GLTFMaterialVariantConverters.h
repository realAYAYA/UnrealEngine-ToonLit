// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"
#include "Json/GLTFJsonMesh.h"
#include "Converters/GLTFConverter.h"
#include "Converters/GLTFBuilderContext.h"

class UVariant;
class UPropertyValueMaterial;

typedef TGLTFConverter<FGLTFJsonMaterialVariant*, const UVariant*> IGLTFMaterialVariantConverter;

class GLTFEXPORTER_API FGLTFMaterialVariantConverter : public FGLTFBuilderContext, public IGLTFMaterialVariantConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonMaterialVariant* Convert(const UVariant* Variant) override;

private:

	bool TryParseMaterialProperty(FGLTFJsonPrimitive*& OutPrimitive, FGLTFJsonMaterial*& OutMaterial, const UPropertyValueMaterial* Property) const;
};
