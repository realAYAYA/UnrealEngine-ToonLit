// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonTextureTransform : IGLTFJsonObject
{
	FGLTFJsonVector2 Offset;
	FGLTFJsonVector2 Scale;
	float Rotation;

	FGLTFJsonTextureTransform()
		: Offset(FGLTFJsonVector2::Zero)
		, Scale(FGLTFJsonVector2::One)
		, Rotation(0)
	{
	}

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

	bool IsNearlyEqual(const FGLTFJsonTextureTransform& Other, float Tolerance = KINDA_SMALL_NUMBER) const;
	bool IsExactlyEqual(const FGLTFJsonTextureTransform& Other) const;

	bool IsNearlyDefault(float Tolerance = KINDA_SMALL_NUMBER) const;
	bool IsExactlyDefault() const;
};
