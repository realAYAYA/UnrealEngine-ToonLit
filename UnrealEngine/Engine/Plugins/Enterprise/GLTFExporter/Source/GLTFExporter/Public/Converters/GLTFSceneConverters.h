// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"
#include "Converters/GLTFConverter.h"
#include "Converters/GLTFBuilderContext.h"

typedef TGLTFConverter<FGLTFJsonScene*, const UWorld*> IGLTFSceneConverter;

class GLTFEXPORTER_API FGLTFSceneConverter : public FGLTFBuilderContext, public IGLTFSceneConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonScene* Convert(const UWorld* Level) override;
};
