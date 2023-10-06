// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3sharp MeshEdgeSelection

#pragma once

#include "DynamicMesh/DynamicMesh3.h"
#include "CoreMinimal.h"
#include "Containers/Set.h"
#include "Selections/MeshVertexSelection.h"

namespace UE
{
namespace Geometry
{


/**
 * Currently a thin wrapper of a TSet<int> of Edge IDs paired with a Mesh; the backing storage will likely change as we need to optimize in the future
 */
class FMeshEdgeSelection
{
private:
	const FDynamicMesh3* Mesh;

	TSet<int> Selected;

public:

	FMeshEdgeSelection(const FDynamicMesh3* mesh)
	{
		Mesh = mesh;
	}


	// convert vertex selection to edge selection. Require at least minCount verts of edge to be selected
	GEOMETRYCORE_API FMeshEdgeSelection(const FDynamicMesh3* mesh, const FMeshVertexSelection& convertV, int minCount = 2);

	// convert face selection to edge selection. Require at least minCount tris of edge to be selected
	GEOMETRYCORE_API FMeshEdgeSelection(const FDynamicMesh3* mesh, const FMeshFaceSelection& convertT, int minCount = 1);



	TSet<int> AsSet() const
	{
		return Selected;
	}
	TArray<int> AsArray() const
	{
		return Selected.Array();
	}
	TBitArray<FDefaultBitArrayAllocator> AsBitArray() const
	{
		TBitArray<FDefaultBitArrayAllocator> Bitmap(false, Mesh->MaxEdgeID());
		for (int tid : Selected)
		{
			Bitmap[tid] = true;
		}
		return Bitmap;
	}

public:
	/**
	* DO NOT USE DIRECTLY
	* STL-like iterators to enable range-based for loop support.
	*/
	TSet<int>::TRangedForIterator      begin() { return Selected.begin(); }
	TSet<int>::TRangedForConstIterator begin() const { return Selected.begin(); }
	TSet<int>::TRangedForIterator      end() { return Selected.end(); }
	TSet<int>::TRangedForConstIterator end() const { return Selected.end(); }

private:
	void add(int eid)
	{
		Selected.Add(eid);
	}
	void remove(int eid)
	{
		Selected.Remove(eid);
	}


public:
	int Num()
	{
		return Selected.Num();
	}



	bool IsSelected(int eid)
	{
		return Selected.Contains(eid);
	}


	void Select(int eid)
	{
		ensure(Mesh->IsEdge(eid));
		if (Mesh->IsEdge(eid))
		{
			add(eid);
		}
	}
	
	void Select(const TArray<int>& edges)
	{
		for (int eid : edges)
		{
			if (Mesh->IsEdge(eid))
			{
				add(eid);
			}
		}
	}
	void Select(TArrayView<const int> edges)
	{
		for (int eid : edges)
		{
			if (Mesh->IsEdge(eid))
			{
				add(eid);
			}
		}
	}
	void Select(TFunctionRef<bool(int)> SelectF)
	{
		int NT = Mesh->MaxEdgeID();
		for (int eid = 0; eid < NT; ++eid)
		{
			if (Mesh->IsEdge(eid) && SelectF(eid))
			{
				add(eid);
			}
		}
	}

	void SelectVertexEdges(TArrayView<const int> vertices)
	{
		for (int vid : vertices) {
			for (int eid : Mesh->VtxEdgesItr(vid))
			{
				add(eid);
			}
		}
	}

	void SelectTriangleEdges(TArrayView<const int> Triangles)
	{
		for (int tid : Triangles)
		{
			FIndex3i et = Mesh->GetTriEdges(tid);
			add(et.A); add(et.B); add(et.C);
		}
	}


	GEOMETRYCORE_API void SelectBoundaryTriEdges(const FMeshFaceSelection& Triangles);

	void Deselect(int tid) {
		remove(tid);
	}
	void Deselect(TArrayView<const int> edges) {
		for (int tid : edges)
		{
			remove(tid);
		}
	}
	void DeselectAll()
	{
		Selected.Empty();
	}

};

} // end namespace UE::Geometry
} // end namespace UE
