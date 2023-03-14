// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IndexTypes.h"
#include "SegmentTypes.h"

#include "PersistentMeshSelection.generated.h"

class UPrimitiveComponent;
class UInteractiveTool;
PREDECLARE_USE_GEOMETRY_STRUCT(FGroupTopologySelection);
PREDECLARE_USE_GEOMETRY_CLASS(FGroupTopology);
PREDECLARE_USE_GEOMETRY_CLASS(FCompactMaps);




/**
 * FGenericMeshSelection represents various types of selection on a Mesh.
 * This includes various types of indices that could be interpreted in different ways.
 * 
 * In addition, "Render" geometry is stored, which can be used by higher-level
 * code to draw the selection in some way (eg a selection highlight)
 * 
 * @warning this class is likely to change in the future
 */
struct MODELINGCOMPONENTS_API FGenericMeshSelection
{
	// selection type
	enum class ETopologyType
	{
		FGroupTopology,
		FTriangleGroupTopology,
		FUVGroupTopology,
	};



	//
	// Selection representation (may be interpreted differently depending on TopologyType)
	//

	// selection type
	ETopologyType TopologyType = ETopologyType::FGroupTopology;

	// selected vertices or "corners" (eg of polygroup topology)
	TArray<int32> VertexIDs;
	// selected edges, represented as index pairs because for many selections, 
	// using a pair of vertices defining/on the edge is more reliable (due to unstable edge IDs)
	TArray<UE::Geometry::FIndex2i> EdgeIDs;
	// selected triangles/faces/regions
	TArray<int32> FaceIDs;


	//
	// Selection Target Information
	//

	// Component this selection applies to (eg that owns mesh, etc)
	UPrimitiveComponent* SourceComponent = nullptr;

	//
	// Renderable Selection Representation
	//

	// set of 3D points representing selection (in world space)
	TArray<FVector3d> RenderVertices;
	// set of 3D lines representing selection (in world space)
	TArray<UE::Geometry::FSegment3d> RenderEdges;


	/** @return selected GroupIDs */
	const TArray<int32>& GetGroupIDs() const { return FaceIDs; }

	/** @return true if selection is empty */
	bool IsEmpty() const
	{
		return VertexIDs.IsEmpty() && EdgeIDs.IsEmpty() && FaceIDs.IsEmpty();
	}

	/** @return true if selection has 3D lines that can be rendered */
	bool HasRenderableLines() const { return RenderEdges.Num() > 0; }


	bool operator==(const FGenericMeshSelection& Other) const
	{
		return SourceComponent == Other.SourceComponent
			&& TopologyType == Other.TopologyType
			&& VertexIDs == Other.VertexIDs
			&& EdgeIDs == Other.EdgeIDs
			&& FaceIDs == Other.FaceIDs;
	}

};





/**
 * UPersistentMeshSelection is a UObject wrapper for a FGenericMeshSelection
 */
UCLASS()
class MODELINGCOMPONENTS_API UPersistentMeshSelection : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Resets the contents of the object using the given selection.
	 *
	 * @param CompactMaps If the mesh was compacted without updating the passed in topology object, these
	 *  maps will be used to give an object that will work with the new mesh vids.
	 */
	void SetSelection(const FGroupTopology& TopologyIn, const FGroupTopologySelection& SelectionIn, const FCompactMaps* CompactMaps = nullptr);

	/**
	 * Initializes a FGroupTopologySelection using the current contents of the object. The topology
	 * must already be initialized.
	 */
	void ExtractIntoSelectionObject(const FGroupTopology& TopologyIn, FGroupTopologySelection& SelectionOut) const;

	/** @return true if the selection is empty */
	bool IsEmpty() const
	{
		return Selection.IsEmpty();
	}

	UPrimitiveComponent* GetTargetComponent() const { return Selection.SourceComponent; }
	FGenericMeshSelection::ETopologyType GetSelectionType() const { return Selection.TopologyType; }

	/** replace the internal Selection data */
	void SetSelection(const FGenericMeshSelection& SelectionIn) { Selection = SelectionIn; }

	/** @return the internal Selection data */
	const FGenericMeshSelection& GetSelection() const { return Selection; }

protected:
	FGenericMeshSelection Selection;
};

