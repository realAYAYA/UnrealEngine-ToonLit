// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/ConvertData.h"
#include "MuR/Platform.h"
#include "MuR/OpMeshChartDifference.h"
#include "MuR/MutableTrace.h"
#include "Math/Ray.h"
#include "TriangleTypes.h"
#include "BoxTypes.h"
#include "Intersection/IntrRay3Triangle3.h"
#include "MathUtil.h"
#include "IntVectorTypes.h"

#include "Math/UnrealMathUtility.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //! Create a map from vertices into vertices, collapsing vertices that have the same position, 
	//! This version uses UE Containers to return.	
    //---------------------------------------------------------------------------------------------
    inline void MeshCreateCollapsedVertexMap( const Mesh* pMesh, TArray<int32>& OutCollapsedVertexMap, TArray<FVector3f>& Vertices )
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshCreateCollapsedVertexMap);
    	
        int32 VCount = pMesh->GetVertexCount();

        OutCollapsedVertexMap.SetNum(VCount);
        Vertices.SetNum(VCount);

        const FMeshBufferSet& MBSPriv = pMesh->GetVertexBuffers();
        for (int32 b = 0; b < MBSPriv.m_buffers.Num(); ++b)
        {
            for (int32 c = 0; c < MBSPriv.m_buffers[b].m_channels.Num(); ++c)
            {
                MESH_BUFFER_SEMANTIC Sem = MBSPriv.m_buffers[b].m_channels[c].m_semantic;
                int32 SemIndex = MBSPriv.m_buffers[b].m_channels[c].m_semanticIndex;

                UntypedMeshBufferIteratorConst It(pMesh->GetVertexBuffers(), Sem, SemIndex);

                switch (Sem)
                {
                case MBS_POSITION:
				{
                    // First create a cache of the vertices in the vertices array
                    for (int32 V = 0; V < VCount; ++V)
                    {
                        FVector3f Vertex(0.0f, 0.0f, 0.0f);
                        for (int32 I = 0; I < 3; ++I)
                        {
                            ConvertData(I, &Vertex[0], MBF_FLOAT32, It.ptr(), It.GetFormat());
                        }

                        Vertices[V] = Vertex;

                        ++It;
                    }
					
                    // Create map to store which vertices are the same (collapse nearby vertices)
					const int32 VerticesNum = Vertices.Num();
                    for (int32 V = 0; V < VerticesNum; ++V)
                    {
                        OutCollapsedVertexMap[V] = V;

                        for (int32 CandidateV = 0; CandidateV < V; ++CandidateV)
                        {
                            const int32 CollapsedCandidateVIdx = OutCollapsedVertexMap[CandidateV];

                            // Test whether v and the collapsed candidate are close enough to be collapsed
                            const FVector3f R = Vertices[CollapsedCandidateVIdx] - Vertices[V];

                            if (R.Dot(R) <= TMathUtilConstants<float>::ZeroTolerance)
                            {
                                OutCollapsedVertexMap[V] = CollapsedCandidateVIdx;
                                break;
                            }
                        }
                    }

                    break;
				}
                default:
                    break;
                }
            }
        }
    }

    //---------------------------------------------------------------------------------------------
    //! Return true if a mesh is closed
    //---------------------------------------------------------------------------------------------
    inline bool MeshIsClosed( const Mesh* pMesh )
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshIsClosed);

        int vcount = pMesh->GetVertexCount();
        int fcount = pMesh->GetFaceCount();

        TArray<FVector3f> vertices;

        // Map in ClipMesh from vertices to the one they are collapsed to because they are very similar, if they aren't collapsed then they are mapped to themselves
		TArray<int> collapsedVertexMap;

        MeshCreateCollapsedVertexMap( pMesh, collapsedVertexMap, vertices );

        // Acumulate edges
        TMap< TPair<int,int>, int > faceCountPerEdge;

        UntypedMeshBufferIteratorConst itClipMesh(pMesh->GetIndexBuffers(), MBS_VERTEXINDEX);
        for (int f = 0; f < fcount; ++f)
        {
            int face[3];
            face[0] = collapsedVertexMap[itClipMesh.GetAsUINT32()]; ++itClipMesh;
            face[1] = collapsedVertexMap[itClipMesh.GetAsUINT32()]; ++itClipMesh;
            face[2] = collapsedVertexMap[itClipMesh.GetAsUINT32()]; ++itClipMesh;

            for (int e=0; e<3; ++e)
            {
                int v0 = face[e];
                int v1 = face[(e+1)%3];

                if(v0==v1)
                {
                    // Degenerated mesh
                    return false;
                }

                TPair<int,int> edge;
                edge.Key = FMath::Min( v0, v1 );
                edge.Value = FMath::Max( v0, v1 );

				if (faceCountPerEdge.Contains(edge))
				{
					faceCountPerEdge[edge]++;
				}
				else
				{
					faceCountPerEdge.Add(edge,1);
				}
            }
        }


        // See if every edge has 2 faces
        for( const auto& e: faceCountPerEdge)
        {
            if (e.Value!=2)
            {
                return false;
            }
        }

        return true;
    }


    //---------------------------------------------------------------------------------------------
    //! Remove all unused vertices from a mesh, and fix its index buffers.
    //---------------------------------------------------------------------------------------------
    inline void MeshRemoveUnusedVertices( Mesh* pMesh )
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshRemoveUnusedVertices);

        // Mark used vertices
		TArray<uint8> used;
		used.SetNumZeroed(pMesh->GetVertexCount());
        UntypedMeshBufferIteratorConst iti(pMesh->GetIndexBuffers(), MBS_VERTEXINDEX);
        int IndexCount = pMesh->GetIndexCount();
        for (int i = 0; i < IndexCount; ++i)
        {
            uint32 Index = iti.GetAsUINT32();
            ++iti;
            used[Index] = true;
        }

        // Build vertex map
		TArray<int32> oldToNewVertex;
		oldToNewVertex.SetNumUninitialized(pMesh->GetVertexCount());
        int32 totalNewVertices = 0;
        for (int32 v = 0; v<pMesh->GetVertexCount(); ++v)
        {
            if (used[v])
            {
                oldToNewVertex[v] = totalNewVertices;
                ++totalNewVertices;
            }
            else
            {
                oldToNewVertex[v] = -1;
            }
        }

        // Remove unused vertices and build index map
        for (int b = 0; b<pMesh->GetVertexBuffers().GetBufferCount(); ++b)
        {
            int elemSize = pMesh->GetVertexBuffers().GetElementSize(b);
            const uint8* pSourceData = pMesh->GetVertexBuffers().GetBufferData(b);
            uint8* pData = pMesh->GetVertexBuffers().GetBufferData(b);
            for (int v = 0; v<pMesh->GetVertexCount(); ++v)
            {
                if (oldToNewVertex[v]!=-1)
                {
                    uint8* pElemData = pData + elemSize*oldToNewVertex[v];
                    const uint8* pElemSourceData = pSourceData + elemSize*v;
                    // Avoid warning for overlapping memcpy in valgrind
                    if (pElemData != pElemSourceData)
                    {
                        memcpy(pElemData, pElemSourceData, elemSize);
                    }
                }
            }
        }
        pMesh->GetVertexBuffers().SetElementCount(totalNewVertices);

        // Update indices
        UntypedMeshBufferIteratorConst ito(pMesh->GetIndexBuffers(), MBS_VERTEXINDEX);
        if (ito.GetFormat() == MBF_UINT32)
        {
            for (int i = 0; i < IndexCount; ++i)
            {
                uint32 Index = *(uint32*)ito.ptr();
                int32 NewIndex = oldToNewVertex[Index];
                check(NewIndex >= 0);
                *(uint32*)ito.ptr() = (uint32)NewIndex;
                ++ito;
            }
        }
        else if (ito.GetFormat() == MBF_UINT16)
        {
            for (int i = 0; i < IndexCount; ++i)
            {
                uint16 Index = *(uint16*)ito.ptr();
                int32 NewIndex = oldToNewVertex[Index];
                check(NewIndex >= 0);
                *(uint16*)ito.ptr() = (uint16)NewIndex;
                ++ito;
            }
        }
        else
        {
            checkf(false, TEXT("Format not implemented.") );
        }


        // \todo: face buffers?
    }


    //---------------------------------------------------------------------------------------------
    //! Reference version
    //---------------------------------------------------------------------------------------------
    inline int get_num_intersections( const vec3f& vertex,
                                      const vec3f& ray,
                                      const TArray<vec3f>& vertices,
                                      const TArray<uint32>& faces,
                                      TArray<int>& collapsedVertexMap,
									  TArray<uint8>& vertex_already_intersected,
                                      TMap<TPair<int, int>, bool>& edge_already_intersected,
									  float dynamic_epsilon
                                      )
    {
    	
        MUTABLE_CPUPROFILER_SCOPE(get_num_intersections);
    	
        int num_intersections = 0;
        vec3f intersection;

		FMemory::Memzero( vertex_already_intersected.GetData(), vertex_already_intersected.Num());

        edge_already_intersected.Empty();

    #define get_collapsed_vertex(vertex_idx) (vertices[collapsedVertexMap[vertex_idx]])

        size_t fcount = faces.Num()/3;

        // Check vertex against all ClipMorph faces
        for (uint32 face = 0; face < fcount; ++face)
        {
            uint32 vertex_indexs[3] = { faces[3 * face], faces[3 * face + 1], faces[3 * face + 2] };

            vec3f v0 = get_collapsed_vertex(vertex_indexs[0]);
            vec3f v1 = get_collapsed_vertex(vertex_indexs[1]);
            vec3f v2 = get_collapsed_vertex(vertex_indexs[2]);

            int out_intersected_vert, out_intersected_edge_v0, out_intersected_edge_v1;

            if (rayIntersectsFace(vertex, ray, v0, v1, v2, intersection, out_intersected_vert, out_intersected_edge_v0, out_intersected_edge_v1, dynamic_epsilon))
            {
                bool vertex_not_intersected_before = true;
                bool edge_not_intersected_before = true;

                if (out_intersected_vert >= 0)
                {
                    int collapsed_vert_index = collapsedVertexMap[vertex_indexs[out_intersected_vert]];
                    vertex_not_intersected_before = !vertex_already_intersected[collapsed_vert_index];

                    vertex_already_intersected[collapsed_vert_index] = true;
                }

                if (out_intersected_edge_v0 >= 0)
                {
                    int collapsed_edge_vert_index0 = collapsedVertexMap[vertex_indexs[out_intersected_edge_v0]];
                    int collapsed_edge_vert_index1 = collapsedVertexMap[vertex_indexs[out_intersected_edge_v1]];

                    TPair<int, int> key;
                    key.Key = collapsed_edge_vert_index0 <= collapsed_edge_vert_index1 ? collapsed_edge_vert_index0 : collapsed_edge_vert_index1;
                    key.Value = collapsed_edge_vert_index1 > collapsed_edge_vert_index0 ? collapsed_edge_vert_index1 : collapsed_edge_vert_index0;
                    edge_not_intersected_before = edge_already_intersected.Find(key) != nullptr;

                    if (edge_not_intersected_before)
                    {
                        edge_already_intersected.Add( key, true );
                    }
                }

                if (vertex_not_intersected_before && edge_not_intersected_before)
                {
                    num_intersections++;
                }
            }
        }

        return num_intersections;
    }


    //---------------------------------------------------------------------------------------------
    //! Core Geometry version
    //---------------------------------------------------------------------------------------------
    inline int GetNumIntersections( const FRay3f& Ray,
                                    const TArray<FVector3f>& Vertices,
                                    const TArray<uint32>& Faces,
                                    const TArray<int32>& CollapsedVertexMap,
                                    TArray<uint8>& InOutVertexAlreadyIntersected,
                                    TSet<uint64>& InOutEdgeAlreadyIntersected,
									const float DynamicEpsilon )
    {
		using namespace UE::Geometry;    

        MUTABLE_CPUPROFILER_SCOPE(GetNumIntersections);
    	
        int32 NumIntersections = 0;

		FMemory::Memzero( InOutVertexAlreadyIntersected.GetData(), InOutVertexAlreadyIntersected.Num() );
		InOutEdgeAlreadyIntersected.Empty();

		auto GetCollapsedVertex = [&CollapsedVertexMap, &Vertices]( const uint32 V ) -> const FVector3f&
		{
			return Vertices[ CollapsedVertexMap[V] ];
		};

        int32 FaceCount = Faces.Num() / 3;

		UE::Geometry::FIntrRay3Triangle3f Intersector(Ray, UE::Geometry::FTriangle3f());

        // Check vertex against all ClipMorph faces
        for (int32 Face = 0; Face < FaceCount; ++Face)
        {
            const uint32 VertexIndexs[3] = { Faces[3 * Face], Faces[3 * Face + 1], Faces[3 * Face + 2] };

            const FVector3f& V0 = GetCollapsedVertex(VertexIndexs[0]);
            const FVector3f& V1 = GetCollapsedVertex(VertexIndexs[1]);
            const FVector3f& V2 = GetCollapsedVertex(VertexIndexs[2]);

			Intersector.Triangle = UE::Geometry::FTriangle3f( V0, V1, V2 );

			if ( Intersector.Find() )
            {
				// Find if close to edge using barycentric coordinates form intersector.
	
				// Is the Dynamic Epsilon needed?. Intersector bary coords are in the range [0.0, 1.0], 
				// furthermore, IntrRay3Triangle3f::TriangleBaryCoords are double pressison, not sure 
				// is intentional or is a bug ( is double even when the FReal type is float ) 
				const bool IntersectsEdge01 = FMath::IsNearlyZero( Intersector.TriangleBaryCoords.Z, DynamicEpsilon );
				const bool IntersectsEdge02 = FMath::IsNearlyZero( Intersector.TriangleBaryCoords.Y, DynamicEpsilon );
				const bool IntersectsEdge12 = FMath::IsNearlyZero( Intersector.TriangleBaryCoords.X, DynamicEpsilon );

				int32 IntersectedTriangleVertexId = -1;
				IntersectedTriangleVertexId =  (IntersectsEdge01 & IntersectsEdge02) ? 0 : IntersectedTriangleVertexId;
				IntersectedTriangleVertexId =  (IntersectsEdge01 & IntersectsEdge12) ? 1 : IntersectedTriangleVertexId;
				IntersectedTriangleVertexId =  (IntersectsEdge02 & IntersectsEdge12) ? 2 : IntersectedTriangleVertexId;

				bool bIsAlreadyIntersected = false;

                if ( IntersectedTriangleVertexId >= 0 )
                {
                    const int32 CollapsedVertIndex = CollapsedVertexMap[VertexIndexs[IntersectedTriangleVertexId]];
                    bIsAlreadyIntersected = InOutVertexAlreadyIntersected[CollapsedVertIndex] == 0;

                    InOutVertexAlreadyIntersected[CollapsedVertIndex] = true;
                }
				else if ( IntersectsEdge01 | IntersectsEdge02 | IntersectsEdge12 )
                {
					int32 EdgeV0 = (IntersectsEdge01 | IntersectsEdge02) ? 0 : 1;
					int32 EdgeV1 = IntersectsEdge01 ? 1 : 2;				

                    const int32 CollapsedEdgeVertIndex0 = CollapsedVertexMap[VertexIndexs[EdgeV0]];
                    const int32 CollapsedEdgeVertIndex1 = CollapsedVertexMap[VertexIndexs[EdgeV1]];

                    const uint64 EdgeKey = ( ( (uint64)FMath::Max( CollapsedEdgeVertIndex0, CollapsedEdgeVertIndex1 ) ) << 32 ) | 
							                   (uint64)FMath::Min( CollapsedEdgeVertIndex0, CollapsedEdgeVertIndex1 );
	
					InOutEdgeAlreadyIntersected.FindOrAdd( EdgeKey, &bIsAlreadyIntersected );
                }

                NumIntersections += (int32)(!bIsAlreadyIntersected);
            }
        }

        return NumIntersections;
    }


    //---------------------------------------------------------------------------------------------
    //! Core Geometry version
    //---------------------------------------------------------------------------------------------
    inline void MeshClipMeshClassifyVertices(TArray<uint8>& VertexInClipMesh,
                                             const Mesh* pBase, const Mesh* pClipMesh)
    {
		using namespace UE::Geometry;

        MUTABLE_CPUPROFILER_SCOPE(MeshClipMeshClassifyVertices);
    	
        // TODO: this fails in bro for some object. do it at graph construction time to report it 
        // nicely.
        //check( MeshIsClosed(pClipMesh) );

        const int32 VCount = pClipMesh->GetVertexBuffers().GetElementCount();
        const int32 FCount = pClipMesh->GetFaceCount();

        int32 OrigVertCount = pBase->GetVertexBuffers().GetElementCount();

        // Stores whether each vertex in the original mesh in the clip mesh volume
        VertexInClipMesh.AddZeroed(OrigVertCount);

        if (VCount == 0)
        {
            return;
        }

        TArray<FVector3f> Vertices; // ClipMesh vertex cache
		Vertices.SetNumUninitialized(VCount);

        TArray<uint32> Faces; // ClipMesh face cache
		Faces.SetNumUninitialized(FCount * 3); 

        // Map in ClipMesh from vertices to the one they are collapsed to because they are very 
        // similar, if they aren't collapsed then they are mapped to themselves
        TArray<int32> CollapsedVertexMap;
		CollapsedVertexMap.AddUninitialized(VCount);

        MeshCreateCollapsedVertexMap( pClipMesh, CollapsedVertexMap, Vertices );

        // Create cache of the faces
        UntypedMeshBufferIteratorConst ItClipMesh(pClipMesh->GetIndexBuffers(), MBS_VERTEXINDEX);

        for ( int32 F = 0; F < FCount; ++F )
        {
            Faces[3 * F]     = ItClipMesh.GetAsUINT32(); ++ItClipMesh;
            Faces[3 * F + 1] = ItClipMesh.GetAsUINT32(); ++ItClipMesh;
            Faces[3 * F + 2] = ItClipMesh.GetAsUINT32(); ++ItClipMesh;
        }


        // Create a bounding box of the clip mesh
		UE::Geometry::FAxisAlignedBox3f ClipMeshBoundingBox = UE::Geometry::FAxisAlignedBox3f::Empty();

        for ( const FVector3f& Vert : Vertices )
        {
            ClipMeshBoundingBox.Contain( Vert );
        }

		// Dynamic distance epsilon to support different engines
		const float MaxDimensionBoundingBox = ClipMeshBoundingBox.DiagonalLength();
		// 0.000001 is the value that helps to achieve the dynamic epsilon, do not change it
		const float DynamicEpsilon = 0.000001f * MaxDimensionBoundingBox * (MaxDimensionBoundingBox < 1.0f ? MaxDimensionBoundingBox : 1.0f);

        // Create an acceleration grid to avoid testing all clip-mesh triangles.
        // This assumes that the testing ray direction is Z
        constexpr int32 GRID_SIZE = 8;
        TUniquePtr<TArray<uint32>[]> GridFaces( new TArray<uint32>[GRID_SIZE*GRID_SIZE] );
        const FVector2f GridCellSize = FVector2f( ClipMeshBoundingBox.Width(), ClipMeshBoundingBox.Height() ) / (float)GRID_SIZE;

        for ( int32 I = 0; I < GRID_SIZE; ++I )
        {
            for ( int32 J = 0; J < GRID_SIZE; ++J )
            {
				const FVector2f BBoxMin = FVector2f(ClipMeshBoundingBox.Min.X, ClipMeshBoundingBox.Min.Y) + GridCellSize * FVector2f(I, J);

                FAxisAlignedBox2f CellBox( BBoxMin, BBoxMin + GridCellSize );

                TArray<uint32>& CellFaces = GridFaces[I+J*GRID_SIZE];
                CellFaces.Empty(FCount/GRID_SIZE);
                for (int32 F = 0; F < FCount; ++F)
                {
                    // Imprecise, conservative classification of faces.
					const FVector3f& V0 = Vertices[ Faces[3*F + 0] ];
					const FVector3f& V1 = Vertices[ Faces[3*F + 1] ];
					const FVector3f& V2 = Vertices[ Faces[3*F + 2] ];

					FAxisAlignedBox2f FaceBox;
					FaceBox.Contain( FVector2f( V0.X, V0.Y ) );
                    FaceBox.Contain( FVector2f( V1.X, V1.Y ) );
                    FaceBox.Contain( FVector2f( V2.X, V2.Y ) );

                    if (CellBox.Intersects(FaceBox))
                    {
                        CellFaces.Add( Faces[3*F + 0] );
                        CellFaces.Add( Faces[3*F + 1] );
                        CellFaces.Add( Faces[3*F + 2] );
                    }
                }
            }
        }	

        // Now go through all vertices in the mesh and record whether they are inside or outside of the ClipMesh
        uint32 DestVertexCount = pBase->GetVertexCount();

        const FMeshBufferSet& MBSPriv2 = pBase->GetVertexBuffers();
        for (int32 b = 0; b < MBSPriv2.m_buffers.Num(); ++b)
        {
            for (int32 c = 0; c < MBSPriv2.m_buffers[b].m_channels.Num(); ++c)
            {
                MESH_BUFFER_SEMANTIC Sem = MBSPriv2.m_buffers[b].m_channels[c].m_semantic;
                int32 SemIndex = MBSPriv2.m_buffers[b].m_channels[c].m_semanticIndex;

                UntypedMeshBufferIteratorConst It(pBase->GetVertexBuffers(), Sem, SemIndex);

                TArray<uint8> VertexAlreadyIntersected;
				VertexAlreadyIntersected.AddZeroed(VCount);

                TSet<uint64> EdgeAlreadyIntersected;

                switch ( Sem )
                {
                case MBS_POSITION:
                    for (uint32 V = 0; V < DestVertexCount; ++V)
                    {
                        FVector3f Vertex(0.0f, 0.0f, 0.0f);
                        for (int32 Offset = 0; Offset < 3; ++Offset)
                        {
                            ConvertData(Offset, &Vertex[0], MBF_FLOAT32, It.ptr(), It.GetFormat());
                        }
						
						const FVector2f ClipBBoxMin = FVector2f( ClipMeshBoundingBox.Min.X, ClipMeshBoundingBox.Min.Y);
						const FVector2f ClipBBoxSize = FVector2f( ClipMeshBoundingBox.Width(), ClipMeshBoundingBox.Height() );

						const FVector2i HPos = FVector2i( ( ( FVector2f( Vertex.X, Vertex.Y ) - ClipBBoxMin ) / ClipBBoxSize ) * (float)GRID_SIZE );   
						const FVector2i CellCoord = FVector2i( FMath::Clamp( HPos.X, 0, GRID_SIZE - 1 ), 
														       FMath::Clamp( HPos.Y, 0, GRID_SIZE - 1 ) );

                        // Early discard test: if the vertex is not inside the bounding box of the clip mesh, it won't be clipped.
                        const bool bContainsVertex = ClipMeshBoundingBox.Contains( Vertex );

                        if ( bContainsVertex )
                        {
                            // Optimised test	
							
							// Z-direction. Don't change this without reviewing the acceleration structure.
							FVector3f RayDir = FVector3f(0.0f, 0.0f, 1.0f);	
                            const int32 CellIndex = CellCoord.X + CellCoord.Y*GRID_SIZE;

                            int32 NumIntersections = GetNumIntersections( 
									FRay3f( Vertex, RayDir ), 
									Vertices,
									GridFaces[CellIndex],
									CollapsedVertexMap, 
									VertexAlreadyIntersected, 
									EdgeAlreadyIntersected, 
								    DynamicEpsilon);

                            // Full test BLEH, debug
//                            int32 FullNumIntersections = GetNumIntersections(
//									FRay3f( Vertex, RayDir ), 
//									Vertices,
//									Faces,
//									CollapsedVertexMap, 
//									VertexAlreadyIntersected, 
//									EdgeAlreadyIntersected, 
//								    DynamicEpsilon);

							VertexInClipMesh[V] = NumIntersections % 2 == 1;

                            // This may be used to debug degenerated cases if the conditional above is also removed.
                            // \todo: make sure it works well
//                          if (!bContainsVertex && VertexInClipMesh[V])
//                          {
//                              assert(false);
//                          }
                        }

                        ++It;
                    }
                    break;

                default:
                    break;
                }
            }
        }
    }	


    //---------------------------------------------------------------------------------------------
    //! Reference version
    //---------------------------------------------------------------------------------------------
    inline MeshPtr MeshClipWithMesh_Reference(const Mesh* pBase, const Mesh* pClipMesh)
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshClipWithMesh_Reference);
        
		MeshPtr pDest = pBase->Clone();

        uint32 vcount = pClipMesh->GetVertexBuffers().GetElementCount();
        if (!vcount)
        {
            return pDest; // Since there's nothing to clip against return a copy of the original base mesh
        }

        TArray<uint8> vertex_in_clip_mesh;  // Stores whether each vertex in the original mesh in in the clip mesh volume
        MeshClipMeshClassifyVertices( vertex_in_clip_mesh, pBase, pClipMesh );

        // Now remove all the faces from the result mesh that have all the vertices outside the clip volume
        UntypedMeshBufferIteratorConst itBase(pDest->GetIndexBuffers(), MBS_VERTEXINDEX);
        UntypedMeshBufferIterator itDest(pDest->GetIndexBuffers(), MBS_VERTEXINDEX);
        int aFaceCount = pDest->GetFaceCount();

        UntypedMeshBufferIteratorConst ito(pDest->GetIndexBuffers(), MBS_VERTEXINDEX);
        for (int f = 0; f < aFaceCount; ++f)
        {
            vec3<uint32> ov;
            ov[0] = ito.GetAsUINT32(); ++ito;
            ov[1] = ito.GetAsUINT32(); ++ito;
            ov[2] = ito.GetAsUINT32(); ++ito;

            bool all_verts_in = vertex_in_clip_mesh[ov[0]] && vertex_in_clip_mesh[ov[1]] && vertex_in_clip_mesh[ov[2]];

            if (!all_verts_in)
            {
                if (itDest.ptr() != itBase.ptr())
                {
					FMemory::Memcpy(itDest.ptr(), itBase.ptr(), itBase.GetElementSize() * 3);
                }

                itDest += 3;
            }

            itBase += 3;
        }

        SIZE_T removedIndices = itBase - itDest;
        check(removedIndices % 3 == 0);

        pDest->GetFaceBuffers().SetElementCount(aFaceCount - (int)removedIndices / 3);
        pDest->GetIndexBuffers().SetElementCount(aFaceCount * 3 - (int)removedIndices);

        // TODO: Should redo/reorder the face buffer before SetElementCount since some deleted faces could be left and some remaining faces deleted.

        //if (pDest->GetFaceBuffers().GetElementCount() == 0)
        //{
        //	pDest = pBase->Clone(); // If all faces have been discarded, return a copy of the unmodified mesh because unreal doesn't like empty meshes
        //}

        // [jordi] Remove unused vertices. This is necessary to avoid returning a mesh with vertices and no faces, which screws some
        // engines like Unreal Engine 4.
        MeshRemoveUnusedVertices( pDest.get() );

        return pDest;
    }


    //---------------------------------------------------------------------------------------------
    //! CoreGeometry version
    //---------------------------------------------------------------------------------------------
    inline MeshPtr MeshClipWithMesh(const Mesh* pBase, const Mesh* pClipMesh)
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshClipWithMesh);

        MeshPtr pDest = pBase->Clone();

        uint32 VCount = pClipMesh->GetVertexBuffers().GetElementCount();
        if (!VCount)
        {
            return pDest; // Since there's nothing to clip against return a copy of the original base mesh
        }

        TArray<uint8> VertexInClipMesh;  // Stores whether each vertex in the original mesh in in the clip mesh volume
        MeshClipMeshClassifyVertices( VertexInClipMesh, pBase, pClipMesh );

        // Now remove all the faces from the result mesh that have all the vertices outside the clip volume
        UntypedMeshBufferIteratorConst ItBase(pDest->GetIndexBuffers(), MBS_VERTEXINDEX);
        UntypedMeshBufferIterator ItDest(pDest->GetIndexBuffers(), MBS_VERTEXINDEX);
        int32 AFaceCount = pDest->GetFaceCount();

        UntypedMeshBufferIteratorConst Ito(pDest->GetIndexBuffers(), MBS_VERTEXINDEX);
        for (int32 F = 0; F < AFaceCount; ++F)
        {
			uint32 OV[3] = {0, 0, 0};

            OV[0] = Ito.GetAsUINT32(); ++Ito;
            OV[1] = Ito.GetAsUINT32(); ++Ito;
            OV[2] = Ito.GetAsUINT32(); ++Ito;

            bool AllVertsIn = VertexInClipMesh[OV[0]] && VertexInClipMesh[OV[1]] && VertexInClipMesh[OV[2]];

            if (!AllVertsIn)
            {
                if (ItDest.ptr() != ItBase.ptr())
                {
					FMemory::Memcpy(ItDest.ptr(), ItBase.ptr(), ItBase.GetElementSize() * 3);
                }

                ItDest += 3;
            }

            ItBase += 3;
        }

        SIZE_T RemovedIndices = ItBase - ItDest;
        check(RemovedIndices % 3 == 0);

        pDest->GetFaceBuffers().SetElementCount(AFaceCount - (int32)RemovedIndices / 3);
        pDest->GetIndexBuffers().SetElementCount(AFaceCount * 3 - (int32)RemovedIndices);

        // TODO: Should redo/reorder the face buffer before SetElementCount since some deleted faces could be left and some remaining faces deleted.

        //if (pDest->GetFaceBuffers().GetElementCount() == 0)
        //{
        //	pDest = pBase->Clone(); // If all faces have been discarded, return a copy of the unmodified mesh because unreal doesn't like empty meshes
        //}

        // [jordi] Remove unused vertices. This is necessary to avoid returning a mesh with vertices and no faces, which screws some
        // engines like Unreal Engine 4.
        MeshRemoveUnusedVertices( pDest.get() );

        return pDest;
    }


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    inline MeshPtr CreateMask( MeshPtrConst pBase, const TArray<uint8>& excludedVertices )
    {

        int maskVertexCount = 0;
        for( uint8 b: excludedVertices )
        {
            if (!b) ++maskVertexCount;
        }

        MeshPtr pMask = new Mesh();

        // Create the vertex buffer
        {
            pMask->GetVertexBuffers().SetElementCount( maskVertexCount );
            pMask->GetVertexBuffers().SetBufferCount( 1 );

			// Vertex index channel
			MESH_BUFFER_SEMANTIC semantic = MBS_VERTEXINDEX;
			int semanticIndex = 0;
			MESH_BUFFER_FORMAT format = MBF_UINT32;
			int components = 1;
			int offsets = 0;

            pMask->GetVertexBuffers().SetBuffer
                (
                    0,
                    4,
                    1,
                    &semantic,
                    &semanticIndex,
                    &format,
                    &components,
                    &offsets
                );
        }

        MeshBufferIterator<MBF_UINT32,uint32,1> itMask( pMask->GetVertexBuffers(), MBS_VERTEXINDEX, 0 );
        UntypedMeshBufferIteratorConst itBase( pBase->GetVertexBuffers(), MBS_VERTEXINDEX, 0 );

        for (size_t v = 0; v<excludedVertices.Num(); ++v)
        {
            if (!excludedVertices[v])
            {
                (*itMask)[0] = itBase.GetAsUINT32();
                ++itMask;
            }

            ++itBase;
        }

        return pMask;
    }


    //---------------------------------------------------------------------------------------------
    //! Generate a mask mesh with the faces of the base mesh inside the clip mesh.
    //---------------------------------------------------------------------------------------------
    inline MeshPtr MeshMaskClipMesh(const Mesh* pBase, const Mesh* pClipMesh )
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshMaskClipMesh);

        uint32 VCount = pClipMesh->GetVertexBuffers().GetElementCount();
        if (!VCount)
        {
            return nullptr; // Since there's nothing to clip against return a copy of the original base mesh
        }

        TArray<uint8> VertexInClipMesh;  // Stores whether each vertex in the original mesh in in the clip mesh volume
        MeshClipMeshClassifyVertices( VertexInClipMesh, pBase, pClipMesh );

        // We only remove vertices if all their faces are clipped
        TArray<uint8> VertexWithFaceNotClipped;
		VertexWithFaceNotClipped.AddZeroed( VertexInClipMesh.Num() );

        UntypedMeshBufferIteratorConst Ito(pBase->GetIndexBuffers(), MBS_VERTEXINDEX);
        int32 AFaceCount = pBase->GetFaceCount();
        for (int32 F = 0; F < AFaceCount; ++F)
        {
			uint32 OV[3] = { 0, 0, 0 };

            OV[0] = Ito.GetAsUINT32(); ++Ito;
            OV[1] = Ito.GetAsUINT32(); ++Ito;
            OV[2] = Ito.GetAsUINT32(); ++Ito;

            bool bFaceClipped =
                    VertexInClipMesh[OV[0]] &&
                    VertexInClipMesh[OV[1]] &&
                    VertexInClipMesh[OV[2]];

            if (!bFaceClipped)
            {
                VertexWithFaceNotClipped[OV[0]] = true;
                VertexWithFaceNotClipped[OV[1]] = true;
                VertexWithFaceNotClipped[OV[2]] = true;
            }
        }

        return CreateMask( pBase, VertexWithFaceNotClipped );
    }

    //---------------------------------------------------------------------------------------------
    //! Generate a mask mesh with the faces of the base mesh inside the clip mesh.
    //---------------------------------------------------------------------------------------------
    inline MeshPtr MeshMaskClipMesh_Reference(const Mesh* pBase, const Mesh* pClipMesh )
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshMaskClipMesh_Reference);

        uint32 vcount = pClipMesh->GetVertexBuffers().GetElementCount();
        if (!vcount)
        {
            return nullptr; // Since there's nothing to clip against return a copy of the original base mesh
        }

        TArray<uint8> vertex_in_clip_mesh;  // Stores whether each vertex in the original mesh in in the clip mesh volume
        MeshClipMeshClassifyVertices( vertex_in_clip_mesh, pBase, pClipMesh );

        // We only remove vertices if all their faces are clipped
		TArray<uint8> vertex_with_face_not_clipped;
		vertex_with_face_not_clipped.SetNumZeroed(vertex_in_clip_mesh.Num());

        UntypedMeshBufferIteratorConst ito(pBase->GetIndexBuffers(), MBS_VERTEXINDEX);
        int aFaceCount = pBase->GetFaceCount();
        for (int f = 0; f < aFaceCount; ++f)
        {
            vec3<uint32> ov;
            ov[0] = ito.GetAsUINT32(); ++ito;
            ov[1] = ito.GetAsUINT32(); ++ito;
            ov[2] = ito.GetAsUINT32(); ++ito;

            bool faceClipped =
                    vertex_in_clip_mesh[ov[0]] &&
                    vertex_in_clip_mesh[ov[1]] &&
                    vertex_in_clip_mesh[ov[2]];

            if (!faceClipped)
            {
                vertex_with_face_not_clipped[ov[0]] = true;
                vertex_with_face_not_clipped[ov[1]] = true;
                vertex_with_face_not_clipped[ov[2]] = true;
            }
        }


        return CreateMask( pBase, vertex_with_face_not_clipped );
    }

    //---------------------------------------------------------------------------------------------
    //! Generate a mask mesh with the faces of the base mesh matching the fragment.
    //---------------------------------------------------------------------------------------------
    inline MeshPtr MeshMaskDiff(const Mesh* pBase, const Mesh* pFragment )
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshMaskDiff);

        uint32 vcount = pFragment->GetVertexBuffers().GetElementCount();
        if (!vcount)
        {
            return nullptr;
        }

        int sourceFaceCount = pBase->GetFaceCount();
        int sourceVertexCount = pBase->GetVertexCount();
        int fragmentFaceCount = pFragment->GetFaceCount();


        // Make a tolerance proportional to the mesh bounding box size
        // TODO: Use precomputed bounding box
        box< vec3<float> > aabbox;
        if ( fragmentFaceCount > 0 )
        {
            MeshBufferIteratorConst<MBF_FLOAT32,float,3> itp( pFragment->GetVertexBuffers(), MBS_POSITION );

            aabbox.min = *itp;
            ++itp;

            for ( int v=1; v<pFragment->GetVertexBuffers().GetElementCount(); ++v )
            {
                aabbox.Bound( *itp );
                ++itp;
            }
        }
        float tolerance = 1e-5f * length(aabbox.size);
        Mesh::VERTEX_MATCH_MAP vertexMap;
        pFragment->GetVertexMap( *pBase, vertexMap, tolerance );


        // Classify the target faces in buckets along the Y axis
#define NUM_BUCKETS	128
#define AXIS		1
        TArray<int> buckets[ NUM_BUCKETS ];
        float bucketStart = aabbox.min[AXIS];
        float bucketSize = aabbox.size[AXIS] / NUM_BUCKETS;

        float bucketThreshold = ( 4 * tolerance ) / bucketSize;
        UntypedMeshBufferIteratorConst iti( pFragment->GetIndexBuffers(), MBS_VERTEXINDEX );
        MeshBufferIteratorConst<MBF_FLOAT32,float,3> itp( pFragment->GetVertexBuffers(), MBS_POSITION );
        for ( int tf=0; tf<fragmentFaceCount; tf++ )
        {
            uint32 index0 = iti.GetAsUINT32(); ++iti;
            uint32 index1 = iti.GetAsUINT32(); ++iti;
            uint32 index2 = iti.GetAsUINT32(); ++iti;
            float y = ( (*(itp+index0))[AXIS] + (*(itp+index1))[AXIS] + (*(itp+index2))[AXIS] ) / 3;
            float fbucket = (y-bucketStart) / bucketSize;
            int bucket = FMath::Min( NUM_BUCKETS-1, FMath::Max( 0, (int)fbucket ) );
            buckets[bucket].Add(tf);
            int hibucket = FMath::Min( NUM_BUCKETS-1, FMath::Max( 0, (int)(fbucket+bucketThreshold) ) );
            if (hibucket!=bucket)
            {
                buckets[hibucket].Add(tf);
            }
            int lobucket = FMath::Min( NUM_BUCKETS-1, FMath::Max( 0, (int)(fbucket-bucketThreshold) ) );
            if (lobucket!=bucket)
            {
                buckets[lobucket].Add(tf);
            }
        }

//		LogDebug("Box : min %.3f, %.3f, %.3f    size %.3f,%.3f,%.3f\n",
//				aabbox.min[0], aabbox.min[1], aabbox.min[2],
//				aabbox.size[0], aabbox.size[1], aabbox.size[2] );
//		for ( int b=0; b<NUM_BUCKETS; ++b )
//		{
//			LogDebug("bucket : %d\n", buckets[b].size() );
//		}

		TArray<uint8> faceClipped;
		faceClipped.SetNumZeroed(sourceFaceCount);

        UntypedMeshBufferIteratorConst ito( pBase->GetIndexBuffers(), MBS_VERTEXINDEX );
        MeshBufferIteratorConst<MBF_FLOAT32,float,3> itop( pBase->GetVertexBuffers(), MBS_POSITION );
        UntypedMeshBufferIteratorConst itti( pFragment->GetIndexBuffers(), MBS_VERTEXINDEX );
        for ( int f=0; f<sourceFaceCount; ++f )
        {
            bool hasFace = false;
            vec3<uint32> ov;
            ov[0] = ito.GetAsUINT32(); ++ito;
            ov[1] = ito.GetAsUINT32(); ++ito;
            ov[2] = ito.GetAsUINT32(); ++ito;

            // find the bucket for this face
            float y = ( (*(itop+ov[0]))[AXIS] + (*(itop+ov[1]))[AXIS] + (*(itop+ov[2]))[AXIS] ) / 3;
            float fbucket = (y-bucketStart) / bucketSize;
            int bucket = FMath::Min( NUM_BUCKETS-1, FMath::Max( 0, (int)fbucket ) );

            for ( int32 btf=0; !hasFace && btf<buckets[bucket].Num(); btf++ )
            {
                int tf =  buckets[bucket][btf];

                vec3<uint32> v;
                v[0] = (itti+3*tf+0).GetAsUINT32();
                v[1] = (itti+3*tf+1).GetAsUINT32();
                v[2] = (itti+3*tf+2).GetAsUINT32();

                hasFace = true;
                for ( int vi=0; hasFace && vi<3; ++vi )
                {
                    hasFace = vertexMap.Matches(v[vi],ov[0])
                         || vertexMap.Matches(v[vi],ov[1])
                         || vertexMap.Matches(v[vi],ov[2]);
                }
            }

            if ( hasFace )
            {
                faceClipped[f] = true;
            }
        }

        // We only remove vertices if all their faces are clipped
		TArray<uint8> vertex_with_face_not_clipped;
		vertex_with_face_not_clipped.SetNumZeroed(sourceVertexCount);

        UntypedMeshBufferIteratorConst itoi(pBase->GetIndexBuffers(), MBS_VERTEXINDEX);
        int aFaceCount = pBase->GetFaceCount();
        for (int f = 0; f < aFaceCount; ++f)
        {
            vec3<uint32> ov;
            ov[0] = itoi.GetAsUINT32(); ++itoi;
            ov[1] = itoi.GetAsUINT32(); ++itoi;
            ov[2] = itoi.GetAsUINT32(); ++itoi;

            if (!faceClipped[f])
            {
                vertex_with_face_not_clipped[ov[0]] = true;
                vertex_with_face_not_clipped[ov[1]] = true;
                vertex_with_face_not_clipped[ov[2]] = true;
            }
        }

        return CreateMask( pBase, vertex_with_face_not_clipped );
    }

}
