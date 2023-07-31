// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonImage : IGLTFJsonIndexedObject
{
	FString Name;
	FString URI;

	EGLTFJsonMimeType MimeType;

	FGLTFJsonBufferView* BufferView;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;

protected:

	friend TGLTFJsonIndexedObjectArray<FGLTFJsonImage, void>;

	FGLTFJsonImage(int32 Index)
		: IGLTFJsonIndexedObject(Index)
		, MimeType(EGLTFJsonMimeType::None)
		, BufferView(nullptr)
	{
	}
};
