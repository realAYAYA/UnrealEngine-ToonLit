// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonNode : IGLTFJsonIndexedObject
{
	FString Name;

	FGLTFJsonVector3    Translation;
	FGLTFJsonQuaternion Rotation;
	FGLTFJsonVector3    Scale;

	FGLTFJsonCamera*    Camera;
	FGLTFJsonSkin*      Skin;
	FGLTFJsonMesh*      Mesh;
	FGLTFJsonBackdrop*  Backdrop;
	FGLTFJsonLight*     Light;
	FGLTFJsonLightMap*  LightMap;
	FGLTFJsonSkySphere* SkySphere;

	TArray<FGLTFJsonNode*> Children;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

protected:

	friend TGLTFJsonIndexedObjectArray<FGLTFJsonNode, void>;

	FGLTFJsonNode(int32 Index)
		: IGLTFJsonIndexedObject(Index)
		, Translation(FGLTFJsonVector3::Zero)
		, Rotation(FGLTFJsonQuaternion::Identity)
		, Scale(FGLTFJsonVector3::One)
		, Camera(nullptr)
		, Skin(nullptr)
		, Mesh(nullptr)
		, Backdrop(nullptr)
		, Light(nullptr)
		, LightMap(nullptr)
		, SkySphere(nullptr)
	{
	}
};
