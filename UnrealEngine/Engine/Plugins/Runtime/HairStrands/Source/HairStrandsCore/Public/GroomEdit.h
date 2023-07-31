// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"

class UGroomAsset;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Strands
struct FEditableHairStrandControlPoint
{
	FVector3f Position;
	float Radius;
	float U;
	FLinearColor BaseColor;
	float Roughness;
};

struct FEditableHairStrand
{
	TArray<FEditableHairStrandControlPoint> ControlPoints;

	uint32 StrandID;
	FVector2f RootUV;

	// Pre-computed Simulation
	uint32 GuideIDs[3];
	float GuideWeights[3];
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Guides
struct FEditableHairGuideControlPoint
{
	FVector3f Position;
	float U;
};

struct FEditableHairGuide
{
	TArray<FEditableHairGuideControlPoint> ControlPoints;

	uint32 GuideID;
	FVector2f RootUV;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Group & Groom
struct FEditableGroomGroup
{
	uint32 GroupID;
	FName GroupName;
	TArray<FEditableHairStrand> Strands;
	TArray<FEditableHairGuide> Guides;
};

struct FEditableGroom
{
	TArray<FEditableGroomGroup> Groups;
};

enum EEditableGroomOperations
{
	ControlPoints_Added    = 0x1,
	ControlPoints_Modified = 0x2,
	ControlPoints_Deleted  = 0x4,

	Strands_Added          = 0x8,
	Strands_Modified       = 0x10,
	Strands_Deleted        = 0x20,

	Group_Added            = 0x40,
	Group_Deleted          = 0x80
};

// Convert a groom asset into an editable groom asset
HAIRSTRANDSCORE_API void ConvertFromGroomAsset(UGroomAsset* In, FEditableGroom* Out);

// Convert an editable groom asset into a groom asset
// The 'operations' flag indicates what type of modifications have been done onto the editable groom. 
// This helps to drive the update mechanism.
HAIRSTRANDSCORE_API void ConvertToGroomAsset(UGroomAsset* Out, const FEditableGroom* In, uint32 Operations);