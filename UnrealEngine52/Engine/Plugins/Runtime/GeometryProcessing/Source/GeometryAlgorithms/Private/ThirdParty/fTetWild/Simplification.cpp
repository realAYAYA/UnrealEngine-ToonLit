// This file is part of fTetWild, a software for generating tetrahedral meshes.
//
// Copyright (C) 2019 Yixin Hu <yixin.hu@nyu.edu>
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.
//

#include "ThirdParty/fTetWild/Simplification.h"
#include "ThirdParty/fTetWild/LocalOperations.h"

#include "VectorUtil.h"
#include "MathUtil.h"

#ifdef FLOAT_TETWILD_USE_TBB
#include <tbb/task_scheduler_init.h>
#include <tbb/parallel_for.h>
#include <tbb/atomic.h>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_sort.h>
#include <tbb/concurrent_unordered_set.h>
#endif

#include <iomanip>

void floatTetWild::simplify(std::vector<Vector3>& input_vertices, std::vector<Vector3i>& input_faces, std::vector<int>& input_tags,
        const AABBWrapper& tree, const Parameters& params, bool skip_simplify) {

    remove_duplicates(input_vertices, input_faces, input_tags, params);
    if (skip_simplify)
        return;

    std::vector<bool> v_is_removed(input_vertices.size(), false);
    std::vector<bool> f_is_removed(input_faces.size(), false);
    std::vector<std::unordered_set<int>> conn_fs(input_vertices.size());
    for (int i = 0; i < input_faces.size(); i++) {
        for (int j = 0; j < 3; j++)
            conn_fs[input_faces[i][j]].insert(i);
    }

    collapsing(input_vertices, input_faces, tree, params, v_is_removed, f_is_removed, conn_fs);

    swapping(input_vertices, input_faces, tree, params, v_is_removed, f_is_removed, conn_fs);

    //clean up vs, fs
    //v
    std::vector<int> map_v_ids(input_vertices.size(), -1);
    int cnt = 0;
    for (int i = 0; i < input_vertices.size(); i++) {
        if (v_is_removed[i] || conn_fs[i].empty())
            continue;
        map_v_ids[i] = cnt;
        cnt++;
    }

    std::vector<Vector3> new_input_vertices(cnt);
    cnt = 0;
    for (int i = 0; i < input_vertices.size(); i++) {
        if (v_is_removed[i] || conn_fs[i].empty())
            continue;
        new_input_vertices[cnt++] = input_vertices[i];
    }
    input_vertices = new_input_vertices;

    //f
    cnt = 0;
    for (int i = 0; i < input_faces.size(); i++) {
        if (f_is_removed[i])
            continue;
        for (int j = 0; j < 3; j++)
            input_faces[i][j] = map_v_ids[input_faces[i][j]];
        cnt++;
    }

    std::vector<Vector3i> new_input_faces(cnt);
    std::vector<int> new_input_tags(cnt);
    cnt = 0;
    for (int i = 0; i < input_faces.size(); i++) {
        if (f_is_removed[i])
            continue;
        new_input_faces[cnt] = input_faces[i];
        new_input_tags[cnt] = input_tags[i];
        cnt++;
    }
    input_faces = new_input_faces;
    input_tags = new_input_tags;

    remove_duplicates(input_vertices, input_faces, input_tags, params);
}

bool floatTetWild::remove_duplicates(std::vector<Vector3>& input_vertices, std::vector<Vector3i>& input_faces, std::vector<int>& input_tags, const Parameters& params)
{
	// Note this removes duplicates but also sorts vertices by coordinates and faces by index, and rotates face indices so the smallest index is first
	
	// Note the concept of 'duplicate' used here is 'rounds to the same position after dividing by 10*epsilon'
	// This could in theory leaves some vertices that are arbitrarily close together ...
	const double Inv10Epsilon = 1.0 / (10 * SCALAR_ZERO * params.bbox_diag_length);
	auto ToRoundedVertex = [Inv10Epsilon](const Vector3& V) -> FVector
	{
		return FVector(
			FMathd::Round(V[0] * Inv10Epsilon),
			FMathd::Round(V[1] * Inv10Epsilon),
			FMathd::Round(V[2] * Inv10Epsilon));
	};
	TArray<FVector> RoundedV;
	RoundedV.SetNumUninitialized((int)input_vertices.size());
	TArray<int32> VNewToOld;
	VNewToOld.SetNumUninitialized((int)input_vertices.size());
	for (int32 Idx = 0; Idx < VNewToOld.Num(); ++Idx)
	{
		VNewToOld[Idx] = Idx;
		RoundedV[Idx] = ToRoundedVertex(input_vertices[Idx]);
	}
	VNewToOld.Sort([&](int32 A, int32 B)
	{
		const FVector& RA = RoundedV[A];
		const FVector& RB = RoundedV[B];
		if (RA.X != RB.X) return RA.X < RB.X;
		else if (RA.Y != RB.Y) return RA.Y < RB.Y;
		else if (RA.Z != RB.Z) return RA.Z < RB.Z;
		else return false;
	});
	int NumKeptV = 0;
	std::vector<Vector3> vertices_copy = input_vertices;
	TArray<int32> VOldToNew;
	VOldToNew.SetNumUninitialized(VNewToOld.Num());
	for (int i = 0, Num = VNewToOld.Num(); i < Num;)
	{
		int KeptIdx = NumKeptV++;
		input_vertices[KeptIdx] = vertices_copy[VNewToOld[i]];
		VOldToNew[VNewToOld[i]] = KeptIdx;
		const FVector& RV = RoundedV[VNewToOld[i]];
		int Match = i + 1;
		for (; Match < Num && RV == RoundedV[VNewToOld[Match]]; ++Match)
		{
			VOldToNew[VNewToOld[Match]] = KeptIdx;
		}
		i = Match;
	}
	input_vertices.resize(NumKeptV);

	auto RemapTri = [&VOldToNew](const Vector3i& Tri) -> Vector3i
	{
		Vector3i Remapped(VOldToNew[Tri[0]], VOldToNew[Tri[1]], VOldToNew[Tri[2]]);
		// Rotate the triangle so the smallest index is first
		if (Remapped[0] < Remapped[1])
		{
			if (Remapped[0] < Remapped[2])
			{
				return Remapped;
			}
			else
			{
				return Vector3i(Remapped[2], Remapped[0], Remapped[1]);
			}
		}
		else
		{
			if (Remapped[1] < Remapped[2])
			{
				return Vector3i(Remapped[1], Remapped[2], Remapped[0]);
			}
			else
			{
				return Vector3i(Remapped[2], Remapped[0], Remapped[1]);
			}
		}
	};
	// Note the smallest index is always first, from the remapping above, so we only need to consider the other two
	auto ToSortedInds = [](const Vector3i& Tri) -> UE::Geometry::FIndex3i
	{
		UE::Geometry::FIndex3i Sorted(Tri[0], Tri[1], Tri[2]);
		if (Sorted.B > Sorted.C)
		{
			Swap(Sorted.B, Sorted.C);
		}
		return Sorted;
	};
	TSet<UE::Geometry::FIndex3i> TriSet;
	int KeptTriCount = 0;
	for (int i = 0, Num = (int)input_faces.size(); i < Num; ++i)
	{
		// Update tri indices with the new vertex indices
		Vector3i Remapped = RemapTri(input_faces[i]);

		// Skip tris with duplicate vertices
		if (Remapped[0] == Remapped[1] || Remapped[1] == Remapped[2] || Remapped[0] == Remapped[2])
		{
			continue;
		}

		// Skip small area tris
		Vector3 u = input_vertices[Remapped[1]] - input_vertices[Remapped[0]];
		Vector3 v = input_vertices[Remapped[2]] - input_vertices[Remapped[0]];
		Vector3 area = u.cross(v);
		if (area.norm() / 2 <= SCALAR_ZERO * params.bbox_diag_length)
			continue;

		// Skip tris that are already in the mesh (including flipped versions)
		UE::Geometry::FIndex3i Sorted = ToSortedInds(Remapped);
		if (!TriSet.Contains(Sorted))
		{
			TriSet.Add(Sorted);
			int32 NewTriID = KeptTriCount++;
			input_faces[NewTriID] = Remapped;
			input_tags[NewTriID] = input_tags[i];
		}
	}

	input_faces.resize(KeptTriCount);
	input_tags.resize(KeptTriCount);

	if (!KeptTriCount)
	{
		return false;
	}

	// Reorder faces so smaller indices are first
	// This probably isn't necessary but it does keep results more consistent with the original ftetwild
	TArray<int32> FaceOrder;
	FaceOrder.SetNumUninitialized(KeptTriCount);
	for (int32 Idx = 0; Idx < KeptTriCount; ++Idx)
	{
		FaceOrder[Idx] = Idx;
	}
	FaceOrder.Sort([&](int32 A, int32 B)
		{
			const Vector3i& FA = input_faces[A];
			const Vector3i& FB = input_faces[B];
			if (FA[0] != FB[0]) return FA[0] < FB[0];
			else if (FA[1] != FB[1]) return FA[1] < FB[1];
			else if (FA[2] != FB[2]) return FA[2] < FB[2];
			else return false;
		});
	std::vector<Vector3i> faces_copy = input_faces;
	std::vector tags_copy = input_tags;
	for (int32 Idx = 0; Idx < KeptTriCount; ++Idx)
	{
		input_faces[Idx] = faces_copy[FaceOrder[Idx]];
		input_tags[Idx] = tags_copy[FaceOrder[Idx]];
	}

	return true;
}

void floatTetWild::collapsing(std::vector<Vector3>& input_vertices, std::vector<Vector3i>& input_faces,
        const AABBWrapper& tree, const Parameters& params,
        std::vector<bool>& v_is_removed, std::vector<bool>& f_is_removed, std::vector<std::unordered_set<int>>& conn_fs){

#ifdef FLOAT_TETWILD_USE_TBB
    std::vector<std::array<int, 2>> edges;
    tbb::concurrent_vector<std::array<int, 2>> edges_tbb;

    const auto build_edges = [&](){
        edges.clear();
        edges.reserve(input_faces.size()*3);

        edges_tbb.clear();
        edges_tbb.reserve(input_faces.size()*3);

        tbb::parallel_for( size_t(0), input_faces.size(), [&](size_t f_id)
        {
            if(f_is_removed[f_id])
                return;

            for (int j = 0; j < 3; j++) {
                std::array<int, 2> e = {{input_faces[f_id][j], input_faces[f_id][mod3(j + 1)]}};
                if (e[0] > e[1])
                    std::swap(e[0], e[1]);
                edges_tbb.push_back(e);
            }
        });


        edges.reserve(edges_tbb.size());
        edges.insert(edges.end(), edges_tbb.begin(), edges_tbb.end());
        assert(edges_tbb.size() == edges.size());
        tbb::parallel_sort(edges.begin(), edges.end());

        edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    };
#else
    std::vector<std::array<int, 2>> edges;
    edges.clear();
    edges.reserve(input_faces.size()*3);
    for(size_t f_id = 0; f_id < input_faces.size(); ++f_id)
    {
        if(f_is_removed[f_id])
            continue;

        const auto& f = input_faces[f_id];
        for (int j = 0; j < 3; j++) {
            std::array<int, 2> e = {{f[j], f[mod3(j + 1)]}};
            if (e[0] > e[1])
                std::swap(e[0], e[1]);
            edges.push_back(e);
        }
    }
    vector_unique(edges);

    std::priority_queue<ElementInQueue, std::vector<ElementInQueue>, cmp_s> sm_queue;
    for (auto& e: edges) {
        Scalar weight = (input_vertices[e[0]] - input_vertices[e[1]]).squaredNorm();
        sm_queue.push(ElementInQueue(e, weight));
        sm_queue.push(ElementInQueue(std::array<int, 2>({{e[1], e[0]}}), weight));
    }
#endif

    auto is_onering_clean = [&](int v_id) {
        std::vector<int> v_ids;
        v_ids.reserve(conn_fs[v_id].size() * 2);
        for (const auto &f_id:conn_fs[v_id]) {
            for (int j = 0; j < 3; j++) {
                if (input_faces[f_id][j] != v_id)
                    v_ids.push_back(input_faces[f_id][j]);
            }
        }
        std::sort(v_ids.begin(), v_ids.end());

        if (v_ids.size() % 2 != 0)
            return false;
        for (int i = 0; i < v_ids.size(); i += 2) {
            if (v_ids[i] != v_ids[i + 1])
                return false;
        }

        return true;
    };

    static const int SUC = 1;
    static const int FAIL_CLEAN = 0;
    static const int FAIL_FLIP = -1;
    static const int FAIL_ENV = -2;

    auto remove_an_edge = [&](int v1_id, int v2_id, const std::vector<int>& n12_f_ids) {
        if (!is_onering_clean(v1_id) || !is_onering_clean(v2_id))
            return FAIL_CLEAN;

//        std::unordered_set<int> new_f_ids;
        std::vector<int> new_f_ids;
        for (int f_id:conn_fs[v1_id]) {
            if (f_id != n12_f_ids[0] && f_id != n12_f_ids[1])
                new_f_ids.push_back(f_id);
        }
        for (int f_id:conn_fs[v2_id]) {
            if (f_id != n12_f_ids[0] && f_id != n12_f_ids[1])
                new_f_ids.push_back(f_id);
        }
        vector_unique(new_f_ids);

        //compute new point
        Vector3 p = (input_vertices[v1_id] + input_vertices[v2_id]) / 2;
        tree.project_to_sf(p);

        //computing normal for checking flipping
        for (int f_id:new_f_ids) {
            Vector3 old_nv = (input_vertices[input_faces[f_id][1]] - input_vertices[input_faces[f_id][2]]).cross(input_vertices[input_faces[f_id][0]] - input_vertices[input_faces[f_id][2]]);

            for (int j = 0; j < 3; j++) {
                if (input_faces[f_id][j] == v1_id || input_faces[f_id][j] == v2_id) {
                    Vector3 new_nv = (input_vertices[input_faces[f_id][mod3(j + 1)]] - input_vertices[input_faces[f_id][mod3(j + 2)]]).cross(p - input_vertices[input_faces[f_id][mod3(j + 2)]]);
                    if (old_nv.dot(new_nv) <= 0)
                        return FAIL_FLIP;
                    //check new tris' area
                    if (new_nv.norm() / 2 <= SCALAR_ZERO_2)
                        return FAIL_FLIP;
                    break;
                }
            }
        }

        //check if go outside of envelope
        for (int f_id:new_f_ids) {
            for (int j = 0; j < 3; j++) {
                if (input_faces[f_id][j] == v1_id || input_faces[f_id][j] == v2_id) {
                    const std::array<Vector3, 3> tri = {{
                        p,
                        input_vertices[input_faces[f_id][mod3(j + 1)]],
                        input_vertices[input_faces[f_id][mod3(j + 2)]]
                    }};
                    if (is_out_envelope(tri, tree, params))
                        return FAIL_ENV;
                    break;
                }
            }
        }

        //real update
//        std::unordered_set<int> n_v_ids;//get this info before real update for later usage
        std::vector<int> n_v_ids;//get this info before real update for later usage
        for (int f_id:new_f_ids) {
            for (int j = 0; j < 3; j++) {
                if (input_faces[f_id][j] != v1_id && input_faces[f_id][j] != v2_id)
                    n_v_ids.push_back(input_faces[f_id][j]);
            }
        }
        vector_unique(n_v_ids);

        v_is_removed[v1_id] = true;
        input_vertices[v2_id] = p;
        for (int f_id:n12_f_ids) {
            f_is_removed[f_id] = true;
#ifndef FLOAT_TETWILD_USE_TBB
            for (int j = 0; j < 3; j++) {//rm conn_fs
                if (input_faces[f_id][j] != v1_id) {
                    conn_fs[input_faces[f_id][j]].erase(f_id);
                }
            }
#endif
        }
        for (int f_id:conn_fs[v1_id]) {//add conn_fs
            if (f_is_removed[f_id])
                continue;
            conn_fs[v2_id].insert(f_id);
            for (int j = 0; j < 3; j++) {
                if (input_faces[f_id][j] == v1_id)
                    input_faces[f_id][j] = v2_id;
            }
        }

#ifndef FLOAT_TETWILD_USE_TBB
        //push new edges into the queue
        for (int v_id:n_v_ids) {
            double weight = (input_vertices[v2_id] - input_vertices[v_id]).squaredNorm();
            sm_queue.push(ElementInQueue(std::array<int, 2>({{v2_id, v_id}}), weight));
            sm_queue.push(ElementInQueue(std::array<int, 2>({{v_id, v2_id}}), weight));
        }
#endif
        return SUC;
    };

#ifdef FLOAT_TETWILD_USE_TBB
    tbb::atomic<int> cnt(0);
    int cnt_suc = 0;
    // tbb::atomic<int> fail_clean(0);
    // tbb::atomic<int> fail_flip(0);
    // tbb::atomic<int> fail_env(0);

    const int stopping = static_cast<int>(input_vertices.size()/10000.);

    std::vector<int> safe_set;
    do {
        build_edges();
        Mesh::one_ring_edge_set(edges, v_is_removed, f_is_removed, conn_fs, input_vertices, safe_set);
        cnt = 0;

        tbb::parallel_for(size_t(0), safe_set.size(), [&](size_t i) {
//        for (int i = 0; i < safe_set.size(); i++) {
            std::array<int, 2> &v_ids = edges[safe_set[i]];

//            if (v_is_removed[v_ids[0]] || v_is_removed[v_ids[1]])
//                return;

            std::vector<int> n12_f_ids;
            set_intersection(conn_fs[v_ids[0]], conn_fs[v_ids[1]], n12_f_ids);

            if (n12_f_ids.size() != 2)
                return;
//                continue;

            int res = remove_an_edge(v_ids[0], v_ids[1], n12_f_ids);
            if (res == SUC)
                cnt++;
        });
//        }

        //cleanup conn_fs
        tbb::parallel_for(size_t(0), conn_fs.size(), [&](size_t i) {
//        for (int i = 0; i < conn_fs.size(); i++) {
            if (v_is_removed[i])
//                continue;
                return;
            std::vector<int> r_f_ids;
            for (int f_id: conn_fs[i]) {
                if (f_is_removed[f_id])
                    r_f_ids.push_back(f_id);
            }
            for (int f_id:r_f_ids)
                conn_fs[i].erase(f_id);
//        }
        });

        cnt_suc += cnt;
    } while(cnt > stopping);

#else
    int cnt_suc = 0;
    int fail_clean = 0;
    int fail_flip = 0;
    int fail_env = 0;

    while (!sm_queue.empty()) {
        std::array<int, 2> v_ids = sm_queue.top().v_ids;
        Scalar old_weight = sm_queue.top().weight;
        sm_queue.pop();

        if (v_is_removed[v_ids[0]] || v_is_removed[v_ids[1]])
            continue;
        if(old_weight!=(input_vertices[v_ids[0]] - input_vertices[v_ids[1]]).squaredNorm())
            continue;

        std::vector<int> n12_f_ids;
        set_intersection(conn_fs[v_ids[0]], conn_fs[v_ids[1]], n12_f_ids);
        if(n12_f_ids.size()!=2)
            continue;

        int res = remove_an_edge(v_ids[0], v_ids[1], n12_f_ids);
        if (res == SUC)
            cnt_suc++;
    }
#endif

}

void floatTetWild::swapping(std::vector<Vector3>& input_vertices, std::vector<Vector3i>& input_faces,
        const AABBWrapper& tree, const Parameters& params,
        std::vector<bool>& v_is_removed, std::vector<bool>& f_is_removed, std::vector<std::unordered_set<int>>& conn_fs) {
    std::vector<std::array<int, 2>> edges;
    edges.reserve(input_faces.size() * 6);
    for (int i = 0; i < input_faces.size(); i++) {
        if (f_is_removed[i])
            continue;
        auto &f = input_faces[i];
        for (int j = 0; j < 3; j++) {
            std::array<int, 2> e = {{f[j], f[mod3(j + 1)]}};
            if (e[0] > e[1])
                std::swap(e[0], e[1]);
            edges.push_back(e);
        }
    }
    vector_unique(edges);

    std::priority_queue<ElementInQueue, std::vector<ElementInQueue>, cmp_l> sm_queue;
    for (auto &e: edges) {
        Scalar weight = (input_vertices[e[0]] - input_vertices[e[1]]).squaredNorm();
        sm_queue.push(ElementInQueue(e, weight));
        sm_queue.push(ElementInQueue(std::array<int, 2>({{e[1], e[0]}}), weight));
    }

    int cnt = 0;
    while (!sm_queue.empty()) {
        int v1_id = sm_queue.top().v_ids[0];
        int v2_id = sm_queue.top().v_ids[1];
        sm_queue.pop();

        std::vector<int> n12_f_ids;
        set_intersection(conn_fs[v1_id], conn_fs[v2_id], n12_f_ids);
        if (n12_f_ids.size() != 2)
            continue;

        std::array<int, 2> n_v_ids = {{-1, -1}};
        for (int j = 0; j < 3; j++) {
            if (n_v_ids[0] < 0
                && input_faces[n12_f_ids[0]][j] != v1_id && input_faces[n12_f_ids[0]][j] != v2_id)
                n_v_ids[0] = input_faces[n12_f_ids[0]][j];

            if (n_v_ids[1] < 0
                && input_faces[n12_f_ids[1]][j] != v1_id && input_faces[n12_f_ids[1]][j] != v2_id)
                n_v_ids[1] = input_faces[n12_f_ids[1]][j];
        }

        //check coplanar
        Scalar cos_a0 = get_angle_cos(input_vertices[n_v_ids[0]], input_vertices[v1_id], input_vertices[v2_id]);
        Scalar cos_a1 = get_angle_cos(input_vertices[n_v_ids[1]], input_vertices[v1_id], input_vertices[v2_id]);
        std::array<Vector3, 2> old_nvs;
        for (int f = 0; f < 2; f++) {
            auto &a = input_vertices[input_faces[n12_f_ids[f]][0]];
            auto &b = input_vertices[input_faces[n12_f_ids[f]][1]];
            auto &c = input_vertices[input_faces[n12_f_ids[f]][2]];
            old_nvs[f] = ((b - c).cross(a - c)).normalized();
        }
        if (cos_a0 > -0.999) {//maybe it's for avoiding numerical issue
            if (old_nvs[0].dot(old_nvs[1]) < 1 - 1e-6)//not coplanar
                continue;
        }

        //check inversion
        auto &old_nv = cos_a1 < cos_a0 ? old_nvs[0] : old_nvs[1];
        bool is_filp = false;
        for (int f_id:n12_f_ids) {
            auto &a = input_vertices[input_faces[f_id][0]];
            auto &b = input_vertices[input_faces[f_id][1]];
            auto &c = input_vertices[input_faces[f_id][2]];
            if (old_nv.dot(((b - c).cross(a - c)).normalized()) < 0) {
                is_filp = true;
                break;
            }
        }
        if (is_filp)
            continue;

        //check quality
        Scalar cos_a0_new = get_angle_cos(input_vertices[v1_id], input_vertices[n_v_ids[0]],
                                          input_vertices[n_v_ids[1]]);
        Scalar cos_a1_new = get_angle_cos(input_vertices[v2_id], input_vertices[n_v_ids[0]],
                                          input_vertices[n_v_ids[1]]);
        if (std::min(cos_a0_new, cos_a1_new) <= std::min(cos_a0, cos_a1))
            continue;

        if (is_out_envelope({{input_vertices[v1_id], input_vertices[n_v_ids[0]], input_vertices[n_v_ids[1]]}}, tree,
                            params)
            || is_out_envelope({{input_vertices[v2_id], input_vertices[n_v_ids[0]], input_vertices[n_v_ids[1]]}}, tree,
                               params)) {
            continue;
        }

        // real update
        for (int j = 0; j < 3; j++) {
            if (input_faces[n12_f_ids[0]][j] == v2_id)
                input_faces[n12_f_ids[0]][j] = n_v_ids[1];
            if (input_faces[n12_f_ids[1]][j] == v1_id)
                input_faces[n12_f_ids[1]][j] = n_v_ids[0];
        }
        conn_fs[v1_id].erase(n12_f_ids[1]);
        conn_fs[v2_id].erase(n12_f_ids[0]);
        conn_fs[n_v_ids[0]].insert(n12_f_ids[1]);
        conn_fs[n_v_ids[1]].insert(n12_f_ids[0]);
        cnt++;
    }

    return;

    ///////////////////

    for (int i = 0; i < input_faces.size(); i++) {//todo go over edges!!!
        if(f_is_removed[i])
            continue;

        bool is_swapped = false;
        for (int j = 0; j < 3; j++) {
            int v_id = input_faces[i][j];
            int v1_id = input_faces[i][mod3(j+1)];
            int v2_id = input_faces[i][mod3(j+2)];

            // manifold
            std::vector<int> n12_f_ids;
            set_intersection(conn_fs[v1_id], conn_fs[v2_id], n12_f_ids);
            if (n12_f_ids.size() != 2) {
                continue;
            }
            if (n12_f_ids[1] == i)
                n12_f_ids = {n12_f_ids[1], n12_f_ids[0]};
            int v3_id = -1;
            for (int k = 0; k < 3; k++)
                if (input_faces[n12_f_ids[1]][k] != v1_id && input_faces[n12_f_ids[1]][k] != v2_id) {
                    v3_id = input_faces[n12_f_ids[1]][k];
                    break;
                }

            // check quality
            Scalar cos_a = get_angle_cos(input_vertices[v_id], input_vertices[v1_id], input_vertices[v2_id]);
            Scalar cos_a1 = get_angle_cos(input_vertices[v3_id], input_vertices[v1_id], input_vertices[v2_id]);
            std::array<Vector3, 2> old_nvs;
            for (int f = 0; f < 2; f++) {
                auto& a = input_vertices[input_faces[n12_f_ids[f]][0]];
                auto& b = input_vertices[input_faces[n12_f_ids[f]][1]];
                auto& c = input_vertices[input_faces[n12_f_ids[f]][2]];
                old_nvs[f] = ((b - c).cross(a - c)).normalized();
            }
            if (cos_a > -0.999) {
                if (old_nvs[0].dot(old_nvs[1]) < 1-1e-6)//not coplanar
                    continue;
            }
            Scalar cos_a_new = get_angle_cos(input_vertices[v1_id], input_vertices[v_id], input_vertices[v3_id]);
            Scalar cos_a1_new = get_angle_cos(input_vertices[v2_id], input_vertices[v_id], input_vertices[v3_id]);
            if (std::min(cos_a_new, cos_a1_new) <= std::min(cos_a, cos_a1))
                continue;

            // non flipping
            auto f1_old = input_faces[n12_f_ids[0]];
            auto f2_old = input_faces[n12_f_ids[1]];
            for (int k = 0; k < 3; k++) {
                if (input_faces[n12_f_ids[0]][k] == v2_id)
                    input_faces[n12_f_ids[0]][k] = v3_id;
                if (input_faces[n12_f_ids[1]][k] == v1_id)
                    input_faces[n12_f_ids[1]][k] = v_id;
            }
            auto& old_nv = cos_a1 < cos_a ? old_nvs[0] : old_nvs[1];
            bool is_filp = false;
            for (int f_id:n12_f_ids) {
                auto& a = input_vertices[input_faces[f_id][0]];
                auto& b = input_vertices[input_faces[f_id][1]];
                auto& c = input_vertices[input_faces[f_id][2]];
                if (old_nv.dot(((b - c).cross(a - c)).normalized()) < 0) {
                    is_filp = true;
                    break;
                }
            }
            if (is_filp) {
                input_faces[n12_f_ids[0]] = f1_old;
                input_faces[n12_f_ids[1]] = f2_old;
                continue;
            }

            // non outside envelop
            bool is_valid = true;
            for(int f_id: n12_f_ids) {
                if (is_out_envelope({{input_vertices[input_faces[f_id][0]], input_vertices[input_faces[f_id][1]],
                                     input_vertices[input_faces[f_id][2]]}}, tree, params)) {
                    is_valid = false;
                    break;
                }
            }
            if(!is_valid){
                input_faces[n12_f_ids[0]] = f1_old;
                input_faces[n12_f_ids[1]] = f2_old;
                continue;
            }

            // real update
            conn_fs[v1_id].erase(n12_f_ids[1]);
            conn_fs[v2_id].erase(n12_f_ids[0]);
            conn_fs[v_id].insert(n12_f_ids[1]);
            conn_fs[v3_id].insert(n12_f_ids[0]);
            is_swapped = true;
            break;
        }
        if (is_swapped)
            cnt++;
    }
}

void floatTetWild::flattening(std::vector<Vector3>& input_vertices, std::vector<Vector3i>& input_faces,
        const AABBWrapper& sf_tree, const Parameters& params) {
    std::vector<Vector3> ns(input_faces.size());
    for (int i = 0; i < input_faces.size(); i++) {
        ns[i] = ((input_vertices[input_faces[i][2]] - input_vertices[input_faces[i][0]]).cross(
                input_vertices[input_faces[i][1]] - input_vertices[input_faces[i][0]])).normalized();
    }

    std::vector<std::vector<int>> conn_fs(input_vertices.size());
    std::vector<std::array<int, 2>> edges;
    edges.reserve(input_faces.size() * 6);
    for (int i = 0; i < input_faces.size(); i++) {
        auto &f = input_faces[i];
        for (int j = 0; j < 3; j++) {
            conn_fs[f[j]].push_back(i);
            std::array<int, 2> e = {{f[j], f[mod3(j + 1)]}};
            if (e[0] > e[1])
                std::swap(e[0], e[1]);
            edges.push_back(e);
        }
    }
    vector_unique(edges);

    //auto needs_flattening = [](const Vector3 &n1, const Vector3 &n2) {
    //    if(n1.dot(n2)>0.98) {
    //        cout << std::setprecision(17) << n1.dot(n2) << endl;
    //        cout << n1.norm() << " " << n2.norm() << endl;
    //    }
    //    return true; // Note: always returns true

    //    double d = std::abs(n1.dot(n2) - 1);
    //    cout<<n1.dot(n2)<<endl;
    //    if (d > 1e-15 && d < 1e-5)
    //        return true;
    //    return false;
    //};

    std::vector<int> f_tss(input_faces.size(), 0);
    int ts = 0;
    std::queue<std::array<int, 5>> edge_queue;
    for (auto &e: edges) {
        std::vector<int> n_f_ids;
        set_intersection(conn_fs[e[0]], conn_fs[e[1]], n_f_ids);
        if (n_f_ids.size() != 2)
            continue;
        //if (!needs_flattening(ns[n_f_ids[0]], ns[n_f_ids[1]]))
        //    continue;
        edge_queue.push({{e[0], e[1], n_f_ids[0], n_f_ids[1], ts}});
    }

    while (!edge_queue.empty()) {
        std::array<int, 2> e = {{edge_queue.front()[0], edge_queue.front()[1]}};
        std::array<int, 2> n_f_ids = {{edge_queue.front()[2], edge_queue.front()[3]}};
        int e_ts = edge_queue.front().back();
        edge_queue.pop();

        std::vector<int> all_f_ids;
        for (int j = 0; j < 2; j++)
            all_f_ids.insert(all_f_ids.end(), conn_fs[e[j]].begin(), conn_fs[e[j]].end());
        vector_unique(all_f_ids);

        bool is_valid = true;
        for(int f_id: all_f_ids){
            if(f_tss[f_id]>e_ts) {
                is_valid = false;
                break;
            }
        }
        if(!is_valid)
            continue;

        auto &n1 = ns[n_f_ids[0]];
        auto &n2 = ns[n_f_ids[1]];
        std::vector<int> n_v_ids;
        for (int f_id: n_f_ids) {
            for (int j = 0; j < 3; j++) {
                if (input_faces[f_id][j] != e[0] && input_faces[f_id][j] != e[1]) {
                    n_v_ids.push_back(input_faces[f_id][j]);
                    break;
                }
            }
        }
        assert(n_v_ids.size() == 2 && n_v_ids[0] != n_v_ids[1]);
        Vector3 n = (n1 + n2) / 2;
        Vector3 p = (input_vertices[n_v_ids[0]] + input_vertices[n_v_ids[1]]) / 2;

        std::array<Vector3, 2> old_ps;
        for (int j = 0; j < 2; j++) {
            old_ps[j] = input_vertices[e[j]];
            input_vertices[e[j]] -= n.dot(input_vertices[e[j]] - p) * n;
        }

        is_valid = true;
        for (int f_id: all_f_ids) {
            const std::array<Vector3, 3> tri = {{input_vertices[input_faces[f_id][0]],
                                                        input_vertices[input_faces[f_id][1]],
                                                        input_vertices[input_faces[f_id][2]]}};
            if (is_out_envelope(tri, sf_tree, params)) {
                is_valid = false;
                break;
            }
        }
        if (!is_valid) {
            for (int j = 0; j < 2; j++)
                input_vertices[e[j]] = old_ps[j];
        }

        ///update
        //
        ns[n_f_ids[0]] = n;
        ns[n_f_ids[1]] = n;
        //
        ts++;
    }
}

floatTetWild::Scalar floatTetWild::get_angle_cos(const Vector3& p, const Vector3& p1, const Vector3& p2) {
    Vector3 v1 = p1 - p;
    Vector3 v2 = p2 - p;
    Scalar res = v1.dot(v2) / (v1.norm() * v2.norm());
    if(res > 1)
        return 1;
    if (res < -1)
        return -1;
    return res;
}

bool floatTetWild::is_out_envelope(const std::array<Vector3, 3>& vs, const AABBWrapper& tree, const Parameters& params) {
#ifdef NEW_ENVELOPE
    return tree.is_out_sf_envelope_exact_simplify(vs);
#else
    #ifdef STORE_SAMPLE_POINTS
        std::vector<Vector3> ps;
        sample_triangle(vs, ps, params.dd_simplification);
        return tree.is_out_sf_envelope(ps, params.eps_2_simplification);
    #else
        FMeshIndex prev_facet = INDEX_NONE;
        return sample_triangle_and_check_is_out(vs, params.dd_simplification, params.eps_2_simplification, tree, prev_facet);
    #endif
#endif
}

void floatTetWild::check_surface(std::vector<Vector3>& input_vertices, std::vector<Vector3i>& input_faces, const std::vector<bool>& f_is_removed,
                   const AABBWrapper& tree, const Parameters& params) {
    bool is_valid = true;
    for (int i = 0; i < input_faces.size(); i++) {
        if(f_is_removed[i])
            continue;
        std::vector<Vector3> ps;
        sample_triangle(
                {{input_vertices[input_faces[i][0]], input_vertices[input_faces[i][1]], input_vertices[input_faces[i][2]]}},
                ps, params.dd_simplification);
        Scalar dist = tree.dist_sf_envelope(ps, params.eps_2);
		ensureMsgf(dist == 0, TEXT("Surface outside envelope with dist=%f, f=%d,%d,%d"), dist, input_faces[i][0], input_faces[i][1], input_faces[i][2]);
    }
}


