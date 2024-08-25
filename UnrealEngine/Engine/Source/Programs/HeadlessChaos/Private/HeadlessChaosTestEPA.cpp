// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeadlessChaosTestEPA.h"

#include "HeadlessChaos.h"
#include "HeadlessChaosTestUtility.h"
#include "Chaos/EPA.h"
#include "Chaos/Sphere.h"
#include "Chaos/Capsule.h"
#include "Chaos/GJK.h"
#include "Chaos/Convex.h"
#include "Chaos/ImplicitObjectScaled.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Chaos/Triangle.h"
#include "Chaos/TriangleRegister.h"

namespace ChaosTest
{
	using namespace Chaos;

	constexpr FReal EpaEps = 1e-6;

	void ValidFace(const FVec3* Verts, const TEPAWorkingArray<TEPAEntry<FReal>>& TetFaces, int32 Idx)
	{
		const TEPAEntry<FReal>& Entry = TetFaces[Idx];

		//does not contain vertex associated with face
		EXPECT_NE(Entry.IdxBuffer[0], Idx);
		EXPECT_NE(Entry.IdxBuffer[1], Idx);
		EXPECT_NE(Entry.IdxBuffer[2], Idx);

		//doesn't have itself as adjacent face
		EXPECT_NE(Entry.AdjFaces[0], Idx);
		EXPECT_NE(Entry.AdjFaces[1], Idx);
		EXPECT_NE(Entry.AdjFaces[2], Idx);

		//adjacent edges and faces are valid for both sides of the edge
		EXPECT_EQ(TetFaces[Entry.AdjFaces[0]].AdjFaces[Entry.AdjEdges[0]], Idx);
		EXPECT_EQ(TetFaces[Entry.AdjFaces[1]].AdjFaces[Entry.AdjEdges[1]], Idx);
		EXPECT_EQ(TetFaces[Entry.AdjFaces[2]].AdjFaces[Entry.AdjEdges[2]], Idx);

		//make sure that adjacent faces share vertices
		//src dest on the edge of face 0 matches to dest src of face 1
		for (int EdgeIdx = 0; EdgeIdx < 3; ++EdgeIdx)
		{
			const int32 FromFace0 = Entry.IdxBuffer[EdgeIdx];
			const int32 ToFace0 = Entry.IdxBuffer[(EdgeIdx+1)%3];
			const int32 Face1EdgeIdx = Entry.AdjEdges[EdgeIdx];
			const TEPAEntry<FReal>& Face1 = TetFaces[Entry.AdjFaces[EdgeIdx]];
			const int32 FromFace1 = Face1.IdxBuffer[Face1EdgeIdx];
			const int32 ToFace1 = Face1.IdxBuffer[(Face1EdgeIdx+1)%3];
			EXPECT_EQ(FromFace0, ToFace1);
			EXPECT_EQ(FromFace1, ToFace0);
			
		}
		switch (Entry.AdjEdges[0])
		{
		case 0:
		{
			EXPECT_EQ(Entry.IdxBuffer[0], TetFaces[Entry.AdjFaces[0]].IdxBuffer[1]);
			EXPECT_EQ(Entry.IdxBuffer[1], TetFaces[Entry.AdjFaces[0]].IdxBuffer[0]);
			break;
		}
		case 1:
		{
			EXPECT_EQ(Entry.IdxBuffer[0], TetFaces[Entry.AdjFaces[0]].IdxBuffer[2]);
			EXPECT_EQ(Entry.IdxBuffer[1], TetFaces[Entry.AdjFaces[0]].IdxBuffer[1]);
			break;
		}
		case 2:
		{
			EXPECT_EQ(Entry.IdxBuffer[0], TetFaces[Entry.AdjFaces[0]].IdxBuffer[0]);
			EXPECT_EQ(Entry.IdxBuffer[1], TetFaces[Entry.AdjFaces[0]].IdxBuffer[2]);
			break;
		}
		default: break;
		}

		EXPECT_LT(FVec3::DotProduct(Verts[Idx], Entry.PlaneNormal), 0);	//normal faces out

		EXPECT_GE(Entry.Distance, 0);	//positive distance since origin is inside tet

		EXPECT_NEAR(Entry.DistanceToPlane(Verts[Entry.IdxBuffer[0]]), 0, EpaEps);
		EXPECT_NEAR(Entry.DistanceToPlane(Verts[Entry.IdxBuffer[1]]), 0, EpaEps);
		EXPECT_NEAR(Entry.DistanceToPlane(Verts[Entry.IdxBuffer[2]]), 0, EpaEps);
	}

	FVec3 ErrorSupport(const FVec3& V)
	{
		check(false);
		return FVec3(0);
	}

	void EPAInitTest()
	{

		//make sure faces are properly oriented
		{
			TArray<FVec3> VertsA = { {-1,-1,1}, {-1,-1,-1}, {-1,1,-1}, {1,1,-1} };
			TArray<FVec3> VertsB = { FVec3(0), FVec3(0), FVec3(0), FVec3(0) };
			TEPAWorkingArray<TEPAEntry<FReal>> TetFaces;
			FVec3 TouchingNormal;
			EXPECT_TRUE(InitializeEPA(VertsA,VertsB,ErrorSupport,ErrorSupport,TetFaces,TouchingNormal));

			EXPECT_EQ(TetFaces.Num(), 4);
			for (int i = 0; i < TetFaces.Num(); ++i)
			{
				ValidFace(VertsA.GetData(), TetFaces, i);
			}
		}

		{
			TArray<FVec3> VertsA = { {-1,-1,-1}, {-1,-1,1}, {-1,1,-1}, {1,1,-1} };
			TArray<FVec3> VertsB = { FVec3(0), FVec3(0), FVec3(0), FVec3(0) };
			TEPAWorkingArray<TEPAEntry<FReal>> TetFaces;
			FVec3 TouchingNormal;

			EXPECT_TRUE(InitializeEPA(VertsA,VertsB,ErrorSupport,ErrorSupport,TetFaces,TouchingNormal));

			EXPECT_EQ(TetFaces.Num(), 4);
			for (int i = 0; i < TetFaces.Num(); ++i)
			{
				ValidFace(VertsA.GetData(), TetFaces, i);
			}
		}

		auto EmptySupport = [](const FVec3& V) { return FVec3(0); };

		//triangle
		{
			FVec3 AllVerts[] = { {0,-1,1 + 1 / (FReal)3}, {0,-1,-1 + 1 / (FReal)3}, {0,1,-1 + 1 / (FReal)3},{-1,0,0}, {0.5,0,0} };

			auto ASupport = [&](const FVec3& V)
			{
				FVec3 Best = AllVerts[0];
				for (const FVec3& Vert : AllVerts)
				{
					if (FVec3::DotProduct(Vert, V) > FVec3::DotProduct(Best, V))
					{
						Best = Vert;
					}
				}
				return Best;
			};

			auto ASupportNoPositiveX = [&](const FVec3& V)
			{
				FVec3 Best = AllVerts[0];
				for (const FVec3& Vert : AllVerts)
				{
					if (Vert.X > 0)
					{
						continue;
					}
					if (FVec3::DotProduct(Vert, V) > FVec3::DotProduct(Best, V))
					{
						Best = Vert;
					}
				}
				return Best;
			};

			auto ASupportNoX = [&](const FVec3& V)
			{
				FVec3 Best = AllVerts[0];
				for (const FVec3& Vert : AllVerts)
				{
					if (Vert.X > 0 || Vert.X < 0)
					{
						continue;
					}
					if (FVec3::DotProduct(Vert, V) > FVec3::DotProduct(Best, V))
					{
						Best = Vert;
					}
				}
				return Best;
			};

			//first winding
			{
				TArray<FVec3> VertsA = { AllVerts[0], AllVerts[1], AllVerts[2] };
				TArray<FVec3> VertsB = { FVec3(0), FVec3(0), FVec3(0) };

				TEPAWorkingArray<TEPAEntry<FReal>> TetFaces;
				FVec3 TouchingNormal;
				EXPECT_TRUE(InitializeEPA(VertsA,VertsB,ASupport,EmptySupport,TetFaces,TouchingNormal));
				EXPECT_VECTOR_NEAR(VertsA[3], AllVerts[3], 1e-4);
				EXPECT_VECTOR_NEAR(VertsB[3], FVec3(0), 1e-4);

				EXPECT_EQ(TetFaces.Num(), 4);
				for (int i = 0; i < TetFaces.Num(); ++i)
				{
					ValidFace(VertsA.GetData(), TetFaces, i);
				}

				FReal Penetration;
				FVec3 Dir, WitnessA, WitnessB;

				//Try EPA. Note that we are IGNORING the positive x vert to ensure a triangle right on the origin boundary works
				EPA(VertsA, VertsB, ASupportNoPositiveX, EmptySupport, Penetration, Dir, WitnessA, WitnessB, EpaEps);
				EXPECT_NEAR(Penetration, 0, 1e-4);
				EXPECT_VECTOR_NEAR(Dir, FVec3(1,0,0), 1e-4);
				EXPECT_VECTOR_NEAR(WitnessA, FVec3(0), 1e-4);
				EXPECT_VECTOR_NEAR(WitnessB, FVec3(0), 1e-4);
			}

			//other winding
			{
				TArray<FVec3> VertsA = { AllVerts[1], AllVerts[0], AllVerts[2] };
				TArray<FVec3> VertsB = { FVec3(0), FVec3(0), FVec3(0) };

				TEPAWorkingArray<TEPAEntry<FReal>> TetFaces;
				FVec3 TouchingNormal;
				EXPECT_TRUE(InitializeEPA(VertsA,VertsB,ASupport,EmptySupport,TetFaces,TouchingNormal));
				EXPECT_VECTOR_NEAR(VertsA[3], AllVerts[3], 1e-4);
				EXPECT_VECTOR_NEAR(VertsB[3], FVec3(0), 1e-4);

				EXPECT_EQ(TetFaces.Num(), 4);
				for (int i = 0; i < TetFaces.Num(); ++i)
				{
					ValidFace(VertsA.GetData(), TetFaces, i);
				}

				FReal Penetration;
				FVec3 Dir, WitnessA, WitnessB;

				//Try EPA. Note that we are IGNORING the positive x vert to ensure a triangle right on the origin boundary works
				EPA(VertsA, VertsB, ASupportNoPositiveX, EmptySupport, Penetration, Dir, WitnessA, WitnessB, EpaEps);
				EXPECT_NEAR(Penetration, 0, 1e-4);
				EXPECT_VECTOR_NEAR(Dir, FVec3(1, 0, 0), 1e-4);
				EXPECT_VECTOR_NEAR(WitnessA, FVec3(0), 1e-4);
				EXPECT_VECTOR_NEAR(WitnessB, FVec3(0), 1e-4);
			}

			//touching triangle
			{
				TArray<FVec3> VertsA = { AllVerts[1], AllVerts[0], AllVerts[2] };
				TArray<FVec3> VertsB = { FVec3(0), FVec3(0), FVec3(0) };

				TEPAWorkingArray<TEPAEntry<FReal>> TetFaces;
				FVec3 TouchingNormal;
				EXPECT_FALSE(InitializeEPA(VertsA,VertsB,ASupportNoX,EmptySupport,TetFaces,TouchingNormal));
				EXPECT_EQ(TouchingNormal.Z,0);
				EXPECT_EQ(TouchingNormal.Y,0);

				//make sure EPA handles this bad case properly
				VertsA = { AllVerts[1], AllVerts[0], AllVerts[2] };
				VertsB = { FVec3(0), FVec3(0), FVec3(0) };

				//touching so penetration 0, normal is 0,0,1
				FReal Penetration;
				FVec3 Dir, WitnessA, WitnessB;
				EXPECT_EQ(EPA(VertsA, VertsB, ASupportNoX, EmptySupport, Penetration, Dir, WitnessA, WitnessB, EpaEps), EEPAResult::BadInitialSimplex);
				EXPECT_EQ(Penetration, 0);
				//EXPECT_VECTOR_NEAR(Dir, FVec3(0, 0, 1), 1e-7);
				EXPECT_VECTOR_NEAR(WitnessA, FVec3(0), 1e-7);
				EXPECT_VECTOR_NEAR(WitnessB, FVec3(0), 1e-7);
			}
		}

		//line
		{
			FVec3 AllVerts[] = { {0,-1,1 + 1 / (FReal)3}, {0,-1,-1 + 1 / (FReal)3}, {0,1,-1 + 1 / (FReal)3},{-1,0,0}, {0.5,0,0} };

			auto ASupport = [&](const FVec3& V)
			{
				FVec3 Best = AllVerts[0];
				for (const FVec3& Vert : AllVerts)
				{
					if (FVec3::DotProduct(Vert, V) > FVec3::DotProduct(Best, V))
					{
						Best = Vert;
					}
				}
				return Best;
			};

			//first winding
			{
				TArray<FVec3> VertsA = { AllVerts[0], AllVerts[2] };
				TArray<FVec3> VertsB = { FVec3(0), FVec3(0) };

				TEPAWorkingArray<TEPAEntry<FReal>> TetFaces;
				FVec3 TouchingNormal;
				EXPECT_TRUE(InitializeEPA(VertsA,VertsB,ASupport,EmptySupport,TetFaces,TouchingNormal));
				EXPECT_VECTOR_NEAR(VertsA[2], AllVerts[1], 1e-4);
				EXPECT_VECTOR_NEAR(VertsB[2], FVec3(0), 1e-4);

				EXPECT_VECTOR_NEAR(VertsA[3], AllVerts[3], 1e-4);
				EXPECT_VECTOR_NEAR(VertsB[3], FVec3(0), 1e-4);

				EXPECT_EQ(TetFaces.Num(), 4);
				for (int i = 0; i < TetFaces.Num(); ++i)
				{
					ValidFace(VertsA.GetData(), TetFaces, i);
				}
			}

			//other winding
			{
				TArray<FVec3> VertsA = { AllVerts[2], AllVerts[0] };
				TArray<FVec3> VertsB = { FVec3(0), FVec3(0) };

				TEPAWorkingArray<TEPAEntry<FReal>> TetFaces;
				FVec3 TouchingNormal;
				EXPECT_TRUE(InitializeEPA(VertsA,VertsB,ASupport,EmptySupport,TetFaces,TouchingNormal));
				EXPECT_VECTOR_NEAR(VertsA[2], AllVerts[1], 1e-4);
				EXPECT_VECTOR_NEAR(VertsB[2], FVec3(0), 1e-4);

				EXPECT_VECTOR_NEAR(VertsA[3], AllVerts[3], 1e-4);
				EXPECT_VECTOR_NEAR(VertsB[3], FVec3(0), 1e-4);

				EXPECT_EQ(TetFaces.Num(), 4);
				for (int i = 0; i < TetFaces.Num(); ++i)
				{
					ValidFace(VertsA.GetData(), TetFaces, i);
				}
			}

			//touching triangle
			{
				auto ASupportNoX = [&](const FVec3& V)
				{
					FVec3 Best = AllVerts[0];
					for (const FVec3& Vert : AllVerts)
					{
						if (Vert.X > 0 || Vert.X < 0)
						{
							continue;
						}
						if (FVec3::DotProduct(Vert, V) > FVec3::DotProduct(Best, V))
						{
							Best = Vert;
						}
					}
					return Best;
				};

				TArray<FVec3> VertsA = { AllVerts[2], AllVerts[0] };
				TArray<FVec3> VertsB = { FVec3(0), FVec3(0) };

				TEPAWorkingArray<TEPAEntry<FReal>> TetFaces;
				FVec3 TouchingNormal;
				EXPECT_FALSE(InitializeEPA(VertsA,VertsB,ASupportNoX,EmptySupport, TetFaces, TouchingNormal));
				EXPECT_EQ(TouchingNormal.X,0);
			}

			//touching line
			{

				auto ASupportNoXOrZ = [&](const FVec3& V)
				{
					FVec3 Best = AllVerts[0];
					for (const FVec3& Vert : AllVerts)
					{
						if (Vert.X > 0 || Vert.X < 0 || Vert.Z > 0)
						{
							continue;
						}
						if (FVec3::DotProduct(Vert, V) > FVec3::DotProduct(Best, V))
						{
							Best = Vert;
						}
					}
					return Best;
				};

				TArray<FVec3> VertsA = { AllVerts[2], AllVerts[0] };
				TArray<FVec3> VertsB = { FVec3(0), FVec3(0) };

				TEPAWorkingArray<TEPAEntry<FReal>> TetFaces;
				FVec3 TouchingNormal;
				EXPECT_FALSE(InitializeEPA(VertsA,VertsB,ASupportNoXOrZ,EmptySupport,TetFaces,TouchingNormal));
				EXPECT_EQ(TouchingNormal.X,0);
			}
		}
	}

	void EPASimpleTest()
	{
		auto ZeroSupport = [](const auto& V) { return FVec3(0); };

		{

			//simple box hull. 0.5 depth on x, 1 depth on y, 1 depth on z. Made z non symmetric to avoid v on tet close to 0 for this case
			FVec3 HullVerts[8] = { {-0.5, -1, -1}, {2, -1, -1}, {-0.5, 1, -1}, {2, 1, -1},
									  {-0.5, -1, 2}, {2, -1, 2}, {-0.5, 1, 2}, {2, 1, 2} };

			auto SupportA = [&HullVerts](const auto& V)
			{
				auto MaxDist = TNumericLimits<FReal>::Lowest();
				auto BestVert = HullVerts[0];
				for (const auto& Vert : HullVerts)
				{
					const auto Dist = FVec3::DotProduct(V, Vert);
					if (Dist > MaxDist)
					{
						MaxDist = Dist;
						BestVert = Vert;
					}
				}
				return BestVert;
			};

			TArray<FVec3> Tetrahedron = { HullVerts[0], HullVerts[2], HullVerts[3], HullVerts[4] };
			TArray<FVec3> Zeros = { FVec3(0), FVec3(0), FVec3(0), FVec3(0) };

			FReal Penetration;
			FVec3 Dir, WitnessA, WitnessB;
			EXPECT_EQ(EPA(Tetrahedron, Zeros, SupportA, ZeroSupport, Penetration, Dir, WitnessA, WitnessB, EpaEps), EEPAResult::Ok);
			EXPECT_NEAR(Penetration, 0.5, 1e-4);
			EXPECT_NEAR(Dir[0], -1, 1e-4);
			EXPECT_NEAR(Dir[1], 0, 1e-4);
			EXPECT_NEAR(Dir[2], 0, 1e-4);
			EXPECT_NEAR(WitnessA[0], -0.5, 1e-4);
			EXPECT_NEAR(WitnessA[1], 0, 1e-4);
			EXPECT_NEAR(WitnessA[2], 0, 1e-4);
			EXPECT_NEAR(WitnessB[0], 0, 1e-4);
			EXPECT_NEAR(WitnessB[1], 0, 1e-4);
			EXPECT_NEAR(WitnessB[2], 0, 1e-4);
		}

		{
			//sphere with deep penetration to make sure we have max iterations
			TSphere<FReal,3> Sphere(FVec3(0), 10);
			int32 VertexIndex = INDEX_NONE;
			auto Support = [&Sphere,&VertexIndex](const auto& V)
			{
				return Sphere.Support(V, 0,VertexIndex);
			};

			TArray<FVec3> Tetrahedron = { 
				Support(FVec3(-1,0,0)), Support(FVec3(1,0,0)),
				Support(FVec3(0,1,0)), Support(FVec3(0,0,1))
			};
			TArray <FVec3> Zeros = { FVec3(0), FVec3(0), FVec3(0), FVec3(0) };

			FReal Penetration;
			FVec3 Dir, WitnessA, WitnessB;
			EXPECT_EQ(EPA(Tetrahedron, Zeros, Support, ZeroSupport, Penetration, Dir, WitnessA, WitnessB, EpaEps), EEPAResult::MaxIterations);
			EXPECT_GT(Penetration, 9);
			EXPECT_LE(Penetration, 10);
			EXPECT_GT(WitnessA.Size(), 9);	//don't know exact point, but should be 9 away from origin
			EXPECT_LE(WitnessA.Size(), 10);	//point should be interior to sphere
		}

		{
			//capsule with origin in middle
			FCapsule Capsule(FVec3(0, 0, 10), FVec3(0, 0, -10), 3);
			int32 VertexIndex = INDEX_NONE;
			auto Support = [&Capsule,&VertexIndex](const auto& V)
			{
				return Capsule.Support(V, 0, VertexIndex);
			};

			TArray<FVec3> Tetrahedron = { 
				Support(FVec3(-1,0,0)), Support(FVec3(1,0,0)),
				Support(FVec3(0,1,0)), Support(FVec3(0,0,1)) 
			};
			TArray<FVec3> Zeros = { FVec3(0), FVec3(0), FVec3(0), FVec3(0) };

			FReal Penetration;
			FVec3 Dir, WitnessA, WitnessB;
			EXPECT_EQ(EPA(Tetrahedron, Zeros, Support, ZeroSupport, Penetration, Dir, WitnessA, WitnessB, EpaEps), EEPAResult::MaxIterations);
			EXPECT_NEAR(Penetration, 3, 1e-1);
			EXPECT_NEAR(Dir[2], 0, 1e-1);	//don't know direction, but it should be in xy plane
			EXPECT_NEAR(WitnessA.Size(), 3, 1e-1);	//don't know exact point, but should be 3 away from origin
		}
		{
			//capsule with origin near top
			FCapsule Capsule(FVec3(0, 0, -2), FVec3(0, 0, -12), 3);
			int32 VertexIndex = INDEX_NONE;
			auto Support = [&Capsule,&VertexIndex](const auto& V)
			{
				return Capsule.Support(V, 0, VertexIndex);
			};

			TArray<FVec3> Tetrahedron = { 
				Support(FVec3(-1,0,0)), Support(FVec3(1,0,0)),
				Support(FVec3(0,1,0)), Support(FVec3(0,0,1)) 
			};
			TArray<FVec3> Zeros = { FVec3(0), FVec3(0), FVec3(0), FVec3(0) };

			FReal Penetration;
			FVec3 Dir, WitnessA, WitnessB;
			const EEPAResult Result = EPA(Tetrahedron, Zeros, Support, ZeroSupport, Penetration, Dir, WitnessA, WitnessB, EpaEps);
			EXPECT_TRUE(Result == EEPAResult::Ok);
			EXPECT_NEAR(Penetration, 1, 1e-1);
			EXPECT_NEAR(Dir[0], 0, 1e-1);
			EXPECT_NEAR(Dir[1], 0, 1e-1);
			EXPECT_NEAR(Dir[2], 1, 1e-1);
			EXPECT_NEAR(WitnessA[0], 0, 1e-1);
			EXPECT_NEAR(WitnessA[1], 0, 1e-1);
			EXPECT_NEAR(WitnessA[2], 1, 1e-1);
			EXPECT_NEAR(WitnessB[0], 0, 1e-1);
			EXPECT_NEAR(WitnessB[1], 0, 1e-1);
			EXPECT_NEAR(WitnessB[2], 0, 1e-1);
		}

		{
			//box is 1,1,1 with origin in the middle to handle cases when origin is right on tetrahedron
			FVec3 HullVerts[8] = { {-1, -1, -1}, {1, -1, -1}, {-1, 1, -1}, {1, 1, -1},
								   {-1, -1, 1}, {1, -1, 2}, {-1, 1, 1}, {1, 1, 1} };

			auto Support = [&HullVerts](const auto& V)
			{
				auto MaxDist = TNumericLimits<FReal>::Lowest();
				auto BestVert = HullVerts[0];
				for (const auto& Vert : HullVerts)
				{
					const auto Dist = FVec3::DotProduct(V, Vert);
					if (Dist > MaxDist)
					{
						MaxDist = Dist;
						BestVert = Vert;
					}
				}
				return BestVert;
			};

			TArray<FVec3> Tetrahedron = { HullVerts[0], HullVerts[2], HullVerts[3], HullVerts[4] };
			TArray<FVec3> Zeros = { FVec3(0), FVec3(0), FVec3(0), FVec3(0) };

			FReal Penetration;
			FVec3 Dir, WitnessA, WitnessB;
			EXPECT_EQ(EPA(Tetrahedron, Zeros, Support, ZeroSupport, Penetration, Dir, WitnessA, WitnessB, EpaEps), EEPAResult::Ok);
			EXPECT_FLOAT_EQ(Penetration, 1);
			EXPECT_NEAR(WitnessA.Size(), 1, 1e-1);	//don't know exact point, but should be 1 away from origin
		}

		// Tetrahedron that does not quite contain the origin (so not in penetration)
		{
			FReal eps = 0.00001f;
			FVec3 HullVerts[4] = { {-1, 0, 0}, {0 - eps, 1, -1}, {0 - eps, 0, 1}, {0 - eps, -1, -1}};

			auto Support = [&HullVerts](const auto& V)
			{
				auto MaxDist = TNumericLimits<FReal>::Lowest();
				auto BestVert = HullVerts[0];
				for (const auto& Vert : HullVerts)
				{
					const auto Dist = FVec3::DotProduct(V, Vert);
					if (Dist > MaxDist)
					{
						MaxDist = Dist;
						BestVert = Vert;
					}
				}
				return BestVert;
			};

			TArray<FVec3> Tetrahedron = { HullVerts[0], HullVerts[1], HullVerts[2], HullVerts[3] };
			TArray <FVec3> Zeros = { FVec3(0), FVec3(0), FVec3(0), FVec3(0) };

			FReal Penetration;
			FVec3 Dir, WitnessA, WitnessB;
			EXPECT_EQ(EPA(Tetrahedron, Zeros, Support, ZeroSupport, Penetration, Dir, WitnessA, WitnessB, EpaEps), EEPAResult::Ok);
			EXPECT_LT(Penetration, 0); // Negative penetration
			EXPECT_NEAR(Dir.X, 1.0f, 0.001f);
		}

		// Tetrahedron that is quite a bit away from the origin (so not in penetration)
		// This takes a slightly different code path as compared to the previous test
		{
			FReal eps = 0.1f;
			FVec3 HullVerts[4] = { {-1, 0, 0}, {0 - eps, 1, -1}, {0 - eps, 0, 1}, {0 - eps, -1, -1} };

			auto Support = [&HullVerts](const auto& V)
			{
				auto MaxDist = TNumericLimits<FReal>::Lowest();
				auto BestVert = HullVerts[0];
				for (const auto& Vert : HullVerts)
				{
					const auto Dist = FVec3::DotProduct(V, Vert);
					if (Dist > MaxDist)
					{
						MaxDist = Dist;
						BestVert = Vert;
					}
				}
				return BestVert;
			};

			TArray<FVec3> Tetrahedron = { HullVerts[0], HullVerts[1], HullVerts[2], HullVerts[3] };
			TArray<FVec3> Zeros = { FVec3(0), FVec3(0), FVec3(0), FVec3(0) };

			FReal Penetration;
			FVec3 Dir, WitnessA, WitnessB;
			EXPECT_EQ(EPA(Tetrahedron, Zeros, Support, ZeroSupport, Penetration, Dir, WitnessA, WitnessB, EpaEps), EEPAResult::Ok);
			EXPECT_LT(Penetration, 0); // Negative penetration
			EXPECT_NEAR(Dir.X, 1.0f, 0.001f);
		}
	}

	// Previously failing test cases that we would like to keep testing to prevent regression.
	GTEST_TEST(EPATests, EPARealFailures_Fixed)
	{
		{
			//get to EPA from GJKPenetration
			// Boxes that are very close to each other (Almost penetrating).
			FAABB3 Box({ -50, -50, -50 }, { 50, 50, 50 });

			const FRigidTransform3 BToATM({ -8.74146843, 4.58291769, -100.029655 }, FRotation3::FromElements(6.63562241e-05, -0.000235952888, 0.00664712908, 0.999977887));
			FVec3 ClosestA, ClosestB, Normal;
			int32 ClosestVertexIndexA, ClosestVertexIndexB;
			FReal Penetration;

			GJKPenetration<true>(Box, Box, BToATM, Penetration, ClosestA, ClosestB, Normal, ClosestVertexIndexA, ClosestVertexIndexB);
			EXPECT_NEAR(Penetration, 0.0, 0.01);
		}

		// Problem: EPA was selecting the wrong face on the second box, resulting in a large penetration depth (131cm, but the box is only 20cm thick)
		// Fixed CL: 10615422
		{
			TBox<FReal, 3> A({ -12.5000000, -1.50000000, -12.5000000 }, { 12.5000000, 1.50000000, 12.5000000 });
			TBox<FReal, 3> B({ -100.000000, -100.000000, -10.0000000 }, { 100.000000, 100.000000, 10.0000000 });
			const FRigidTransform3 BToATM({ -34.9616776, 64.0135651, -10.9833698 }, FRotation3::FromElements(-0.239406615, -0.664629698, 0.637779951, 0.306901455));

			FVec3 ClosestA, ClosestB, NormalA;
			int32 ClosestVertexIndexA, ClosestVertexIndexB;
			FReal Penetration;
			GJKPenetration(A, B, BToATM, Penetration, ClosestA, ClosestB, NormalA, ClosestVertexIndexA, ClosestVertexIndexB);
			FVec3 Normal = BToATM.InverseTransformVector(NormalA);

			// Why do we have two result depending on the precision of FReal:
			// The double result is actually the correct one ( 0.04752 ), I have checked it in a 3D modeling package and I get the same value down to the 5th decimal digit
			// The float version is certainly because of the imprecision of the quaternion and the rather large ( 200 x 200 x 20 ) object, amplifying the errors, 
			EXPECT_NEAR(Penetration, 0.04752f, 0.005f);
			EXPECT_NEAR(Normal.Z, -1.0f, 0.001f);
		}

		// Problem: EPA was selecting the wrong face on the second box, this was because LastEntry was initialized to first face, not best first face
		// Fixed CL: 10635151>
		{
			TBox<FReal, 3> A({ -12.5000000, -1.50000000, -12.5000000 }, { 12.5000000, 1.50000000, 12.5000000 });
			TBox<FReal, 3> B({ -100.000000, -100.000000, -10.0000000 }, { 100.000000, 100.000000, 10.0000000 });
			const FRigidTransform3 BToATM({ -50.4365005, 52.8003693, -35.1415100 }, FRotation3::FromElements(-0.112581111, -0.689017475, 0.657892346, 0.282414317));

			FVec3 ClosestA, ClosestB, NormalA;
			int32 ClosestVertexIndexA, ClosestVertexIndexB;
			FReal Penetration;
			GJKPenetration(A, B, BToATM, Penetration, ClosestA, ClosestB, NormalA, ClosestVertexIndexA, ClosestVertexIndexB);
			FVec3 Normal = BToATM.InverseTransformVector(NormalA);

			EXPECT_LT(Penetration, 20);
			EXPECT_NEAR(Normal.Z, -1.0f, 0.001f);
		}

		// Do not know what the expected output for this test is, but it is here because it once produced NaN in the V vector in GJKRaycast2.
		// Turn on NaN diagnostics if you want to be sure to catch the failure. (Fixed now)
		{
			TArray<TPlaneConcrete<FReal, 3>> ConvexPlanes(
				{
					{{0.000000000, -1024.00000, 2.84217094e-14}, {0.000000000, -1.00000000, 0.000000000}},
					{{0.000000000, -256.000000, 8.00000000}, {0.000000000, 0.000000000, 1.00000000}},
					{{0.000000000, -1024.00000, 8.00000000}, {0.000000000, -1.00000000, 0.000000000}},
					{{0.000000000, -256.000000, 8.00000000}, {-1.00000000, -0.000000000, 0.000000000}},
					{{768.000000, -1024.00000, 2.84217094e-14}, {-0.000000000, -6.47630076e-17, -1.00000000}},
					{{0.000000000, -1024.00000, 2.84217094e-14}, {-1.00000000, 0.000000000, 0.000000000}},
					{{0.000000000, -256.000000, 8.00000000}, {0.000000000, 0.000000000, 1.00000000}},
					{{768.000000, -1024.00000, 8.00000000}, {1.00000000, -0.000000000, 0.000000000}},
					{{768.000000, -1024.00000, 2.84217094e-14}, {6.62273836e-09, 6.62273836e-09, -1.00000000}},
					{{768.000000, -448.000000, 8.00000000}, {1.00000000, 0.000000000, 0.000000000}},
					{{0.000000000, -256.000000, -2.13162821e-14}, {0.000000000, 1.00000000, 0.000000000}},
					{{0.000000000, -256.000000, 8.00000000}, {-0.000000000, 0.000000000, 1.00000000}},
					{{768.000000, -448.000000, 8.00000000}, {0.707106829, 0.707106829, 0.000000000}},
					{{576.000000, -256.000000, 3.81469727e-06}, {0.000000000, 1.00000000, -0.000000000}},
					{{768.000000, -448.000000, 8.00000000}, {0.707106829, 0.707106829, 0.000000000}},
					{{768.000000, -448.000000, 3.81469727e-06}, {6.62273836e-09, 6.62273836e-09, -1.00000000}}
				});

			TArray<FConvex::FVec3Type> SurfaceParticles(
				{
					{0.000000000, -1024.00000, 2.84217094e-14},
					{768.000000, -1024.00000, 2.84217094e-14},
					{0.000000000, -1024.00000, 8.00000000},
					{0.000000000, -256.000000, 8.00000000},
					{768.000000, -1024.00000, 8.00000000},
					{0.000000000, -256.000000, -2.13162821e-14},
					{768.000000, -448.000000, 8.00000000},
					{768.000000, -448.000000, 3.81469727e-06},
					{576.000000, -256.000000, 3.81469727e-06},
					{576.000000, -256.000000, 8.00000000}
				});

			// Test used to pass the planes to FConvex, but this is not supported any more. Planes are derived from points.
			FConvexPtr Convex( new FConvex(SurfaceParticles, 0.0f));
			TImplicitObjectScaled<FConvex> ScaledConvex(Convex, FVec3(1.0f), 0.0f);

			TSphere<FReal, 3> Sphere(FVec3(0.0f), 34.2120171);

			const FRigidTransform3 BToATM({ 568.001648, -535.998352, 8.00000000 }, FRotation3::FromElements(0.000000000, 0.000000000, -0.707105696, 0.707107902));
			const FVec3 LocalDir(0.000000000, 0.000000000, -1.00000000);
			const FReal Length = 384.000000;
			const FReal Thickness = 0.0;
			const bool bComputeMTD = true;
			const FVec3 Offset(-536.000000, -568.000000, -8.00000000);

			FReal OutTime = -1.0f;
			FVec3 LocalPosition(-1.0f);
			FVec3 LocalNormal(-1.0f);


			bool bResult = GJKRaycast2(ScaledConvex, Sphere, BToATM, LocalDir, Length, OutTime, LocalPosition, LocalNormal, Thickness, bComputeMTD, Offset, Thickness);

		}
		
		// Sphere sweep against triangle, fails when it should hit. Raycast added as well for verification purposes.
		{
			const FTriangle Triangle({ 0.000000000, 0.000000000, 0.000000000 }, { 128.000000, 0.000000000, -114.064575 }, { 128.000000, 128.000000, 2.35327148 });
			const FTriangleRegister TriangleReg(
				MakeVectorRegisterFloat( 0.000000000f, 0.000000000f, 0.000000000f, 0.0f ), 
				MakeVectorRegisterFloat(128.000000f, 0.000000000f, -114.064575, 0.0f), 
				MakeVectorRegisterFloat(128.000000, 128.000000, 2.35327148, 0.0f));
			const TSphere<FReal, 3> Sphere({ 0.0, 0.0, 0.0 }, 4);
			const TRigidTransform<FReal, 3> Transform({ 174.592773, -161.781250, -68.0469971 }, FQuat::Identity);
			const FVec3 Dir(-0.406315684, 0.913382649, -0.0252906363);
			const FReal Length = 430.961548;
			const FReal Thickness = 0.0f;
			const bool bComputeMTD = true;

			FReal OutTime = -1.0f;
			FVec3 OutPosition;
			FVec3 OutNormal;

			bool bSweepResult = GJKRaycast2(TriangleReg, Sphere, Transform, Dir, Length, OutTime, OutPosition, OutNormal, Thickness, bComputeMTD);


			// Do a raycast w/ same inputs instead of sweep against triangle to verify sweep should be a hit.
			const FVec3 TriNormal = Triangle.GetNormal();
			const TPlane<FReal, 3> TriPlane{ Triangle[0], TriNormal };
			FVec3 RaycastPosition;
			FVec3 RaycastNormal;
			FReal Time;

			int32 DummyFaceIndex;

			bool bTriangleIntersects = false;
			if (TriPlane.Raycast(Transform.GetTranslation(), Dir, Length, Thickness, Time, RaycastPosition, RaycastNormal, DummyFaceIndex))
			{
				FVec3 IntersectionPosition = RaycastPosition;
				FVec3 IntersectionNormal = RaycastNormal;

				const FVec3 ClosestPtOnTri = FindClosestPointOnTriangle(RaycastPosition, Triangle[0], Triangle[1], Triangle[2], RaycastPosition);	//We know Position is on the triangle plane
				const FReal DistToTriangle2 = (RaycastPosition - ClosestPtOnTri).SizeSquared();
				bTriangleIntersects = DistToTriangle2 <= SMALL_NUMBER;	//raycast gave us the intersection point so sphere radius is already accounted for
			}

			EXPECT_EQ(bTriangleIntersects, bSweepResult); // uncomment to demonstrate failure.
		}

		{
			// Large scaling leads to a degenerate with GJK terminating while still 0.57 away, fallback on EPA
			// Scaled Convex vs Box
			{
				TArray<TPlaneConcrete<FReal,3>> ConvexPlanes(
					{
						{{0.000000000,-512.000000,-32.0000000},{0.000000000,0.000000000,-1.00000000}},
					{{512.000000,0.000000000,-32.0000000},{1.00000000,0.000000000,0.000000000}},
					{{512.000000,-512.000000,0.000000000},{0.000000000,-1.00000000,-0.000000000}},
					{{512.000000,0.000000000,-32.0000000},{-0.000000000,0.000000000,-1.00000000}},
					{{0.000000000,-512.000000,-32.0000000},{-1.00000000,0.000000000,0.000000000}},
					{{0.000000000,0.000000000,0.000000000},{0.000000000,1.00000000,0.000000000}},
					{{0.000000000,-512.000000,-32.0000000},{0.000000000,-1.00000000,0.000000000}},
					{{512.000000,-512.000000,0.000000000},{0.000000000,0.000000000,1.00000000}},
					{{0.000000000,0.000000000,0.000000000},{-1.00000000,-0.000000000,0.000000000}},
					{{512.000000,-512.000000,0.000000000},{1.00000000,-0.000000000,0.000000000}},
					{{512.000000,0.000000000,-32.0000000},{0.000000000,1.00000000,-0.000000000}},
					{{0.000000000,0.000000000,0.000000000},{-0.000000000,0.000000000,1.00000000}}
					});

				TArray<FConvex::FVec3Type> SurfaceParticles(
					{
						{0.000000000,-512.000000,-32.0000000},
					{512.000000,0.000000000,-32.0000000},
					{512.000000,-512.000000,-32.0000000},
					{512.000000,-512.000000,0.000000000},
					{0.000000000,0.000000000,-32.0000000},
					{0.000000000,0.000000000,0.000000000},
					{0.000000000,-512.000000,0.000000000},
					{512.000000,0.000000000,0.000000000}
					});

				// Test used to pass planes and verts to FConvex but this is not suported an more. 
				// Planes will derived from the points now, and also faces are merged (not triangles any more)
				FVec3 ConvexScale ={25,25,1};
				FConvexPtr Convex( new FConvex(SurfaceParticles, 0.0f));
				TImplicitObjectScaled<FConvex> ScaledConvex(Convex, ConvexScale,0.0f);

				TBox<FReal,3> Box({-50.0000000,-60.0000000,-30.0000000},{50.0000000,60.0000000,30.0000000});

				const TRigidTransform<FReal,3> BToATM({4404.39404,-5311.81934,44.1764526},FQuat(0.100362606,0.0230407044,-0.818859160,0.564682245),FVec3(1.0f));
				const FVec3 LocalDir ={-0.342119515,-0.920166731,-0.190387309};
				const FReal Length = 53.3335228;
				const FReal Thickness = 0.0f;
				const bool bComputeMTD = true;
				const FVec3 Offset = FVec3(-5311.83203,-4404.37891,-44.1764526);

				FReal OutTime = -1;
				FVec3 LocalPosition(-1);
				FVec3 LocalNormal(-1);

				bool bResult = GJKRaycast2(ScaledConvex,Box,BToATM,LocalDir,Length,OutTime,LocalPosition,LocalNormal,Thickness,bComputeMTD,Offset,Thickness);
			}

			// InGJKPreDist2 is barely over 1e-6, fall back on EPA
			// Triangle v Box
			{
				FTriangle Triangle(FVec3(0.000000000,0.000000000,0.000000000),FVec3(128.000000,0.000000000,35.9375000),FVec3(128.000000,128.000000,134.381042));
				FTriangleRegister TriangleReg(
					MakeVectorRegisterFloat(0.000000000f, 0.000000000f, 0.000000000f, 0.0f), 
					MakeVectorRegisterFloat(128.000000f, 0.000000000f, 35.9375000f, 0.0f), 
					MakeVectorRegisterFloat(128.000000f, 128.000000f, 134.381042f, 0.0f));
				TBox<FReal,3> Box(FVec3(-50.0000000,-60.0000000,-30.0000000),FVec3 (50.0000000,60.0000000,30.0000000));

				const TRigidTransform<FReal,3> Transform({127.898438,35.0742188,109.781067},FQuat(0.374886870,-0.0289460570,0.313643545,0.871922970),FVec3(1.0));
				//const TRigidTransform<FReal, 3> Transform({ 127.898438, 35.0742188, 109.781067 }, FQuat::Identity, FVec3(1.0));
				const FVec3 Dir ={0.801564395,0.525258720,0.285653293};
				const FReal Length = 26.7055893;
				const FReal Thickness = 0.0f;
				const bool bComputeMTD = true;

				FReal OutTime = -1;
				FVec3 LocalPosition(-1);
				FVec3 LocalNormal(-1);
				GJKRaycast2(TriangleReg,Box,Transform,Dir,Length,OutTime,LocalPosition,LocalNormal,Thickness,bComputeMTD);
			}
		}

		// Defining boat geom data outside of single test scope as it's used in multiple.
		const TArray<FConvex::FVec3Type> BoatSurfaceVertices(
			{
				{-118.965088, -100.379936, 105.818298},
				{-128.562881, 80.0933762, 107.703270},
				{-139.344116, -97.6986847, 77.2006836},
				{-150.005661, -100.080147, 51.9458313},
				{-139.707321, 97.7620926, 68.6699982},
				{-162.950150, 15.7422667, -12.4111595},
				{-133.124146, -77.7715225, 16.0983276},
				{-162.950150, -18.5639591, -8.11873245},
				{80.0627518, -94.3176956, 23.9974003},
				{144.317383, -64.4015045, 12.9772358},
				{-134.336945, 83.6453476, 19.9467926},
				{-28.9211884, 95.5794601, 24.0703869},
				{-29.2745361, 115.021286, 39.6718407},
				{270.352173, -9.99338818, 13.3286972},
				{168.206909, -9.18884468, 4.79613113},
				{152.132553, 26.3735561, 5.07503510},
				{-96.0630951, -5.13696861, -10.5715485},
				{-34.2370224, 118.708824, 51.9164314},
				{-115.816895, 100.536232, 105.218788},
				{190.384033, 113.884377, 39.8578377},
				{75.6613998, 116.777252, 48.2003098},
				{75.6914368, 116.705940, 56.3343391},
				{223.461243, 34.3406334, 119.657486},
				{154.527252, 7.14775467, 133.457825},
				{224.732254, -33.3490448, 120.477432},
				{181.900131, -70.9029160, 125.913544},
				{190.739014, 114.494293, 68.6749573},
				{-124.285568, 84.3786621, 111.995697},
				{-55.6235504, 105.829025, 107.703270},
				{144.056519, 88.6111145, 111.072426},
				{293.764893, 97.7141037, 68.6531982},
				{357.075989, 27.2315865, 88.9283295},
				{377.644470, 48.4299507, 68.7059937},
				{299.537781, 67.8833160, 92.9634094},
				{217.357620, 91.4785843, 96.6879578},
				{277.666443, 91.4017410, 36.2159767},
				{412.009827, 15.7422667, 69.0567322},
				{160.430008, 34.6351395, 128.190872},
				{251.469971, 54.2859497, 20.7005157},
				{179.907928, 69.9437637, 16.8336792},
				{132.472702, 94.3125381, 24.5205002},
				{373.345215, -14.2786741, 90.5335693},
				{104.055634, -116.337784, 64.5478134},
				{206.939423, -112.547020, 39.8371887},
				{215.453522, -112.922203, 68.6651306},
				{235.525650, -88.4538803, 28.6447029},
				{175.841675, -40.6552811, 9.08371735},
				{316.693298, -74.9164505, 39.9856682},
				{281.028259, -96.0662766, 39.8370399},
				{353.332031, -67.8981171, 68.7482452},
				{269.801300, -105.132248, 68.7129745},
				{257.381836, -76.0536499, 110.256386},
				{-47.0126572, -104.365433, 107.688568},
				{377.622498, 15.7422667, 39.0685501},
				{401.314575, -21.9763966, 64.5559235},
				{407.676208, -14.2786741, 73.3785629},
				{312.919617, -33.3405228, 28.3129959},
				{334.736877, 11.4330406, 26.2059746},
				{309.059326, 80.6674042, 39.9196320},
				{-142.015427, -9.19016743, -11.2502632},
				{-27.1481628, -111.977615, 35.8436165},
				{-23.9894104, -116.828667, 43.9551315}
			});

		const TArray<TPlaneConcrete<FReal, 3>> BoatConvexPlanes(
		{
				{{-118.965088, -100.379936, 105.818298},{-0.815755606, -0.0494019315, 0.576283097}},
				{{-128.562881, 80.0933762, 107.703270},{-0.920841396, -0.0110327024, 0.389781147}},
				{{-150.005661, -100.080147, 51.9458313},{-0.519791245, -0.801730216, 0.295035094}},
				{{-128.562881, 80.0933762, 107.703270},{-0.958117247, 0.0257631000, 0.285215139}},
				{{-139.707321, 97.7620926, 68.6699982},{-0.968365312, 0.0294600632, 0.247791693}},
				{{-133.124146, -77.7715225, 16.0983276},{0.00511667505, -0.376362234, -0.926458478}},
				{{-162.950150, -18.5639591, -8.11873245},{0.00966687687, -0.363861620, -0.931402802}},
				{{-162.950150, 15.7422667, -12.4111595},{-0.0140216080, 0.434951097, -0.900344849}},
				{{-134.336945, 83.6453476, 19.9467926},{-0.0402486622, 0.624916077, -0.779653668}},
				{{270.352173, -9.99338818, 13.3286972},{0.0835136175, 0.0455556102, -0.995464802}},
				{{168.206909, -9.18884468, 4.79613113},{0.0585431270, 0.0342863351, -0.997695863}},
				{{-96.0630951, -5.13696861, -10.5715485},{0.0525218807, 0.0805560499, -0.995365262}},
				{{-29.2745361, 115.021286, 39.6718407},{-0.204991043, 0.911147118, -0.357476622}},
				{{-134.336945, 83.6453476, 19.9467926},{-0.230919138, 0.927439809, -0.294162780}},
				{{-139.707321, 97.7620926, 68.6699982},{-0.187245682, 0.981143415, 0.0479236171}},
				{{190.384033, 113.884377, 39.8578377},{0.0107297711, 0.981200278, -0.192693755}},
				{{-34.2370224, 118.708824, 51.9164314},{0.0178666674, 0.999802530, 0.00869940408}},
				{{190.384033, 113.884377, 39.8578377},{0.00520140817, 0.958088040, -0.286426455}},
				{{223.461243, 34.3406334, 119.657486},{0.190408617, 0.0154655315, 0.981583059}},
				{{154.527252, 7.14775467, 133.457825},{0.159681797, -0.0393412262, 0.986384273}},
				{{75.6914368, 116.705940, 56.3343391},{0.0176137071, 0.999732912, 0.0149621498}},
				{{-115.816895, 100.536232, 105.218788},{-0.715656042, 0.553675890, 0.425769299}},
				{{-139.707321, 97.7620926, 68.6699982},{-0.817192018, 0.400413275, 0.414567769}},
				{{-128.562881, 80.0933762, 107.703270},{-0.685338736, -0.0440392531, 0.726891577}},
				{{-118.965088, -100.379936, 105.818298},{-0.0865496397, -0.0357803851, 0.995604813}},
				{{-55.6235504, 105.829025, 107.703270},{-0.0741617680, 0.418523729, 0.905172884}},
				{{144.056519, 88.6111145, 111.072426},{0.0611657463, 0.820554078, 0.568286657}},
				{{190.739014, 114.494293, 68.6749573},{0.00145421748, 0.974245131, 0.225486547}},
				{{-34.2370224, 118.708824, 51.9164314},{-0.0937685072, 0.977354348, 0.189699650}},
				{{293.764893, 97.7141037, 68.6531982},{0.160708517, 0.986737013, -0.0228640344}},
				{{190.384033, 113.884377, 39.8578377},{0.0236439183, 0.999490380, -0.0214455500}},
				{{75.6613998, 116.777252, 48.2003098},{0.0182868484, 0.999794960, 0.00869778637}},
				{{357.075989, 27.2315865, 88.9283295},{0.363979012, 0.433337778, 0.824462056}},
				{{377.644470, 48.4299507, 68.7059937},{0.369141936, 0.628997087, 0.684176087}},
				{{293.764893, 97.7141037, 68.6531982},{0.217538610, 0.641553879, 0.735585213}},
				{{217.357620, 91.4785843, 96.6879578},{0.159607396, 0.414469659, 0.895957828}},
				{{144.056519, 88.6111145, 111.072426},{0.156273633, 0.373305798, 0.914451420}},
				{{223.461243, 34.3406334, 119.657486},{0.229725912, 0.231315732, 0.945367098}},
				{{293.764893, 97.7141037, 68.6531982},{0.134402916, 0.824485123, 0.549690902}},
				{{190.739014, 114.494293, 68.6749573},{0.0830841213, 0.807267189, 0.584308803}},
				{{277.666443, 91.4017410, 36.2159767},{0.226969868, 0.928666651, -0.293364942}},
				{{357.075989, 27.2315865, 88.9283295},{0.384961128, 0.413572103, 0.825083613}},
				{{144.056519, 88.6111145, 111.072426},{0.0102420887, 0.305115640, 0.952260256}},
				{{-55.6235504, 105.829025, 107.703270},{-0.0136526823, 0.238040313, 0.971159339}},
				{{-124.285568, 84.3786621, 111.995697},{-0.0221296977, 0.192725569, 0.981003106}},
				{{154.527252, 7.14775467, 133.457825},{0.133184910, 0.158850595, 0.978278220}},
				{{223.461243, 34.3406334, 119.657486},{0.127949536, 0.334882438, 0.933532357}},
				{{270.352173, -9.99338818, 13.3286972},{0.113541767, 0.146057680, -0.982738674}},
				{{152.132553, 26.3735561, 5.07503510},{0.0967332050, 0.201390326, -0.974722803}},
				{{179.907928, 69.9437637, 16.8336792},{0.118871428, 0.310366035, -0.943155587}},
				{{152.132553, 26.3735561, 5.07503510},{0.0460564680, 0.232807800, -0.971431613}},
				{{-28.9211884, 95.5794601, 24.0703869},{0.00696368096, 0.603951454, -0.796990752}},
				{{190.384033, 113.884377, 39.8578377},{0.0805319995, 0.456198364, -0.886226594}},
				{{277.666443, 91.4017410, 36.2159767},{0.0808695257, 0.439591616, -0.894549847}},
				{{179.907928, 69.9437637, 16.8336792},{0.0254166014, 0.345391214, -0.938114524}},
				{{-162.950150, 15.7422667, -12.4111595},{0.00574636925, 0.407610655, -0.913137734}},
				{{-28.9211884, 95.5794601, 24.0703869},{0.00389994099, 0.625906646, -0.779888213}},
				{{412.009827, 15.7422667, 69.0567322},{0.367606252, 0.179364890, 0.912520528}},
				{{357.075989, 27.2315865, 88.9283295},{0.228729501, 0.126970738, 0.965174258}},
				{{223.461243, 34.3406334, 119.657486},{0.195577502, 0.0155502548, 0.980564892}},
				{{104.055634, -116.337784, 64.5478134},{0.0314623937, -0.999256194, -0.0222970415}},
				{{206.939423, -112.547020, 39.8371887},{0.0439339988, -0.463305235, -0.885109067}},
				{{80.0627518, -94.3176956, 23.9974003},{0.0430704951, -0.425489426, -0.903937817}},
				{{144.317383, -64.4015045, 12.9772358},{0.0910515115, -0.277681828, -0.956348479}},
				{{175.841675, -40.6552811, 9.08371735},{0.121729963, -0.241943270, -0.962624073}},
				{{270.352173, -9.99338818, 13.3286972},{0.176452607, -0.263455838, -0.948396325}},
				{{316.693298, -74.9164505, 39.9856682},{0.180675939, -0.298087925, -0.937283218}},
				{{281.028259, -96.0662766, 39.8370399},{0.117892988, -0.529992998, -0.839766979}},
				{{316.693298, -74.9164505, 39.9856682},{0.467810631, -0.786031187, -0.404114038}},
				{{353.332031, -67.8981171, 68.7482452},{0.403864205, -0.905904770, -0.127398163}},
				{{269.801300, -105.132248, 68.7129745},{0.211974993, -0.952931166, -0.216769129}},
				{{353.332031, -67.8981171, 68.7482452},{0.323771894, -0.726920843, 0.605605364}},
				{{257.381836, -76.0536499, 110.256386},{0.113805972, -0.797622085, 0.592323542}},
				{{215.453522, -112.922203, 68.6651306},{0.141719818, -0.988393307, -0.0547193065}},
				{{257.381836, -76.0536499, 110.256386},{0.0748343095, -0.782468021, 0.618177652}},
				{{181.900131, -70.9029160, 125.913544},{-0.0393289365, -0.256881803, 0.965642214}},
				{{104.055634, -116.337784, 64.5478134},{0.0170975495, -0.946409166, 0.322517306}},
				{{215.453522, -112.922203, 68.6651306},{0.0658586845, -0.785601914, 0.615217268}},
				{{353.332031, -67.8981171, 68.7482452},{0.384226114, -0.466992885, 0.796421945}},
				{{373.345215, -14.2786741, 90.5335693},{0.206286892, -0.0757761970, 0.975552976}},
				{{224.732254, -33.3490448, 120.477432},{0.196953446, -0.0832281485, 0.976873755}},
				{{316.693298, -74.9164505, 39.9856682},{0.434601337, -0.300671607, -0.848951280}},
				{{377.622498, 15.7422667, 39.0685501},{0.654218376, -0.0959888026, -0.750189602}},
				{{412.009827, 15.7422667, 69.0567322},{0.867719710, -0.191302225, -0.458765566}},
				{{407.676208, -14.2786741, 73.3785629},{0.691795230, -0.711691737, 0.122124225}},
				{{353.332031, -67.8981171, 68.7482452},{0.551859438, -0.626838267, -0.550022721}},
				{{412.009827, 15.7422667, 69.0567322},{0.446075022, 0.0641205981, 0.892695665}},
				{{373.345215, -14.2786741, 90.5335693},{0.394906074, -0.468489796, 0.790295124}},
				{{377.622498, 15.7422667, 39.0685501},{0.325547844, -0.228074327, -0.917605937}},
				{{316.693298, -74.9164505, 39.9856682},{0.197628111, -0.248304784, -0.948307872}},
				{{270.352173, -9.99338818, 13.3286972},{0.144101545, 0.154426888, -0.977438986}},
				{{251.469971, 54.2859497, 20.7005157},{0.195229396, 0.257776380, -0.946275234}},
				{{377.622498, 15.7422667, 39.0685501},{0.299516141, -0.189948440, -0.934991837}},
				{{312.919617, -33.3405228, 28.3129959},{0.245874554, -0.164760798, -0.955196083}},
				{{277.666443, 91.4017410, 36.2159767},{0.339942038, 0.877070487, -0.339391768}},
				{{293.764893, 97.7141037, 68.6531982},{0.492510736, 0.837980986, -0.234991312}},
				{{377.644470, 48.4299507, 68.7059937},{0.530996740, 0.568982124, -0.627934515}},
				{{377.622498, 15.7422667, 39.0685501},{0.250460476, 0.276656479, -0.927755713}},
				{{334.736877, 11.4330406, 26.2059746},{0.200788274, 0.261470705, -0.944095910}},
				{{377.644470, 48.4299507, 68.7059937},{0.542767346, 0.563946247, -0.622389138}},
				{{270.352173, -9.99338818, 13.3286972},{0.0817910284, -0.115049526, -0.989986837}},
				{{175.841675, -40.6552811, 9.08371735},{0.0557664521, -0.121505104, -0.991023004}},
				{{144.317383, -64.4015045, 12.9772358},{0.0422874913, -0.216078743, -0.975459754}},
				{{-162.950150, -18.5639591, -8.11873245},{-0.0924396142, -0.123621307, -0.988014519}},
				{{-162.950150, 15.7422667, -12.4111595},{0.0175636765, -0.0317834914, -0.999340415}},
				{{-96.0630951, -5.13696861, -10.5715485},{0.0355855115, -0.241038486, -0.969862998}},
				{{144.317383, -64.4015045, 12.9772358},{0.0132575780, -0.343341976, -0.939116895}},
				{{-133.124146, -77.7715225, 16.0983276},{-0.579293728, -0.541147888, -0.609571695}},
				{{-150.005661, -100.080147, 51.9458313},{-0.967441142, 0.0314226188, 0.251137793}},
				{{-133.124146, -77.7715225, 16.0983276},{-0.00871447474, -0.520026803, -0.854105532}},
				{{80.0627518, -94.3176956, 23.9974003},{0.0120858839, -0.606598854, -0.794916213}},
				{{104.055634, -116.337784, 64.5478134},{-0.0256504230, -0.982792795, 0.182921574}},
				{{-47.0126572, -104.365433, 107.688568},{-0.0589226782, -0.983500481, 0.171040595}},
				{{-118.965088, -100.379936, 105.818298},{-0.127220407, -0.989554763, 0.0677959770}},
				{{-150.005661, -100.080147, 51.9458313},{-0.145546019, -0.873032987, -0.465434998}},
				{{-27.1481628, -111.977615, 35.8436165},{0.00670416094, -0.857060790, -0.515171707}},
				{{206.939423, -112.547020, 39.8371887},{0.0170129854, -0.996484399, -0.0820325986}},
				{{-150.005661, -100.080147, 51.9458313},{-0.153074399, -0.805065334, -0.573095083}},
				{{154.527252, 7.14775467, 133.457825},{-0.0549643822, -0.115145117, 0.991826892}},
				{{-134.336945, 83.6453476, 19.9467926},{-0.852820277, 0.468897045, -0.229854882}},
			});


		// Boat vs ground in athena empty, barely in contact
		// Normal ideally would point down, but returned normal pointed up before it was fixed
		{
			TArray<TPlaneConcrete<FReal, 3>> GroundConvexPlanes(
				{
					{{0.000000000,-512.000000,-32.0000000},{0.000000000,0.000000000,-1.00000000}},
					{{512.000000,0.000000000,-32.0000000},{1.00000000,0.000000000,0.000000000}},
					{{512.000000,-512.000000,0.000000000},{0.000000000,-1.00000000,-0.000000000}},
					{{512.000000,0.000000000,-32.0000000},{-0.000000000,0.000000000,-1.00000000}},
					{{0.000000000,-512.000000,-32.0000000},{-1.00000000,0.000000000,0.000000000}},
					{{0.000000000,0.000000000,0.000000000},{0.000000000,1.00000000,0.000000000}},
					{{0.000000000,-512.000000,-32.0000000},{0.000000000,-1.00000000,0.000000000}},
					{{512.000000,-512.000000,0.000000000},{0.000000000,0.000000000,1.00000000}},
					{{0.000000000,0.000000000,0.000000000},{-1.00000000,-0.000000000,0.000000000}},
					{{512.000000,-512.000000,0.000000000},{1.00000000,-0.000000000,0.000000000}},
					{{512.000000,0.000000000,-32.0000000},{0.000000000,1.00000000,-0.000000000}},
					{{0.000000000,0.000000000,0.000000000},{-0.000000000,0.000000000,1.00000000}}
				});

			TArray<FConvex::FVec3Type> GroundSurfaceParticles(
				{
					{0.000000000,-512.000000,-32.0000000},
					{512.000000,0.000000000,-32.0000000},
					{512.000000,-512.000000,-32.0000000},
					{512.000000,-512.000000,0.000000000},
					{0.000000000,0.000000000,-32.0000000},
					{0.000000000,0.000000000,0.000000000},
					{0.000000000,-512.000000,0.000000000},
					{512.000000,0.000000000,0.000000000}
				});



			// Test used to pass planes and verts to FConvex but this is not suported an more. 
			// Planes will derived from the points now, and also faces are merged (not triangles any more)
			FVec3 GroundConvexScale = { 25,25,1 };
			FConvexPtr GroundConvex( new FConvex(GroundSurfaceParticles, 0.0f));
			TImplicitObjectScaled<FConvex> ScaledGroundConvex(GroundConvex, GroundConvexScale, 0.0f);


			// Test used to pass planes and verts to FConvex but this is not suported an more. 
			// Planes will derived from the points now, and also faces are merged (not triangles any more)
			FConvex BoatConvex = FConvex(BoatSurfaceVertices, 0.0f);


			TRigidTransform<FReal, 3> BoatTransform(FVec3(-5421.507324, 2335.360840, 6.972876), FQuat(-0.016646, -0.008459, 0.915564, -0.401738), FVec3(1.0f));
			TRigidTransform<FReal, 3> GroundTransform(FVec3(0, 0, 0), FQuat(0.000000, 0.000000, -1.000000, 0.000001), FVec3(1.0f));

			const TRigidTransform<FReal, 3> BToATM = GroundTransform.GetRelativeTransform(BoatTransform);

			FReal Penetration;
			FVec3 ClosestA, ClosestB, Normal;
			int32 ClosestVertexIndexA, ClosestVertexIndexB;

			auto result = GJKPenetration<true>(BoatConvex, ScaledGroundConvex, BToATM, Penetration, ClosestA, ClosestB, Normal, ClosestVertexIndexA, ClosestVertexIndexB, (FReal)0., (FReal)0., FVec3(1, 0, 0));

			FVec3 WorldLocation = BoatTransform.TransformPosition(ClosestA);
			FVec3 WorldNormal = BoatTransform.TransformVectorNoScale(Normal);
			EXPECT_LT(Normal.Z, -0.9f); // Should point roughly down
		} // End of Boat test

		// Boat vs rock wall at carl POI behind waterfall w/ water level 0 in season 13
		// GJK was not progressing and reporting larger distance, when dist was actually 0, and was making nan in normal.
		{
			// Triangle
			const FVec3 A = { -10934.1797, -11431.4863, -5661.06982 };
			const FVec3 B = { -10025.8525, -11155.4160, -6213.55322 };
			const FVec3 C = { -10938.6836, -11213.3320, -6213.55322 };
			const FTriangle Triangle(A, B, C);
			FVec3 ExpectedNormal = FVec3::CrossProduct(Triangle[1] - Triangle[0], Triangle[2] - Triangle[0]);
			ExpectedNormal.Normalize();

			// Test used to pass planes and verts to FConvex but this is not suported an more. 
			// Planes will derived from the points now, and also faces are merged (not triangles any more)
			FConvex BoatConvex = FConvex(BoatSurfaceVertices, 0.0f);

			TRigidTransform<FReal, 3> QueryTM(FVec3(-10831.1875, -11206.3750, -6140.38135), FQuat(-0.524916053, -0.0370868668, -0.126528814, 0.840879321), FVec3(1.0f));

			FReal Penetration;
			FVec3 ClosestA, ClosestB, Normal;
			int32 ClosestVertexIndexA, ClosestVertexIndexB;

			auto result = GJKPenetration<true, FReal>(Triangle, BoatConvex, QueryTM, Penetration, ClosestA, ClosestB, Normal, ClosestVertexIndexA, ClosestVertexIndexB);

			// Confirm normal is valid and close to expected normal.
			float dot = FVec3::DotProduct(ExpectedNormal, Normal);
			EXPECT_NEAR(dot, 1, 0.0001f);
		} // End of Boat test

	}

	// Currently broken EPA edge cases
	// A box above a triangle, almost exactly parallel and touching.
	// EPA fails due to numerical error and returns an very bad contact.
	// We hit this condition in EPA: if (UpperBound <= UpperBoundTolerance)
	// but have previously rejected all of the actual closest faces
	// because of numerical error.
	GTEST_TEST(EPATests, DISABLED_EPARealFailures_TouchingBoxTriangle)
	{
		{
			FImplicitBox3 Box = FImplicitBox3(
				FVec3(-50.000000000000000, -50.000000000000000, -15.000000000000000),
				FVec3(50.000000000000000, 50.000000000000000, 15.000000000000000)
			);

			FTriangle Triangle = FTriangle(
				FVec3(94.478362706670822, -120.65494588586357, -14.999999386949069),
				FVec3(89.056288683336533, 179.29605196669289, -15.000000556768336),
				FVec3(-210.89470916921991, 173.87397794335860, -15.000000537575422)
			);

			const TGJKShape<FImplicitBox3> GJKConvex(Box);
			const TGJKShape<FTriangle> GJKTriangle(Triangle);

			const FReal GJKEpsilon = 1.e-6;
			const FReal EPAEpsilon = 1.e-6;
			FReal UnusedMaxMarginDelta = FReal(0);
			int32 ConvexVertexIndex = INDEX_NONE;
			int32 TriangleVertexIndex = INDEX_NONE;
			FReal Penetration;
			FVec3 ConvexClosest, TriangleClosest, ConvexNormal;
			FVec3 InitialGJKDir = FVec3(1, 0, 0);

			const bool bHaveContact = GJKPenetrationSameSpace(
				GJKConvex,
				GJKTriangle,
				Penetration,
				ConvexClosest,
				TriangleClosest,
				ConvexNormal,
				ConvexVertexIndex,
				TriangleVertexIndex,
				UnusedMaxMarginDelta,
				InitialGJKDir,
				GJKEpsilon, EPAEpsilon);

			EXPECT_TRUE(bHaveContact);

			// Should be touching
			EXPECT_NEAR(Penetration, 0, UE_KINDA_SMALL_NUMBER);

			// Normal should point directly down
			EXPECT_NEAR(ConvexNormal.Z, -1, UE_KINDA_SMALL_NUMBER);

			// Contact should be on bottom of box
			EXPECT_NEAR(ConvexClosest.Z, -15.0, UE_KINDA_SMALL_NUMBER);
		}
	}

	//
	// Two boxes, one large and flat, with another smaller box on top.
	// All normals should be either Up or Down depending on which body is first (in this case, they are down)
	// Problem:
	//		Occasionally we get a non-vertical normal from EPA
	// This test reproduces an in-game example. 
	// NOTE: These are shapes in a skeletal mesh, hence the rotation (mesh is on its side so its Y is "up")
	//
	GTEST_TEST(EPATests, EPARealFailures_BoxBox)
	{
		TBox<FReal, 3> A(FVec3(-250.000000, 250.000000, -750.000000), FVec3(1250.00000, 350.000000, 750.000000));
		TBox<FReal, 3> B(FVec3(50.0000000, -50.0000000, -50.0000000), FVec3(150.000000, 50.0000000, 50.0000000));
		FRigidTransform3 ATM = FRigidTransform3(FVec3(0.000000000, 0.000000000, 0.000000000), FRotation3::FromElements(-0.707106709, 0.000000000, 0.000000000, 0.707106829));
		FRigidTransform3 BTM = FRigidTransform3(FVec3(804.534912, 17.9698277, -199.983994), FRotation3::FromElements(0.706765056, -2.78512689e-05, 0.000485646888, -0.707448125));
		FVec3 InitialDir = FVec3(1.00000000, 0.000000000, 0.000000000);

		const FRigidTransform3 BToATM = BTM.GetRelativeTransform(ATM);
		FReal Penetration;
		FVec3 ClosestA, ClosestB, NormalA;
		int32 ClosestVertexIndexA, ClosestVertexIndexB;
		GJKPenetration<true>(A, B, BToATM, Penetration, ClosestA, ClosestB, NormalA, ClosestVertexIndexA, ClosestVertexIndexB, (FReal)0., (FReal)0., InitialDir);

		FVec3 Location = ATM.TransformPosition(ClosestA);
		FVec3 Normal = -ATM.TransformVectorNoScale(NormalA);
		FReal Phi = -Penetration;

		EXPECT_GT(FMath::Abs(Normal.Z), 0.8f);
	}

	GTEST_TEST(EPATests, EPA_EdgeCases)
	{
		// Two boxes exactly touching each other (This was a failing case that was fixed)
		{
			TBox<FReal, 3> A(FVec3(0.0f, 0.0f, 0.0f), FVec3(100.0f, 100.0f, 100.0f));
			TBox<FReal, 3> B(FVec3(100.0f, 0.0f, 0.0f), FVec3(200.0f, 100.0f, 100.0f));
			FRigidTransform3 ATM = FRigidTransform3(FVec3(0.0f, 0.0f, 0.0f), FRotation3::FromElements(0.0f, 0.0f, 0.0f, 1.0f));
			FRigidTransform3 BTM = FRigidTransform3(FVec3(0.0f, 0.0f, 0.0f), FRotation3::FromElements(0.0f, 0.0f, 0.0f, 1.0f));
			FVec3 InitialDir = FVec3(1.0f, 0.0f, 0.0f);

			const FRigidTransform3 BToATM = BTM.GetRelativeTransform(ATM);
			FReal Penetration;
			FVec3 ClosestA, ClosestB, NormalA;
			int32 ClosestVertexIndexA, ClosestVertexIndexB;
			GJKPenetration<true>(A, B, BToATM, Penetration, ClosestA, ClosestB, NormalA, ClosestVertexIndexA, ClosestVertexIndexB, (FReal)0., (FReal)0., InitialDir);

			FVec3 Location = ATM.TransformPosition(ClosestA);  // These transforms are not really necessary since they are identity
			FVec3 Normal = ATM.TransformVectorNoScale(NormalA);
			FReal Phi = -Penetration;

			EXPECT_NEAR(Normal.X, 1.0f, 1e-3f);
			EXPECT_NEAR(Normal.Y, 0.0f, 1e-3f);
			EXPECT_NEAR(Normal.Z, 0.0f, 1e-3f);
		}

		// Two boxes almost touching each other (Separation is small enough for GJK to fail and fall back to EPA)
		// (This was a failing case that was fixed)
		{
			TBox<FReal, 3> A(FVec3(0.0f, 0.0f, 0.0f), FVec3(100.0f, 100.0f, 100.0f));
			TBox<FReal, 3> B(FVec3(100.0f + 0.00001f, 0.0f, 0.0f), FVec3(200.0f, 100.0f, 100.0f));
			FRigidTransform3 ATM = FRigidTransform3(FVec3(0.0f, 0.0f, 0.0f), FRotation3::FromElements(0.0f, 0.0f, 0.0f, 1.0f));
			FRigidTransform3 BTM = FRigidTransform3(FVec3(0.0f, 0.0f, 0.0f), FRotation3::FromElements(0.0f, 0.0f, 0.0f, 1.0f));
			FVec3 InitialDir = FVec3(1.0f, 0.0f, 0.0f);

			const FRigidTransform3 BToATM = BTM.GetRelativeTransform(ATM);
			FReal Penetration;
			FVec3 ClosestA, ClosestB, NormalA;
			int32 ClosestVertexIndexA, ClosestVertexIndexB;
			GJKPenetration<true>(A, B, BToATM, Penetration, ClosestA, ClosestB, NormalA, ClosestVertexIndexA, ClosestVertexIndexB, (FReal)0., (FReal)0., InitialDir);

			FVec3 Location = ATM.TransformPosition(ClosestA);  // These transforms are not really necessary since they are identity
			FVec3 Normal = ATM.TransformVectorNoScale(NormalA);
			FReal Phi = -Penetration;

			EXPECT_NEAR(Normal.X, 1.0f, 1e-3f);
			EXPECT_NEAR(Normal.Y, 0.0f, 1e-3f);
			EXPECT_NEAR(Normal.Z, 0.0f, 1e-3f);
		}

	}

	//
	// Performs the same initially overlapping sweep twice, with slightly different rotations, gives different normals.
	// this is convex box slightly penetrating surface of triangle mesh. Trimesh normals point up.
	//
	GTEST_TEST(EPATests, EPARealFailures_ConvexTrimeshRotationalDifferencesBreakNormal)
	{
		using namespace Chaos;
		TArray<FConvex::FVec3Type> ConvexBoxSurfaceParticles(
			{
			{50.0999985, -50.1124992, -50.1250000},
			{-50.0999985, 50.1250000, -50.1250000},
			{50.0999985, 50.1250000, -50.0999985},
			{50.0999985, 50.1250000, 50.1250000},
			{-50.0999985, -50.1124992, -50.0999985},
			{-50.0999985, -50.1124992, 50.1250000},
			{50.0999985, -50.1124992, 50.1250000},
			{-50.0999985, 50.1250000, 50.1250000},
			});

		FConvex ConvexBox(MoveTemp(ConvexBoxSurfaceParticles), 0.0f);

		FTriangleMeshImplicitObject::ParticlesType TrimeshParticles(
			{
				{50.0000000, 50.0000000, -8.04061356e-15},
				{50.0000000, -50.0000000, 8.04061356e-15},
				{-50.0000000, 50.0000000, -8.04061356e-15},
				{-50.0000000, -50.0000000, 8.04061356e-15}
			});

		TArray<TVec3<int32>> Indices;
		Indices.Emplace(1, 0, 2);
		Indices.Emplace(1, 2, 3);

		TArray<uint16> Materials;
		Materials.Emplace(0);
		Materials.Emplace(0);
		FTriangleMeshImplicitObjectPtr TriangleMesh( new FTriangleMeshImplicitObject(MoveTemp(TrimeshParticles), MoveTemp(Indices), MoveTemp(Materials)));
		TImplicitObjectScaled<FTriangleMeshImplicitObject> ScaledTriangleMesh = TImplicitObjectScaled<FTriangleMeshImplicitObject>(TriangleMesh, FVec3(11.5, 11.5, 11.5));

		FQuat Rotation0(0.00488796039, 0.00569311855, -0.000786740216, 0.999971569);
		FQuat Rotation1(0.0117356628, -0.0108017093, -0.000888462295, 0.999872327);

		FVec3 Translation(309.365723, -69.4132690, 51.2289352);

		TRigidTransform<FReal, 3> Transform0(Translation, Rotation0);
		TRigidTransform<FReal, 3> Transform1(Translation, Rotation1);

		FVec3 Dir(-0.00339674903, 5.76980747e-05, -0.999994159);
		FReal Length = 1.83530724;

		FReal OutTime = -1;
		FVec3 Normal(0.0f);
		FVec3 Position(0.0f);
		int32 FaceIndex = -1;
		FVec3 FaceNormal(0.0);
		bool bResult = ScaledTriangleMesh.LowLevelSweepGeom(ConvexBox, Transform0, Dir, Length, OutTime, Position, Normal, FaceIndex, FaceNormal, 0.0f, true);

		bResult = ScaledTriangleMesh.LowLevelSweepGeom(ConvexBox, Transform1, Dir, Length, OutTime, Position, Normal, FaceIndex, FaceNormal, 0.0f, true);
			
		// Observe that normals are in opposite direction, while rotations are very similar.

	}

	//
	// Player can clip through RockWall trimesh, this repros a failure, MTD seems wrong.
	// This is failing GJKRaycast2 call.
	// Fixed: (11457046) ClosestB computation was transformed wrong messing up normal.
	//
	GTEST_TEST(EPATests, EPARealFailures_CapsuleVsTrimeshRockWallWrongNormalGJKRaycast2)
	{
		using namespace Chaos;
		// Triangle w/ world scale
		FTriangle Triangle({
			{-306.119476, 1674.38647, 117.138489},
			{-491.015747, 1526.35803, 116.067123},
			{-91.0660172, 839.028320, 118.413063}
			});

		FTriangleRegister TriangleReg({
			MakeVectorRegisterFloat(-306.119476f, 1674.38647f, 117.138489f, 0.0f),
			MakeVectorRegisterFloat(-491.015747f, 1526.35803f, 116.067123f, 0.0f),
			MakeVectorRegisterFloat(-91.0660172f, 839.028320f, 118.413063f, 0.0f)
			});


		FVec3 ExpectedNormal = FVec3::CrossProduct(Triangle[1] - Triangle[0], Triangle[2] - Triangle[0]);
		ExpectedNormal.Normalize();

		TRigidTransform<FReal, 3> StartTM(FVec3(-344.031799, 1210.37158, 134.252747), FQuat(-0.255716801, -0.714108050, 0.0788889676, -0.646866322), FVec3(1));

		// Wrapping in 1,1,1 scale is unnecessary, but this is technically what is happening when sweeping against scaled trimesh.
		FCapsulePtr Capsule( new FCapsule(FVec3(0, 0, -33), FVec3(0, 0, 33), 42));
		TImplicitObjectScaled<FCapsule> ScaledCapsule = TImplicitObjectScaled<FCapsule>(Capsule, FVec3(1));


		const FVec3 Dir(-0.102473199, 0.130887285, -0.986087084);
		const FReal LengthScale = 9.31486130;
		const FReal  CurrentLength = 2.14465737;
		const FReal Length = LengthScale * CurrentLength;
		const bool bComputeMTD = true;
		const FReal Thickness = 0;

		FReal OutTime = -1.0f;
		FVec3 Normal(0.0f);
		FVec3 Position(0.0f);
		int32 FaceIndex = -1;

		// This is local to trimesh, world scale.
		bool bResult = GJKRaycast2<FReal>(TriangleReg, ScaledCapsule, StartTM, Dir, Length, OutTime, Position, Normal, Thickness, bComputeMTD);

		// Compare results against GJKPenetration, sweep is initial overlap, so this should be the same.
		FVec3 Normal2, ClosestA, ClosestB;
		int32 ClosestVertexIndexA, ClosestVertexIndexB;
		FReal OutTime2;
		bool bResult2 = GJKPenetration(Triangle, ScaledCapsule, StartTM, OutTime2, ClosestA, ClosestB, Normal2, ClosestVertexIndexA, ClosestVertexIndexB);


		EXPECT_VECTOR_NEAR(Normal, Normal2, KINDA_SMALL_NUMBER);
		EXPECT_NEAR(OutTime, -OutTime2, KINDA_SMALL_NUMBER);

		const FVec3 ClosestBShouldBe{ -287.344025, 1211.66296, 101.851364 };
		EXPECT_VECTOR_NEAR(ClosestB, ClosestBShouldBe, KINDA_SMALL_NUMBER);
	}

	//
	// A real failure case where EPA was terminating too early and returning an incorrect normal
	// because the UpperBound-LowerBound tolerance check was an absolute rather than relative value
	// and had an incorrect use of Abs()
	//
	GTEST_TEST(EPATests, EPARealFailures_ConvexVsTriangleWrongNormalGJKRaycast2)
	{
		using namespace Chaos;

		FTriangleRegister Triangle({
			MakeVectorRegisterFloat(0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f),
			MakeVectorRegisterFloat(100.000000f, 100.000000f, 0.00000000f, 0.00000000f),
			MakeVectorRegisterFloat(0.00000000f, 100.000000f, 0.00000000f, 0.00000000f)
			});

		TArray<FVec3f> ConvexVerts = {
			{1.64776051, 0.976988614, 8.85045052},
			{1.64776051, -0.976994812, 8.85045052},
			{-1.64776051, -0.976994812, 8.85045052},
			{-1.64776051, 0.976988614, 8.85045052},
			{1.64776051, -0.976994812, -0.191102192},
			{1.64776051, 0.976988614, -0.191102192},
			{-1.64776051, -0.976994812, -0.191102028},
			{-1.64776051, 0.976988614, -0.191102028},
		};

		FImplicitConvex3 Convex(ConvexVerts, 0.0f);

		VectorRegister4Float TranslationSimd = MakeVectorRegisterFloat(13.7357206f, 81.0178833f, 0.975698411f, 0.00000000f);
		VectorRegister4Float RotationSimd = MakeVectorRegisterFloat(-0.349331319f, -0.614945233f, 0.615562916f, 0.347695649f);
		VectorRegister4Float DirSimd = MakeVectorRegisterFloat(0.00133391307f, -0.00691976305f, -0.999975145f, 0.00000000f);
		FReal CurrentLength = 1.0890472489398730;

		FRealSingle Distance;
		VectorRegister4Float PositionSimd, NormalSimd;
		bool bHit = GJKRaycast2ImplSimd(Triangle, Convex, RotationSimd, TranslationSimd, DirSimd, FRealSingle(CurrentLength), Distance, PositionSimd, NormalSimd, true, GlobalVectorConstants::Float1000);
		EXPECT_TRUE(bHit);

		// We should get a hit with a normal pointing upwards but we were getting a normal facing downwards
		EXPECT_NEAR(VectorGetComponent(NormalSimd, 2), 1.0f, UE_KINDA_SMALL_NUMBER);
	}

}