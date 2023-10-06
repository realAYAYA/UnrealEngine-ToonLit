// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"
#include "Converters/GLTFConverter.h"
#include "Converters/GLTFBuilderContext.h"
#include "Converters/GLTFSuperfluous.h"
#include "Converters/GLTFSharedArray.h"

typedef TGLTFConverter<FGLTFJsonImage*, TGLTFSuperfluous<FString>, bool, FIntPoint, TGLTFSharedArray<FColor>> IGLTFImageConverter;

class GLTFEXPORTER_API FGLTFImageConverter : public FGLTFBuilderContext, public IGLTFImageConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonImage* Convert(TGLTFSuperfluous<FString> Name, bool bIgnoreAlpha, FIntPoint Size, TGLTFSharedArray<FColor> Pixels) override;

private:

	EGLTFJsonMimeType GetMimeType(const FColor* Pixels, FIntPoint Size, bool bIgnoreAlpha) const;
};
