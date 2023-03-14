// Copyright Epic Games, Inc. All Rights Reserved.

#include "Operations/OffsetMeshRegion.h"

#include "Algo/Reverse.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMeshEditor.h"
#include "Selections/MeshVertexSelection.h"
#include "DynamicMesh/DynamicMeshChangeTracker.h"
#include "Selections/MeshConnectedComponents.h"
#include "Operations/ExtrudeMesh.h"
#include "DynamicSubmesh3.h"

using namespace UE::Geometry;

namespace OffsetMeshRegionLocals
{
	bool EdgesAreParallel(FDynamicMesh3* Mesh, int32 Eid1, int32 Eid2)
	{
		FIndex2i Vids1 = Mesh->GetEdgeV(Eid1);
		FIndex2i Vids2 = Mesh->GetEdgeV(Eid2);

		FVector3d Vec1 = Mesh->GetVertex(Vids1.A) - Mesh->GetVertex(Vids1.B);
		FVector3d Vec2 = Mesh->GetVertex(Vids2.A) - Mesh->GetVertex(Vids2.B);
		if (!Vec1.Normalize(KINDA_SMALL_NUMBER) || !Vec2.Normalize(KINDA_SMALL_NUMBER))
		{
			// A degenerate edge is parallel enough for our purposes
			return true;
		}
		return FMath::Abs(Vec1.Dot(Vec2)) >= 1 - KINDA_SMALL_NUMBER;
	}
}

FOffsetMeshRegion::FOffsetMeshRegion(FDynamicMesh3* mesh) : Mesh(mesh)
{
}

bool FOffsetMeshRegion::Apply()
{
	FMeshConnectedComponents RegionComponents(Mesh);
	RegionComponents.FindConnectedTriangles(Triangles);

	bool bAllOK = true;
	OffsetRegions.SetNum(RegionComponents.Num());
	for (int k = 0; k < RegionComponents.Num(); ++k)
	{
		FOffsetInfo& Region = OffsetRegions[k];
		Region.OffsetTids = MoveTemp(RegionComponents.Components[k].Indices);

		if (bOffsetFullComponentsAsSolids)
		{
			TArray<int32> AllTriangles;
			FMeshConnectedComponents::GrowToConnectedTriangles(Mesh, Region.OffsetTids, AllTriangles);
			Region.bIsSolid = AllTriangles.Num() == Region.OffsetTids.Num();
		}

		bool bRegionOK = ApplyOffset(Region);

		bAllOK = bAllOK && bRegionOK;
	}

	return bAllOK;


}

bool FOffsetMeshRegion::ApplyOffset(FOffsetInfo& Region)
{
	// Store offset groups
	if (Mesh->HasTriangleGroups())
	{
		for (int32 Tid : Region.OffsetTids)
		{
			Region.OffsetGroups.AddUnique(Mesh->GetTriangleGroup(Tid));
		}
	}

	FMeshRegionBoundaryLoops InitialLoops(Mesh, Region.OffsetTids, false);
	bool bOK = InitialLoops.Compute();
	if (bOK == false)
	{
		return false;
	}

	AllModifiedAndNewTriangles.Append(Region.OffsetTids);

	// Before we start changing triangles, prepare by allocating group IDs that we'll use
	// for the stitched sides (doing it before changes to the mesh allows user-provided
	// LoopEdgesShouldHaveSameGroup functions to operate on the original mesh).
	TArray<TArray<int32>> LoopsEdgeGroups;
	TArray<int32> NewGroupIDs;
	LoopsEdgeGroups.SetNum(InitialLoops.Loops.Num());
	for (int32 i = 0; i < InitialLoops.Loops.Num(); ++i)
	{
		TArray<int32>& LoopEids = InitialLoops.Loops[i].Edges;
		int32 NumEids = LoopEids.Num();

		if (!ensure(NumEids > 2))
		{
			// Shouldn't actually happen because we're extruding triangles
			continue;
		}

		TArray<int32>& CurrentEdgeGroups = LoopsEdgeGroups[i];
		CurrentEdgeGroups.SetNumUninitialized(NumEids);
		CurrentEdgeGroups[0] = Mesh->AllocateTriangleGroup();
		NewGroupIDs.Add(CurrentEdgeGroups[0]);

		// Propagate the group backwards first so we don't allocate an unnecessary group
		// at the end and then have to fix it.
		int32 LastDifferentGroupIndex = NumEids - 1;
		while (LastDifferentGroupIndex > 0
			&& LoopEdgesShouldHaveSameGroup(LoopEids[0], LoopEids[LastDifferentGroupIndex]))
		{
			CurrentEdgeGroups[LastDifferentGroupIndex] = CurrentEdgeGroups[0];
			--LastDifferentGroupIndex;
		}

		// Now add new groups forward
		for (int32 j = 1; j <= LastDifferentGroupIndex; ++j)
		{
			if (!LoopEdgesShouldHaveSameGroup(LoopEids[j], LoopEids[j - 1]))
			{
				CurrentEdgeGroups[j] = Mesh->AllocateTriangleGroup();
				NewGroupIDs.Add(CurrentEdgeGroups[j]);
			}
			else
			{
				CurrentEdgeGroups[j] = CurrentEdgeGroups[j-1];
			}
		}
	}

	FDynamicMeshEditor Editor(Mesh);
	TArray<FDynamicMeshEditor::FLoopPairSet> LoopPairs;

	FDynamicMeshEditResult DuplicateResult;
	if (Region.bIsSolid)
	{
		// In the solid case, we want to duplicate the region so we can cap it.
		FMeshIndexMappings IndexMap;
		Editor.DuplicateTriangles(Region.OffsetTids, IndexMap, DuplicateResult);

		AllModifiedAndNewTriangles.Append(DuplicateResult.NewTriangles);

		// Populate LoopPairs
		LoopPairs.SetNum(InitialLoops.Loops.Num());
		for (int LoopIndex = 0; LoopIndex < InitialLoops.Loops.Num(); ++LoopIndex)
		{
			FEdgeLoop& BaseLoop = InitialLoops.Loops[LoopIndex];
			FDynamicMeshEditor::FLoopPairSet& LoopPair = LoopPairs[LoopIndex];

			// The original OffsetTids are the ones that are offset, so InnerVertices/Edges
			// should be the boundaries of those.
			LoopPair.InnerVertices = BaseLoop.Vertices;
			LoopPair.InnerEdges = BaseLoop.Edges;

			// However depending on whether we extruded down or up, we may need to reverse
			// the loops to get them to be stitched right side out.
			if (!bIsPositiveOffset)
			{
				Algo::Reverse(LoopPair.InnerVertices);

				// Reversing the edges is slightly different because the last edge is between the first
				// and last vertex, and that needs to stay in the same place when vertices are reversed.
				int32 LastEid = LoopPair.InnerEdges.Pop();
				Algo::Reverse(LoopPair.InnerEdges);
				LoopPair.InnerEdges.Add(LastEid);

				int32 LastEdgeGroupID = LoopsEdgeGroups[LoopIndex].Pop();
				Algo::Reverse(LoopsEdgeGroups[LoopIndex]);
				LoopsEdgeGroups[LoopIndex].Add(LastEdgeGroupID);
			}

			// Now assemble the paired loop
			for (int32 Vid : LoopPair.InnerVertices)
			{
				LoopPair.OuterVertices.Add(IndexMap.GetNewVertex(Vid));
			}
			FEdgeLoop::VertexLoopToEdgeLoop(Mesh, LoopPair.OuterVertices, LoopPair.OuterEdges);
		}
	}
	else
	{
		bOK = Editor.DisconnectTriangles(Region.OffsetTids, LoopPairs, true /*bHandleBoundaryVertices*/);
	}

	if (bOK == false)
	{
		return false;
	}

	// Store the vid-independent offset loop before we break bowties
	typedef TPair<int32, TPair<int8, int8>> TriVertPair;
	TArray<TArray<TriVertPair>> OffsetStitchSides;
	OffsetStitchSides.SetNum(LoopPairs.Num());
	for (int32 i = 0; i < LoopPairs.Num(); ++i)
	{
		bOK = bOK && FDynamicMeshEditor::ConvertLoopToTriVidPairSequence(*Mesh, LoopPairs[i].InnerVertices, LoopPairs[i].InnerEdges, OffsetStitchSides[i]);
	}

	if (bOK == false)
	{
		return false;
	}
	
	// Split bowties in the chosen region
	FDynamicMeshEditResult Result;
	Editor.SplitBowtiesAtTriangles(Region.OffsetTids, Result);
	bool bSomeLoopsBroken = Result.NewVertices.Num() > 0;

	// If we broke bowties, the loops in the offset region have changed, and our OffsetLoops no longer
	// match BaseLoops.
	if (bSomeLoopsBroken)
	{
		FMeshRegionBoundaryLoops UpdatedOffsetLoops(Mesh, Region.OffsetTids, false);
		bOK = UpdatedOffsetLoops.Compute();
		if (!bOK)
		{
			return false;
		}
		Region.OffsetLoops = UpdatedOffsetLoops.Loops;
	}

	FMeshVertexSelection SelectionV(Mesh);
	SelectionV.SelectTriangleVertices(Region.OffsetTids);
	TArray<int32> SelectedVids = SelectionV.AsArray();

	// If we need to, assemble the vertex vectors for us to use (before we actually start moving things)
	TArray<FVector3d> VertexExtrudeVectors;
	if (ExtrusionVectorType == EVertexExtrusionVectorType::SelectionTriNormalsAngleWeightedAverage
		|| ExtrusionVectorType == EVertexExtrusionVectorType::SelectionTriNormalsAngleWeightedAdjusted)
	{
		VertexExtrudeVectors.SetNumUninitialized(SelectedVids.Num());

		// Used to test which triangles are in selection
		TSet<int32> TriangleSet(Region.OffsetTids);

		for (int32 i = 0; i < SelectedVids.Num(); ++i)
		{
			int32 Vid = SelectedVids[i];
			FVector3d ExtrusionVector = FVector3d::Zero();

			// Get angle-weighted normalized average vector
			for (int32 Tid : Mesh->VtxTrianglesItr(Vid))
			{
				if (TriangleSet.Contains(Tid))
				{
					double Angle = Mesh->GetTriInternalAngleR(Tid, Mesh->GetTriangle(Tid).IndexOf(Vid));
					ExtrusionVector += Angle * Mesh->GetTriNormal(Tid);
				}
			}
			ExtrusionVector.Normalize();

			if (ExtrusionVectorType == EVertexExtrusionVectorType::SelectionTriNormalsAngleWeightedAdjusted)
			{
				// Perform an angle-weighted adjustment of the vector length. For each triangle normal, the
				// length needs to be multiplied by 1/cos(theta) to place the vertex in the plane that it
				// would be in if the face was moved a unit along triangle normal (where theta is angle of
				// triangle normal to the current extrusion vector).
				double AngleSum = 0;
				double Adjustment = 0;
				for (int32 Tid : Mesh->VtxTrianglesItr(Vid))
				{
					if (TriangleSet.Contains(Tid))
					{
						double Angle = Mesh->GetTriInternalAngleR(Tid, Mesh->GetTriangle(Tid).IndexOf(Vid));
						double CosTheta = Mesh->GetTriNormal(Tid).Dot(ExtrusionVector);

						double InvertedMaxScaleFactor = FMath::Max(FMathd::ZeroTolerance, 1.0 / MaxScaleForAdjustingTriNormalsOffset);
						if (CosTheta <= InvertedMaxScaleFactor)
						{
							CosTheta = InvertedMaxScaleFactor;
						}
						Adjustment += Angle / CosTheta;

						// For the average at the end
						AngleSum += Angle;
					}
				}
				Adjustment /= AngleSum;
				ExtrusionVector *= Adjustment;
			}

			VertexExtrudeVectors[i] = ExtrusionVector;
		}
	}
	else if (ExtrusionVectorType == EVertexExtrusionVectorType::VertexNormal)
	{
		VertexExtrudeVectors.SetNumUninitialized(SelectedVids.Num());
		for (int32 i = 0; i < SelectedVids.Num(); ++i)
		{
			int32 Vid = SelectedVids[i];
			VertexExtrudeVectors[i] = Mesh->HasVertexNormals() ? (FVector3d)Mesh->GetVertexNormal(Vid) : FMeshNormals::ComputeVertexNormal(*Mesh, Vid);
		}
	}

	// Perform the actual vertex displacement.
	for (int32 i = 0; i < SelectedVids.Num(); ++i)
	{
		int32 Vid = SelectedVids[i];
		FVector3d OldPosition = Mesh->GetVertex(Vid);
		FVector3d ExtrusionVector = (ExtrusionVectorType == EVertexExtrusionVectorType::Zero) ? FVector3d::Zero() : VertexExtrudeVectors[i];

		FVector3d NewPosition = OffsetPositionFunc(OldPosition, ExtrusionVector, Vid);
		Mesh->SetVertex(Vid, NewPosition);
	}

	// Stitch the loops

	bool bSuccess = true;
	int NumInitialLoops = LoopPairs.Num();
	Region.BaseLoops.SetNum(NumInitialLoops);
	if (!bSomeLoopsBroken)
	{
		Region.OffsetLoops.SetNum(NumInitialLoops);
	}
	Region.StitchTriangles.SetNum(NumInitialLoops);
	Region.StitchPolygonIDs.SetNum(NumInitialLoops);

	for (int32 LoopIndex = 0; LoopIndex < LoopPairs.Num(); ++LoopIndex)
	{
		FDynamicMeshEditor::FLoopPairSet& LoopPair = LoopPairs[LoopIndex];
		const TArray<int32>& EdgeGroups = LoopsEdgeGroups[LoopIndex];

		TArray<int32>& BaseLoopV = LoopPair.OuterVertices;
		TArray<int32>& OffsetLoopV = LoopPair.InnerVertices;

		TArray<TriVertPair>& OffsetLoopTriVertPairs = OffsetStitchSides[LoopIndex];

		// stitch the loops
		FDynamicMeshEditResult StitchResult;
		bool bStitchSuccess = Editor.StitchVertexLoopToTriVidPairSequence(OffsetLoopTriVertPairs, LoopPair.OuterVertices, StitchResult);
		if (!bStitchSuccess)
		{
			bSuccess = false;
			continue;
		}

		// set the groups of the new quads along the stitch
		int NumNewQuads = StitchResult.NewQuads.Num();
		for (int32 k = 0; k < NumNewQuads; k++)
		{
			Mesh->SetTriangleGroup(StitchResult.NewQuads[k].A, EdgeGroups[k]);
			Mesh->SetTriangleGroup(StitchResult.NewQuads[k].B, EdgeGroups[k]);
		}

		// save the stitch triangles set and associated group IDs
		StitchResult.GetAllTriangles(Region.StitchTriangles[LoopIndex]);
		Region.StitchPolygonIDs[LoopIndex] = NewGroupIDs;

		AllModifiedAndNewTriangles.Append(Region.StitchTriangles[LoopIndex]);

		// for each polygon we created in stitch, set UVs and normals
		if (Mesh->HasAttributes())
		{
			float AccumUVTranslation = 0;
			FFrame3d FirstProjectFrame;
			FVector3d FrameUp;

			for (int k = 0; k < NumNewQuads; k++)
			{
				FVector3f Normal = Editor.ComputeAndSetQuadNormal(StitchResult.NewQuads[k], true);

				// align axis 0 of projection frame to first edge, then for further edges,
				// rotate around 'up' axis to keep normal aligned and frame horizontal
				FFrame3d ProjectFrame;
				if (k == 0)
				{
					FVector3d FirstEdge = Mesh->GetVertex(BaseLoopV[1]) - Mesh->GetVertex(BaseLoopV[0]);
					Normalize(FirstEdge);
					FirstProjectFrame = FFrame3d(FVector3d::Zero(), (FVector3d)Normal);
					FirstProjectFrame.ConstrainedAlignAxis(0, FirstEdge, (FVector3d)Normal);
					FrameUp = FirstProjectFrame.GetAxis(1);
					ProjectFrame = FirstProjectFrame;
				}
				else
				{
					ProjectFrame = FirstProjectFrame;
					ProjectFrame.ConstrainedAlignAxis(2, (FVector3d)Normal, FrameUp);
				}

				if (k > 0)
				{
					AccumUVTranslation += (float)Distance(Mesh->GetVertex(BaseLoopV[k]), Mesh->GetVertex(BaseLoopV[k - 1]));
				}

				// translate horizontally such that vertical spans are adjacent in UV space (so textures tile/wrap properly)
				float TranslateU = UVScaleFactor * AccumUVTranslation;
				Editor.SetQuadUVsFromProjection(StitchResult.NewQuads[k], ProjectFrame, UVScaleFactor, FVector2f(TranslateU, 0));
			}
		}

		Region.BaseLoops[LoopIndex].InitializeFromVertices(Mesh, BaseLoopV);
		if (!bSomeLoopsBroken)
		{
			Region.OffsetLoops[LoopIndex].InitializeFromVertices(Mesh, OffsetLoopV);
		}
	}

	if (Region.bIsSolid)
	{
		if (bIsPositiveOffset)
		{
			// Flip the "bottom" of the region to face outwards
			Editor.ReverseTriangleOrientations(DuplicateResult.NewTriangles, true);
		}
		else
		{
			Editor.ReverseTriangleOrientations(Region.OffsetTids, true);
		}
	}

	return bSuccess;
}

bool FOffsetMeshRegion::EdgesSeparateSameGroupsAndAreColinearAtBorder(FDynamicMesh3* Mesh, 
	int32 Eid1, int32 Eid2, bool bCheckColinearityAtBorder)
{
	if (!Mesh->IsEdge(Eid1) || !Mesh->IsEdge(Eid2))
	{
		return ensure(false);
	}

	FIndex2i Tris1 = Mesh->GetEdgeT(Eid1);
	FIndex2i Groups1(Mesh->GetTriangleGroup(Tris1.A),
		Tris1.B == IndexConstants::InvalidID ? IndexConstants::InvalidID : Mesh->GetTriangleGroup(Tris1.B));

	FIndex2i Tris2 = Mesh->GetEdgeT(Eid2);
	FIndex2i Groups2(Mesh->GetTriangleGroup(Tris2.A), 
		Tris2.B == IndexConstants::InvalidID ? IndexConstants::InvalidID : Mesh->GetTriangleGroup(Tris2.B));

	if (bCheckColinearityAtBorder
		&& Groups1.A == Groups2.A
		&& Groups1.B == IndexConstants::InvalidID
		&& Groups2.B == IndexConstants::InvalidID)
	{
		return OffsetMeshRegionLocals::EdgesAreParallel(Mesh, Eid1, Eid2);
	}
	else return (Groups1.A == Groups2.A && Groups1.B == Groups2.B)
		|| (Groups1.A == Groups2.B && Groups1.B == Groups2.A);
}

