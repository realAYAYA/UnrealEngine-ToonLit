// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Json/GLTFJsonEnums.h"

struct GLTFEXPORTER_API FGLTFJsonExtensions
{
	TSet<EGLTFJsonExtension> Used;
	TSet<EGLTFJsonExtension> Required;
};
