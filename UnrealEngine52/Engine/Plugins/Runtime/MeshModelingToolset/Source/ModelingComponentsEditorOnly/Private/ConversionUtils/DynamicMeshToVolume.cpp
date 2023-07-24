// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConversionUtils/DynamicMeshToVolume.h"

#include "BSPOps.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/Polys.h"
#include "GameFramework/Volume.h"
#include "DynamicMesh/MeshNormals.h"
#include "MeshRegionBoundaryLoops.h"
#include "Model.h"
#include "Selections/MeshConnectedComponents.h"

#include "MeshSimplification.h"

using namespace UE::Geometry;

namespace UE {
namespace Conversion {

/**
 * A slightly modified version of FPoly::OptimizeIntoConvexPolys. Unlike the original, it won't 
 * arbitrarily delete colinear verts or snap things to grid. Like the original, it assumes that 
 * the passed in polys are convex and coplanar (usually just a triangulation), and it will not 
 * merge polygons sharing a multi-edge (colinear) side.
 *
 * @param	InOwnerBrush	The brush that owns the polygons.
 * @param	InPolygons		An array of FPolys that will be replaced with a new set of polygons that are merged together as much as possible.
 */
static void OptimizeIntoConvexPolys(ABrush* InOwnerBrush, TArray<FPoly>& InPolygons)
{
	bool bDidMergePolygons = true;

	while (bDidMergePolygons)
	{
		bDidMergePolygons = false;

		for (int32 PolyIndex = 0; PolyIndex < InPolygons.Num() && !bDidMergePolygons; ++PolyIndex)
		{
			const FPoly* PolyMain = &InPolygons[PolyIndex];

			// Find all the polygons that neighbor this one (aka share an edge)

			for (int32 p2 = PolyIndex + 1; p2 < InPolygons.Num() && !bDidMergePolygons; ++p2)
			{
				const FPoly* PolyNeighbor = &InPolygons[p2];

				if (PolyMain->Normal.Equals(PolyNeighbor->Normal))
				{
					// See if PolyNeighbor is sharing an edge with Poly

					int32 PolyMainIndex1 = INDEX_NONE, PolyMainIndex2 = INDEX_NONE;
					int32 PolyNeighborIndex1 = INDEX_NONE, PolyNeighborIndex2 = INDEX_NONE;

					int32 PolyMainNumVerts = PolyMain->Vertices.Num();
					int32 PolyNeighborNumVerts = PolyNeighbor->Vertices.Num();

					for (int32 v = 0; v < PolyMainNumVerts; ++v)
					{
						FVector3f vtx = PolyMain->Vertices[v];

						int32 idx = INDEX_NONE;

						for (int32 v2 = 0; v2 < PolyNeighborNumVerts; ++v2)
						{
							const FVector3f* vtx2 = &PolyNeighbor->Vertices[v2];

							if (vtx.Equals(*vtx2))
							{
								idx = v2;
								break;
							}
						}

						// TODO: Could clean this up. We only need to find the first shared vert
						// and then check the next neighbor (and maybe previous, if at start)
						if (idx != INDEX_NONE)
						{
							if (PolyMainIndex1 == INDEX_NONE)
							{
								PolyMainIndex1 = v;
								PolyNeighborIndex1 = idx;
							}
							else if (PolyMainIndex2 == INDEX_NONE)
							{
								PolyMainIndex2 = v;
								PolyNeighborIndex2 = idx;
								break;
							}
						}
					}

					// Check that we found two shared verts and that their indices are adjacent in PolyMain.
					if (!(PolyMainIndex1 != INDEX_NONE && PolyMainIndex2 != INDEX_NONE
						&& (PolyMainIndex1 + 1 == PolyMainIndex2 || (PolyMainIndex1 == 0 && PolyMainIndex2 == PolyMainNumVerts - 1))))
					{
						continue; // skip neighbor
					}

					// Swap indices around so that they correspond to the start and end of the edge counterclockwise
					// in PolyMain (this will only not be the case if the edge spans end and start of vert array).
					if (PolyMainIndex1 + 1 != PolyMainIndex2)
					{
						Swap(PolyMainIndex1, PolyMainIndex2);
						Swap(PolyNeighborIndex1, PolyNeighborIndex2);
					}

					// Check that the edge goes the opposite direction in PolyNeighbor
					if ((PolyNeighborIndex2 + 1) % PolyNeighborNumVerts != PolyNeighborIndex1)
					{
						continue; // skip neighbor
					}

					// Found a shared edge.  Let's see if we can merge these polygons together.
					// The most robust thing to do is to just go ahead and try the merge and see if FPoly::IsConvex() 
					// returns true. 

					FPoly PolyMerged = *PolyMain;
					int32 NumTotalVerts = PolyMainNumVerts + PolyNeighborNumVerts - 2;
					PolyMerged.Vertices.SetNumUninitialized(NumTotalVerts);

					// The new vertices get added after PolyMainIndex1. First shift over everything that
					// is to the right of PolyMainIndex1
					int32 NumVertsToShift = PolyMainNumVerts - 1 - PolyMainIndex1;
					for (int32 i = 0; i < NumVertsToShift; ++i)
					{
						PolyMerged.Vertices[NumTotalVerts - 1 - i] = PolyMerged.Vertices[PolyMainNumVerts - 1 - i];
					}
					// Now splice in PolyNeighbor vertices.
					for (int32 i = 1; i < PolyNeighborNumVerts - 1; ++i) // Skipping the shared verts
					{
						int32 NeighborIndex = (PolyNeighborIndex1 + i) % PolyNeighborNumVerts;
						PolyMerged.Vertices[PolyMainIndex1 + i] = PolyNeighbor->Vertices[NeighborIndex];
					}

					// Check if the result is both coplanar and convex, and then finalize it.
					if (PolyMerged.IsCoplanar() && PolyMerged.IsConvex() && PolyMerged.Finalize(InOwnerBrush, 1) == 0)
					{
						// Remove the original polygons from the list. Uses knowledge that p2 > PolyIndex
						InPolygons.RemoveAtSwap(p2);
						InPolygons.RemoveAtSwap(PolyIndex);

						// Add the newly merged polygon into the list
						InPolygons.Add(PolyMerged);

						// Tell the outside loop that we merged polygons and need to run through it all again
						bDidMergePolygons = true;
					}
				}
			}
		}
	}
}

/**
 * Largely copied from FGeomObject::FinalizeSourceData(). Does various cleanup
 * operations, particularly to make the volume polygons all planar and convex.
 */
static void ApplyLegacyVolumeCleanup(ABrush* brush)
{
	// Remove invalid polygons from the brush
	for (int32 x = 0; x < brush->Brush->Polys->Element.Num(); ++x)
	{
		FPoly* Poly = &brush->Brush->Polys->Element[x];
		if (Poly->Vertices.Num() < 3)
		{
			brush->Brush->Polys->Element.RemoveAt(x);
			x = -1;
		}
	}
	for (int32 p = 0; p < brush->Brush->Polys->Element.Num(); ++p)
	{
		FPoly* Poly = &(brush->Brush->Polys->Element[p]);
		Poly->iLink = p;
		int32 SaveNumVertices = Poly->Vertices.Num();
		if (!Poly->IsCoplanar() || !Poly->IsConvex())
		{
			// If the polygon is no longer coplanar and/or convex, break it up into separate triangles.
			FPoly WkPoly = *Poly;
			brush->Brush->Polys->Element.RemoveAt(p);
			TArray<FPoly> Polygons;
			if (WkPoly.Triangulate(brush, Polygons) > 0)
			{
				OptimizeIntoConvexPolys(brush, Polygons);
				for (int32 t = 0; t < Polygons.Num(); ++t)
				{
					brush->Brush->Polys->Element.Add(Polygons[t]);
				}
			}
			p = -1;
		}
		else
		{
			int32 FixResult = Poly->Fix();
			if (FixResult != SaveNumVertices)
			{
				// If the polygon collapses after running "Fix" against it, it needs to be
				// removed from the brushes polygon list.
				if (FixResult == 0)
				{
					brush->Brush->Polys->Element.RemoveAt(p);
				}
				p = -1;
			}
			else
			{
				// If we get here, the polygon is valid and needs to be kept.  Finalize its internals.
				Poly->Finalize(brush, 1);
			}
		}
	}
}

void DynamicMeshToVolume(const FDynamicMesh3& InputMesh, AVolume* TargetVolume, const FMeshToVolumeOptions& Options)
{
	FDynamicMesh3 LocalMesh;
	const FDynamicMesh3* UseMesh = &InputMesh;
	if (Options.bAutoSimplify && InputMesh.TriangleCount() > Options.MaxTriangles)
	{
		LocalMesh = InputMesh;
		LocalMesh.DiscardAttributes();

		// collapse to minimal planar first
		FQEMSimplification PlanarSimplifier(&LocalMesh);
		PlanarSimplifier.SimplifyToMinimalPlanar(0.1);

		if (LocalMesh.TriangleCount() > Options.MaxTriangles)
		{
			FVolPresMeshSimplification Simplifier(&LocalMesh);
			Simplifier.SimplifyToTriangleCount(Options.MaxTriangles);
		}

		UseMesh = &LocalMesh;
	}

	TArray<FDynamicMeshFace> Faces;
	GetPolygonFaces(*UseMesh, Faces, Options.bRespectGroupBoundaries);
	DynamicMeshToVolume(*UseMesh, Faces, TargetVolume);
}

void DynamicMeshToVolume(const FDynamicMesh3& InputMesh, TArray<FDynamicMeshFace>& Faces, AVolume* TargetVolume)
{
	check(TargetVolume->Brush);

	UModel* Model = TargetVolume->Brush;

	Model->Modify();

	Model->Initialize(TargetVolume);
	UPolys* Polys = Model->Polys;

	for (FDynamicMeshFace& Face : Faces)
	{
		int32 NumVertices = Face.BoundaryLoop.Num();
		FVector Normal = (FVector)Face.Plane.Z();
		FVector U = (FVector)Face.Plane.X();
		FVector V = (FVector)Face.Plane.Y();

		// create FPoly. This is Editor-only and I'm not entirely sure we need it?
		// int32 PolyIndex = Polys->Element.Num();
		FPoly NewPoly;
		NewPoly.Base = (FVector3f)Face.BoundaryLoop[0];
		NewPoly.Normal = (FVector3f)Normal;
		NewPoly.TextureU = (FVector3f)U;
		NewPoly.TextureV = (FVector3f)V;
		NewPoly.Vertices.SetNum(NumVertices);
		for (int32 k = 0; k < NumVertices; ++k)
		{
			NewPoly.Vertices[k] = (FVector3f)Face.BoundaryLoop[k];
		}
		NewPoly.PolyFlags = 0;
		NewPoly.iLink = NewPoly.iLinkSurf = NewPoly.iBrushPoly = -1;
		NewPoly.SmoothingMask = 0;
		Polys->Element.Add(NewPoly);

		/*  
		 *  // This is an alternate implementation that does not depend on Polys,
		 *  // and creates the FBspNode directly. However it's not clear that it
		 *  // works properly on non-convex shapes (or at all). Leaving this code
		 *  // here as we would like to have a path that is independent of Polys
		 *  // in the future (to potentially work at Runtime)

			// create points for this face in UModel::Points
			// TODO: can we share points between faces?
			TArray<int32> PointIndices;
			PointIndices.SetNum(NumVertices);
			for (int32 k = 0; k < NumVertices; ++k)
			{
				int32 NewIdx = Model->Points.Num();
				Model->Points.Add((FVector)Face.BoundaryLoop[k]);
				PointIndices[k] = NewIdx;
			}
			int32 BasePointIndex = PointIndices[0];

			// create normal for this face in UModel::Vectors along with U and V direction vectors
			int32 NormalIdx = Model->Vectors.Num();
			Model->Vectors.Add(Normal);
			int32 TextureUIdx = Model->Vectors.Num();
			Model->Vectors.Add(U);
			int32 TextureVIdx = Model->Vectors.Num();
			Model->Vectors.Add(V);

			// create FVerts for this face in UModel::Verts
			int32 iVertPoolStart = Model->Verts.Num();
			for (int32 k = 0; k < NumVertices; ++k)
			{
				FVert NewVert;
				NewVert.pVertex = PointIndices[k];		// Index of vertex point.
				NewVert.iSide = INDEX_NONE;				// If shared, index of unique side. Otherwise INDEX_NONE.
				NewVert.ShadowTexCoord = FVector2D::ZeroVector;			// The vertex's shadow map coordinate.
				NewVert.BackfaceShadowTexCoord = FVector2D::ZeroVector;	// The vertex's shadow map coordinate for the backface of the node.
				Model->Verts.Add(NewVert);
			}

			// create Surf

			int32 SurfIndex = Model->Surfs.Num();
			FBspSurf NewSurf;
			NewSurf.Material = nullptr;			// 4 Material.
			NewSurf.PolyFlags = 0;				// 4 Polygon flags.
			NewSurf.pBase = BasePointIndex;		// 4 Polygon & texture base point index (where U,V==0,0).
			NewSurf.vNormal = NormalIdx;		// 4 Index to polygon normal.
			NewSurf.vTextureU = TextureUIdx;	// 4 Texture U-vector index.
			NewSurf.vTextureV = TextureVIdx;	// 4 Texture V-vector index.
			//NewSurf.iBrushPoly = PolyIndex;		// 4 Editor brush polygon index.
			NewSurf.iBrushPoly = -1;
			//NewSurf.Actor = NewVolume;			// 4 Brush actor owning this Bsp surface.
			NewSurf.Actor = nullptr;
			NewSurf.Plane = FacePlane;			// 16 The plane this surface lies on.
			Model->Surfs.Add(NewSurf);


			// create nodes for this face in UModel::Nodes

			FBspNode NewNode;
			NewNode.Plane = FacePlane;					// 16 Plane the node falls into (X, Y, Z, W).
			NewNode.iVertPool = iVertPoolStart;			// 4  Index of first vertex in vertex pool, =iTerrain if NumVertices==0 and NF_TerrainFront.
			NewNode.iSurf = SurfIndex;					// 4  Index to surface information.
			NewNode.iVertexIndex = INDEX_NONE;			// The index of the node's first vertex in the UModel's vertex buffer.
														// This is initialized by UModel::UpdateVertices()
			NewNode.NumVertices = NumVertices;			// 1  Number of vertices in node.
			NewNode.NodeFlags = 0;						// 1  Node flags.
			Model->Nodes.Add(NewNode);
		*/
	}

	ApplyLegacyVolumeCleanup(TargetVolume);
	FBSPOps::bspUnlinkPolys(Model);

	// requires editor
	FBSPOps::csgPrepMovingBrush(TargetVolume);

	// do we need to do this?
	[[maybe_unused]] bool MarkedDirty = TargetVolume->MarkPackageDirty();
}


void GetPolygonFaces(const FDynamicMesh3& InputMesh, TArray<FDynamicMeshFace>& Faces, bool bRespectGroupBoundaries)
{
	Faces.SetNum(0);

	// We'll find faces using a connected component search based on normals. Note that we give
	// degenerate tris the normal of their neighbor so that they don't connect non-planar
	// components.

	FMeshNormals Normals(&InputMesh);
	Normals.ComputeTriangleNormals();
	Normals.SetDegenerateTriangleNormalsToNeighborNormal(); // See comment above

	double NormalTolerance = FMathf::ZeroTolerance;

	FMeshConnectedComponents Components(&InputMesh);
	Components.FindConnectedTriangles([&InputMesh, &Normals, NormalTolerance, bRespectGroupBoundaries](int32 Triangle0, int32 Triangle1)
	{
		return (!bRespectGroupBoundaries || InputMesh.GetTriangleGroup(Triangle0) == InputMesh.GetTriangleGroup(Triangle1))

			// This test is only performed if triangles share an edge, so checking the normal is 
			// sufficient for coplanarity.
			&& Normals[Triangle0].Dot(Normals[Triangle1]) >= 1 - NormalTolerance;
	});

	for (const FMeshConnectedComponents::FComponent& Component : Components)
	{
		FVector3d FaceNormal = Normals[Component.Indices[0]];
		FMeshRegionBoundaryLoops Loops(&InputMesh, Component.Indices);

		// The FDynamicMeshFaces we emit, and the FPolys we create from them, do
		// not support multiple boundary loops (eg a face with a hole). So in this
		// case we fall back to emitting each triangle separately. 
		// TODO: convex clustering of these triangles?
		if (Loops.Num() > 1)
		{
			for (int32 tid : Component.Indices)
			{
				FDynamicMeshFace Face;
				Face.BoundaryLoop.SetNum(3);
				InputMesh.GetTriVertices(tid, Face.BoundaryLoop[0], Face.BoundaryLoop[1], Face.BoundaryLoop[2]);
				Algo::Reverse(Face.BoundaryLoop);
				Face.Plane = FFrame3d(InputMesh.GetTriCentroid(tid), FaceNormal);
				Faces.Add(Face);
			}
		}
		else
		{
			const FEdgeLoop& Loop = Loops[0];
			FDynamicMeshFace Face;

			FVector3d AvgPos(0, 0, 0);
			for (int32 vid : Loop.Vertices)
			{
				FVector3d Position = InputMesh.GetVertex(vid);
				Face.BoundaryLoop.Add(Position);
				AvgPos += Position;
			}
			AvgPos /= (double)Loop.Vertices.Num();
			Algo::Reverse(Face.BoundaryLoop);

			Face.Plane = FFrame3d(AvgPos, FaceNormal);

			Faces.Add(Face);
		}
	}
}

void GetTriangleFaces(const FDynamicMesh3& InputMesh, TArray<FDynamicMeshFace>& Faces)
{
	Faces.SetNum(0);

	for (int32 tid : InputMesh.TriangleIndicesItr())
	{
		FVector3d A, B, C;
		InputMesh.GetTriVertices(tid, A, B, C);
		FVector3d Centroid, Normal; double Area;
		InputMesh.GetTriInfo(tid, Normal, Area, Centroid);

		FDynamicMeshFace Face;
		Face.Plane = FFrame3d(Centroid, Normal);
		Face.BoundaryLoop.Add(A);
		Face.BoundaryLoop.Add(C);
		Face.BoundaryLoop.Add(B);

		Faces.Add(Face);
	}
}

}//end namespace UE::Conversion
}//end namespace UE