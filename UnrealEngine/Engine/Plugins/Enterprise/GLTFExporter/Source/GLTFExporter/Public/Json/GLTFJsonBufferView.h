// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonBufferView : IGLTFJsonIndexedObject
{
	FString Name;

	FGLTFJsonBuffer* Buffer;

	int64 ByteLength;
	int64 ByteOffset;
	int32 ByteStride;

	EGLTFJsonBufferTarget Target;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

protected:

	friend TGLTFJsonIndexedObjectArray<FGLTFJsonBufferView, void>;

	FGLTFJsonBufferView(int32 Index)
		: IGLTFJsonIndexedObject(Index)
		, Buffer(nullptr)
		, ByteLength(0)
		, ByteOffset(0)
		, ByteStride(0)
		, Target(EGLTFJsonBufferTarget::None)
	{
	}
};
