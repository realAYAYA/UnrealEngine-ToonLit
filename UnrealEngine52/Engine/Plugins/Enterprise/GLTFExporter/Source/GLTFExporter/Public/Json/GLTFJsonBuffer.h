// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonBuffer : IGLTFJsonIndexedObject
{
	FString Name;

	FString URI;
	int64   ByteLength;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

protected:

	friend TGLTFJsonIndexedObjectArray<FGLTFJsonBuffer, void>;

	FGLTFJsonBuffer(int32 Index)
		: IGLTFJsonIndexedObject(Index)
		, ByteLength(0)
	{
	}
};
