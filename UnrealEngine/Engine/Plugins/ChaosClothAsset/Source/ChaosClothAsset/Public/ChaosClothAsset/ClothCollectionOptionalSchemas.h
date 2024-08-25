// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Misc/EnumClassFlags.h"

namespace UE::Chaos::ClothAsset
{
	/** Cloth collection optional schemas. */
	enum class EClothCollectionOptionalSchemas : uint8
	{
		None = 0,
		RenderDeformer = 1 << 1,
		Solvers = 1 << 2,
		Fabrics = 1 << 3
	};
	ENUM_CLASS_FLAGS(EClothCollectionOptionalSchemas)
}  // End namespace UE::Chaos::ClothAsset
