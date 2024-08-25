// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generators/SweepGenerator.h"

#include "Async/ParallelFor.h"

// TODO: Other functions from SweepGenerator.h should go here too.

using namespace UE::Geometry;

// FProfileSweepGenerator

// Various indexing utility functions. See the comment in Generate() for a description of vertex/triangle/uv layout.
int32 GetVertIndex(bool VertIsWelded, int32 SweepIndex, int32 ProfileIndex, int32 NumWelded, int32 NumNonWelded, const TArray<int32> &VertPositionOffsets)
{
	return VertIsWelded ? 
		VertPositionOffsets[ProfileIndex] 
		: NumWelded + SweepIndex * NumNonWelded + VertPositionOffsets[ProfileIndex];
}
int32 GetTriangleIndex(int32 SweepIndex, int32 ProfileIndex, int32 NumTrisPerSweepSegment, const TArray<int32> &VertTriangleOffsets)
{
	return (SweepIndex * NumTrisPerSweepSegment) + VertTriangleOffsets[ProfileIndex];
}
int32 GetUvIndex(int32 SweepIndex, int32 ProfileIndex, int32 NumUvColumns)
{
	return (SweepIndex * NumUvColumns) + ProfileIndex;
}
int32 GetPolygonGroup(EProfileSweepPolygonGrouping PolygonGroupingMode, int32 SweepIndex, int32 ProfileIndex, int32 NumProfileSegments)
{
	int32 PolygonId = -1;
	switch (PolygonGroupingMode)
	{
	case EProfileSweepPolygonGrouping::Single:
		PolygonId = 0;
		break;
	case EProfileSweepPolygonGrouping::PerFace:
		PolygonId = SweepIndex * NumProfileSegments + ProfileIndex;
		break;
	case EProfileSweepPolygonGrouping::PerProfileSegment:
		PolygonId = ProfileIndex;
		break;
	case EProfileSweepPolygonGrouping::PerSweepSegment:
		PolygonId = SweepIndex;
		break;
	}
	return PolygonId;
}

/**
 * Utility function for calculating the triangle normal contributions to average normals.
 */
void FProfileSweepGenerator::AdjustNormalsForTriangle(int32 TriIndex, int32 FirstIndex, int32 SecondIndex, int32 ThirdIndex,
	TArray<FVector3d> &WeightedNormals)
{
	FVector3d AbNormalized = Normalized(Vertices[SecondIndex] - Vertices[FirstIndex]);
	AdjustNormalsForTriangle(TriIndex, FirstIndex, SecondIndex, ThirdIndex, WeightedNormals, AbNormalized);
}

/**
 * Utility function for calculating the contribution of triangle normals to average normals. AbNormalized (ie,
 * the normalized vector from the first vertex to the second) is taken as a parameter so that it can be reused in
 * dealing with a planar quad.
 */
void FProfileSweepGenerator::AdjustNormalsForTriangle(int32 TriIndex, int32 FirstIndex, int32 SecondIndex, int32 ThirdIndex,
	TArray<FVector3d>& WeightedNormals, const FVector3d& AbNormalized)
{
	// For the code below, this is the naming of the vertices:
	//  a-c
	//   \|
	//    b

	// We store contribution of this normal to each vertex's average. These will need to get summed
	// per vertex later- we avoid adding the result into the per-vertex sums as we go along to avoid
	// concurrent writes since this is done in parallel.

	FVector3d Bc = Normalized(Vertices[ThirdIndex] - Vertices[SecondIndex]);
	FVector3d Ac = Normalized(Vertices[ThirdIndex] - Vertices[FirstIndex]);
	FVector3d TriangleNormal = Normalized(Ac.Cross(AbNormalized));

	// Note that AngleR requires normalized inputs
	WeightedNormals[TriIndex * 3] = TriangleNormal * AngleR(AbNormalized,Ac);
	WeightedNormals[TriIndex * 3 + 1] = TriangleNormal * AngleR(Bc, -AbNormalized);
	WeightedNormals[TriIndex * 3 + 2] = TriangleNormal * AngleR(Ac, Bc);

	// We can safely hook the triangle normals up, even though their calculation is incomplete.
	SetTriangleNormals(TriIndex, FirstIndex, SecondIndex, ThirdIndex);
}

FMeshShapeGenerator& FProfileSweepGenerator::Generate()
{
	if (Progress && Progress->Cancelled())
	{
		return *this;
	}

	// Check that we have our inputs
	if (ProfileCurve.Num() < 2 || SweepCurve.Num() < 2 || (!SweepScaleCurve.IsEmpty() && SweepScaleCurve.Num() != SweepCurve.Num()))
	{
		Reset();
		return *this;;
	}

	// If all points are welded, nothing to do
	int32 NumWelded = WeldedVertices.Num();
	int32 NumNonWelded = ProfileCurve.Num() - WeldedVertices.Num();
	check(NumNonWelded >= 0); // We should never have more welded points than there are points total.
	if (NumNonWelded == 0)
	{
		Reset();
		return *this;
	}
	
	TRACE_CPUPROFILER_EVENT_SCOPE(ProfileSweepGenerator_Generate);

	/*
	The generated vertices are organized and connected as follows:

		o-o-o-o-o-o-o-o
		| | | |/ \| | |   |
		o-o-o-o-o-o-o-o   | Sweep index direction
		| | | |/ \| | |   v
		o-o-o-o-o-o-o-o
		| | | |/ \| | |
		o-o-o-o   o-o-o
		        ^
		        | welded vertex

		---> Profile index direction

	The welded vertex actually has a single position but multiple UV coordinates for different triangles,
	and those UV coordinates are actually centered vertically like this in the UV grid:
	o     o
	| >o< |
	o     o
	
	Vertex positions are stored with the welded vertices first, followed by rows of non welded vertices, one
	row for each sweep point. Triangles, similarly, are stored as rows of "sweep segments" (horizontal rows
	of triangles corresponding to a sweep step). We can index into the rows of these data structures with
	the sweep index and into the columns using a mapping from profile index to offset, since these offsets
	vary depending on the presence of welded vertices (which also generate fewer triangles per sweep segment).

	UVs are similary stored as rows per sweep point, but because welded points have different UV coordinates
	per triangle and because coincident UV's (when there are welded-to-welded edges) still need separate UV
	elements per vertex, there is no need to have special offseting per profile index. However, there is 
	an extra UV row/column in cases where the sweep/profile curves are closed, to avoid wrapping back around
	to 0, so indexing to the next element should not use the modulo operator.
	
	Note that despite their arrangement in memory, U's actually increase in the sweep direction and V's increase
	in the profile direction, since we typically imagine a vertical profile being swept or rotated in a horizontal
	direction. The spacing in the UV grid is weighted by the distance between profile and sweep points.
	
	For calculating normals, we sum together the triangle normals at each vertex weighted by the angle of
	that triangle vertex, and we normalize at the end.
	*/

	// A few additional convenience variables
	int32 NumProfilePoints = ProfileCurve.Num();
	int32 NumSweepPoints = SweepCurve.Num();
	int32 NumSweepSegments = bSweepCurveIsClosed ? NumSweepPoints : NumSweepPoints - 1 ;
	int32 NumProfileSegments = bProfileCurveIsClosed ? NumProfilePoints : NumProfilePoints - 1;

	int32 NumVerts = NumWelded + NumNonWelded * NumSweepPoints;

	// Set up structures needed to index to the correct vertex or triangle within the rows that we 
	// store per sweep instance.
	TArray<int32> VertPositionOffsets;
	TArray<int32> TriangleOffsets;
	VertPositionOffsets.SetNum(NumProfilePoints);
	TriangleOffsets.SetNum(NumProfilePoints);

	int32 NextWeldedOffset = 0;
	int32 NextNonWeldedOffset = 0;
	int32 NextTriangleOffset = 0;
	for (int32 ProfileIndex = 0; ProfileIndex < NumProfilePoints; ++ProfileIndex)
	{
		bool CurrentIsWelded = WeldedVertices.Contains(ProfileIndex);

		VertPositionOffsets[ProfileIndex] = CurrentIsWelded ? NextWeldedOffset++ : NextNonWeldedOffset++;

		if (bProfileCurveIsClosed || ProfileIndex < NumProfilePoints - 1) // if there's a next profile point
		{
			// Set up indexing for the triangle (or triangles in the quad) immediately to the right of this vertex.
			int32 NextProfileIndex = (ProfileIndex + 1) % NumProfilePoints;
			bool NextIsWelded = WeldedVertices.Contains(NextProfileIndex);
			if (CurrentIsWelded)
			{
				// No triangles for a welded-to-welded connection, otherwise one triangle.
				TriangleOffsets[ProfileIndex] = NextIsWelded ? -1 : NextTriangleOffset++;
			}
			else
			{
				// Depending on whether next is welded or not, we're adding one or two triangles.
				TriangleOffsets[ProfileIndex] = NextTriangleOffset;
				NextTriangleOffset += (NextIsWelded ? 1 : 2);
			}
		}
	}
	int32 NumTrisPerSweepSegment = NextTriangleOffset;
	int32 NumTriangles = NumTrisPerSweepSegment * NumSweepSegments;

	// Determine number of normals needed. When converting to a dynamic mesh later, we are not allowed to have vertices of
	// the same triangle share a normal, so for sharp normals, we need  NumTriangles*3 rather than NumTriangles normals.
	int32 NumNormals = NumVerts;

	// If we're going to be averaging normals for vertices, we'll want some temporary storage so that we can avoid
	// coincident writes when we're parallelized.
	TArray<FVector3d> WeightedNormals;
	WeightedNormals.SetNum(NumTriangles * 3);

	// Perform all allocations except UV's, which will get done later and for which we may want some vertex positions.
	SetBufferSizes(NumVerts, NumTriangles, 0, NumNormals);

	if (Progress && Progress->Cancelled())
	{
		return *this;
	}

	// Create positions of all our vertices. We don't connect triangles yet because that is best done at
	// the same time as dealing with normals, and normals will require the positions to have been set.
	ParallelFor(NumProfilePoints, 
		[this, NumWelded, NumNonWelded, NumSweepPoints, &VertPositionOffsets]
		(int ProfileIndex)
	{
		if (WeldedVertices.Contains(ProfileIndex))
		{
			FVector3d FramePoint = ProfileCurve[ProfileIndex];
			// Position stays locked into the first frame
			if (!SweepScaleCurve.IsEmpty())
			{
				FramePoint *= SweepScaleCurve[0];
			}
			SetVertex(GetVertIndex(true, 0, ProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets), 
				SweepCurve[0].FromFramePoint(FramePoint));
		}
		else
		{
			// Generate copies of the vertex in all the sweep frames.
			for (int32 SweepIndex = 0; SweepIndex < NumSweepPoints; ++SweepIndex)
			{
				FVector3d FramePoint = ProfileCurve[ProfileIndex];
				if (!SweepScaleCurve.IsEmpty())
				{
					FramePoint *= SweepScaleCurve[SweepIndex];
				}
				SetVertex(GetVertIndex(false, SweepIndex, ProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets),
					SweepCurve[SweepIndex].FromFramePoint(FramePoint));
			}
		}
	});

	if (Progress && Progress->Cancelled())
	{
		return *this;
	}

	// Now set up UV's and UV indexing. This performs the UV allocation for us.
	int32 NumUvColumns = 0;
	int32 NumUvRows = 0;
	InitializeUvBuffer(VertPositionOffsets, NumUvRows, NumUvColumns);

	// Connect up the triangles, calculate normals, and associate with UV's
	ParallelFor(NumSweepSegments, 
		[this, NumSweepPoints, NumProfilePoints, NumWelded, NumNonWelded, &VertPositionOffsets,  
			NumProfileSegments, NumTrisPerSweepSegment, &TriangleOffsets, &WeightedNormals, NumUvColumns]
		(int SweepIndex)
	{
		int32 NextSweepIndex = (SweepIndex + 1) % NumSweepPoints;

		for (int32 ProfileIndex = 0; ProfileIndex < NumProfileSegments; ++ProfileIndex)
		{
			int32 NextProfileIndex = (ProfileIndex + 1) % NumProfilePoints;
			bool CurrentIsWelded = WeldedVertices.Contains(ProfileIndex);
			bool NextIsWelded = WeldedVertices.Contains(NextProfileIndex);

			if (CurrentIsWelded)
			{
				if (NextIsWelded)
				{
					// No triangles between adjacent welded triangles
					continue;
				}
				else
				{
					// Welded to non-welded: one triangle
					int32 CurrentVert = GetVertIndex(true, SweepIndex, ProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets);
					int32 BottomRightVert = GetVertIndex(false, NextSweepIndex, NextProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets);
					int32 RightVert = GetVertIndex(false, SweepIndex, NextProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets);

					int32 TriIndex = GetTriangleIndex(SweepIndex, ProfileIndex, NumTrisPerSweepSegment, TriangleOffsets);

					SetTriangle(TriIndex, CurrentVert, BottomRightVert, RightVert);
					AdjustNormalsForTriangle(TriIndex, CurrentVert, BottomRightVert, RightVert, WeightedNormals);
					SetTriangleUVs(TriIndex,
						// Do not wrap around when looking for UV elements, since there will be an extra on the end if needed
						GetUvIndex(SweepIndex, ProfileIndex, NumUvColumns),
						GetUvIndex(SweepIndex + 1, ProfileIndex + 1, NumUvColumns),
						GetUvIndex(SweepIndex, ProfileIndex + 1, NumUvColumns));
					SetTrianglePolygon(TriIndex, GetPolygonGroup(PolygonGroupingMode, SweepIndex, ProfileIndex, NumProfileSegments));
				}
			}
			else
			{
				if (NextIsWelded)
				{
					// Non-welded to welded: one triangle
					int32 CurrentVert = GetVertIndex(false, SweepIndex, ProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets);
					int32 BottomVert = GetVertIndex(false, NextSweepIndex, ProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets);
					int32 RightVert = GetVertIndex(true, SweepIndex, NextProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets);

					int32 TriIndex = GetTriangleIndex(SweepIndex, ProfileIndex, NumTrisPerSweepSegment, TriangleOffsets);

					SetTriangle(TriIndex, CurrentVert, BottomVert, RightVert);
					AdjustNormalsForTriangle(TriIndex, CurrentVert, BottomVert, RightVert, WeightedNormals);
					SetTriangleUVs(TriIndex,
						// Do not wrap around when looking for UV elements, since there will be an extra on the end if needed
						GetUvIndex(SweepIndex, ProfileIndex, NumUvColumns),
						GetUvIndex(SweepIndex + 1, ProfileIndex, NumUvColumns),
						GetUvIndex(SweepIndex, ProfileIndex + 1, NumUvColumns));
					SetTrianglePolygon(TriIndex, GetPolygonGroup(PolygonGroupingMode, SweepIndex, ProfileIndex, NumProfileSegments));
				}
				else
				{
					// Non-welded to non-welded creates a quad.
					int32 CurrentVert = GetVertIndex(false, SweepIndex, ProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets);
					int32 BottomVert = GetVertIndex(false, NextSweepIndex, ProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets);
					int32 BottomRightVert = GetVertIndex(false, NextSweepIndex, NextProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets);
					int32 RightVert = GetVertIndex(false, SweepIndex, NextProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets);

					int32 TriIndex = GetTriangleIndex(SweepIndex, ProfileIndex, NumTrisPerSweepSegment, TriangleOffsets);

					// The currently supported modes are to either always connect diagonally down, or connect the shorter diagonal.
					// For comparing diagonals, we allow some percent difference to triangulate symmetric quads uniformly.
					if (QuadSplitMethod == EProfileSweepQuadSplit::Uniform
						|| DistanceSquared(Vertices[CurrentVert], Vertices[BottomRightVert])
							/ DistanceSquared(Vertices[BottomVert], Vertices[RightVert]) <= (1 + DiagonalTolerance)*(1 + DiagonalTolerance))
					{
						FVector3d DiagonalDown = Normalized(Vertices[BottomRightVert] - Vertices[CurrentVert]);

						SetTriangle(TriIndex, CurrentVert, BottomRightVert, RightVert);
						AdjustNormalsForTriangle(TriIndex, CurrentVert, BottomRightVert, RightVert, WeightedNormals, DiagonalDown);
						SetTriangleUVs(TriIndex,
							GetUvIndex(SweepIndex, ProfileIndex, NumUvColumns),
							GetUvIndex(SweepIndex + 1, ProfileIndex + 1, NumUvColumns),
							GetUvIndex(SweepIndex, ProfileIndex + 1, NumUvColumns));
						SetTrianglePolygon(TriIndex, GetPolygonGroup(PolygonGroupingMode, SweepIndex, ProfileIndex, NumProfileSegments));
						++TriIndex;

						// Do the second triangle in such a way that the diagonal is goes from first vertex to second.
						SetTriangle(TriIndex, BottomRightVert, CurrentVert, BottomVert);
						AdjustNormalsForTriangle(TriIndex, BottomRightVert, CurrentVert, BottomVert, WeightedNormals, -DiagonalDown);
						SetTriangleUVs(TriIndex,
							GetUvIndex(SweepIndex + 1, ProfileIndex + 1, NumUvColumns),
							GetUvIndex(SweepIndex, ProfileIndex, NumUvColumns),
							GetUvIndex(SweepIndex + 1, ProfileIndex, NumUvColumns));
						SetTrianglePolygon(TriIndex, GetPolygonGroup(PolygonGroupingMode, SweepIndex, ProfileIndex, NumProfileSegments));
					}
					else
					{
						FVector3d DiagonalUp = Normalized(Vertices[RightVert] - Vertices[BottomVert]);
						SetTriangle(TriIndex, BottomVert, RightVert, CurrentVert);
						AdjustNormalsForTriangle(TriIndex, BottomVert, RightVert, CurrentVert, WeightedNormals, DiagonalUp);
						SetTriangleUVs(TriIndex,
							GetUvIndex(SweepIndex + 1, ProfileIndex, NumUvColumns),
							GetUvIndex(SweepIndex, ProfileIndex + 1, NumUvColumns),
							GetUvIndex(SweepIndex, ProfileIndex, NumUvColumns));
						SetTrianglePolygon(TriIndex, GetPolygonGroup(PolygonGroupingMode, SweepIndex, ProfileIndex, NumProfileSegments));
						++TriIndex;

						SetTriangle(TriIndex, RightVert, BottomVert, BottomRightVert);
						AdjustNormalsForTriangle(TriIndex, RightVert, BottomVert, BottomRightVert, WeightedNormals, -DiagonalUp);
						SetTriangleUVs(TriIndex,
							GetUvIndex(SweepIndex, ProfileIndex + 1, NumUvColumns),
							GetUvIndex(SweepIndex + 1, ProfileIndex, NumUvColumns),
							GetUvIndex(SweepIndex + 1, ProfileIndex + 1, NumUvColumns));
						SetTrianglePolygon(TriIndex, GetPolygonGroup(PolygonGroupingMode, SweepIndex, ProfileIndex, NumProfileSegments));
					}//end splitting quad
				}//end if nonwelded to nonwelded
			}//end if nonwelded
		}//end for profile points
	});//end parallel across sweep segments

	if (Progress && Progress->Cancelled())
	{
		return *this;
	}

	// Combine the weighted normals that we accumulated.
	TArray<FVector3d> NormalSums;
	NormalSums.SetNumZeroed(NumVerts);
	for (int32 TriIndex = 0; TriIndex < NumTriangles; ++TriIndex)
	{
		FIndex3i TriangleVerts = Triangles[TriIndex];
		NormalSums[TriangleVerts.A] += WeightedNormals[TriIndex * 3];
		NormalSums[TriangleVerts.B] += WeightedNormals[TriIndex * 3 + 1];
		NormalSums[TriangleVerts.C] += WeightedNormals[TriIndex * 3 + 2];
	}

	if (Progress && Progress->Cancelled())
	{
		return *this;
	}

	// Normalize and set them
	ParallelFor(NumVerts, [this, &NormalSums](int VertIndex)
	{
		SetNormal(VertIndex, (FVector3f)(Normalized(NormalSums[VertIndex])), VertIndex);
	});

	if (Progress && Progress->Cancelled())
	{
		return *this;
	}

	// Save the beginning and end profile curve instances, if relevant
	if (!bSweepCurveIsClosed)
	{
		for (int32 ProfileIndex = 0; ProfileIndex < NumProfilePoints; ++ProfileIndex)
		{
			EndProfiles[0].Add(GetVertIndex(WeldedVertices.Contains(ProfileIndex), 0, ProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets));
			EndProfiles[1].Add(GetVertIndex(WeldedVertices.Contains(ProfileIndex), NumSweepPoints-1, ProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets));
		}
	}

	return *this;
}//end Generate()

/**
 * Initializes the UV buffer with UV's that are set according to the diagram at the start of Generate(), with an extra
 * element on each end in the case of closed curves, and distances weighted by distances in the corresponding curves.
 * This function should get called after setting vertex positions so that the function can use them in case 
 * bUVScaleRelativeWorld is true.
 *
 * @param VertPositionOffsets Offsets needed to get the correct vertex indices, for setting the parents of UV elements
 * @param NumUvRowsOut Number of resulting UV rows allocated (a row corresponds to an instance of the profile curve)
 * @param NumUvColumnsOut Number of resulting UV columns allocated (number of columns relates to number of profile points)
 */
void FProfileSweepGenerator::InitializeUvBuffer(const TArray<int32>& VertPositionOffsets,
	int32& NumUvRowsOut, int32& NumUvColumnsOut)
{
	// Convenience variables
	int32 NumProfilePoints = ProfileCurve.Num();
	int32 NumProfileSegments = bProfileCurveIsClosed ? NumProfilePoints : NumProfilePoints - 1;
	int32 NumSweepPoints = SweepCurve.Num();
	int32 NumSweepSegments = bSweepCurveIsClosed ? NumSweepPoints : NumSweepPoints - 1;
	int32 NumWelded = WeldedVertices.Num();
	int32 NumNonWelded = NumProfilePoints - NumWelded;

	// Since we're working with a grid, the coordinates we generate are combinations of a limited number of U and V options, which are
	// determined by spacing between the corresponding curve points. The V elements are determined by ProfileIndex.
	TArray<double> Vs;
	double CumulativeDistance = 0;
	Vs.Add(CumulativeDistance);
	for (int32 ProfileIndex = 0; ProfileIndex < NumProfileSegments; ++ProfileIndex)
	{
		int32 NextProfileIndex = (ProfileIndex + 1) % NumProfilePoints;
		if (!bUVsSkipFullyWeldedEdges
			|| !(WeldedVertices.Contains(ProfileIndex) && WeldedVertices.Contains(NextProfileIndex))) // check for welded-to-welded
		{
			CumulativeDistance += Distance(ProfileCurve[ProfileIndex], ProfileCurve[NextProfileIndex]);
		}
		Vs.Add(CumulativeDistance);
	}

	// Figure out how we'll be normalizing/scaling the V's
	double VScale = UVScale[1];
	if (!bUVScaleRelativeWorld)
	{
		// The normal case: normalizing and scaling.
		VScale = ensure(CumulativeDistance != 0) ? // Profile points shouldn't be coincident
			VScale /= CumulativeDistance 
			: 0;
	}
	else
	{
		// Convert using custom scale
		VScale  = (UnitUVInWorldCoordinates != 0) ? VScale / UnitUVInWorldCoordinates : 0;
	}

	// Scale and adjust
	for (int i = 0; i < Vs.Num(); ++i)
	{
		Vs[i] =  Vs[i] * VScale + UVOffset[1];
	}

	// U elements depend on distances in the sweep direction. For that, we need to accumulate the displacement of
	// the profile vertices across sweep frames. We can divide by NumProfileVertices later, if we need to.
	TArray<double> Distances;
	Distances.SetNumZeroed(NumSweepSegments);
	ParallelFor(NumSweepSegments,
		[this, NumSweepPoints, NumProfilePoints, NumWelded, NumNonWelded, &VertPositionOffsets, &Distances]
	(int SweepIndex)
	{
		int32 NextSweepIndex = (SweepIndex + 1) % NumSweepPoints;
		for (int32 ProfileIndex = 0; ProfileIndex < NumProfilePoints; ++ProfileIndex)
		{
			if (!WeldedVertices.Contains(ProfileIndex))
			{
				FVector3d A = Vertices[GetVertIndex(false, SweepIndex, ProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets)];
				FVector3d B = Vertices[GetVertIndex(false, NextSweepIndex, ProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets)];
				Distances[SweepIndex] += Distance(A, B);
			}
		}
	});

	// U elements differ between regular and welded vertices, since welded are centered between adjacent non-welded
	TArray<double> WeldedUs;
	TArray<double> RegularUs;
	CumulativeDistance = 0;
	RegularUs.Add(0);
	for (int i = 0; i < NumSweepSegments; ++i)
	{
		CumulativeDistance += Distances[i];
		RegularUs.Add(CumulativeDistance);
		WeldedUs.Add(CumulativeDistance - Distances[i] / 2);
	}
	// There's one fewer welded U, but it's more convenient to make the two arrays the same length
	// so we can index into them the same way and initialize all UV elements including the few
	// that we don't use.
	WeldedUs.Add(0);

	double UScale = UVScale[0];
	if (!bUVScaleRelativeWorld)
	{
		UScale = (CumulativeDistance != 0) ? UScale / CumulativeDistance : 0;
	}
	else
	{
		UScale = (UnitUVInWorldCoordinates != 0) ? 
			// Get an average and convert to UV from world using the scale.
			UScale / (float(NumProfilePoints) * UnitUVInWorldCoordinates) 
			: 0;
	}
	// Adjust
	for (int i = 1; i < RegularUs.Num(); ++i)
	{
		RegularUs[i] = RegularUs[i] * UScale + UVOffset[0];
		WeldedUs[i - 1] = WeldedUs[i-1] * UScale + UVOffset[0];
	}

	// Set up storage
	NumUvRowsOut = RegularUs.Num();
	NumUvColumnsOut = Vs.Num();
	SetBufferSizes(0, 0, NumUvRowsOut * NumUvColumnsOut, 0);

	// Initialize the UV buffer
	ParallelFor(NumUvRowsOut, 
		[this, NumSweepPoints, NumProfilePoints, NumUvColumnsOut, NumWelded,
		NumNonWelded, &VertPositionOffsets, &WeldedUs, &RegularUs, &Vs]
		(int i)
	{
		int32 SweepIndex = i % NumSweepPoints;
		for (int j = 0; j < NumUvColumnsOut; ++j)
		{
			int32 ProfileIndex = j % NumProfilePoints;
			if (WeldedVertices.Contains(ProfileIndex))
			{
				int32 VertIndex = GetVertIndex(true, SweepIndex, ProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets);
				SetUV(i * NumUvColumnsOut + j, FVector2f((float)WeldedUs[i], (float)Vs[j]), VertIndex);
			}
			else
			{
				int32 VertIndex = GetVertIndex(false, SweepIndex, ProfileIndex, NumWelded, NumNonWelded, VertPositionOffsets);
				SetUV(i * NumUvColumnsOut + j, FVector2f((float)RegularUs[i], (float)Vs[j]), VertIndex);
			}
		}
	});
}