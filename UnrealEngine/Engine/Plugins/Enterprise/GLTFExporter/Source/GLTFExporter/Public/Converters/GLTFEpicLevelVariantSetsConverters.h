// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"
#include "Json/GLTFJsonEpicLevelVariantSets.h"
#include "Converters/GLTFConverter.h"
#include "Converters/GLTFBuilderContext.h"

class ULevelVariantSets;
class UVariantSet;
class UVariant;
class UVariantObjectBinding;
class UPropertyValue;
class UPropertyValueMaterial;

typedef TGLTFConverter<FGLTFJsonEpicLevelVariantSets*, const ULevelVariantSets*> IGLTFEpicLevelVariantSetsConverter;

class GLTFEXPORTER_API FGLTFEpicLevelVariantSetsConverter : public FGLTFBuilderContext, public IGLTFEpicLevelVariantSetsConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonEpicLevelVariantSets* Convert(const ULevelVariantSets* LevelVariantSets) override;

private:

	bool TryParseVariant(FGLTFJsonEpicVariant& OutVariant, const UVariant* Variant) const;
	bool TryParseVariantBinding(FGLTFJsonEpicVariant& OutVariant, const UVariantObjectBinding* Binding) const;
	bool TryParseVisibilityPropertyValue(FGLTFJsonEpicVariant& OutVariant, const UPropertyValue* Property) const;
	bool TryParseMaterialPropertyValue(FGLTFJsonEpicVariant& OutVariant, const UPropertyValueMaterial* Property) const;

	template <typename MeshType>
	bool TryParseMeshPropertyValue(FGLTFJsonEpicVariant& OutVariant, const UPropertyValue* Property) const;
};
