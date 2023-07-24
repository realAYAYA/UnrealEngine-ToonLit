// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGEditorCommon.generated.h"

UENUM()
enum class EPCGElementType : uint8
{
	Native = 1 << 0,
	Blueprint = 1 << 1,
	Subgraph = 1 << 2,
	Settings = 1 << 3,
	Other = 1 << 4,
	All = (1 << 5) - 1
};
ENUM_CLASS_FLAGS(EPCGElementType);

/** Used to make sure UHT generates properly */
USTRUCT()
struct FPCGEditorCommonDummyStruct
{
	GENERATED_BODY()
};

namespace FPCGEditorCommon
{
	const FString ContextIdentifier = TEXT("PCGEditorContext");

	const FName SpatialDataType = FName(TEXT("Spatial Data"));
	const FName ParamDataType = FName(TEXT("Attribute Set"));
	const FName SettingsDataType = FName(TEXT("Settings Data"));
	const FName OtherDataType = FName(TEXT("Other Data"));

	const FName ConcreteDataType = FName(TEXT("Concrete Data"));
	const FName PointDataType = FName(TEXT("Point Data"));
	const FName PolyLineDataType = FName(TEXT("Poly Line Data"));
	const FName SurfaceDataType = FName(TEXT("Surface Data"));
	const FName LandscapeDataType = FName(TEXT("Landscape Data"));
	const FName TextureDataType = FName(TEXT("Texture Data"));
	const FName RenderTargetDataType = FName(TEXT("Render Target Data"));
	const FName VolumeDataType = FName(TEXT("Volume Data"));
	const FName PrimitiveDataType = FName(TEXT("Primitive Data"));	
};
