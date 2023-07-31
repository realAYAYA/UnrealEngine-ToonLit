// Copyright Epic Games, Inc. All Rights Reserved.

#include "Operations/SubdividePoly.h"
#include "GroupTopology.h"
#include "DynamicMesh/MeshNormals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SubdividePoly)

// OpenSubdiv currently only available on Windows. On other platforms we will make this a no-op
#if PLATFORM_WINDOWS
#define HAVE_OPENSUBDIV 1
#else
#define HAVE_OPENSUBDIV 0
#endif


#if HAVE_OPENSUBDIV

// OpenSubdiv needs M_PI defined
#ifndef M_PI
#define M_PI PI
#define LOCAL_M_PI 1
#endif

#pragma warning(push, 0)     
#include "opensubdiv/far/topologyRefiner.h"
#include "opensubdiv/far/topologyDescriptor.h"
#include "opensubdiv/far/primvarRefiner.h"
#pragma warning(pop)     

#if LOCAL_M_PI
#undef M_PI
#endif

#endif

using namespace UE::Geometry;

namespace SubdividePolyLocal
{
	// Vertices with positions and normals. Used for interpolating from polymesh to subdivision surface mesh.
	struct SubdVertex
	{
		FVertexInfo VertexInfo;

		void Clear()
		{
			VertexInfo = FVertexInfo();
		}

		void AddWithWeight(SubdVertex const& Src, float Weight)
		{
			VertexInfo.Position += Weight * Src.VertexInfo.Position;
			VertexInfo.Normal += Weight * Src.VertexInfo.Normal;
			VertexInfo.bHaveN = Src.VertexInfo.bHaveN;
			// TODO: add support for vertex color
		}
	};

	// Vertex with only UV data. This is used for "face-varying" interpolation, i.e. vertices can have multiple UV 
	// values if they are incident on triangles in different UV islands.
	struct SubdUVVertex
	{
		FVector2f VertexUV;
		bool bIsValid;

		SubdUVVertex(const FVector2f& InVertexUV, bool bInIsValid) :
			VertexUV{ InVertexUV },
			bIsValid(bInIsValid)
		{}

		void Clear()
		{
			VertexUV = FVector2f::Zero();
			bIsValid = true;
		}

		void AddWithWeight(SubdUVVertex const& Src, float Weight)
		{
			VertexUV += Weight * Src.VertexUV;
			bIsValid &= Src.bIsValid;
		}
	};


	// Compute the average of all normals corresponding to the given VertexID
	FVector3f GetAverageVertexNormalFromOverlay(const FDynamicMeshNormalOverlay* NormalOverlay,
												int VertexID)
	{
		TArray<int> NormalElements;
		NormalOverlay->GetVertexElements(VertexID, NormalElements);

		FVector3f SumElements{ 0.0f, 0.0f, 0.0f };
		float ElementCount = 0.0f;
		for (int ElementID : NormalElements)
		{
			SumElements += NormalOverlay->GetElement(ElementID);
			ElementCount += 1.0f;
		}

		return SumElements / ElementCount;
	}

	// Get the indices of GroupTopology "Corners" for a particular group boundary.
	void GetBoundaryCorners(const FGroupTopology::FGroupBoundary& Boundary,
							const FGroupTopology& Topology,
							TArray<int>& Corners)
	{
		int FirstEdgeIndex = Boundary.GroupEdges[0];
		Corners.Add(Topology.Edges[FirstEdgeIndex].EndpointCorners[0]);
		Corners.Add(Topology.Edges[FirstEdgeIndex].EndpointCorners[1]);

		int NextEdgeIndex = Boundary.GroupEdges[1];
		FIndex2i NextEdgeCorners = Topology.Edges[NextEdgeIndex].EndpointCorners;
		if (Corners[1] != NextEdgeCorners[0] && Corners[1] != NextEdgeCorners[1])
		{
			Swap(Corners[0], Corners[1]);
			check(Corners[1] == NextEdgeCorners[0] || Corners[1] == NextEdgeCorners[1]);
		}

		for (int i = 1; i < Boundary.GroupEdges.Num() - 1; ++i)
		{
			int EdgeIndex = Boundary.GroupEdges[i];
			FIndex2i CurrEdgeCorners = Topology.Edges[EdgeIndex].EndpointCorners;
			if (Corners.Last() == CurrEdgeCorners[0])
			{
				Corners.Add(CurrEdgeCorners[1]);
			}
			else
			{
				check(Corners.Last() == CurrEdgeCorners[1]);
				Corners.Add(CurrEdgeCorners[0]);
			}
		}
	}

	SubdVertex GetVertexInfo(int32 VertexID, const FDynamicMesh3& Mesh, bool bGetNormals)
	{
		SubdVertex Vertex;
		constexpr bool bWantColors = false;
		constexpr bool bWantVertexUVs = false;
		constexpr bool bWantNormals = false;

		Mesh.GetVertex(VertexID, Vertex.VertexInfo, bWantNormals, bWantColors, bWantVertexUVs);

		if (bGetNormals)
		{
			if (Mesh.HasAttributes() && (Mesh.Attributes()->NumNormalLayers() > 0))
			{
				Vertex.VertexInfo.Normal = GetAverageVertexNormalFromOverlay(Mesh.Attributes()->PrimaryNormals(),
																			 VertexID);
				Vertex.VertexInfo.bHaveN = true;
			}
		}

		return Vertex;
	}

	// Treat the given FGroupTopology as a polygonal mesh, and get its vertices
	void GetGroupPolyMeshVertices(const FDynamicMesh3& Mesh,
								  const FGroupTopology& Topology,
								  bool bGetNormals,
								  TArray<SubdVertex>& OutVertices )
	{
		for (const FGroupTopology::FCorner& Corner : Topology.Corners)
		{
			OutVertices.Add(GetVertexInfo(Corner.VertexID, Mesh, bGetNormals));
		}
	}

	void GetAllMeshVertices(const FDynamicMesh3& Mesh,
							bool bGetNormals,
							TArray<SubdVertex>& OutVertices)
	{
		for (int32 VertexID : Mesh.VertexIndicesItr())
		{
			OutVertices.Add(GetVertexInfo(VertexID, Mesh, bGetNormals));
		}
	}

	// Given a group and a vertex ID, return:
	// - a triangle in the group with one of its corners equal to vertex ID
	// - the (0-2) triangle corner index corresponding to the given vertex ID
	bool FindTriangleVertex(const FGroupTopology::FGroup& Group,
							int VertexID,
							const FDynamicMesh3& Mesh,
							TTuple<int, int>& OutTriangleVertex)
	{
		for (int Tri : Group.Triangles)		// TODO: do better than linear search
		{
			FIndex3i TriVertices = Mesh.GetTriangle(Tri);
			for (int i = 0; i < 3; ++i)
			{
				if (TriVertices[i] == VertexID)
				{
					OutTriangleVertex = TTuple<int, int>{ Tri, i };
					return true;
				}
			}
		}
		return false;
	}


	// Treating the GroupTopology as a polygonal mesh, get its "face-varying" UV coordinates. Assumes that each 
	// polygonal face has no UV seams cutting through it, but that UV seams might exist at polygon boundaries.
	// The UV data is returned in a buffer, with index buffer accompanying
	// NOTE: This is currently called twice -- first time to get the index buffers for topology refinement, and 
	// the second time to get the data for interpolation.

	template<bool bBuildingCoordBuffer, bool bBuildingIndexBuffer>
	bool GetGroupPolyMeshUVs(const FGroupTopology& Topology,
							 const FDynamicMesh3& Mesh,
							 const FDynamicMeshUVOverlay& UVOverlay,
							 TArray<SubdUVVertex>* OutUVs,
							 TArray<int>* OutUVFaceIndices)
	{
		TMap<int, int> ElementIDToUVBufferIndex;
		int UVVertexIndex = 0;

		for (const FGroupTopology::FGroup& Group : Topology.Groups)
		{
			if (!ensure(Group.Boundaries.Num() == 1))
			{
				return false;
			}
			if (!ensure(Group.Triangles.Num() > 0))
			{
				return false;
			}

			const FGroupTopology::FGroupBoundary& Bdry = Group.Boundaries[0];

			TArray<int> Corners;
			GetBoundaryCorners(Bdry, Topology, Corners);

			TArray<int> CornerUVIndices;

			for (int CornerID : Corners)
			{
				const int CornerVertexID = Topology.Corners[CornerID].VertexID;

				// Find a triangle in the group that has a vertex ID equal to the given corner
				TTuple<int, int> TriangleVertex;
				if (FindTriangleVertex(Group, CornerVertexID, Mesh, TriangleVertex))
				{
					const int TriangleID = TriangleVertex.Get<0>();
					const int TriVertexIndex = TriangleVertex.Get<1>();	// The (0,1,2) index of the polygon corner wrt the triangle

					const FIndex3i ElementTri = UVOverlay.GetTriangle(TriangleID);
					const int CornerElementID = ElementTri[TriVertexIndex];

					if (ElementIDToUVBufferIndex.Contains(CornerElementID))
					{
						// CornerElementID has already been added to the buffer
						if constexpr (bBuildingIndexBuffer)
						{
							CornerUVIndices.Add(ElementIDToUVBufferIndex[CornerElementID]);
						}
					}
					else
					{
						if (CornerElementID != FDynamicMesh3::InvalidID)
						{
							ElementIDToUVBufferIndex.Add(CornerElementID, UVVertexIndex);
						}

						if constexpr (bBuildingIndexBuffer)
						{
							CornerUVIndices.Add(UVVertexIndex);
						}

						if constexpr (bBuildingCoordBuffer)
						{
							FIndex3i ElementIndices = UVOverlay.GetTriangle(TriangleID);
							if ( ElementIndices[TriVertexIndex] == FDynamicMesh3::InvalidID)
							{
								// Missing UV data for the triangle
								OutUVs->Add(SubdUVVertex{ FVector2f{0,0}, false });
							}
							else
							{
								FVector2f UV = UVOverlay.GetElement(ElementIndices[TriVertexIndex]);
								OutUVs->Add(SubdUVVertex{ UV, true });
							}
						}

						++UVVertexIndex;
					}
				}
				else
				{
					return false;
				}
			}

			if constexpr (bBuildingIndexBuffer)
			{
				OutUVFaceIndices->Append(CornerUVIndices);
			}
		}
		return true;
	}


	inline bool GetGroupPolyMeshUVCoords(const FGroupTopology& Topology,
		const FDynamicMesh3& Mesh,
		const FDynamicMeshUVOverlay& UVOverlay,
		TArray<SubdUVVertex>* OutUVCoords)
	{
		return GetGroupPolyMeshUVs<true, false>(Topology, Mesh, UVOverlay, OutUVCoords, nullptr);
	}

	inline bool GetGroupPolyMeshUVIndexBuffer(const FGroupTopology& Topology,
		const FDynamicMesh3& Mesh,
		const FDynamicMeshUVOverlay& UVOverlay,
		TArray<int>* OutUVFaceIndices)
	{
		return GetGroupPolyMeshUVs<false, true>(Topology, Mesh, UVOverlay, nullptr, OutUVFaceIndices);
	}


	// Get the "face-varying" UV coordinates for each triangle in the mesh
	// The UV data is returned in a buffer, with index buffer accompanying
	// NOTE: This is currently called twice -- first time to get the index buffers for topology refinement, and 
	// the second time to get the data for interpolation.
	// TODO: A lot of code duplication with GetGroupPolyMeshUVs

	template<bool bBuildingCoordBuffer, bool bBuildingIndexBuffer>
	bool GetMeshUVs(const FDynamicMesh3& Mesh,
					const FDynamicMeshUVOverlay& UVOverlay,
					TArray<SubdUVVertex>* OutUVs,
					TArray<int>* OutUVFaceIndices)
	{
		TMap<int, int> ElementIDToUVBufferIndex;
		int UVVertexIndex = 0;

		for (int32 TriangleID : Mesh.TriangleIndicesItr())
		{
			const FIndex3i MeshTri = Mesh.GetTriangle(TriangleID);
			const FIndex3i ElementTri = UVOverlay.GetTriangle(TriangleID);
			
			FIndex3i CornerUVIndices{ FDynamicMesh3::InvalidID, FDynamicMesh3::InvalidID, FDynamicMesh3::InvalidID };

			for (int I = 0; I < 3; ++I)
			{
				const int CornerElementID = ElementTri[I];

				ensure(UVOverlay.GetParentVertex(CornerElementID) == MeshTri[I]);

				if (ElementIDToUVBufferIndex.Contains(CornerElementID))
				{
					if constexpr (bBuildingIndexBuffer)
					{
						CornerUVIndices[I] = ElementIDToUVBufferIndex[CornerElementID];
					}
				}
				else
				{
					if (CornerElementID != FDynamicMesh3::InvalidID)
					{
						ElementIDToUVBufferIndex.Add(CornerElementID, UVVertexIndex);
					}

					if constexpr (bBuildingIndexBuffer)
					{
						CornerUVIndices[I] = UVVertexIndex;
					}

					if constexpr (bBuildingCoordBuffer)
					{
						FIndex3i ElementIndices = UVOverlay.GetTriangle(TriangleID);
						if (ElementIndices[I] == FDynamicMesh3::InvalidID)
						{
							// Missing UV data for the triangle
							OutUVs->Add(SubdUVVertex{ FVector2f{0,0}, false });
						}
						else
						{
							FVector2f UV = UVOverlay.GetElement(ElementIndices[I]);
							OutUVs->Add(SubdUVVertex{ UV, true });
						}

					}

					++UVVertexIndex;
				}
			}

			if constexpr (bBuildingIndexBuffer)
			{
				OutUVFaceIndices->Add(CornerUVIndices[0]);
				OutUVFaceIndices->Add(CornerUVIndices[1]);
				OutUVFaceIndices->Add(CornerUVIndices[2]);
			}
		}

		return true;
	}


	inline bool GetMeshUVCoords(const FDynamicMesh3& Mesh,
		const FDynamicMeshUVOverlay& UVOverlay,
		TArray<SubdUVVertex>* OutUVCoords)
	{
		return GetMeshUVs<true, false>(Mesh, UVOverlay, OutUVCoords, nullptr);
	}

	inline bool GetMeshUVIndexBuffer(const FDynamicMesh3& Mesh,
		const FDynamicMeshUVOverlay& UVOverlay,
		TArray<int>* OutUVFaceIndices)
	{
		return GetMeshUVs<false, true>(Mesh, UVOverlay, nullptr, OutUVFaceIndices);
	}



	void InitializeOverlayToFaceVertexUVs(FDynamicMeshUVOverlay* UVOverlay, const TArray<FIndex3i>& UVTriangles, const TArray<FVector2f>& UVElements)
	{
		const FDynamicMesh3* Mesh = UVOverlay->GetParentMesh();
		check(Mesh->IsCompact());

		UVOverlay->ClearElements();
		for (int ElementIndex = 0; ElementIndex < UVElements.Num(); ++ElementIndex)
		{
			UVOverlay->AppendElement(UVElements[ElementIndex]);
		}

		int NumTriangles = Mesh->TriangleCount();
		check(NumTriangles == Mesh->MaxTriangleID());
		check(UVTriangles.Num() == NumTriangles);

		UVOverlay->InitializeTriangles(NumTriangles);
		for (int TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
		{
			const FIndex3i& UVTri = UVTriangles[TriangleIndex];

			if (ensure(Mesh->IsTriangle(TriangleIndex)))
			{
				const FIndex3i MeshTriVertices = Mesh->GetTriangle(TriangleIndex);
				for (int I = 0; I < 3; ++I)
				{
					if (UVTri[I] != FDynamicMesh3::InvalidID)
					{
						UVOverlay->SetParentVertex(UVTri[I], MeshTriVertices[I]);
					}
				}
			}

			if (UVTri[0] == FDynamicMesh3::InvalidID || UVTri[1] == FDynamicMesh3::InvalidID || UVTri[2] == FDynamicMesh3::InvalidID)
			{
				UVOverlay->UnsetTriangle(TriangleIndex);
			}
			else
			{
				UVOverlay->SetTriangle(TriangleIndex, UVTri);
			}
		}
	}


}	// namespace SubdivisionSurfaceLocal


class FSubdividePoly::RefinerImpl
{
public:
#if HAVE_OPENSUBDIV
	OpenSubdiv::Far::TopologyRefiner* TopologyRefiner = nullptr;
#endif
};

FSubdividePoly::FSubdividePoly(const FGroupTopology& InTopology,
							   const FDynamicMesh3& InOriginalMesh,
							   int InLevel) :
	GroupTopology(InTopology)
	, OriginalMesh(InOriginalMesh)
	, Level(InLevel)
{
	Refiner = MakeUnique<RefinerImpl>();
}

FSubdividePoly::~FSubdividePoly()
{
#if HAVE_OPENSUBDIV
	if (Refiner && Refiner->TopologyRefiner)
	{
		// This was created by TopologyRefinerFactory; looks like we are responsible for cleaning it up.
		delete Refiner->TopologyRefiner;
	}
#endif
	Refiner = nullptr;
}


bool FSubdividePoly::ComputeTopologySubdivision()
{
#if HAVE_OPENSUBDIV
	if (Level < 1)
	{
		return false;
	}

	TArray<int> BoundaryVertsPerFace;
	TArray<int> NumVertsPerFace;
	
	OpenSubdiv::Far::TopologyDescriptor::FVarChannel UVChannel;
	TArray<int> UVIndexBuffer;

	auto DescriptorFromTriangleMesh = [this, &NumVertsPerFace, &BoundaryVertsPerFace, &UVChannel, &UVIndexBuffer]
	(OpenSubdiv::Far::TopologyDescriptor& Descriptor)
	{
		for (auto TriangleID : OriginalMesh.TriangleIndicesItr())
		{
			FIndex3i TriangleVertices = OriginalMesh.GetTriangle(TriangleID);
			NumVertsPerFace.Add(3);
			BoundaryVertsPerFace.Add(TriangleVertices[0]);
			BoundaryVertsPerFace.Add(TriangleVertices[1]);
			BoundaryVertsPerFace.Add(TriangleVertices[2]);
		}

		// TODO: We should probably create a compact mesh descriptor for subdivision to operate on. UETOOL-2944
		Descriptor.numVertices = OriginalMesh.MaxVertexID();
		Descriptor.numFaces = OriginalMesh.TriangleCount();
		Descriptor.numVertsPerFace = NumVertsPerFace.GetData();
		Descriptor.vertIndicesPerFace = BoundaryVertsPerFace.GetData();

		if (UVComputationMethod == ESubdivisionOutputUVs::Interpolated)
		{
			UVChannel.numValues = BoundaryVertsPerFace.Num();

			const bool bGetUVsOK = SubdividePolyLocal::GetMeshUVIndexBuffer(OriginalMesh,
				*OriginalMesh.Attributes()->PrimaryUV(),
				&UVIndexBuffer);

			if (!bGetUVsOK)
			{
				return false;
			}

			UVChannel.valueIndices = UVIndexBuffer.GetData();
			Descriptor.numFVarChannels = 1;
			Descriptor.fvarChannels = &UVChannel;
		}
		else
		{
			Descriptor.numFVarChannels = 0;
		}

		return true;
	};

	auto DescriptorFromGroupTopology = [this, &NumVertsPerFace, &BoundaryVertsPerFace, &UVChannel, &UVIndexBuffer]
	(OpenSubdiv::Far::TopologyDescriptor& Descriptor)
	{
		int TotalNumVertices = 0;

		for (const FGroupTopology::FGroup& Group : GroupTopology.Groups)
		{
			if (Group.Boundaries.Num() != 1)
			{
				return false;
			}

			const FGroupTopology::FGroupBoundary& Boundary = Group.Boundaries[0];
			if (Boundary.GroupEdges.Num() < 2)
			{
				return false;
			}

			TArray<int> Corners;
			SubdividePolyLocal::GetBoundaryCorners(Boundary, GroupTopology, Corners);

			NumVertsPerFace.Add(Corners.Num());
			TotalNumVertices += Corners.Num();
			BoundaryVertsPerFace.Append(Corners);
		}

		Descriptor.numVertices = TotalNumVertices;
		Descriptor.numFaces = GroupTopology.Groups.Num();
		Descriptor.numVertsPerFace = NumVertsPerFace.GetData();
		Descriptor.vertIndicesPerFace = BoundaryVertsPerFace.GetData();

		const bool bShouldInterpolateUVs = (UVComputationMethod == ESubdivisionOutputUVs::Interpolated) &&
			OriginalMesh.HasAttributes() && 
			(OriginalMesh.Attributes()->PrimaryUV() != nullptr);

		if (bShouldInterpolateUVs)
		{
			UVChannel.numValues = BoundaryVertsPerFace.Num();

			bool bGetUVsOK = SubdividePolyLocal::GetGroupPolyMeshUVIndexBuffer(GroupTopology,
				OriginalMesh,
				*OriginalMesh.Attributes()->PrimaryUV(),
				&UVIndexBuffer);

			if (!bGetUVsOK)
			{
				return false;
			}

			UVChannel.valueIndices = UVIndexBuffer.GetData();
			Descriptor.numFVarChannels = 1;
			Descriptor.fvarChannels = &UVChannel;
		}
		else
		{
			Descriptor.numFVarChannels = 0;
		}

		return true;
	};

	OpenSubdiv::Far::TopologyDescriptor Descriptor;

	if (SubdivisionScheme == ESubdivisionScheme::Loop)
	{
		if (!DescriptorFromTriangleMesh(Descriptor))
		{
			return false;
		}
	}
	else
	{
		if (!DescriptorFromGroupTopology(Descriptor))
		{
			return false;
		}
	}

	using RefinerFactory = OpenSubdiv::Far::TopologyRefinerFactory<OpenSubdiv::Far::TopologyDescriptor>;
	RefinerFactory::Options RefinerOptions;

	RefinerOptions.schemeOptions.SetVtxBoundaryInterpolation(OpenSubdiv::Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER);

	switch (SubdivisionScheme)
	{
	case ESubdivisionScheme::Bilinear:
		RefinerOptions.schemeType = OpenSubdiv::Sdc::SchemeType::SCHEME_BILINEAR;
		break;
	case ESubdivisionScheme::CatmullClark:
		RefinerOptions.schemeType = OpenSubdiv::Sdc::SchemeType::SCHEME_CATMARK;
		break;
	case ESubdivisionScheme::Loop:
		RefinerOptions.schemeType = OpenSubdiv::Sdc::SchemeType::SCHEME_LOOP;
		break;
	}

	Refiner->TopologyRefiner = RefinerFactory::Create(Descriptor, RefinerOptions);

	if (Refiner->TopologyRefiner == nullptr)
	{
		return false;
	}

	Refiner->TopologyRefiner->RefineUniform(OpenSubdiv::Far::TopologyRefiner::UniformOptions(Level));

#endif		// HAVE_OPENSUBDIV

	return true;
}


bool FSubdividePoly::ComputeSubdividedMesh(FDynamicMesh3& OutMesh)
{
#if HAVE_OPENSUBDIV
	if (Level < 1)
	{
		return false;
	}

	if (!Refiner || !(Refiner->TopologyRefiner))
	{
		return false;
	}

	OpenSubdiv::Far::PrimvarRefiner Interpolator(*Refiner->TopologyRefiner);

	//
	// Interpolate vertex positions from initial Group poly mesh down to refinement level
	// 
	TArray<SubdividePolyLocal::SubdVertex> SourceVertices;
	bool bInterpolateVertexNormals = (NormalComputationMethod == ESubdivisionOutputNormals::Interpolated);
	if (SubdivisionScheme == ESubdivisionScheme::Loop)
	{
		GetAllMeshVertices(OriginalMesh, bInterpolateVertexNormals, SourceVertices);
	}
	else
	{
		GetGroupPolyMeshVertices(OriginalMesh, GroupTopology, bInterpolateVertexNormals, SourceVertices);
	}

	TArray<SubdividePolyLocal::SubdVertex> RefinedVertices;
	for (int CurrentLevel = 1; CurrentLevel <= Level; ++CurrentLevel)
	{
		// TODO: Don't keep resizing -- preallocate one big buffer and move through it
		int NumVertices = Refiner->TopologyRefiner->GetLevel(CurrentLevel).GetNumVertices();
		RefinedVertices.SetNumUninitialized(NumVertices);
		SubdividePolyLocal::SubdVertex* s = SourceVertices.GetData();
		SubdividePolyLocal::SubdVertex* d = RefinedVertices.GetData();
		Interpolator.Interpolate(CurrentLevel, s, d);
		SourceVertices = RefinedVertices;
	}

	//
	// Interpolate face group IDs
	// 
	TArray<int> SourceGroupIDs;
	if (SubdivisionScheme == ESubdivisionScheme::Loop)
	{
		for ( int TriangleID : OriginalMesh.TriangleIndicesItr() )
		{
			SourceGroupIDs.Add(OriginalMesh.GetTriangleGroup(TriangleID));
		}
	}
	else
	{
		for (const FGroupTopology::FGroup& Group : GroupTopology.Groups)
		{
			SourceGroupIDs.Add(Group.GroupID);
		}
	}
	check(SourceGroupIDs.Num() == Refiner->TopologyRefiner->GetLevel(0).GetNumFaces());

	TArray<int> RefinedGroupIDs;
	if (!bNewPolyGroups)
	{
		for (int CurrentLevel = 1; CurrentLevel <= Level; ++CurrentLevel)
		{
			// TODO: Don't keep resizing -- preallocate one big buffer and move through it
			RefinedGroupIDs.SetNumUninitialized(Refiner->TopologyRefiner->GetLevel(CurrentLevel).GetNumFaces());
			int* s = SourceGroupIDs.GetData();
			int* d = RefinedGroupIDs.GetData();
			Interpolator.InterpolateFaceUniform(CurrentLevel, s, d);
			SourceGroupIDs = RefinedGroupIDs;
		}
	}

	//
	// Interpolate material IDs
	//

	const bool bHasMaterialIDs = OriginalMesh.HasAttributes() && OriginalMesh.Attributes()->HasMaterialID();

	TArray<int> RefinedMaterialIDs;
	if (bHasMaterialIDs)
	{
		const FDynamicMeshMaterialAttribute* MaterialIDs = OriginalMesh.Attributes()->GetMaterialID();

		// Find the most common material ID for a given group
		auto MaterialIDForGroup = [this, MaterialIDs](const FGroupTopology::FGroup& Group) -> int32
		{
			TMap<int32, int> MaterialIDVotes;
			for (int32 TriangleID : Group.Triangles)
			{
				int32 MatID = MaterialIDs->GetValue(TriangleID);

				if (MaterialIDVotes.Contains(MatID))
				{
					++MaterialIDVotes[MatID];
				}
				else
				{
					MaterialIDVotes.Add(MatID, 1);
				}
			}

			int MaxVotes = -1;
			int32 MaxVoteMaterialID = 0;
			for (const TPair<int32, int>& IDVotePair : MaterialIDVotes)
			{
				if (IDVotePair.Value > MaxVotes)
				{
					MaxVotes = IDVotePair.Value;
					MaxVoteMaterialID = IDVotePair.Key;
				}
			}

			return MaxVoteMaterialID;
		};

		TArray<int32> SourceMaterialIDs;
		if (SubdivisionScheme == ESubdivisionScheme::Loop)
		{
			for (int TriangleID : OriginalMesh.TriangleIndicesItr())
			{
				SourceMaterialIDs.Add(MaterialIDs->GetValue(TriangleID));
			}
		}
		else
		{
			for (const FGroupTopology::FGroup& Group : GroupTopology.Groups)
			{
				SourceMaterialIDs.Add(MaterialIDForGroup(Group));
			}
		}

		for (int CurrentLevel = 1; CurrentLevel <= Level; ++CurrentLevel)
		{
			// TODO: Don't keep resizing -- preallocate one big buffer and move through it
			RefinedMaterialIDs.SetNumUninitialized(Refiner->TopologyRefiner->GetLevel(CurrentLevel).GetNumFaces());
			int* s = SourceMaterialIDs.GetData();
			int* d = RefinedMaterialIDs.GetData();
			Interpolator.InterpolateFaceUniform(CurrentLevel, s, d);
			SourceMaterialIDs = RefinedMaterialIDs;
		}
	}


	//
	// Interpolate UVs
	// 
	TArray<SubdividePolyLocal::SubdUVVertex> RefinedUVs;

	const bool bShouldInterpolateUVs = (UVComputationMethod == ESubdivisionOutputUVs::Interpolated) &&
		OriginalMesh.HasAttributes() &&
		(OriginalMesh.Attributes()->PrimaryUV() != nullptr);

	if (bShouldInterpolateUVs)
	{
		TArray<SubdividePolyLocal::SubdUVVertex> SourceUVs;
		bool bGetUVsOK;

		if (SubdivisionScheme == ESubdivisionScheme::Loop)
		{
			bGetUVsOK = GetMeshUVCoords(OriginalMesh,
				*OriginalMesh.Attributes()->PrimaryUV(), 
				&SourceUVs);
		}
		else
		{
			bGetUVsOK = SubdividePolyLocal::GetGroupPolyMeshUVCoords(GroupTopology,
				OriginalMesh,
				*OriginalMesh.Attributes()->PrimaryUV(),
				&SourceUVs);
		}

		if (!bGetUVsOK)
		{
			return false;
		}

		for (int CurrentLevel = 1; CurrentLevel <= Level; ++CurrentLevel)
		{
			// TODO: Don't keep resizing -- preallocate one big buffer and move through it
			RefinedUVs.SetNumUninitialized(Refiner->TopologyRefiner->GetLevel(CurrentLevel).GetNumFVarValues());
			SubdividePolyLocal::SubdUVVertex* s = SourceUVs.GetData();
			SubdividePolyLocal::SubdUVVertex* d = RefinedUVs.GetData();
			Interpolator.InterpolateFaceVarying(CurrentLevel, s, d);
			SourceUVs = RefinedUVs;
		}
	}

	// Now transfer to output mesh
	OutMesh.Clear();

	OutMesh.EnableTriangleGroups();
	if (bInterpolateVertexNormals)
	{
		OutMesh.EnableVertexNormals(FVector3f{ 0,0,0 });
	}

	if ((NormalComputationMethod != ESubdivisionOutputNormals::None) || (UVComputationMethod != ESubdivisionOutputUVs::None) || bHasMaterialIDs)
	{
		OutMesh.EnableAttributes();
	}

	// Add the vertices
	// TODO: Can we resize the Mesh vertex array up front and then populate it?
	for (const SubdividePolyLocal::SubdVertex& V : RefinedVertices)
	{
		OutMesh.AppendVertex(V.VertexInfo);
	}

	const OpenSubdiv::Far::TopologyLevel& FinalLevel = Refiner->TopologyRefiner->GetLevel(Level);

	check(!bShouldInterpolateUVs || FinalLevel.GetNumFVarValues() == RefinedUVs.Num());
	check(bNewPolyGroups || FinalLevel.GetNumFaces() == RefinedGroupIDs.Num());

	// Add the faces (manually triangulate the output here)

	int NumFaceVertices = 0;
	TArray<FVector2f> UVElements;
	TArray<FIndex3i> UVTriangles;


	auto AddUVTriangleIfValid = [&UVTriangles, &RefinedUVs](const FIndex3i& UVTri)
	{
		if (RefinedUVs[UVTri[0]].bIsValid && RefinedUVs[UVTri[1]].bIsValid && RefinedUVs[UVTri[2]].bIsValid)
		{
			UVTriangles.Add(UVTri);
		}
		else
		{
			UVTriangles.Add(FIndex3i{ FDynamicMesh3::InvalidID,FDynamicMesh3::InvalidID,FDynamicMesh3::InvalidID });
		}
	};

	if (bHasMaterialIDs)
	{
		OutMesh.Attributes()->EnableMaterialID();
	}

	for (int FaceID = 0; FaceID < FinalLevel.GetNumFaces(); ++FaceID)
	{
		int GroupID = bNewPolyGroups ? OutMesh.AllocateTriangleGroup() : RefinedGroupIDs[FaceID];

		OpenSubdiv::Far::ConstIndexArray Face = FinalLevel.GetFaceVertices(FaceID);
		NumFaceVertices += Face.size();

		if (Face.size() == 4)
		{
			FIndex3i TriA{ Face[0], Face[1], Face[3] };
			FIndex3i TriB{ Face[2], Face[3], Face[1] };
			int TriAIndex = OutMesh.AppendTriangle(TriA, GroupID);
			int TriBIndex = OutMesh.AppendTriangle(TriB, GroupID);

			if (bShouldInterpolateUVs)
			{
				OpenSubdiv::Far::ConstIndexArray FaceUVIndices = FinalLevel.GetFaceFVarValues(FaceID);
				
				FIndex3i UVTriA{ FaceUVIndices[0], FaceUVIndices[1], FaceUVIndices[3] };
				AddUVTriangleIfValid(UVTriA);

				FIndex3i UVTriB{ FaceUVIndices[2], FaceUVIndices[3], FaceUVIndices[1] };
				AddUVTriangleIfValid(UVTriB);
			}

			if (bHasMaterialIDs)
			{
				OutMesh.Attributes()->GetMaterialID()->SetValue(TriAIndex, RefinedMaterialIDs[FaceID]);
				OutMesh.Attributes()->GetMaterialID()->SetValue(TriBIndex, RefinedMaterialIDs[FaceID]);
			}
		}
		else
		{
			check(Face.size() == 3);
			int TriIndex = OutMesh.AppendTriangle(FIndex3i{ Face[0], Face[1], Face[2] }, GroupID);

			if (bShouldInterpolateUVs)
			{
				OpenSubdiv::Far::ConstIndexArray FaceUVIndices = FinalLevel.GetFaceFVarValues(FaceID);
				FIndex3i UVTri{ FaceUVIndices[0], FaceUVIndices[1], FaceUVIndices[2] };
				AddUVTriangleIfValid(UVTri);
			}

			if (bHasMaterialIDs)
			{
				OutMesh.Attributes()->GetMaterialID()->SetValue(TriIndex, RefinedMaterialIDs[FaceID]);
			}
		}
	}

	for (const SubdividePolyLocal::SubdUVVertex& UVVertex : RefinedUVs)
	{
		if (UVVertex.bIsValid)
		{
			UVElements.Add(UVVertex.VertexUV);
		}
		else
		{
			UVElements.Add(FVector2f::Zero());
		}
	}

	if (NormalComputationMethod != ESubdivisionOutputNormals::None)
	{
		const bool bUseExistingMeshVertexNormals = (NormalComputationMethod == ESubdivisionOutputNormals::Interpolated);
		FMeshNormals::InitializeOverlayToPerVertexNormals(OutMesh.Attributes()->PrimaryNormals(), bUseExistingMeshVertexNormals);
	}

	if (bShouldInterpolateUVs)
	{
		SubdividePolyLocal::InitializeOverlayToFaceVertexUVs(OutMesh.Attributes()->PrimaryUV(), UVTriangles, UVElements);
	}

	// Remove any vertices that are not referenced by a face
	for (int Vid = 0; Vid < OutMesh.MaxVertexID(); ++Vid)
	{
		if (OutMesh.IsVertex(Vid) && !OutMesh.IsReferencedVertex(Vid))
		{
			constexpr bool bPreserveManifold = false;
			OutMesh.RemoveVertex(Vid, bPreserveManifold);
		}
	}

#else	// HAVE_OPENSUBDIV

	OutMesh = OriginalMesh;

#endif	// HAVE_OPENSUBDIV


	return true;
}



FSubdividePoly::ETopologyCheckResult FSubdividePoly::ValidateTopology()
{
	if (GroupTopology.Groups.Num() == 0)
	{
		return ETopologyCheckResult::NoGroups;
	}

	if (GroupTopology.Groups.Num() < 2)
	{
		return ETopologyCheckResult::InsufficientGroups;
	}

	for (const FGroupTopology::FGroup& Group : GroupTopology.Groups)
	{
		if (Group.Boundaries.Num() == 0)
		{
			return ETopologyCheckResult::UnboundedPolygroup;
		}

		if (Group.Boundaries.Num() > 1)
		{
			return ETopologyCheckResult::MultiBoundaryPolygroup;
		}

		for (const FGroupTopology::FGroupBoundary& Boundary : Group.Boundaries)
		{
			if (Boundary.GroupEdges.Num() < 3)
			{
				return ETopologyCheckResult::DegeneratePolygroup;
			}
		}
	}

	return ETopologyCheckResult::Ok;
}


