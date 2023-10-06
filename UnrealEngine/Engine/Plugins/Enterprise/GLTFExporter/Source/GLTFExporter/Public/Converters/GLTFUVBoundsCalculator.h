// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Converters/GLTFConverter.h"
#include "Converters/GLTFIndexArray.h"

struct FMeshDescription;

class GLTFEXPORTER_API FGLTFUVBoundsCalculator : public TGLTFConverter<FBox2f, const FMeshDescription*, FGLTFIndexArray, int32>
{
protected:

	virtual FBox2f Convert(const FMeshDescription* Description, FGLTFIndexArray SectionIndices, int32 TexCoord) override;
};
