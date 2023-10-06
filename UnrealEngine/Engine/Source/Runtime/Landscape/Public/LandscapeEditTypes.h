// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "LandscapeEditTypes.generated.h"

UENUM()
enum class ELandscapeToolTargetType : uint8
{
	Heightmap = 0,
	Weightmap = 1,
	Visibility = 2,
	Invalid = 3 UMETA(Hidden), // only valid for LandscapeEdMode->CurrentToolTarget.TargetType
};

namespace UE::Landscape
{

enum class EOutdatedDataFlags : uint8
{
	None = 0,
	GrassMaps = (1 << 0),
	PhysicalMaterials = (1 << 1),
	NaniteMeshes = (1 << 2),
	// TODO [jonathan.bard] : DirtyActors = (1 << 3) ?
	All = (GrassMaps | PhysicalMaterials | NaniteMeshes /*| DirtyActors*/)
};
ENUM_CLASS_FLAGS(EOutdatedDataFlags);

} // namespace UE::Landscape