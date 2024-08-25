// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"
#include "Json/GLTFJsonTransform.h"

struct GLTFEXPORTER_API FGLTFJsonNode : FGLTFJsonTransform, IGLTFJsonIndexedObject
{
	FString Name;

	FGLTFJsonCamera* Camera;
	FGLTFJsonSkin*   Skin;
	FGLTFJsonMesh*   Mesh;
	FGLTFJsonLight*  Light;

	TArray<FGLTFJsonNode*> Children;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

protected:

	friend TGLTFJsonIndexedObjectArray<FGLTFJsonNode, void>;

	FGLTFJsonNode(int32 Index)
		: IGLTFJsonIndexedObject(Index)
		, Camera(nullptr)
		, Skin(nullptr)
		, Mesh(nullptr)
		, Light(nullptr)
	{
	}
};
