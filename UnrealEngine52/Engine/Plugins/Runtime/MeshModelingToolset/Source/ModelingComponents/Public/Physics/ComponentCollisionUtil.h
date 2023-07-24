// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "TransformTypes.h"
#include "TransformSequence.h"
#include "DynamicMesh/DynamicMesh3.h"

class UBodySetup;
class UStaticMesh;
struct FKAggregateGeom;
class IInterface_CollisionDataProvider;

namespace UE
{
namespace Geometry
{

struct FSimpleShapeSet3d;

/**
 * Component/BodySetup collision settings (eg StaticMeshComponent) we might need to pass through the functions below
 */
struct MODELINGCOMPONENTS_API FComponentCollisionSettings
{
	int32 CollisionTypeFlag = 0;		// this is ECollisionTraceFlag
	bool bIsGeneratedCollision = true;
};

/**
 * @return true if the component type supports collision settings
 */
MODELINGCOMPONENTS_API bool ComponentTypeSupportsCollision(
	const UPrimitiveComponent* Component);


/**
 * @return current Component collision settings
 */
MODELINGCOMPONENTS_API FComponentCollisionSettings GetCollisionSettings(
	const UPrimitiveComponent* Component);


/**
 * Apply Transform to any Simple Collision geometry owned by Component.
 * Note that Nonuniform scaling support is very limited and will generally enlarge collision volumes.
 */
MODELINGCOMPONENTS_API bool TransformSimpleCollision(
	UPrimitiveComponent* Component,
	const FTransform3d& Transform);

/**
 * Replace existing Simple Collision geometry in Component with that defined by ShapeSet,
 * and update the Component/BodySetup collision settings
 */
MODELINGCOMPONENTS_API bool SetSimpleCollision(
	UPrimitiveComponent* Component,
	const FSimpleShapeSet3d* ShapeSet,
	FComponentCollisionSettings CollisionSettings = FComponentCollisionSettings() );


/**
 * Apply Transform to the existing Simple Collision geometry in Component and then append to to ShapeSetOut
 */
MODELINGCOMPONENTS_API bool AppendSimpleCollision(
	const UPrimitiveComponent* SourceComponent,
	FSimpleShapeSet3d* ShapeSetOut,
	const FTransform3d& Transform);

/**
 * Apply TransformSequence (in-order) to the existing Simple Collision geometry in Component and then append to to ShapeSetOut
 */
MODELINGCOMPONENTS_API bool AppendSimpleCollision(
	const UPrimitiveComponent* SourceComponent,
	FSimpleShapeSet3d* ShapeSetOut,
	const TArray<FTransform3d>& TransformSeqeuence);

/**
 * Replace existing Simple Collision geometry in BodySetup with that defined by NewGeometry,
 * and update the Component/BodySetup collision settings. Optional StaticMesh argument allows
 * for necessary updates to it and any active UStaticMeshComponent that reference it
 */
MODELINGCOMPONENTS_API void UpdateSimpleCollision(
	UBodySetup* BodySetup,
	const FKAggregateGeom* NewGeometry,
	UStaticMesh* StaticMesh = nullptr,
	FComponentCollisionSettings CollisionSettings = FComponentCollisionSettings());

/**
 * @return BodySetup on the given SourceComponent, or nullptr if no BodySetup was found
 */
MODELINGCOMPONENTS_API const UBodySetup* GetBodySetup(const UPrimitiveComponent* SourceComponent);
/**
 * @return BodySetup on the given SourceComponent, or nullptr if no BodySetup was found
 */
MODELINGCOMPONENTS_API UBodySetup* GetBodySetup(UPrimitiveComponent* SourceComponent);

/**
 * Extract the simple collision geometry from AggGeom as meshes (ie spheres and capsules are tessellated) and
 * accumulate into MeshOut. TransformSequence is applied to the meshes as they are accumulated.
 * @param bSetToPerTriangleNormals if true and the mesh has a normals attribute overlay,  the mesh is set to face normals, otherwise averaged vertex normals
 * @param bInitializeConvexUVs if true convex hulls have their UVs initialized to per-face planar projections, otherwise no UVs are set
 * @param PerElementMeshCallback if provided, called with each element mesh before transforming/appending to MeshOut. The int parameter is the shape type, 0=Sphere, 1=Box, 2=Capsule, 3=Convex
 */
MODELINGCOMPONENTS_API void ConvertSimpleCollisionToMeshes(
	const FKAggregateGeom& AggGeom,
	UE::Geometry::FDynamicMesh3& MeshOut,
	const FTransformSequence3d& TransformSeqeuence,
	int32 SphereResolution = 16,
	bool bSetToPerTriangleNormals = false,
	bool bInitializeConvexUVs = false,
	TFunction<void(int, const FDynamicMesh3&)> PerElementMeshCallback = nullptr );

/**
 * Extract the complex collision geometry from CollisionDataProvider as a dynamic mesh.
 * TransformSequence is applied to the mesh.
 * @param bFoundMeshErrorsOut will be returned as true if potential non-manifold geometry was detected during construction (in that case some triangles will be disconnected)
 * @param bWeldEdges if true the mesh edges are welded
 * @param bSetToPerTriangleNormals if true and the mesh has a normals attribute overlay, the mesh is set to face normals, otherwise averaged vertex normals
 */
MODELINGCOMPONENTS_API bool ConvertComplexCollisionToMeshes(
	IInterface_CollisionDataProvider* CollisionDataProvider,
	UE::Geometry::FDynamicMesh3& MeshOut,
	const FTransformSequence3d& TransformSeqeuence,
	bool& bFoundMeshErrorsOut,
	bool bWeldEdges = true,
	bool bSetToPerTriangleNormals = false);

}  // end namespace Geometry
}  // end namespace UE