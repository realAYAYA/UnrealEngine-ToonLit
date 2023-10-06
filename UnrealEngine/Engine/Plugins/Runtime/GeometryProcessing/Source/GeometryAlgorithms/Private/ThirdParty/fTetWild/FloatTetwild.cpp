// This file is part of fTetWild, a software for generating tetrahedral meshes.
//
// Copyright (C) 2019 Yixin Hu <yixin.hu@nyu.edu>
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.
//

#include "FloatTetwild.h"
#include "ThirdParty/fTetWild/AABBWrapper.h"
#include "ThirdParty/fTetWild/FloatTetDelaunay.h"
#include "ThirdParty/fTetWild/MeshImprovement.h"
#include "ThirdParty/fTetWild/Parameters.h"
#include "ThirdParty/fTetWild/Simplification.h"
#include "ThirdParty/fTetWild/TriangleInsertion.h"
#include "ThirdParty/fTetWild/LocalOperations.h"
#include "ThirdParty/fTetWild/Statistics.h"
#include "ThirdParty/fTetWild/Types.hpp"
#include <fstream>

#define LOCTEXT_NAMESPACE "FloatTetWild"

namespace floatTetWild {

bool tetrahedralization(
	std::vector<Vector3>& input_vertices,
	std::vector<Vector3i>& input_faces,
	std::vector<int>& input_tags,
	Parameters& params,
	bool bInvertOutputTets,
	TArray<FVector>& OutVertices,
	TArray<FIntVector4>& OutTets,
	TArray<FVector>* OutSurfaceVertices,
	TArray<FIntVector3>* OutSurfaceTris,
	int boolean_op,
	bool skip_simplify,
	FProgressCancel* Progress)
{
	FProgressCancel::FProgressScope ScopeInitAABBTree = FProgressCancel::CreateScopeTo(Progress, .05f, LOCTEXT("AABBInit", "Preparing Acceleration Structures"));

    if (input_vertices.empty() || input_faces.empty()) {
        return false;
    }

    AABBWrapper      tree(input_vertices, input_faces);
	if (input_tags.empty())
	{
		input_tags.resize(input_faces.size());
		std::fill(input_tags.begin(), input_tags.end(), 0);
	}
    if (!params.init(tree.get_sf_diag())) {
        return false;
    }

	if (Progress && Progress->Cancelled())
	{
		return false;
	}
	ScopeInitAABBTree.Done();

    /////////////////////////////////////////////////
    // STEP 1: Preprocessing (mesh simplification) //
    /////////////////////////////////////////////////

	FProgressCancel::FProgressScope ScopeInitSimplify = FProgressCancel::CreateScopeTo(Progress, .1f, LOCTEXT("SimplifyInit", "Creating Initial Simplified Mesh"));
    Mesh mesh;
    mesh.params = params;
    simplify(input_vertices, input_faces, input_tags, tree, mesh.params, skip_simplify);
    tree.init_b_mesh_and_tree(input_vertices, input_faces, mesh);

	if (Progress && Progress->Cancelled())
	{
		return false;
	}
	ScopeInitSimplify.Done();

    ///////////////////////////////////////
    // STEP 2: Volume tetrahedralization //
    ///////////////////////////////////////

	FProgressCancel::FProgressScope ScopeInitTet = FProgressCancel::CreateScopeTo(Progress, .15f, LOCTEXT("TetInit", "Creating Initial Delaunay Mesh"));
    std::vector<bool> is_face_inserted(input_faces.size(), false);

    FloatTetDelaunay::tetrahedralize(input_vertices, input_faces, tree, mesh, is_face_inserted);

	if (Progress && Progress->Cancelled())
	{
		return false;
	}
	ScopeInitTet.Done();

    /////////////////////
    // STEP 3: Cutting //
    /////////////////////

	FProgressCancel::FProgressScope ScopeCutTet = FProgressCancel::CreateScopeTo(Progress, .2f, LOCTEXT("TetCutting", "Cutting Tetrahedra With Surface Mesh"));
	insert_triangles(input_vertices, input_faces, input_tags, mesh, is_face_inserted, tree, false);

	if (Progress && Progress->Cancelled())
	{
		return false;
	}
	ScopeCutTet.Done();

	//////////////////////////////////////
    // STEP 4: Volume mesh optimization //
    //////////////////////////////////////

	FProgressCancel::FProgressScope ScopeOptimizeTet = FProgressCancel::CreateScopeTo(Progress, .9f, LOCTEXT("TetOptimizing", "Optimizing Tetrahedra"));
    optimization(input_vertices, input_faces, input_tags, is_face_inserted, mesh, tree, {{1, 1, 1, 1}});

	if (Progress && Progress->Cancelled())
	{
		return false;
	}

	if (params.correct_tracked_surface_orientation)
	{
		correct_tracked_surface_orientation(mesh, tree);
	}

	if (Progress && Progress->Cancelled())
	{
		return false;
	}
	ScopeOptimizeTet.Done();

    /////////////////////////////////
    // STEP 5: Interior extraction //
    /////////////////////////////////

	FProgressCancel::FProgressScope ScopeFilteringTets = FProgressCancel::CreateScopeTo(Progress, .95f, LOCTEXT("TetFilter", "Filtering Inside Tetrahedra"));
	// Note: ScopeExtractResults is closed by the 
    if (boolean_op < 0) {
        if (params.smooth_open_boundary) {
            smooth_open_boundary(mesh, tree);
            for (auto &t: mesh.tets) {
                if (t.is_outside)
                    t.is_removed = true;
            }
        } else {
            if(!params.disable_filtering) {
                if(params.use_floodfill) {
                    filter_outside_floodfill(mesh);
                } else if(params.use_input_for_wn){
                    filter_outside(mesh, input_vertices, input_faces);
                } else
                    filter_outside(mesh);
            }
        }
    } else {
        boolean_operation(mesh, boolean_op);
    }

	if (Progress && Progress->Cancelled())
	{
		return false;
	}
	ScopeFilteringTets.Done();

	// Extract results
	FProgressCancel::FProgressScope ScopeFinalize = FProgressCancel::CreateScopeTo(Progress, 1.0f, LOCTEXT("ExtractFinalTetrahedra", "Extracting Results"));

	TArray<int> VIDMap;
	VIDMap.Init(-1, mesh.tet_vertices.size());
	OutVertices.Reset();
	OutTets.Reset();
	OutVertices.Reserve(mesh.get_v_num());
	OutTets.Reserve(mesh.get_t_num());
	for (int VID = 0, NumV = mesh.tet_vertices.size(); VID < NumV; ++VID)
	{
		if (!mesh.tet_vertices[VID].is_removed)
		{
			const Vector3& V = mesh.tet_vertices[VID].pos;
			VIDMap[VID] = OutVertices.Add(FVector(V[0], V[1], V[2]));
		}
	}
	for (int TID = 0, NumT = mesh.tets.size(); TID < NumT; ++TID)
	{
		if (!mesh.tets[TID].is_removed)
		{
			const Vector4i& T = mesh.tets[TID].indices;
			if (!bInvertOutputTets) // UE expects tets in the opposite orientation generally, so not inverted == flipped vs ftetwild
			{
				OutTets.Emplace(VIDMap[T[0]], VIDMap[T[2]], VIDMap[T[1]], VIDMap[T[3]]);
			}
			else
			{
				OutTets.Emplace(VIDMap[T[0]], VIDMap[T[1]], VIDMap[T[2]], VIDMap[T[3]]);
			}
		}
	}

	if (OutSurfaceVertices && OutSurfaceTris)
	{
		OutSurfaceVertices->Reset();
		OutSurfaceTris->Reset();

		// Note: Would be more efficient to directly extract to the desired representation
		// but this is a tiny cost compared to the rest of the tet meshing
		Eigen::MatrixXd V_sf;
		Eigen::MatrixXi F_sf;
		if (params.manifold_surface) {
			manifold_surface(mesh, V_sf, F_sf);
		}
		else {
			get_surface(mesh, V_sf, F_sf);
		}
		OutSurfaceVertices->SetNumUninitialized((int)V_sf.rows());
		for (int Row = 0; Row < V_sf.rows(); ++Row)
		{
			const Vector3 V = V_sf.row(Row);
			for (int SubIdx = 0; SubIdx < 3; ++SubIdx)
			{
				(*OutSurfaceVertices)[Row][SubIdx] = V[SubIdx];
			}
		}
		OutSurfaceTris->SetNumUninitialized((int)F_sf.rows());
		for (int Row = 0; Row < F_sf.rows(); ++Row)
		{
			const Vector3i F = F_sf.row(Row);
			for (int SubIdx = 0; SubIdx < 3; ++SubIdx)
			{
				(*OutSurfaceTris)[Row][SubIdx] = F[SubIdx];
			}
		}
	}
	// Note: Closing this final scope could be omitted as the function end will also close it
	ScopeFinalize.Done();

    return true;
}

}  // namespace floatTetWild

#undef LOCTEXT_NAMESPACE
