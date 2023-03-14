// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonObject.h"

struct GLTFEXPORTER_API IGLTFJsonIndexedObject : IGLTFJsonObject
{
	int32 Index;

protected:

	IGLTFJsonIndexedObject(int32 Index)
		: Index(Index)
	{
	}
};
