// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/TriangleMesh.h"
#include "Chaos/HierarchicalSpatialHash.h"


// This does initialization for PBDCollisionSpringConstraints and PBDTriangleMeshIntersections, 
// including intersection detection and global intersection analysis
//
namespace Chaos::Softs
{
class CHAOS_API FPBDTriangleMeshCollisions
{
public:
	FPBDTriangleMeshCollisions(
		const int32 InOffset,
		const int32 InNumParticles,
		const FTriangleMesh& InTriangleMesh,
		bool bInGlobalIntersectionAnalysis,
		bool bInContourMinimization
	)
		:TriangleMesh(InTriangleMesh)
		, Offset(InOffset)
		, NumParticles(InNumParticles)
		, bGlobalIntersectionAnalysis(bInGlobalIntersectionAnalysis)
		, bContourMinimization(bInContourMinimization)
	{}

	~FPBDTriangleMeshCollisions() = default;

	void Init(const FSolverParticles& Particles);

	struct FContourMinimizationIntersection
	{
		TVec2<int32> EdgeVertices;
		TVec3<int32> FaceVertices;
		FSolverVec3 LocalGradientVector;
		FSolverVec3 GlobalGradientVector;
	};
	
	// Vertices (and Triangles) are assigned FGIAColors by flood-filling global intersection contours.
	// ContourIndex represents which pair of global contours, up to 31 unique contour pairs--then the contour index will reused. 
	// In practice this seems to be enough unique contours. It's OK if multiple contours have the same index as long
	// as their intersecting regions don't interact, which is unlikely.
	// Contours come in pairs representing two regions intersecting (except for "loop" contours where a region of cloth intersects itself--think pinch regions). 
	// These are "Colors" represented by setting the ColorBit to 0 or 1 corresponding with the given contour index.
	struct FGIAColor
	{
		int32 ContourIndexBits = 0;
		int32 ColorBits = 0;

		static constexpr int32 LoopContourIndex = 0;
		static constexpr int32 LoopBits = 1 << LoopContourIndex;
		static constexpr int32 NonLoopMask = ~LoopBits;
		
		void SetContourColor(int32 ContourIndex, bool bIsColorB)
		{
			check(ContourIndex < 32);
			ContourIndexBits |= 1 << ContourIndex;
			if (bIsColorB)
			{
				ColorBits |= 1 << ContourIndex;
			}
			else
			{
				ColorBits &= ~(1 << ContourIndex);
			}
		}

		bool HasContourColorSet(int32 ContourIndex) const
		{
			check(ContourIndex < 32);
			return ContourIndexBits & (1 << ContourIndex);
		}

		bool IsLoop() const
		{
			return (ContourIndexBits & LoopBits) && (ColorBits & LoopBits);
		}

		void SetLoop()
		{
			SetContourColor(LoopContourIndex, true);
		}

		// Because they are opposite colors with a shared contour index. This will cause repulsion forces to attract and fix the intersection.
		// NOTE: We flip normals if ANY of the TriVertColors are opposite the PointColor or if the TriColor (used for thin regions is flipped). This does a better job for thin features
		// than only flipping normals if ALL TriVertColors are opposite.
		static bool ShouldFlipNormal(const FGIAColor& Color0, const FGIAColor& Color1)
		{
			const int32 SharedContourBits = Color0.ContourIndexBits & Color1.ContourIndexBits & NonLoopMask;
			const int32 FlippedColorBits = Color0.ColorBits ^ Color1.ColorBits;
			return FlippedColorBits & SharedContourBits;
		}
	};


	// Debug display of intersection contours
	struct FBarycentricPoint
	{
		FSolverVec2 Bary;
		TVec3<int32> Vertices;
	};

	enum struct FContourType : int8
	{
		Open = 0,
		Loop,
		Contour0,
		Contour1,
		Count
	};

	void SetGlobalIntersectionAnalysis(bool bInGlobalIntersectionAnalysis) { bGlobalIntersectionAnalysis = bInGlobalIntersectionAnalysis; }
	void SetContourMinimization(bool bInContourMinimization) { bContourMinimization = bInContourMinimization; }

	const FTriangleMesh::TSpatialHashType<FSolverReal>& GetSpatialHash() const { return SpatialHash; }
	const TArray<FContourMinimizationIntersection>& GetContourMinimizationIntersections() const { return ContourMinimizationIntersections; }
	const TConstArrayView<FGIAColor> GetVertexGIAColors() const { return bGlobalIntersectionAnalysis && VertexGIAColors.Num() == NumParticles ? TConstArrayView<FGIAColor>(VertexGIAColors.GetData() - Offset, NumParticles + Offset) : TConstArrayView<FGIAColor>(); }
	const TArray<FGIAColor>& GetTriangleGIAColors() const { return TriangleGIAColors; }
	const TArray<TArray<FBarycentricPoint>>& GetIntersectionContourPoints() const { return IntersectionContourPoints; }
	const TArray<FContourType>& GetIntersectionContourTypes() const { return IntersectionContourTypes; }
private:

	const FTriangleMesh& TriangleMesh;
	int32 Offset;
	int32 NumParticles;
	bool bGlobalIntersectionAnalysis;
	bool bContourMinimization;

	FTriangleMesh::TSpatialHashType<FSolverReal> SpatialHash;
	TArray<FContourMinimizationIntersection> ContourMinimizationIntersections;
	TArray<FGIAColor> VertexGIAColors;
	TArray<FGIAColor> TriangleGIAColors;

	// Debug display of intersection contours
	TArray<TArray<FBarycentricPoint>> IntersectionContourPoints;
	TArray<FContourType> IntersectionContourTypes;

};

}  // End namespace Chaos::Softs
