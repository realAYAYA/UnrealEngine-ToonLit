// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonBackdrop : IGLTFJsonIndexedObject
{
	FString Name;

	FGLTFJsonMesh* Mesh;
	FGLTFJsonTexture* Cubemap[6];

	float Intensity;
	float Size;
	float Angle;

	FGLTFJsonVector3 ProjectionCenter;

	float LightingDistanceFactor;
	bool UseCameraProjection;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

protected:

	friend TGLTFJsonIndexedObjectArray<FGLTFJsonBackdrop, void>;

	FGLTFJsonBackdrop(int32 Index)
		: IGLTFJsonIndexedObject(Index)
		, Mesh(nullptr)
		, Cubemap{nullptr}
		, Intensity(0)
		, Size(0)
		, Angle(0)
		, ProjectionCenter(FGLTFJsonVector3::Zero)
		, LightingDistanceFactor(0)
		, UseCameraProjection(false)
	{
	}
};
