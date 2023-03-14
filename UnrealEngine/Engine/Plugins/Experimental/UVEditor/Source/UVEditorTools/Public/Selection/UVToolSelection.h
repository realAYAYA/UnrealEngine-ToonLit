// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ToolTargets/UVEditorToolMeshInput.h"
#include "Selection/StoredMeshSelectionUtil.h" //FEdgesFromTriangleSubIndices
#include "DynamicMesh/DynamicMesh3.h"
#include "Util/IndexUtil.h"

namespace UE {
namespace Geometry {

/**
 * Class that represents a selection in the canonical unwrap of a UV editor input object.
 */
class UVEDITORTOOLS_API FUVToolSelection
{
public:
	enum class EType
	{
		Vertex,
		Edge,
		Triangle
	};

	TWeakObjectPtr<UUVEditorToolMeshInput> Target = nullptr;
	EType Type = EType::Vertex;
	TSet<int32> SelectedIDs;

	void Clear()
	{
		Target = nullptr;
		SelectedIDs.Reset();
		StableEdgeIDs.Reset();
	}

	void SelectAll(const FDynamicMesh3& Mesh, EType TypeIn);

	bool IsAllSelected(const FDynamicMesh3& Mesh) const;

	bool IsEmpty() const
	{
		return SelectedIDs.IsEmpty();
	}

	bool HasStableEdgeIdentifiers() const
	{
		return !StableEdgeIDs.IsEmpty();
	}

	bool operator==(const FUVToolSelection& Other) const
	{
		return Target == Other.Target
			&& Type == Other.Type
			&& SelectedIDs.Num() == Other.SelectedIDs.Num()
			// Don't need to check the reverse because we checked Num above.
			&& SelectedIDs.Includes(Other.SelectedIDs); 
	}

	bool operator!=(const FUVToolSelection& Other) const
	{
		return !(*this == Other);
	}

	void SaveStableEdgeIdentifiers(const FDynamicMesh3& Mesh);

	void RestoreFromStableEdgeIdentifiers(const FDynamicMesh3& Mesh);

	bool AreElementsPresentInMesh(const FDynamicMesh3& Mesh) const;

	FAxisAlignedBox3d ToBoundingBox(const FDynamicMesh3& Mesh) const;
	/**
	 * Utility method to construct a new selection from an existing reference selection, only with a new geometry type.
	 */
	FUVToolSelection GetConvertedSelection(const FDynamicMesh3& Mesh, FUVToolSelection::EType ExpectedSelectionType) const;

protected:
	
	FMeshEdgesFromTriangleSubIndices StableEdgeIDs;
};

}} //end namespace UE::Geometry