// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

struct FMeshDescription;

class UProceduralMeshComponent;

FMeshDescription
PROCEDURALMESHCOMPONENT_API
BuildMeshDescription( UProceduralMeshComponent* ProcMeshComp );

void
PROCEDURALMESHCOMPONENT_API
MeshDescriptionToProcMesh( const FMeshDescription& MeshDescription, UProceduralMeshComponent* ProcMeshComp );

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "MeshDescription.h"
#endif
