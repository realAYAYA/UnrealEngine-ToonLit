#pragma once

#include "ThirdParty/fTetWild/Mesh.hpp"

#include "Spatial/MeshAABBTree3.h"
#include "MeshAdapter.h"

#include <memory>

//#define NEW_ENVELOPE //fortest

// Note NEW_ENVELOPE refers to the "Exact and Efficient Polyhedral Envelope Containment Check," see: https://github.com/wangbolun300/fast-envelope
// This is not currently supported.
#ifdef NEW_ENVELOPE
#include <fastenvelope/FastEnvelope.h>
#endif

namespace floatTetWild {

	using FMeshIndex = int;

	struct FTriTreeWrapper
	{
		using FMeshWrapper = UE::Geometry::TIndexVectorMeshArrayAdapter<UE::Geometry::FIndex3i, double, FVector>;

		FTriTreeWrapper()
		{
			Mesh.SetSources(&Vertices, &Tris);
			bTreeBuilt = false;
		}

		void Reset()
		{
			Vertices.Reset();
			Tris.Reset();
			bTreeBuilt = false;
		}

		void SetFromMesh(const std::vector<Vector3>& V, const std::vector<Vector3i>& F)
		{
			Vertices.SetNum(V.size());
			Tris.SetNum(F.size());
			for (int32 VIdx = 0, Num = Vertices.Num(); VIdx < Num; ++VIdx)
			{
				auto GeoV = V[VIdx];
				Vertices[VIdx] = FVector(GeoV[0], GeoV[1], GeoV[2]);
			}
			for (int32 FIdx = 0, Num = Tris.Num(); FIdx < Num; ++FIdx)
			{
				Tris[FIdx] = UE::Geometry::FIndex3i(F[FIdx][0], F[FIdx][1], F[FIdx][2]);
			}
		}

		void BuildTree()
		{
			Tree.SetMesh(&Mesh, true);
			bTreeBuilt = true;
		}

		int32 nearest_facet(
			const Vector3& p, Vector3& nearest_point, double& sq_dist
		) const
		{
			checkSlow(bTreeBuilt);
			FVector Point(p[0], p[1], p[2]);
			int32 NearTriID = Tree.FindNearestTriangle(Point, sq_dist);
			if (NearTriID != INDEX_NONE)
			{
				UE::Geometry::FDistPoint3Triangle3d Query = UE::Geometry::TMeshQueries<FMeshWrapper>::TriangleDistance(Mesh, NearTriID, Point);
				nearest_point[0] = Query.ClosestTrianglePoint[0];
				nearest_point[1] = Query.ClosestTrianglePoint[1];
				nearest_point[2] = Query.ClosestTrianglePoint[2];
			}
			return NearTriID;
		}

		/*
		 * Finds whether point is within a tolerance distance of the mesh
		 */
		bool facet_in_envelope(
			const Vector3& p, double sq_epsilon, int32& NearTriID
		) const
		{
			checkSlow(bTreeBuilt);
			FVector Point(p[0], p[1], p[2]);
			return Tree.IsWithinDistanceSquared(Point, sq_epsilon, NearTriID);
		}

		/*
		 * version of facet_in_envelope that always finds a point (but point is arbitrary if not in envelope); prefer facet_in_envelope
		 */
		FMeshIndex facet_in_envelope(
			const Vector3& p, double sq_epsilon, Vector3& nearest_point, double& sq_dist
		) const
		{
			checkSlow(bTreeBuilt);
			int32 NearTriID = -1;
			FVector Point(p[0], p[1], p[2]);
			bool bFound = Tree.IsWithinDistanceSquared(Point, sq_epsilon, NearTriID);
			if (!bFound)
			{
				NearTriID = Tris.Num() - 1; // arbitrary triangle
			}
			if (NearTriID > -1)
			{
				UE::Geometry::FDistPoint3Triangle3d Query = UE::Geometry::TMeshQueries<FMeshWrapper>::TriangleDistance(Mesh, NearTriID, Point);
				sq_dist = Query.GetSquared();
				nearest_point[0] = Query.ClosestTrianglePoint[0];
				nearest_point[1] = Query.ClosestTrianglePoint[1];
				nearest_point[2] = Query.ClosestTrianglePoint[2];
			}
			return (FMeshIndex)NearTriID;
		}

		// version of facet_in_envelope that always finds a point (but point is arbitrary if not in envelope); prefer facet_in_envelope
		void facet_in_envelope_with_hint(const Vector3& p, double sq_epsilon,
			FMeshIndex& nearest_facet_out, Vector3& nearest_point, double& sq_dist) const
		{
			checkSlow(bTreeBuilt);
			int32 NearTriID = -1;
			FVector Point(p[0], p[1], p[2]);
			bool bFound = Tree.IsWithinDistanceSquared(Point, sq_epsilon, NearTriID);
			if (!bFound)
			{
				int32 InGuess = (int32)nearest_facet_out;
				if (InGuess >= 0 && InGuess < Tris.Num())
				{
					NearTriID = InGuess;
				}
				else
				{
					NearTriID = Tris.Num() - 1; // arbitrary triangle
				}
			}
			if (NearTriID > -1)
			{
				UE::Geometry::FDistPoint3Triangle3d Query = UE::Geometry::TMeshQueries<FMeshWrapper>::TriangleDistance(Mesh, NearTriID, Point);
				sq_dist = Query.GetSquared();
				nearest_point[0] = Query.ClosestTrianglePoint[0];
				nearest_point[1] = Query.ClosestTrianglePoint[1];
				nearest_point[2] = Query.ClosestTrianglePoint[2];
			}
			nearest_facet_out = (FMeshIndex)NearTriID;
		}

		double SquaredDistanceToTri(const Vector3& p, int32 TriIdx) const
		{
			UE::Geometry::FTriangle3d Triangle;
			Mesh.GetTriVertices(TriIdx, Triangle.V[0], Triangle.V[1], Triangle.V[2]);

			FVector Point(p[0], p[1], p[2]);
			UE::Geometry::FDistPoint3Triangle3d Distance(Point, Triangle);
			return Distance.GetSquared();
		}

		double SquaredDistanceToTri(const Vector3& p, int32 TriIdx, Vector3& OutP) const
		{
			UE::Geometry::FTriangle3d Triangle;
			Mesh.GetTriVertices(TriIdx, Triangle.V[0], Triangle.V[1], Triangle.V[2]);

			FVector Point(p[0], p[1], p[2]);
			UE::Geometry::FDistPoint3Triangle3d Distance(Point, Triangle);
			double dsq = Distance.GetSquared();

			OutP[0] = Distance.ClosestTrianglePoint[0];
			OutP[1] = Distance.ClosestTrianglePoint[1];
			OutP[2] = Distance.ClosestTrianglePoint[2];
			return dsq;
		}

		TArray<FVector> Vertices;
		TArray<UE::Geometry::FIndex3i> Tris;
		FMeshWrapper Mesh;
		UE::Geometry::TMeshAABBTree3<FMeshWrapper> Tree;
		bool bTreeBuilt = false;
	};


    class AABBWrapper {
		FTriTreeWrapper SFTree;

		// TODO: It would be more efficient to use a segment tree for BTree and TmpBTree; see FSegmentTree3
		FTriTreeWrapper BTree;
		FTriTreeWrapper TmpBTree;

    public:



        //// initialization
		inline Scalar get_sf_diag() const { return (Scalar)SFTree.Tree.GetBoundingBox().DiagonalLength(); }

		AABBWrapper(const std::vector<Vector3>& V, const std::vector<Vector3i>& F)
		{
			SFTree.SetFromMesh(V, F);
			SFTree.BuildTree();
		}

		Vector3 GetSurfaceVertex(int32 FaceIdx, int32 SubIdx)
		{
			int32 VIdx = SFTree.Tris[FaceIdx][SubIdx];
			FVector V = SFTree.Vertices[VIdx];
			return Vector3(V[0], V[1], V[2]);
		}

#ifdef NEW_ENVELOPE
        fastEnvelope::FastEnvelope b_tree_exact;
        fastEnvelope::FastEnvelope tmp_b_tree_exact;
        fastEnvelope::FastEnvelope sf_tree_exact;
        fastEnvelope::FastEnvelope sf_tree_exact_simplify;
//        std::shared_ptr<fastEnvelope::FastEnvelope> b_tree_exact;
//        std::shared_ptr<fastEnvelope::FastEnvelope> tmp_b_tree_exact;
//        std::shared_ptr<fastEnvelope::FastEnvelope> sf_tree_exact;

        inline void init_sf_tree(const std::vector<Vector3> &vs, const std::vector<Vector3i> &fs, double eps) {
//            sf_tree_exact = std::make_shared<fastEnvelope::FastEnvelope>(vs, fs, eps);
            sf_tree_exact.init(vs, fs, eps);
            sf_tree_exact_simplify.init(vs, fs, 0.8*eps);
        }
        inline void init_sf_tree(const std::vector<Vector3> &vs, const std::vector<Vector3i> &fs,
                std::vector<double>& eps, double bbox_diag_length) {
            for (auto& e: eps)
                e *= bbox_diag_length;
            sf_tree_exact.init(vs, fs, eps);
            std::vector<double> eps_simplify = eps;
            for (auto& e: eps_simplify)
                e *= 0.8;
            sf_tree_exact_simplify.init(vs, fs, eps_simplify);
        }
#endif

        void init_b_mesh_and_tree(const std::vector<Vector3> &input_vertices, const std::vector<Vector3i> &input_faces, Mesh &mesh);

        void init_tmp_b_mesh_and_tree(const std::vector<Vector3> &input_vertices,
                                      const std::vector<Vector3i> &input_faces,
                                      const std::vector<std::array<int, 2>> &b_edges1,
                                      const Mesh &mesh, const std::vector<std::array<int, 2>> &b_edges2);

        //// projection
        inline Scalar project_to_sf(Vector3 &p) const {
			Vector3 nearest_p;
            double sq_dist = std::numeric_limits<double>::max();
			// Note: This projection specifically is very sensitive to floating point differences; final result may change if the nearest facet is computed slightly differently 
			SFTree.nearest_facet(p, nearest_p, sq_dist);
            p[0] = nearest_p[0];
            p[1] = nearest_p[1];
            p[2] = nearest_p[2];

            return sq_dist;
        }

        inline Scalar project_to_b(Vector3 &p) const {
			Vector3 nearest_p;
            double sq_dist = std::numeric_limits<double>::max();
			BTree.nearest_facet(p, nearest_p, sq_dist);
            p[0] = nearest_p[0];
            p[1] = nearest_p[1];
            p[2] = nearest_p[2];

            return sq_dist;
        }

        inline Scalar project_to_tmp_b(Vector3 &p) const {
            Vector3 nearest_p;
            double sq_dist = std::numeric_limits<double>::max();
			TmpBTree.nearest_facet(p, nearest_p, sq_dist);
            p[0] = nearest_p[0];
            p[1] = nearest_p[1];
            p[2] = nearest_p[2];

            return sq_dist;
        }

        inline int get_nearest_face_sf(const Vector3 &p) const {
            Vector3 nearest_p;
            double sq_dist = std::numeric_limits<double>::max();
            return SFTree.nearest_facet(p, nearest_p, sq_dist);
        }

        inline Scalar get_sq_dist_to_sf(const Vector3 &p) const {
            Vector3 nearest_p;
            double sq_dist = std::numeric_limits<double>::max();
			SFTree.nearest_facet(p, nearest_p, sq_dist);
            return sq_dist;
        }

	private:

		inline bool is_out_envelope(const FTriTreeWrapper& Tree, const std::vector<Vector3>& ps, const Scalar eps_2,
			FMeshIndex prev_facet = INDEX_NONE) const
		{
			for (const Vector3& current_point : ps)
			{
				if (prev_facet != INDEX_NONE && Tree.SquaredDistanceToTri(current_point, (int32)prev_facet) <= eps_2)
				{
					continue;
				}
				int32 FoundFacet;
				if (Tree.facet_in_envelope(current_point, eps_2, FoundFacet))
				{
					prev_facet = FMeshIndex(FoundFacet);
				}
				else
				{
					return true;
				}
			}
			return false;
		}
	public:

        //// envelope check - triangle
		inline bool is_out_sf_envelope(const std::vector<Vector3>& ps, const Scalar eps_2,
			FMeshIndex prev_facet = INDEX_NONE) const {
			return is_out_envelope(SFTree, ps, eps_2, prev_facet);
		}

		inline bool is_out_b_envelope(const std::vector<Vector3>& ps, const Scalar eps_2,
			FMeshIndex prev_facet = INDEX_NONE) const {
			return is_out_envelope(BTree, ps, eps_2, prev_facet);
		}

		inline bool is_out_tmp_b_envelope(const std::vector<Vector3>& ps, const Scalar eps_2,
			FMeshIndex prev_facet = INDEX_NONE) const {
			return is_out_envelope(TmpBTree, ps, eps_2, prev_facet);
		}

#ifdef NEW_ENVELOPE
        inline bool is_out_sf_envelope_exact(const std::array<Vector3, 3> &triangle) const {
            return sf_tree_exact.is_outside(triangle);
        }

        inline bool is_out_sf_envelope_exact_simplify(const std::array<Vector3, 3> &triangle) const {
            return sf_tree_exact_simplify.is_outside(triangle);
        }

        inline bool is_out_b_envelope_exact(const std::array<Vector3, 3> &triangle) const {
            return b_tree_exact.is_outside(triangle);
        }

        inline bool is_out_tmp_b_envelope_exact(const std::array<Vector3, 3> &triangle) const {
            return tmp_b_tree_exact.is_outside(triangle);
        }
#endif

        //// envelope check - point
        inline bool is_out_sf_envelope(const Vector3 &p, const Scalar eps_2, FMeshIndex &prev_facet) const {
            Vector3 nearest_p;
            double sq_dist;
			prev_facet = SFTree.facet_in_envelope(p, eps_2, nearest_p, sq_dist);

            if (Scalar(sq_dist) > eps_2)
                return true;
            return false;
        }

        inline bool is_out_sf_envelope(const Vector3& p, const Scalar eps_2,
                                       FMeshIndex& prev_facet, double& sq_dist, Vector3& nearest_p) const {
			if (prev_facet != INDEX_NONE) {
				sq_dist = SFTree.SquaredDistanceToTri(p, (int32)prev_facet, nearest_p);
			}
			if (Scalar(sq_dist) > eps_2) {
				SFTree.facet_in_envelope_with_hint(p, eps_2, prev_facet, nearest_p, sq_dist);
			}

			if (Scalar(sq_dist) > eps_2)
				return true;
			return false;
        }

		inline bool is_out_b_envelope(const Vector3& p, const Scalar eps_2, FMeshIndex& prev_facet) const {
			Vector3 nearest_p;
			double sq_dist;
			prev_facet = BTree.facet_in_envelope(p, eps_2, nearest_p, sq_dist); // TODO @JIMMY can this convert to the version that doesn't always find a facet?

			if (Scalar(sq_dist) > eps_2)
				return true;
			return false;
		}

		inline bool is_out_tmp_b_envelope(const Vector3& p, const Scalar eps_2, FMeshIndex& prev_facet) const {
			Vector3 nearest_p;
			double sq_dist;
			prev_facet = TmpBTree.facet_in_envelope(p, eps_2, nearest_p, sq_dist); // TODO @JIMMY can this convert to the version that doesn't always find a facet?

			if (Scalar(sq_dist) > eps_2)
				return true;
			return false;
		}

#ifdef NEW_ENVELOPE
        inline bool is_out_sf_envelope_exact(const Vector3& p) const {
            return sf_tree_exact.is_outside(p);
        }

        inline bool is_out_b_envelope_exact(const Vector3& p) const {
            return b_tree_exact.is_outside(p);
        }

        inline bool is_out_tmp_b_envelope_exact(const Vector3& p) const {
            return tmp_b_tree_exact.is_outside(p);
        }
#endif


        //fortest
        inline Scalar dist_sf_envelope(const std::vector<Vector3> &ps, const Scalar eps_2) const {///only used for checking correctness
            Vector3 nearest_point;
            double sq_dist = std::numeric_limits<double>::max();

            for (const Vector3&current_point : ps) {
				int32 NearTriID_Unused;
				bool bInEnvelope = SFTree.facet_in_envelope(current_point, eps_2, NearTriID_Unused);
				if (!bInEnvelope)
				{
					// Compute the actual squared distance to report error
					SFTree.nearest_facet(current_point, nearest_point, sq_dist);
					return sq_dist;
				}
            }

            return 0;
        }
        //fortest

    };

}
