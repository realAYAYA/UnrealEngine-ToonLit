// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/CollisionConvexMesh.h"
#include "HAL/IConsoleManager.h"
#include "CompGeom/ConvexHull3.h"

namespace Chaos
{
	// CVars variables for controlling geometry complexity checking and simplification 
	int32 FConvexBuilder::PerformGeometryCheck = 0;

	int32 FConvexBuilder::PerformGeometryReduction = 0;

	int32 FConvexBuilder::VerticesThreshold = 50;

	int32 FConvexBuilder::ComputeHorizonEpsilonFromMeshExtends = 1;

	bool FConvexBuilder::bUseGeometryTConvexHull3 = true;

	FAutoConsoleVariableRef CVarConvexGeometryCheckEnable(TEXT("p.Chaos.ConvexGeometryCheckEnable"), FConvexBuilder::PerformGeometryCheck, TEXT("Perform convex geometry complexity check for Chaos physics."));

	FAutoConsoleVariableRef CVarConvexGeometrySimplifyEnable(TEXT("p.Chaos.PerformGeometryReduction"), FConvexBuilder::PerformGeometryReduction, TEXT("Perform convex geometry simplification to increase performance in Chaos physics."));

	FAutoConsoleVariableRef CVarConvexParticlesWarningThreshold(TEXT("p.Chaos.ConvexParticlesWarningThreshold"), FConvexBuilder::VerticesThreshold, TEXT("Threshold beyond which we warn about collision geometry complexity."));
	
	FAutoConsoleVariableRef CvarUseGeometryTConvexHull3(TEXT("p.Chaos.Convex.UseTConvexHull3Builder"), FConvexBuilder::bUseGeometryTConvexHull3, TEXT("Use the newer Geometry Tools code path for generating convex hulls when default build method is set.[def:true]"));


	bool FConvexBuilder::UseConvexHull3(FConvexBuilder::EBuildMethod BuildMethod)
	{
		if (BuildMethod == EBuildMethod::Default && FConvexBuilder::bUseGeometryTConvexHull3)
		{
			return true;
		}
		return BuildMethod == EBuildMethod::ConvexHull3;
	}

	void FConvexBuilder::BuildIndices(const TArray<FVec3Type>& InVertices, TArray<int32>& OutResultIndexData, EBuildMethod BuildMethod)
	{
		if (UseConvexHull3(BuildMethod))
		{
			UE::Geometry::TConvexHull3<FRealType> HullCompute;
			if (HullCompute.Solve<FVec3Type>(InVertices))
			{
				const TArray<UE::Geometry::FIndex3i>& HullTriangles = HullCompute.GetTriangles();
				OutResultIndexData.Reserve(HullTriangles.Num() * 3);
				for (const UE::Geometry::FIndex3i& Tri : HullTriangles)
				{
					// Winding is backwards from what Chaos expects
					OutResultIndexData.Add(Tri[0]);
					OutResultIndexData.Add(Tri[2]);
					OutResultIndexData.Add(Tri[1]);
				}
			}
		}
		else
		{
			TArray<Chaos::TVec3<int32>> Triangles;
			Params BuildParams;
			BuildParams.HorizonEpsilon = SuggestEpsilon(InVertices);
			BuildConvexHull(InVertices, Triangles, BuildParams);
			OutResultIndexData.Reserve(Triangles.Num() * 3);
			for (Chaos::TVec3<int32> Tri : Triangles)
			{
				OutResultIndexData.Add(Tri[0]);
				OutResultIndexData.Add(Tri[1]);
				OutResultIndexData.Add(Tri[2]);
			}
		}

	}

	void FConvexBuilder::Build(const TArray<FVec3Type>& InVertices, TArray <FPlaneType>& OutPlanes, TArray<TArray<int32>>& OutFaceIndices, TArray<FVec3Type>& OutVertices, FAABB3Type& OutLocalBounds, EBuildMethod BuildMethod)
	{
		OutPlanes.Reset();
		OutVertices.Reset();
		OutLocalBounds = FAABB3Type::EmptyAABB();

		const int32 NumVerticesIn = InVertices.Num();
		if (NumVerticesIn == 0)
		{
			return;
		}


		const TArray<FVec3Type>* VerticesToUse = &InVertices;
		TArray<FVec3Type> ModifiedVertices;

		// For triangles and planar shapes, create a very thin prism as a convex
		auto Inflate = [](const TArray<FVec3Type>& Source, TArray<FVec3Type>& Destination, const FVec3Type& Normal, FRealType Inflation)
		{
			const int32 NumSource = Source.Num();
			Destination.Reset();
			Destination.SetNum(NumSource * 2);

			for (int32 Index = 0; Index < NumSource; ++Index)
			{
				Destination[Index] = Source[Index];
				Destination[NumSource + Index] = Source[Index] + Normal * Inflation;
			}
		};

		FVec3Type PlanarNormal(0);
		if (NumVerticesIn == 3)
		{
			const bool bIsValidTriangle = IsValidTriangle(InVertices[0], InVertices[1], InVertices[2], PlanarNormal);

			//TODO_SQ_IMPLEMENTATION: should do proper cleanup to avoid this
			if (ensureMsgf(bIsValidTriangle, TEXT("FConvexBuilder::Build(): Generated invalid triangle!")))
			{
				Inflate(InVertices, ModifiedVertices, PlanarNormal, TriQuadPrismInflation());
				VerticesToUse = &ModifiedVertices;
				UE_LOG(LogChaos, Verbose, TEXT("Encountered a triangle in convex hull generation. Will prepare a prism of thickness %.5f in place of a triangle."), TriQuadPrismInflation());
			}
			else
			{
				return;
			}
		}
		else if (IsPlanarShape(InVertices, PlanarNormal))
		{
			Inflate(InVertices, ModifiedVertices, PlanarNormal, TriQuadPrismInflation());
			VerticesToUse = &ModifiedVertices;
			UE_LOG(LogChaos, Verbose, TEXT("Encountered a planar shape in convex hull generation. Will prepare a prism of thickness %.5f in place of a triangle."), TriQuadPrismInflation());
		}

		const int32 NumVerticesToUse = VerticesToUse->Num();

		OutLocalBounds = FAABB3Type((*VerticesToUse)[0], (*VerticesToUse)[0]);
		for (int32 VertexIndex = 0; VertexIndex < NumVerticesToUse; ++VertexIndex)
		{
			OutLocalBounds.GrowToInclude((*VerticesToUse)[VertexIndex]);
		}

		if (NumVerticesToUse >= 4)
		{
			if (UseConvexHull3(BuildMethod)) // Use the newer Geometry Tools code path for generating convex hulls.
			{
				UE::Geometry::TConvexHull3<FRealType> HullCompute;
				if (HullCompute.Solve<FVec3Type>(*VerticesToUse))
				{
					// Get Planes, FaceIndices, Vertices, and LocalBoundingBox
					TMap<int32, int32> HullVertMap;
					HullCompute.GetTriangles([&VerticesToUse, &HullVertMap, &OutPlanes, &OutFaceIndices, &OutVertices, &OutLocalBounds](UE::Geometry::FIndex3i Triangle)
						{
							for (int32 j = 0; j < 3; ++j)		// From FMeshConvexHull::Compute_FullMesh
							{
								int32 Index = Triangle[j];
								if (HullVertMap.Contains(Index) == false)
								{
									const FVec3Type& OrigPos = (*VerticesToUse)[Index];
									int32 NewVID = OutVertices.Num();
									OutVertices.Add(OrigPos);
									HullVertMap.Add(Index, NewVID);
									Triangle[j] = NewVID;
								}
								else
								{
									Triangle[j] = HullVertMap[Index];
								}
							}

							// Winding is backwards from what Chaos expects
							OutFaceIndices.Add({ Triangle[0], Triangle[2], Triangle[1] });

							UE::Geometry::THalfspace3<FRealType> Halfspace(OutVertices[Triangle[0]], OutVertices[Triangle[1]], OutVertices[Triangle[2]]);
							const FVec3Type& Normal = Halfspace.Normal;
							const FVec3Type& Point = OutVertices[Triangle[0]];
							OutPlanes.Add(FPlaneType{ Point, Normal });
						});

					OutLocalBounds = FAABB3Type(OutVertices[0], OutVertices[0]);
					for (int32 VertexIndex = 1; VertexIndex < OutVertices.Num(); ++VertexIndex)
					{
						OutLocalBounds.GrowToInclude(OutVertices[VertexIndex]);
					}
				}
			}
			else // Use the older Chaos code path for generating convex hulls.
			{
				// @todo(chaos): Deprecate the older convex code path.
				TArray<TVec3<int32>> Indices;
				Params BuildParams;
				BuildParams.HorizonEpsilon = Chaos::FConvexBuilder::SuggestEpsilon(*VerticesToUse);
				BuildConvexHull(*VerticesToUse, Indices, BuildParams);
				OutPlanes.Reserve(Indices.Num());
				TMap<int32, int32> IndexMap; // maps original particle indices to output particle indices
				int32 NewIdx = 0;

				const auto AddIndex = [&IndexMap, &NewIdx](const int32 OriginalIdx)
				{
					if (int32* Idx = IndexMap.Find(OriginalIdx))
					{
						return *Idx;
					}
					IndexMap.Add(OriginalIdx, NewIdx);
					return NewIdx++;
				};

				for (const TVec3<int32>& Idx : Indices)
				{
					FVec3Type Vs[3] = { (*VerticesToUse)[Idx[0]], (*VerticesToUse)[Idx[1]], (*VerticesToUse)[Idx[2]] };
					const FVec3Type Normal = FVec3Type::CrossProduct(Vs[1] - Vs[0], Vs[2] - Vs[0]).GetUnsafeNormal();
					OutPlanes.Add(FPlaneType(Vs[0], Normal));
					TArray<int32> FaceIndices;
					FaceIndices.SetNum(3);
					FaceIndices[0] = AddIndex(Idx[0]);
					FaceIndices[1] = AddIndex(Idx[1]);
					FaceIndices[2] = AddIndex(Idx[2]);
					OutFaceIndices.Add(FaceIndices);
				}

				OutVertices.SetNum(IndexMap.Num());
				for (const auto& Elem : IndexMap)
				{
					OutVertices[Elem.Value] = (*VerticesToUse)[Elem.Key];
				}
			}
		}

		UE_CLOG(OutVertices.Num() == 0, LogChaos, Warning, TEXT("Convex hull generation produced zero convex particles, collision will fail for this primitive."));
	}

}
