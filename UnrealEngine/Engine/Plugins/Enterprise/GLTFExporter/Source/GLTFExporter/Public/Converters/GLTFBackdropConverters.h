// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"
#include "Converters/GLTFConverter.h"
#include "Converters/GLTFBuilderContext.h"

class AActor;

typedef TGLTFConverter<FGLTFJsonBackdrop*, const AActor*> IGLTFBackdropConverter;


class GLTFEXPORTER_API FGLTFBackdropConverter : public FGLTFBuilderContext, public IGLTFBackdropConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonBackdrop* Convert(const AActor* BackdropActor) override;
};
