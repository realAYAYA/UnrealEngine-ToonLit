// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include <memory>
/**
 * 
 */
struct TEXTUREGRAPHENGINE_API MaterialInfo
{
	FString							name;
	int32							id;
};

typedef std::shared_ptr<MaterialInfo>		MaterialInfoPtr;

