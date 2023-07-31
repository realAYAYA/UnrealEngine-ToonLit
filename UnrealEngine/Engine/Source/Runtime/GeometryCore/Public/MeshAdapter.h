// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TriangleTypes.h"
#include "VectorTypes.h"

#include "Math/IntVector.h"
#include "Math/Vector.h"
#include "IndexTypes.h"

namespace UE
{
namespace Geometry
{

using namespace UE::Math;

/**
 * Most generic / lazy example of a triangle mesh adapter; possibly useful for prototyping / building on top of (but slower than making a more specific-case adapter)
 */
template <typename RealType>
struct TTriangleMeshAdapter
{
	TFunction<bool(int32 index)> IsTriangle;
	TFunction<bool(int32 index)> IsVertex;
	TFunction<int32()> MaxTriangleID;
	TFunction<int32()> MaxVertexID;
	TFunction<int32()> TriangleCount;
	TFunction<int32()> VertexCount;
	TFunction<uint64()> GetChangeStamp;
	TFunction<FIndex3i(int32)> GetTriangle;
	TFunction<TVector<RealType>(int32)> GetVertex;

	inline void GetTriVertices(int TID, UE::Math::TVector<RealType>& V0, UE::Math::TVector<RealType>& V1, UE::Math::TVector<RealType>& V2) const
	{
		FIndex3i TriIndices = GetTriangle(TID);
		V0 = GetVertex(TriIndices.A);
		V1 = GetVertex(TriIndices.B);
		V2 = GetVertex(TriIndices.C);
	}
};

typedef TTriangleMeshAdapter<double> FTriangleMeshAdapterd;
typedef TTriangleMeshAdapter<float> FTriangleMeshAdapterf;

/**
 * Example function to generate a generic mesh adapter from arrays
 * @param Vertices Array of mesh vertices
 * @param Triangles Array of int-vectors, one per triangle, indexing into the vertices array
 */
inline FTriangleMeshAdapterd GetArrayMesh(TArray<FVector>& Vertices, TArray<FIntVector>& Triangles)
{
	return {
		[&](int) { return true; },
		[&](int) { return true; },
		[&]() { return Triangles.Num(); },
		[&]() { return Vertices.Num(); },
		[&]() { return Triangles.Num(); },
		[&]() { return Vertices.Num(); },
		[&]() { return 0; },
		[&](int Idx) { return FIndex3i(Triangles[Idx]); },
		[&](int Idx) { return FVector3d(Vertices[Idx]); }};
}


/**
 * Faster adapter specifically for the common index mesh case
 */
template<typename IndexType, typename OutRealType, typename InVectorType=FVector>
struct TIndexMeshArrayAdapter
{
	const TArray<InVectorType>* SourceVertices;
	const TArray<IndexType>* SourceTriangles;

	void SetSources(const TArray<InVectorType>* SourceVerticesIn, const TArray<IndexType>* SourceTrianglesIn)
	{
		SourceVertices = SourceVerticesIn;
		SourceTriangles = SourceTrianglesIn;
	}

	TIndexMeshArrayAdapter() : SourceVertices(nullptr), SourceTriangles(nullptr)
	{
	}

	TIndexMeshArrayAdapter(const TArray<InVectorType>* SourceVerticesIn, const TArray<IndexType>* SourceTrianglesIn) : SourceVertices(SourceVerticesIn), SourceTriangles(SourceTrianglesIn)
	{
	}

	FORCEINLINE bool IsTriangle(int32 Index) const
	{
		return SourceTriangles->IsValidIndex(Index * 3);
	}

	FORCEINLINE bool IsVertex(int32 Index) const
	{
		return SourceVertices->IsValidIndex(Index);
	}

	FORCEINLINE int32 MaxTriangleID() const
	{
		return SourceTriangles->Num() / 3;
	}

	FORCEINLINE int32 MaxVertexID() const
	{
		return SourceVertices->Num();
	}

	// Counts are same as MaxIDs, because these are compact meshes
	FORCEINLINE int32 TriangleCount() const
	{
		return SourceTriangles->Num() / 3;
	}

	FORCEINLINE int32 VertexCount() const
	{
		return SourceVertices->Num();
	}

	FORCEINLINE uint64 GetChangeStamp() const
	{
		return 1; // source data doesn't have a timestamp concept
	}

	FORCEINLINE FIndex3i GetTriangle(int32 Index) const
	{
		int32 Start = Index * 3;
		return FIndex3i((int)(*SourceTriangles)[Start], (int)(*SourceTriangles)[Start+1], (int)(*SourceTriangles)[Start+2]);
	}

	FORCEINLINE TVector<OutRealType> GetVertex(int32 Index) const
	{
		return TVector<OutRealType>((*SourceVertices)[Index]);
	}

	FORCEINLINE void GetTriVertices(int32 TriIndex, UE::Math::TVector<OutRealType>& V0, UE::Math::TVector<OutRealType>& V1, UE::Math::TVector<OutRealType>& V2) const
	{
		int32 Start = TriIndex * 3;
		V0 = TVector<OutRealType>((*SourceVertices)[(*SourceTriangles)[Start]]);
		V1 = TVector<OutRealType>((*SourceVertices)[(*SourceTriangles)[Start+1]]);
		V2 = TVector<OutRealType>((*SourceVertices)[(*SourceTriangles)[Start+2]]);
	}

};


/**
 * Second version of the above faster adapter
 *  -- for the case where triangle indices are packed into an integer vector type instead of flat
 */
template<typename IndexVectorType, typename OutRealType, typename InVectorType = FVector>
struct TIndexVectorMeshArrayAdapter
{
	const TArray<InVectorType>* SourceVertices;
	const TArray<IndexVectorType>* SourceTriangles;

	void SetSources(const TArray<InVectorType>* SourceVerticesIn, const TArray<IndexVectorType>* SourceTrianglesIn)
	{
		SourceVertices = SourceVerticesIn;
		SourceTriangles = SourceTrianglesIn;
	}

	TIndexVectorMeshArrayAdapter() : SourceVertices(nullptr), SourceTriangles(nullptr)
	{
	}

	TIndexVectorMeshArrayAdapter(const TArray<InVectorType>* SourceVerticesIn, const TArray<IndexVectorType>* SourceTrianglesIn) : SourceVertices(SourceVerticesIn), SourceTriangles(SourceTrianglesIn)
	{
	}

	FORCEINLINE bool IsTriangle(int32 Index) const
	{
		return SourceTriangles->IsValidIndex(Index);
	}

	FORCEINLINE bool IsVertex(int32 Index) const
	{
		return SourceVertices->IsValidIndex(Index);
	}

	FORCEINLINE int32 MaxTriangleID() const
	{
		return SourceTriangles->Num();
	}

	FORCEINLINE int32 MaxVertexID() const
	{
		return SourceVertices->Num();
	}

	// Counts are same as MaxIDs, because these are compact meshes
	FORCEINLINE int32 TriangleCount() const
	{
		return SourceTriangles->Num();
	}

	FORCEINLINE int32 VertexCount() const
	{
		return SourceVertices->Num();
	}

	FORCEINLINE uint64 GetChangeStamp() const
	{
		return 1; // source data doesn't have a timestamp concept
	}

	FORCEINLINE FIndex3i GetTriangle(int32 Index) const
	{
		const IndexVectorType& Tri = (*SourceTriangles)[Index];
		return FIndex3i((int)Tri[0], (int)Tri[1], (int)Tri[2]);
	}

	FORCEINLINE TVector<OutRealType> GetVertex(int32 Index) const
	{
		return TVector<OutRealType>((*SourceVertices)[Index]);
	}

	FORCEINLINE void GetTriVertices(int32 TriIndex, UE::Math::TVector<OutRealType>& V0, UE::Math::TVector<OutRealType>& V1, UE::Math::TVector<OutRealType>& V2) const
	{
		const IndexVectorType& Tri = (*SourceTriangles)[TriIndex];
		V0 = TVector<OutRealType>((*SourceVertices)[Tri[0]]);
		V1 = TVector<OutRealType>((*SourceVertices)[Tri[1]]);
		V2 = TVector<OutRealType>((*SourceVertices)[Tri[2]]);
	}

};

typedef TIndexMeshArrayAdapter<uint32, double> FIndexMeshArrayAdapterd;


/**
 * TMeshWrapperAdapterd<T> can be used to present an arbitrary Mesh / Adapter type as a FTriangleMeshAdapterd.
 * This is useful in cases where it would be difficult or undesirable to write code templated on
 * the standard "Mesh Type" signature. If the code is written for FTriangleMeshAdapterd then this
 * shim can be used to present any compatible mesh type as a FTriangleMeshAdapterd
 */ 
template <class WrappedMeshType>
struct TMeshWrapperAdapterd : public UE::Geometry::FTriangleMeshAdapterd
{
	WrappedMeshType* WrappedAdapter;

	TMeshWrapperAdapterd(WrappedMeshType* WrappedAdapterIn) : WrappedAdapter(WrappedAdapterIn)
	{
		IsTriangle = [this](int index) { return WrappedAdapter->IsTriangle(index); };
		IsVertex = [this](int index) { return WrappedAdapter->IsVertex(index); };
		MaxTriangleID = [this]() { return WrappedAdapter->MaxTriangleID(); };
		MaxVertexID = [this]() { return WrappedAdapter->MaxVertexID(); };
		TriangleCount = [this]() { return WrappedAdapter->TriangleCount(); };
		VertexCount = [this]() { return WrappedAdapter->VertexCount(); };
		GetChangeStamp = [this]() { return WrappedAdapter->GetChangeStamp(); };
		GetTriangle = [this](int32 TriangleID) { return WrappedAdapter->GetTriangle(TriangleID); };
		GetVertex = [this](int32 VertexID) { return WrappedAdapter->GetVertex(VertexID); };
	}
};



} // end namespace UE::Geometry
} // end namespace UE