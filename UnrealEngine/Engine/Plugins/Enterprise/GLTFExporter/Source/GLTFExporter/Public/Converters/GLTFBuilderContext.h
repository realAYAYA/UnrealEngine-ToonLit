// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

class FGLTFConvertBuilder;

class GLTFEXPORTER_API FGLTFBuilderContext
{
public:

	FGLTFBuilderContext(FGLTFConvertBuilder& Builder)
		: Builder(Builder)
	{
	}

protected:

	FGLTFConvertBuilder& Builder;
};
