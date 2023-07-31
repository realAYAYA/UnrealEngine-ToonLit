// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

struct GLTFEXPORTER_API FGLTFJsonAsset : IGLTFJsonObject
{
	FString Version;
	FString Generator;
	FString Copyright;

	FGLTFJsonAsset()
		: Version(TEXT("2.0"))
	{
	}

	virtual void WriteObject(IGLTFJsonWriter& Writer) const override;
};
