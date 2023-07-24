// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"
#include "Converters/GLTFConverter.h"
#include "Converters/GLTFBuilderContext.h"

class UCameraComponent;

typedef TGLTFConverter<FGLTFJsonCamera*, const UCameraComponent*> IGLTFCameraConverter;

class GLTFEXPORTER_API FGLTFCameraConverter : public FGLTFBuilderContext, public IGLTFCameraConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonCamera* Convert(const UCameraComponent* CameraComponent) override;
};
