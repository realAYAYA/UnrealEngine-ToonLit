// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.generated.h"

using FPCGTaskId = uint64;
static const FPCGTaskId InvalidPCGTaskId = (uint64)-1;

UENUM(meta = (Bitflags))
enum class EPCGChangeType : uint8
{
	None = 0,
	Cosmetic = 1 << 0,
	Settings = 1 << 1,
	Input = 1 << 2,
	Edge = 1 << 3,
	Node = 1 << 4,
	Structural = 1 << 5
};
ENUM_CLASS_FLAGS(EPCGChangeType);

UENUM(meta = (Bitflags))
enum class EPCGDataType : uint32
{
	None = 0 UMETA(Hidden),
	Point = 1 << 1,

	Spline = 1 << 2,
	LandscapeSpline = 1 << 3,
	PolyLine = Spline | LandscapeSpline UMETA(Hidden),

	Landscape = 1 << 4,
	Texture = 1 << 5,
	RenderTarget = 1 << 6,
	BaseTexture = Texture | RenderTarget UMETA(Hidden),
	Surface = Landscape | BaseTexture,

	Volume = 1 << 7,
	Primitive = 1 << 8,

	Spatial = Point | PolyLine | Surface | Volume | Primitive,

	Param = 1 << 27,
	Settings = 1 << 28 UMETA(Hidden),
	Other = 1 << 29,
	Any = (1 << 30) - 1
};
ENUM_CLASS_FLAGS(EPCGDataType);

namespace PCGPinConstants
{
	const FName DefaultInputLabel = TEXT("In");
	const FName DefaultOutputLabel = TEXT("Out");
	const FName DefaultParamsLabel = TEXT("Params");
}