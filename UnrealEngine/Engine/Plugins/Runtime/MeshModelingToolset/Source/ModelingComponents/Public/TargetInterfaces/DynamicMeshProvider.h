// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "GeometryBase.h"

#include "DynamicMeshProvider.generated.h"

PREDECLARE_GEOMETRY(class FDynamicMesh3);

UINTERFACE()
class MODELINGCOMPONENTS_API UDynamicMeshProvider : public UInterface
{
	GENERATED_BODY()
};

class MODELINGCOMPONENTS_API IDynamicMeshProvider
{
	GENERATED_BODY()

public:
	/**
	 * Gives a copy of a dynamic mesh for tools to operate on.
	 */
	virtual UE::Geometry::FDynamicMesh3 GetDynamicMesh() = 0;
};
