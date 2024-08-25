// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "MeshTypes.h"
#include "MeshDescription.h"

namespace MeshOperator
{

void FixNonManifoldMesh(FMeshDescription& MeshDescription);

bool OrientMesh(FMeshDescription& MeshDescription);

void RecomputeNullNormal(FMeshDescription& MeshDescription);

void ResolveTJunctions(FMeshDescription& MeshDescription, double Tolerance);

}