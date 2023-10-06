// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonScene : IGLTFJsonIndexedObject
{
	FString Name;

	TArray<FGLTFJsonNode*> Nodes;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

protected:

	friend TGLTFJsonIndexedObjectArray<FGLTFJsonScene, void>;

	FGLTFJsonScene(int32 Index)
		: IGLTFJsonIndexedObject(Index)
	{
	}
};
