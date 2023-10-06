// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"
#include "Converters/GLTFConverter.h"
#include "Converters/GLTFBuilderContext.h"

class ULightComponent;

typedef TGLTFConverter<FGLTFJsonLight*, const ULightComponent*> IGLTFLightConverter;

class GLTFEXPORTER_API FGLTFLightConverter : public FGLTFBuilderContext, public IGLTFLightConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonLight* Convert(const ULightComponent* LightComponent) override;
};
