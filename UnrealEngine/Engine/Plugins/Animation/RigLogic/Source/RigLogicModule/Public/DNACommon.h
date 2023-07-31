// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UObject/ObjectMacros.h"

#include "DNACommon.generated.h"

UENUM(BlueprintType)
enum class EArchetype: uint8
{
	Asian,
	Black,
	Caucasian,
	Hispanic,
	Alien,
	Other
};

UENUM(BlueprintType)
enum class EGender: uint8
{
	Male,
	Female,
	Other
};

UENUM(BlueprintType)
enum class ETranslationUnit: uint8
{
	CM,
	M
};

UENUM(BlueprintType)
enum class ERotationUnit: uint8
{
	Degrees,
	Radians
};

UENUM(BlueprintType)
enum class EDirection: uint8
{
	Left,
	Right,
	Up,
	Down,
	Front,
	Back
};

UENUM(BlueprintType)
enum class EDNADataLayer : uint8
{
	Descriptor,
	Definition,  // Includes Descriptor
	Behavior,  // Includes Descriptor and Definition
	Geometry,  // Includes Descriptor and Definition
	GeometryWithoutBlendShapes,  // Includes Descriptor and Definition
	AllWithoutBlendShapes,  // Includes everything except blend shapes from Geometry
	All
};

USTRUCT(BlueprintType)
struct FCoordinateSystem
{
	GENERATED_BODY()

	FCoordinateSystem() : XAxis(), YAxis(), ZAxis()
	{
	}

	FCoordinateSystem(EDirection XAxis, EDirection YAxis, EDirection ZAxis) : XAxis(XAxis), YAxis(YAxis), ZAxis(ZAxis)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	EDirection XAxis;
	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	EDirection YAxis;
	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	EDirection ZAxis;
};

USTRUCT(BlueprintType)
struct FMeshBlendShapeChannelMapping
{
	GENERATED_BODY()

	FMeshBlendShapeChannelMapping() : MeshIndex(), BlendShapeChannelIndex()
	{
	}

	FMeshBlendShapeChannelMapping(int32 MeshIndex, int32 BlendShapeChannelIndex) : MeshIndex(MeshIndex), BlendShapeChannelIndex(BlendShapeChannelIndex)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	int32 MeshIndex;
	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	int32 BlendShapeChannelIndex;
};

USTRUCT(BlueprintType)
struct FTextureCoordinate
{
	GENERATED_BODY()

	FTextureCoordinate() : U(), V()
	{
	}

	FTextureCoordinate(float U, float V) : U(U), V(V)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	float U;
	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	float V;
};

USTRUCT(BlueprintType)
struct FVertexLayout
{
	GENERATED_BODY()

	FVertexLayout() : Position(), TextureCoordinate(), Normal()
	{
	}

	FVertexLayout(int32 Position, int32 TextureCoordinate, int32 Normal) : Position(Position), TextureCoordinate(TextureCoordinate), Normal(Normal)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	int32 Position;
	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	int32 TextureCoordinate;
	UPROPERTY(BlueprintReadOnly, Category = "RigLogic")
	int32 Normal;
};
