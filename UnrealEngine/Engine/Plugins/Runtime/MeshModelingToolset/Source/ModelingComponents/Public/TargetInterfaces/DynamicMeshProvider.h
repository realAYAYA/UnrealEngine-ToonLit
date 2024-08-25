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

	/**
	 * Gives a copy of a dynamic mesh for tools to operate on.
	 * 
	 * @param bRequestTangents Request tangents on the returned mesh. Not required if tangents are not on the source data and the provider does not have a standard way to generate them.
	 *
	 * Note: Default implementation simply returns GetDynamicMesh(). Overloaded implementations for e.g., Static and Skeletal Mesh sources will enable (and compute if needed) additional tangent data.
	 */
	virtual UE::Geometry::FDynamicMesh3 GetDynamicMesh(bool bRequestTangents);
};
