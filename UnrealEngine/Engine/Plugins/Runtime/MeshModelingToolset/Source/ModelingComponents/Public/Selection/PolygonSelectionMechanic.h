// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshTopologySelectionMechanic.h"
#include "PolygonSelectionMechanic.generated.h"

namespace UE::Geometry { struct FGeometrySelection; }

// DEPRECATED: Use UMeshTopologySelectionMechanicProperties
UCLASS(Deprecated)
class MODELINGCOMPONENTS_API UDEPRECATED_PolygonSelectionMechanicProperties : public UMeshTopologySelectionMechanicProperties
{
	GENERATED_BODY()

public:

	void Initialize(UPolygonSelectionMechanic* MechanicIn)
	{
		Mechanic = MechanicIn;
	}
};


/**
 * UPolygonSelectionMechanic implements the interaction for selecting a set of faces/vertices/edges
 * from a FGroupTopology.
 */
UCLASS()
class MODELINGCOMPONENTS_API UPolygonSelectionMechanic : public UMeshTopologySelectionMechanic
{
	GENERATED_BODY()

public:

	void Initialize(const FDynamicMesh3* Mesh,
		FTransform3d TargetTransform,
		UWorld* World,
		const FGroupTopology* Topology,
		TFunction<FDynamicMeshAABBTree3* ()> GetSpatialSourceFunc
	);

	void Initialize(UDynamicMeshComponent* MeshComponent, const FGroupTopology* Topology,
		TFunction<FDynamicMeshAABBTree3* ()> GetSpatialSourceFunc
	);

	// UMeshTopologySelectionMechanic
	virtual bool UpdateHighlight(const FRay& WorldRay) override;
	virtual bool UpdateSelection(const FRay& WorldRay, FVector3d& LocalHitPositionOut, FVector3d& LocalHitNormalOut) override;

	/**
	 * Convert the Active Selection to a PolyGroup-topology FGeometrySelection, with optional CompactMaps
	 */
	void GetSelection_AsGroupTopology(UE::Geometry::FGeometrySelection& SelectionOut, const FCompactMaps* CompactMapsToApply = nullptr) const;

	/**
	 * Convert the Active Selection to a Triangle-topology FGeometrySelection, with optional CompactMaps
	 */
	void GetSelection_AsTriangleTopology(UE::Geometry::FGeometrySelection& SelectionOut, const FCompactMaps* CompactMapsToApply = nullptr) const;


	/**
	 * Initialize the Active Selection based on the provided PolyGroup-topology FGeometrySelection
	 */
	void SetSelection_AsGroupTopology(const UE::Geometry::FGeometrySelection& Selection);

	/**
	 * Initialize the Active Selection based on the provided Triangle-topology FGeometrySelection
	 */
	void SetSelection_AsTriangleTopology(const UE::Geometry::FGeometrySelection& Selection);


private:

	// TODO: Would be nice to get rid of this and write everything in terms of TopologySelector and TopologyProvider
	const FGroupTopology* Topology;

};
