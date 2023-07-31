// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneTypes.h"

/** Structure extending EMaterialProperty to allow detailed information about custom output */
struct FGLTFMaterialPropertyEx
{
	FGLTFMaterialPropertyEx(EMaterialProperty Type = MP_MAX, const FName& CustomOutput = NAME_None)
		: Type(Type)
		, CustomOutput(CustomOutput)
	{}

	FGLTFMaterialPropertyEx(const FName& CustomOutput)
		: Type(MP_CustomOutput)
		, CustomOutput(CustomOutput)
	{}

	FGLTFMaterialPropertyEx(const TCHAR* CustomOutput)
		: Type(MP_CustomOutput)
		, CustomOutput(CustomOutput)
	{}

	FORCEINLINE bool IsCustomOutput() const
	{
		return Type == MP_CustomOutput;
	}

	FORCEINLINE bool operator ==(const FGLTFMaterialPropertyEx& Other) const
	{
		return Type == Other.Type && (!IsCustomOutput() || CustomOutput == Other.CustomOutput);
	}

	FORCEINLINE bool operator !=(const FGLTFMaterialPropertyEx& Other) const
	{
		return !(*this == Other);
	}

	friend FORCEINLINE uint32 GetTypeHash(const FGLTFMaterialPropertyEx& Other)
	{
		return !Other.IsCustomOutput() ? GetTypeHash(Other.Type) : GetTypeHash(Other.CustomOutput);
	}

	GLTFMATERIALBAKING_API FString ToString() const;

	/** The material property */
	EMaterialProperty Type;

	/** The name of a specific custom output. Only used if property is MP_CustomOutput */
	FName CustomOutput;
};
