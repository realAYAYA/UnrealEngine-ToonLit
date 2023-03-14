// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionProximityUtility.h"

#include "Spatial/SparseDynamicOctree3.h"

#include "Async/ParallelFor.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"


#include "Spatial/PointHashGrid3.h"

DEFINE_LOG_CATEGORY_STATIC(LogChaosProximity, Verbose, All);

namespace UE { namespace GeometryCollectionInternal	{

static inline bool AreNormalsOpposite(const FVector3f& Normal0, const FVector3f& Normal1)
{
	return FVector3f::DotProduct(Normal0, Normal1) < (-1.0f + UE_KINDA_SMALL_NUMBER);
}

static inline bool TriOutsideEdge(const TStaticArray<FVector2f, 3>& Points, const FVector2f& B, const FVector2f& C, float Normal)
{
	const FVector2f CBPerp = UE::Geometry::PerpCW(C - B) * Normal;
	const FVector2f P0B = Points[0] - B;
	const FVector2f P1B = Points[1] - B;
	const FVector2f P2B = Points[2] - B;
	// Note: If we change this comparison to use a tolerance instead of 0, need to normalize CBPerp and also filter out degenerate tris
	bool bAnyInside =
		CBPerp.Dot(P0B) < 0 ||
		CBPerp.Dot(P1B) < 0 ||
		CBPerp.Dot(P2B) < 0;
	return !bAnyInside;
}

static inline bool TrianglesIntersect(const TStaticArray<FVector2f,3>& T0, const TStaticArray<FVector2f,3>& T1)
{
	// Test if one of the triangles has a side with all of the other triangle's points on the outside.

	// Orientation of T0 and T1, as sign of area (computed as sign of the Z component of a cross product of the triangle edges)
	float Normal0 = FMathf::Sign(T0[1].X - T0[0].X) * (T0[2].Y - T0[0].Y) - (T0[1].Y - T0[0].Y) * (T0[2].X - T0[0].X);
	float Normal1 = FMathf::Sign(T1[1].X - T1[0].X) * (T1[2].Y - T1[0].Y) - (T1[1].Y - T1[0].Y) * (T1[2].X - T1[0].X);
	
	// Triangles overlap if there is no edge that the other tri is completely on the 'outside' side of
	return !(
		TriOutsideEdge(T1, T0[0], T0[1], Normal0) ||
		TriOutsideEdge(T1, T0[1], T0[2], Normal0) ||
		TriOutsideEdge(T1, T0[2], T0[0], Normal0) ||
		TriOutsideEdge(T0, T1[0], T1[1], Normal1) ||
		TriOutsideEdge(T0, T1[1], T1[2], Normal1) ||
		TriOutsideEdge(T0, T1[2], T1[0], Normal1)
		);
}

// We'll bin triangles normals based on 20 dodecahedron directions, to filter which ones to test for overlap
struct FBinNormals
{
	static constexpr int NumBins = 20;

	TArray<FVector3f, TFixedAllocator<NumBins>> Bins;

	FBinNormals()
	{
		// We quantize surface normals into 20 uniform bins on a unit sphere surface, ie an icosahedron
		Bins.SetNum(NumBins);

		Bins[0] = FVector3f(0.171535f, -0.793715f, 0.583717f);
		Bins[1] = FVector3f(0.627078f, -0.778267f, 0.034524f);
		Bins[2] = FVector3f(0.491358f, 0.810104f, -0.319894f);
		Bins[3] = FVector3f(0.445554f, 0.804788f, 0.392214f);
		Bins[4] = FVector3f(0.245658f, -0.785111f, -0.568669f);
		Bins[5] = FVector3f(0.984880f, -0.161432f, 0.062144f);
		Bins[6] = FVector3f(0.247864f, -0.186425f, 0.950708f);
		Bins[7] = FVector3f(0.824669f, 0.212942f, -0.523975f);
		Bins[8] = FVector3f(0.750546f, 0.204339f, 0.628411f);
		Bins[9] = FVector3f(0.367791f, -0.172505f, -0.913787f);
		Bins[10] = -Bins[0];
		Bins[11] = -Bins[1];
		Bins[12] = -Bins[2];
		Bins[13] = -Bins[3];
		Bins[14] = -Bins[4];
		Bins[15] = -Bins[5];
		Bins[16] = -Bins[6];
		Bins[17] = -Bins[7];
		Bins[18] = -Bins[8];
		Bins[19] = -Bins[9];
	}

	// Note: a normal could point at an edge or corner of the dodecahedron, resulting in an ambiguous case that misses matching normals ...
	// Currently we just hope that the other proximity tests (vertex proximity and other faces) will find the proximity in these cases.
	// TODO: Consider testing multiple bins in ambiguous cases.
	int32 FindBestBin(const FVector3f& SurfaceNormal) const
	{
		// We select the bin with the highest alignment with the surface normal
		float BestAlignment = -1.0;
		int32 BestBin = INDEX_NONE;

		for (int32 BinIdx = 0; BinIdx < NumBins; ++BinIdx)
		{
			float Alignment = FVector3f::DotProduct(SurfaceNormal, Bins[BinIdx]);
			if (Alignment > BestAlignment)
			{
				BestAlignment = Alignment;
				BestBin = BinIdx;
			}
		}

		return BestBin;
	}
};

// Per-Geometry pre-computed spatial data
struct FPerGeometrySpatial
{
	TArray<TArray<int32>, TFixedAllocator<FBinNormals::NumBins>> Bins;
	UE::Math::TBox<float> Bounds; // bounds in a shared space

	void InitBins(const FGeometryCollection* Collection, const FBinNormals& Binner, int32 GeoIdx, const TArray<FVector3f>& SurfaceNormals)
	{
		Bins.SetNum(FBinNormals::NumBins);
		int32 FaceStart = Collection->FaceStart[GeoIdx];
		int32 FaceEnd = FaceStart + Collection->FaceCount[GeoIdx];
		for (int32 FaceIdx = FaceStart; FaceIdx < FaceEnd; ++FaceIdx)
		{
			int32 BestBin = Binner.FindBestBin(SurfaceNormals[FaceIdx]);
			Bins[BestBin].Add(FaceIdx);
		}
	}

	// Note: Only contains mappings to geometry w/ *higher* indices
	TMap<int32, bool> CandidateContacts; // bool indicates if the contact has been confirmed
};

// Overall geometry-collection spatial data
struct FGeometryCollectionProximitySpatial
{
	TArray<FVector3f> TransformedVertices;
	TArray<FVector3f> SurfaceNormals;
	TArray<FPerGeometrySpatial> GeoInfo;
	TArray<TSet<int32>> KnownProximity;
	UE::Math::TBox<float> OverallBounds;

	FGeometryCollectionProximitySpatial(const FGeometryCollection* Collection, float ProximityTolerance = KINDA_SMALL_NUMBER)
	{
		TransformVertices(Collection);
		GenerateSurfaceNormals(Collection);
		InitProximityFromVertices(Collection, ProximityTolerance);
		InitGeoNormalBins(Collection);
		InitCandidateContacts(Collection, ProximityTolerance);
		ComputeCoplanarContacts(Collection, ProximityTolerance);
	}

	void ComputeCoplanarContacts(const FGeometryCollection* Collection, float ProximityTolerance)
	{
		ParallelFor(GeoInfo.Num(), [this, &Collection, ProximityTolerance](int32 GeoIdx)
			{
				for (TPair<int32, bool>& PossibleContact : GeoInfo[GeoIdx].CandidateContacts)
				{
					if (PossibleContact.Value || KnownProximity[GeoIdx].Contains(PossibleContact.Key))
					{
						continue;
					}

					int32 OtherGeoIdx = PossibleContact.Key;
					for (int32 BinIdx = 0; BinIdx < FBinNormals::NumBins && !PossibleContact.Value; ++BinIdx)
					{
						int32 OtherBinIdx = (BinIdx + (FBinNormals::NumBins / 2)) % FBinNormals::NumBins;
						for (int32 FaceIdx : GeoInfo[GeoIdx].Bins[BinIdx])
						{
							// Skip if the face bounds don't overlap the geo bounds
							FIntVector3 Face = Collection->Indices[FaceIdx];
							UE::Math::TBox<float> FaceBox(EForceInit::ForceInit);
							FaceBox += TransformedVertices[Face.X];
							FaceBox += TransformedVertices[Face.Y];
							FaceBox += TransformedVertices[Face.Z];
							if (!GeoInfo[OtherGeoIdx].Bounds.Intersect(FaceBox.ExpandBy(ProximityTolerance)))
							{
								continue;
							}
							for (int32 OtherFaceIdx : GeoInfo[OtherGeoIdx].Bins[OtherBinIdx])
							{
								if (AreNormalsOpposite(SurfaceNormals[FaceIdx], SurfaceNormals[OtherFaceIdx]))
								{
									if (AreFacesCoPlanar(Collection, FaceIdx, OtherFaceIdx, ProximityTolerance))
									{
										if (DoFacesOverlap(Collection, FaceIdx, OtherFaceIdx))
										{
											PossibleContact.Value = true; // verified contact
											break; // don't need to consider the rest of the geometry
										}
									}
								}
							}
							if (PossibleContact.Value)
							{
								break;
							}
						}
					}
				}
			});
	}

	// Note: This function will destroy/move over the computed proximity data when building it in the output collection
	void MoveProximityToCollection(FGeometryCollection* Collection)
	{
		if (!Collection->HasAttribute("Proximity", FGeometryCollection::GeometryGroup))
		{
			const FManagedArrayCollection::FConstructionParameters GeometryDependency(FGeometryCollection::GeometryGroup);
			Collection->AddAttribute<TSet<int32>>("Proximity", FGeometryCollection::GeometryGroup, GeometryDependency);
		}

		TManagedArray<TSet<int32>>& Proximity = Collection->ModifyAttribute<TSet<int32>>("Proximity", FGeometryCollection::GeometryGroup);

		for (int32 GeoIdx = 0; GeoIdx < KnownProximity.Num(); ++GeoIdx)
		{
			Proximity[GeoIdx] = MoveTemp(KnownProximity[GeoIdx]);
		}
		for (int32 GeoIdx = 0; GeoIdx < KnownProximity.Num(); ++GeoIdx)
		{
			for (const TPair<int32, bool>& PossibleContact : GeoInfo[GeoIdx].CandidateContacts)
			{
				if (PossibleContact.Value)
				{
					Proximity[GeoIdx].Add(PossibleContact.Key);
					Proximity[PossibleContact.Key].Add(GeoIdx);
				}
			}
		}

	}

	bool AreFacesCoPlanar(const FGeometryCollection* Collection, int32 Idx0, int32 Idx1, float ProximityTolerance) const
	{
		// Assumes that faces have already been determined to be parallel.

		const TManagedArray<FIntVector>& Indices = Collection->Indices;

		FVector3f SamplePoint = TransformedVertices[Indices[Idx0].X];
		FVector3f PlaneOrigin = TransformedVertices[Indices[Idx1].X];
		FVector3f PlaneNormal = SurfaceNormals[Idx1];

		return FMath::Abs(FVector3f::DotProduct((SamplePoint - PlaneOrigin), PlaneNormal)) < ProximityTolerance;
	}

	bool DoFacesOverlap(const FGeometryCollection* Collection, int32 Idx0, int32 Idx1) const
	{
		// Assumes that faces have already been determined to be coplanar

		const TManagedArray<FIntVector>& Indices = Collection->Indices;

		// Project the first triangle into its normal plane
		FVector3f Basis0 = TransformedVertices[Indices[Idx0].Y] - TransformedVertices[Indices[Idx0].X];
		Basis0.Normalize();
		FVector3f Basis1 = FVector3f::CrossProduct(SurfaceNormals[Idx0], Basis0);
		Basis1.Normalize();

		FVector3f Origin = TransformedVertices[Indices[Idx0].X];

		TStaticArray<FVector2f, 3> T0;
		// T0[0] is the origin of the system
		T0[0] = FVector2f(0.f, 0.f);

		T0[1] = FVector2f(FVector3f::DotProduct(TransformedVertices[Indices[Idx0].Y] - Origin, Basis0), FVector3f::DotProduct(TransformedVertices[Indices[Idx0].Y] - Origin, Basis1));
		T0[2] = FVector2f(FVector3f::DotProduct(TransformedVertices[Indices[Idx0].Z] - Origin, Basis0), FVector3f::DotProduct(TransformedVertices[Indices[Idx0].Z] - Origin, Basis1));

		// Project the second triangle into these coordinates. We reverse the winding order to flip the normal.
		FVector3f Point0 = TransformedVertices[Indices[Idx1].Z] - Origin;
		FVector3f Point1 = TransformedVertices[Indices[Idx1].Y] - Origin;
		FVector3f Point2 = TransformedVertices[Indices[Idx1].X] - Origin;
		TStaticArray<FVector2f, 3> T1;
		T1[0] = FVector2f(FVector3f::DotProduct(Point0, Basis0), FVector3f::DotProduct(Point0, Basis1));
		T1[1] = FVector2f(FVector3f::DotProduct(Point1, Basis0), FVector3f::DotProduct(Point1, Basis1));
		T1[2] = FVector2f(FVector3f::DotProduct(Point2, Basis0), FVector3f::DotProduct(Point2, Basis1));

		// Note: If this was the only proximity test, we would also need to check for identical triangles here,
		// but we don't need to in practice because we cover that case w/ the proximity-from-vertices that we compute first
		return TrianglesIntersect(T0, T1);
	}

	void InitCandidateContacts(const FGeometryCollection* Collection, float ProximityTolerance)
	{
		UE::Geometry::FSparseDynamicOctree3 GeoOctree;
		GeoOctree.RootDimension = OverallBounds.GetExtent().GetAbsMax();
		FVector3d Center = (FVector3d)OverallBounds.GetCenter();
		TArray<int32> GeoIndices;
		for (int32 GeoIdx = 0; GeoIdx < GeoInfo.Num(); ++GeoIdx)
		{
			UE::Math::TBox<float> ExpandedBounds = GeoInfo[GeoIdx].Bounds.ExpandBy(ProximityTolerance);

			// Center the boxes to work better with the sparse dynamic octree
			UE::Geometry::FAxisAlignedBox3d CenteredBox3d((FVector3d)GeoInfo[GeoIdx].Bounds.Min, (FVector3d)GeoInfo[GeoIdx].Bounds.Max);
			CenteredBox3d.Min -= Center;
			CenteredBox3d.Max -= Center;
			if (GeoIdx > 0)
			{
				GeoIndices.Reset();
				UE::Geometry::FAxisAlignedBox3d CenteredBox3dExpanded = CenteredBox3d;
				CenteredBox3dExpanded.Expand(ProximityTolerance);
				GeoOctree.RangeQuery(CenteredBox3dExpanded, GeoIndices);
				for (int32 CandidateIdx : GeoIndices)
				{
					if (!KnownProximity[CandidateIdx].Contains(GeoIdx) &&
						// FSparseDynamicOctree3 doesn't doesn't filter for actual bounding box overlap, so we need to do so here
						ExpandedBounds.Intersect(GeoInfo[CandidateIdx].Bounds))
					{
						// Note: Only add the lower idx -> higher idx mapping
						GeoInfo[CandidateIdx].CandidateContacts.Add(GeoIdx, false);
					}
				}
			}
			GeoOctree.InsertObject(GeoIdx, CenteredBox3d);
		}
	}

	void InitGeoNormalBins(const FGeometryCollection* Collection)
	{
		FBinNormals Binner;
		ParallelFor(GeoInfo.Num(), [this, &Collection, &Binner](int32 GeoIdx)
			{
				GeoInfo[GeoIdx].InitBins(Collection, Binner, GeoIdx, SurfaceNormals);
			});
	}

	void InitProximityFromVertices(const FGeometryCollection* Collection, float ProximityTolerance)
	{
		UE::Geometry::TPointHashGrid3f<int32> VertHash(ProximityTolerance * 3, -1);
		TArray<int32> NearPts;
		int32 NumGeo = Collection->NumElements(FGeometryCollection::GeometryGroup);
		KnownProximity.SetNum(NumGeo);
		GeoInfo.SetNum(NumGeo);
		for (int32 GeoIdx = 0; GeoIdx < NumGeo; ++GeoIdx)
		{
			GeoInfo[GeoIdx].Bounds = UE::Math::TBox<float>(EForceInit::ForceInit);
			int32 VertStart = Collection->VertexStart[GeoIdx];
			int32 VertEnd = VertStart + Collection->VertexCount[GeoIdx];
			int32 TransformIdx = Collection->TransformIndex[GeoIdx];
			if (!Collection->IsRigid(TransformIdx))
			{
				continue;
			}
			for (int32 VertIdx = VertStart; VertIdx < VertEnd; VertIdx++)
			{
				const FVector3f& Vertex = TransformedVertices[VertIdx];
				GeoInfo[GeoIdx].Bounds += Vertex;
				NearPts.Reset();
				VertHash.FindPointsInBall(Vertex, ProximityTolerance, [&Vertex, this](const int32& Other) -> float
					{
						return FVector3f::DistSquared(TransformedVertices[Other], Vertex);
					}, NearPts);
				for (int32 NearPtIdx : NearPts)
				{
					int32 NearTransformIdx = Collection->BoneMap[NearPtIdx];
					if (NearTransformIdx != TransformIdx)
					{
						int32 NearGeoIdx = Collection->TransformToGeometryIndex[NearTransformIdx];
						KnownProximity[NearGeoIdx].Add(GeoIdx);
						KnownProximity[GeoIdx].Add(NearGeoIdx);
					}
				}
				VertHash.InsertPointUnsafe(VertIdx, Vertex);
			}
			OverallBounds += GeoInfo[GeoIdx].Bounds;
		}
	}

	void TransformVertices(const FGeometryCollection* Collection)
	{
		TransformedVertices.SetNum(Collection->NumElements(FGeometryCollection::VerticesGroup));

		TArray<FTransform> GlobalTransformArray;
		GeometryCollectionAlgo::GlobalMatrices(Collection->Transform, Collection->Parent, GlobalTransformArray);

		ParallelFor(Collection->NumElements(FGeometryCollection::VerticesGroup), [this, &Collection, &GlobalTransformArray](int32 VertIdx)
			{
				const TManagedArray<int32>& BoneMap = Collection->BoneMap;
				const TManagedArray<FVector3f>& Vertex = Collection->Vertex;

				const FTransform& GlobalTransform = GlobalTransformArray[BoneMap[VertIdx]];
				TransformedVertices[VertIdx] = (FVector3f)GlobalTransform.TransformPosition(FVector(Vertex[VertIdx]));
			});

	}

	void GenerateSurfaceNormals(const FGeometryCollection* Collection)
	{
		const int32 NumFaces = Collection->NumElements(FGeometryCollection::FacesGroup);
		// Generate surface normal for each face
		SurfaceNormals.SetNum(NumFaces);
		ParallelFor(NumFaces, [this, &Collection](int32 FaceIdx)
			{
				const TManagedArray<FIntVector>& Indices = Collection->Indices;

				FVector3f Edge0 = (TransformedVertices[Indices[FaceIdx].X] - TransformedVertices[Indices[FaceIdx].Y]);
				FVector3f Edge1 = (TransformedVertices[Indices[FaceIdx].Z] - TransformedVertices[Indices[FaceIdx].Y]);
				SurfaceNormals[FaceIdx] = FVector3f::CrossProduct(Edge0, Edge1);
				SurfaceNormals[FaceIdx].Normalize();
			});
	}

};



}} // namespace UE::GeometryCollectionInternal

FGeometryCollectionProximityUtility::FGeometryCollectionProximityUtility(FGeometryCollection* InCollection)
	: Collection(InCollection)
{
	check(Collection);
}

void FGeometryCollectionProximityUtility::UpdateProximity()
{
	using namespace UE::GeometryCollectionInternal;

	FGeometryCollectionProximitySpatial Spatial(Collection, ProximityThreshold);
	Spatial.MoveProximityToCollection(Collection);
}
