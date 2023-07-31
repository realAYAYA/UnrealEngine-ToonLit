// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataLayerType.generated.h"

UENUM(BlueprintType)
enum class EDataLayerType : uint8
{
	Runtime,
	Editor,
	Unknown UMETA(Hidden),

	Size UMETA(Hidden)
};