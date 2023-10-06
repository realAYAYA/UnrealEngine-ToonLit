#include "ThirdParty/fTetWild/AABBWrapper.h"
#include "ThirdParty/fTetWild/LocalOperations.h"
#include "ThirdParty/fTetWild/TriangleInsertion.h"



void floatTetWild::AABBWrapper::init_b_mesh_and_tree(const std::vector<Vector3>& input_vertices, const std::vector<Vector3i>& input_faces, Mesh& mesh) {
	BTree.Reset();

    std::vector<std::vector<int>> conn_tris(input_vertices.size());
    std::vector<std::array<int, 2>> all_edges;
    all_edges.reserve(input_faces.size() * 3);
    for (int i = 0; i < input_faces.size(); i++) {
        for (int j = 0; j < 3; j++) {
            conn_tris[input_faces[i][j]].push_back(i);
            if (input_faces[i][j] < input_faces[i][(j + 1) % 3])
                all_edges.push_back({{input_faces[i][j], input_faces[i][(j + 1) % 3]}});
            else
                all_edges.push_back({{input_faces[i][(j + 1) % 3], input_faces[i][j]}});
        }
    }
    vector_unique(all_edges);

    std::vector<std::pair<std::array<int, 2>, std::vector<int>>> _;
    std::vector<std::array<int, 2>> b_edges;
    std::vector<bool> _1;
    find_boundary_edges(input_vertices, input_faces,
                        std::vector<bool>(input_faces.size(), true), std::vector<bool>(input_faces.size(), true),
                        _, _1, b_edges);

	check(BTree.Vertices.IsEmpty());
	check(BTree.Tris.IsEmpty());
    if (b_edges.empty()) { // create a degenerate mesh for the empty case
		BTree.Vertices.Emplace(0, 0, 0);
		BTree.Tris.Emplace(0, 0, 0);
    } else {
		BTree.Vertices.Reserve((int)b_edges.size() * 2);
		for (auto& e : b_edges) {
			for (int j = 0; j < 2; j++) {
				BTree.Vertices.Emplace(
					input_vertices[e[j]][0],
					input_vertices[e[j]][1],
					input_vertices[e[j]][2]
				);
			}
		}
		BTree.Tris.Reserve((int)b_edges.size());
		for (int i = 0; i < b_edges.size(); ++i)
		{
			BTree.Tris.Emplace(i * 2, i * 2, i * 2 + 1); // degenerate tri following edge
		}
    }

	BTree.BuildTree();

    if(b_edges.empty())
        mesh.is_closed = true;

#ifdef NEW_ENVELOPE
    std::vector<Vector3> vs;
    std::vector<Vector3i> fs;
    if (b_edges.empty()) {
        vs.push_back(Vector3(0, 0, 0));
        fs.push_back(Vector3i(0, 0, 0));
    } else {
        vs.resize(b_edges.size() * 2);
        fs.resize(b_edges.size());
        for (int i = 0; i < b_edges.size(); i++) {
            vs[i * 2] = input_vertices[b_edges[i][0]];
            vs[i * 2 + 1] = input_vertices[b_edges[i][1]];
            fs[i] = Vector3i(i * 2, i * 2 + 1, i * 2 + 1);
        }
    }
//    b_tree_exact = std::make_shared<fastEnvelope::FastEnvelope>(vs, fs, eps);
    b_tree_exact.init(vs, fs, mesh.params.eps);
#endif
}

void floatTetWild::AABBWrapper::init_tmp_b_mesh_and_tree(const std::vector<Vector3>& input_vertices, const std::vector<Vector3i>& input_faces,
                              const std::vector<std::array<int, 2>>& b_edges1,
                              const Mesh& mesh, const std::vector<std::array<int, 2>>& b_edges2) {
	TmpBTree.Reset();
    if (b_edges1.empty() && b_edges2.empty()) {
		TmpBTree.Vertices.Emplace(0, 0, 0);
		TmpBTree.Tris.Emplace(0, 0, 0);
    } else {
		TmpBTree.Vertices.Reserve((int)(b_edges1.size() + b_edges2.size()) * 2);
		for (auto& e : b_edges1) {
			for (int j = 0; j < 2; j++)
			{
				TmpBTree.Vertices.Emplace(input_vertices[e[j]][0], input_vertices[e[j]][1], input_vertices[e[j]][2]);
			}
		}
		for (auto& e : b_edges2) {
			for (int j = 0; j < 2; j++)
			{
				TmpBTree.Vertices.Emplace(input_vertices[e[j]][0], input_vertices[e[j]][1], input_vertices[e[j]][2]);
			}
		}
		int NumEdges = (int)b_edges1.size() + b_edges2.size();
		TmpBTree.Tris.Reserve(NumEdges);
		for (int i = 0; i < NumEdges; ++i)
		{
			TmpBTree.Tris.Emplace(i * 2, i * 2, i * 2 + 1); // degenerate tri following edge
		}
    }

	TmpBTree.BuildTree();

#ifdef NEW_ENVELOPE
    std::vector<Vector3> vs;
    std::vector<Vector3i> fs;
    if (b_edges1.empty() && b_edges2.empty()) {
        vs.push_back(Vector3(0, 0, 0));
        fs.push_back(Vector3i(0, 0, 0));
    } else {
        vs.resize((b_edges1.size() + b_edges2.size()) * 2);
        fs.resize(b_edges1.size() + b_edges2.size());
        for (int i = 0; i < b_edges1.size(); i++) {
            vs[i * 2] = input_vertices[b_edges1[i][0]];
            vs[i * 2 + 1] = input_vertices[b_edges1[i][1]];
            fs[i] = Vector3i(i * 2, i * 2 + 1, i * 2 + 1);
        }
        for (int i = b_edges1.size(); i < b_edges1.size() + b_edges2.size(); i++) {
            vs[i * 2] = mesh.tet_vertices[b_edges2[i - b_edges1.size()][0]].pos;
            vs[i * 2 + 1] = mesh.tet_vertices[b_edges2[i - b_edges1.size()][1]].pos;
            fs[i] = Vector3i(i * 2, i * 2 + 1, i * 2 + 1);
        }
    }
//    tmp_b_tree_exact = std::make_shared<fastEnvelope::FastEnvelope>(vs, fs, mesh.params.eps_input);
    tmp_b_tree_exact.init(vs, fs, mesh.params.eps);
#endif
}
