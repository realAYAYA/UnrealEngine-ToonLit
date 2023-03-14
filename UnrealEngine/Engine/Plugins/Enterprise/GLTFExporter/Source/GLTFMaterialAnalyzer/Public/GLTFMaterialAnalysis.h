// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

struct FGLTFMaterialAnalysis
{
	FGLTFMaterialAnalysis()
		: bRequiresVertexData(false)
		, bRequiresPrimitiveData(false)
	{
	}

	/** Tracks the texture coordinates used by this material */
	TBitArray<> TextureCoordinates;

	/** Will contain all the shading models picked up from the material expression graph */
	FMaterialShadingModelField ShadingModels;

	/** The resulting code corresponding to the currently compiled property or custom output. */
	FString ParameterCode;

	/** True if this material reads any vertex data */
	bool bRequiresVertexData;

	/** True if this material reads any primitive data */
	bool bRequiresPrimitiveData;
};
