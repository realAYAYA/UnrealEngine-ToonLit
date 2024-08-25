// Copyright Epic Games, Inc. All Rights Reserved.
#include "HeadlessChaosTestConstraints.h"

#include "HeadlessChaos.h"
#include "HeadlessChaosTestUtility.h"
#include "Modules/ModuleManager.h"
#include "Chaos/GeometryQueries.h"
#include "Chaos/HeightField.h"

namespace ChaosTest {

	using namespace Chaos;

	void Raycast()
	{
		const int32 Columns = 10;
		const int32 Rows = 10;
		const FReal CountToWorldScale = 1;

		{

			TArray<FReal> Heights;
			Heights.AddZeroed(Rows * Columns);

			FReal Count = 0;
			for(int32 Row = 0; Row < Rows; ++Row)
			{
				for(int32 Col = 0; Col < Columns; ++Col, ++Count)
				{
					Heights[Row * Columns + Col] = CountToWorldScale * Count;
				}
			}

			auto ComputeExpectedNormal = [&](const FVec3& Scale)
			{
				//Compute expected normal
				const FVec3 A(0,0,0);
				const FVec3 B(Scale[0],0,CountToWorldScale * Scale[2]);
				const FVec3 C(0,Scale[1],Columns*CountToWorldScale * Scale[2]);
				
				FVec3 ExpectedNormal = FVec3::CrossProduct((B - A), (C - A));
				ExpectedNormal.SafeNormalize();
				return ExpectedNormal;
			};

			auto AlongZTest = [&](const FVec3& Scale)
			{
				TArray<FReal> HeightsCopy = Heights;
				FHeightField Heightfield(MoveTemp(HeightsCopy),TArray<uint8>(),Rows,Columns,Scale);
				const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

				//test straight down raycast
				Count = 0;
				FReal TOI;
				FVec3 Position,Normal;
				int32 FaceIdx;

				const FVec3 ExpectedNormal = ComputeExpectedNormal(Scale);

				int32 ExpectedFaceIdx = 0;
				for(int32 Row = 0; Row < Rows; ++Row)
				{
					for(int32 Col = 0; Col < Columns; ++Col)
					{
						const FVec3 Start(Col*Scale[0],Row * Scale[1],1000*Scale[2]);
						EXPECT_TRUE(Heightfield.Raycast(Start,FVec3(0,0,-1),2000*Scale[2],0,TOI,Position,Normal,FaceIdx));
						EXPECT_NEAR(TOI,(1000 - Heights[Row*Columns+Col])*Scale[2],1e-2);
						EXPECT_VECTOR_NEAR(Position,FVec3(Col*Scale[0],Row * Scale[1],Heights[Row*Columns+Col] * Scale[2]),1e-2);
						EXPECT_VECTOR_NEAR(Normal,ExpectedNormal,1e-2);

						const FVec3 RayStart = Start + FVec3(0.2 * Scale[0], 0.1 * Scale[1], 0);
						const FVec3 RayDir = FVec3(0, 0, -1);
						const FReal RayLength = 2000 * Scale[2];
						const FVec3 RayEnd = RayStart + (RayDir * RayLength);

						//offset in from border ever so slightly to get a clear face
						const bool bResult = Heightfield.Raycast(RayStart, RayDir, RayLength,0,TOI,Position,Normal,FaceIdx);
						if(Col + 1 == Columns || Row + 1 == Rows)
						{
							EXPECT_FALSE(bResult);	//went past edge so no hit
							//Went past column so do not increment expected face idx
						} else
						{
							check(bResult == true);
							EXPECT_TRUE(bResult);
							EXPECT_EQ(FaceIdx,ExpectedFaceIdx);	//each quad has two triangles, so for each column we pass two faces

							ExpectedFaceIdx += 2;	//We hit the first triangle in the quad. Since we are going 1 quad at a time we skip 2
						}

						// reverse the ray to test double sided 
						FVec3 ReversePosition;
						FReal ReverseTOI;
						const bool bReverseResult = Heightfield.Raycast(RayEnd, -RayDir, RayLength, 0, ReverseTOI, ReversePosition, Normal, FaceIdx);
						if (Col + 1 == Columns || Row + 1 == Rows)
						{
							EXPECT_FALSE(bReverseResult);	//went past edge so no hit
							//Went past column so do not increment expected face idx
						}
						else
						{
							check(bReverseResult == true);
							EXPECT_TRUE(bReverseResult);
							EXPECT_NEAR(RayLength, (TOI + ReverseTOI), SMALL_NUMBER);
							EXPECT_VECTOR_NEAR(Position, ReversePosition, SMALL_NUMBER);
						}
					}
				}
			};

			AlongZTest(FVec3(1));
			AlongZTest(FVec3(1,1,3));
			AlongZTest(FVec3(1,1,.3));
			AlongZTest(FVec3(3,1,.3));
			AlongZTest(FVec3(2,.1,.3));

			auto AlongXTest = [&](const FVec3& Scale)
			{
				TArray<FReal> HeightsCopy = Heights;
				FHeightField Heightfield(MoveTemp(HeightsCopy),TArray<uint8>(),Rows,Columns,Scale);
				const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

				//test along x axis
				Count = 0;
				FReal TOI;
				FVec3 Position,Normal;
				int32 FaceIdx;

				const FVec3 ExpectedNormal = ComputeExpectedNormal(Scale);

				//move from left to right and raycast down the x-axis. The Row idx indicates which cell we expect to hit
				for(int32 Row = 0; Row < Rows; ++Row)
				{
					for(int32 Col = 0; Col < Columns; ++Col)
					{
						const FVec3 Start(-Scale[0],Row * Scale[1],Heights[Row*Columns + Col] * Scale[2] + 0.01 * Scale[2]);

						const FVec3 RayStart = Start;
						const FVec3 RayDir = FVec3(1, 0, 0);
						const FReal RayLength = 2000 * Scale[0];
						const FVec3 RayEnd = RayStart + (RayDir * RayLength);

						const bool bResult = Heightfield.Raycast(RayStart, RayDir, RayLength,0,TOI,Position,Normal,FaceIdx);
						if(Col + 1 == Columns)
						{
							EXPECT_FALSE(bResult);
							//No more columns so we shot over the final edge
						} else
						{
							check(bResult);
							EXPECT_TRUE(bResult);
							EXPECT_NEAR(TOI,(Scale[0] * (1 + Col)),1e-1);
							EXPECT_VECTOR_NEAR(Position,(Start + FVec3{TOI,0,0}),1e-2);
							EXPECT_VECTOR_NEAR(Normal,ExpectedNormal,1e-1);
						}

						// reverse ray to test double sided
						FVec3 ReversePosition;
						FReal ReverseTOI;
						const bool bReverseResult = Heightfield.Raycast(RayEnd, -RayDir, RayLength, 0, ReverseTOI, ReversePosition, Normal, FaceIdx);
						if (Col + 1 == Columns)
						{
							EXPECT_FALSE(bReverseResult);
							//No more columns so we shot over the final edge
						}
						else
						{
							check(bReverseResult);
							EXPECT_TRUE(bReverseResult);
							EXPECT_VECTOR_NEAR(Normal, (ExpectedNormal*-1), 1e-1);
						}
					}
				}
			};

			AlongXTest(FVec3(1));
			AlongXTest(FVec3(1,1,3));
			AlongXTest(FVec3(1,1,.3));
			AlongXTest(FVec3(3,1,.3));
			AlongXTest(FVec3(2,.1,.3));

			auto AlongYTest = [&](const FVec3& Scale)
			{
				TArray<FReal> HeightsCopy = Heights;
				FHeightField Heightfield(MoveTemp(HeightsCopy),TArray<uint8>(),Rows,Columns,Scale);
				const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

				//test along y axis
				Count = 0;
				FReal TOI;
				FVec3 Position,Normal;
				int32 FaceIdx;

				const FVec3 ExpectedNormal = ComputeExpectedNormal(Scale);

				for(int32 Row = 0; Row < Rows; ++Row)
				{
					for(int32 Col = 0; Col < Columns; ++Col)
					{
						const FVec3 Start(Col * Scale[0],-Scale[1],Heights[Row*Columns + Col] * Scale[2] + 0.01 * Scale[2]);

						const FVec3 RayStart = Start;
						const FVec3 RayDir = FVec3(0, 1, 0);
						const FReal RayLength = 2000 * Scale[0];
						const FVec3 RayEnd = RayStart + (RayDir * RayLength);

						const bool bResult = Heightfield.Raycast(RayStart, RayDir, RayLength,0,TOI,Position,Normal,FaceIdx);
						if(Row + 1 == Rows)
						{
							EXPECT_FALSE(bResult);
							//No more columns so we shot over the final edge
						} else
						{
							check(bResult == true);
							EXPECT_TRUE(bResult);
							EXPECT_NEAR(TOI,(Scale[1] * (1 + Row)),1e-1);
							EXPECT_VECTOR_NEAR(Position,(Start + FVec3{0,TOI,0}),1e-2);
							EXPECT_VECTOR_NEAR(Normal,ExpectedNormal,1e-1);
						}

						// reverse ray to test double sided
						FVec3 ReversePosition;
						FReal ReverseTOI;
						const bool bReverseResult = Heightfield.Raycast(RayEnd, -RayDir, RayLength, 0, ReverseTOI, Position, Normal, FaceIdx);
						if (Row + 1 == Rows)
						{
							EXPECT_FALSE(bReverseResult);
							//No more columns so we shot over the final edge
						}
						else
						{
							check(bReverseResult == true);
							EXPECT_TRUE(bReverseResult);
							EXPECT_VECTOR_NEAR(Normal, (ExpectedNormal * -1), 1e-1);
						}
					}
				}
			};

			AlongYTest(FVec3(1));
			AlongYTest(FVec3(1,1,3));
			AlongYTest(FVec3(1,1,.3));
			AlongYTest(FVec3(3,1,.3));
			AlongYTest(FVec3(2,.1,.3));
		}

		{

			//For diagonal test simply increase height on the y axis
			TArray<FReal> Heights2;
			Heights2.AddZeroed(Rows*Columns);
			for(int32 Row = 0; Row < Rows; ++Row)
			{
				for(int32 Col = 0; Col < Columns; ++Col)
				{
					Heights2[Row * Columns + Col] = CountToWorldScale * Row;
				}
			}

			auto ComputeExpectedNormal2 = [&](const FVec3& Scale)
			{
				//Compute expected normal
				const FVec3 A(0,0,0);
				const FVec3 B(Scale[0],0,0);
				const FVec3 C(0,Scale[1],CountToWorldScale * Scale[2]);
				const FVec3 ExpectedNormal = FVec3::CrossProduct((B-A),(C-A)).GetUnsafeNormal();
				return ExpectedNormal;
			};

			auto AlongXYTest = [&](const FVec3& Scale)
			{
				TArray<FReal> HeightsCopy = Heights2;
				FHeightField Heightfield(MoveTemp(HeightsCopy),TArray<uint8>(),Rows,Columns,Scale);
				const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

				//test along x-y axis
				FReal TOI;
				FVec3 Position,Normal;
				int32 FaceIdx;

				const FVec3 ExpectedNormal = ComputeExpectedNormal2(Scale);

				for(int32 Row = 0; Row < Rows; ++Row)
				{
					for(int32 Col = 0; Col < Columns; ++Col)
					{
						const FVec3 Start(Col * Scale[0],0,Heights2[Row*Columns + Col] * Scale[2] + 0.01 * Scale[2]);
						const FVec3 Dir = FVec3(1,1,0).GetUnsafeNormal();
						const bool bResult = Heightfield.Raycast(Start,Dir,2000*Scale[0],0,TOI,Position,Normal,FaceIdx);

						//As we increase the row, fewer columns will hit because the ray will exit the heightfield
						const bool bShouldHit = Col + Row + 1 < Columns;
						EXPECT_EQ(bShouldHit,bResult);
						if(bResult)
						{
							EXPECT_VECTOR_NEAR(Normal,ExpectedNormal,1e-1);
						}
					}
				}
			};

			AlongXYTest(FVec3(1));


			auto ToCellsTest = [&](const FVec3& Scale)
			{
				//Pick cells and shoot ray at them
				//This should always succeed because 0,0 is the lowest and n,n is the heighest
				TArray<FReal> HeightsCopy = Heights2;
				FHeightField Heightfield(MoveTemp(HeightsCopy),TArray<uint8>(),Rows,Columns,Scale);
				const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

				FReal TOI;
				FVec3 Position,Normal;
				int32 FaceIdx;

				const FVec3 ExpectedNormal = ComputeExpectedNormal2(Scale);

				const FVec3 Start(0,0,Heights2.Last() * Scale[2]);
				for(int32 TargetIdx = 0; TargetIdx < Heights2.Num(); ++TargetIdx)
				{
					const int32 Col = TargetIdx % Columns;
					const int32 Row = TargetIdx / Columns;
					const FVec3 EndUnscaled(Col,Row,Heights2[TargetIdx]);
					FVec3 End = EndUnscaled * Scale;
					if(Col + 1 == Columns)
					{
						//pull back slightly to avoid precision issues at edge
						End[0] -= 0.1 * Scale[0];
					}

					if(Row + 1 == Rows)
					{
						//pulling back row affects Z so just skip
						continue;
					}

					const FVec3 Dir = (End - Start).GetUnsafeNormal();

					const bool bResult = Heightfield.Raycast(Start,Dir,2000,0,TOI,Position,Normal,FaceIdx);
					check(bResult);
					EXPECT_TRUE(bResult);
					EXPECT_VECTOR_NEAR(Normal,ExpectedNormal,1e-1);
					EXPECT_VECTOR_NEAR(Position,End,1e-1);
				}
			};

			ToCellsTest(FVec3(1));
			ToCellsTest(FVec3(1,1,10));
			ToCellsTest(FVec3(1,1,.1));
			ToCellsTest(FVec3(3,1,.1));
			ToCellsTest(FVec3(.3,1,.1));


		}
		
	}

	void RaycastOnFlatHeightField()
	{
		int32 Rows = 64;
		int32 Columns = 64;
		FVec3 Scale(100.0, 100.0, 100.0);
		TArray<FReal> Heights;
		Heights.AddZeroed(Rows * Columns);

		TArray<FReal> HeightsCopy = Heights;
		FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, Scale);
		const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

		FReal TOI;
		FVec3 Position, Normal;
		int32 FaceIdx = 0;
	
		const FVec3 Start(8224.6524537283822, 2631.7542424011549, 2265.2052028112184);
		const FVec3 Dir(-0.92887444870972680, -0.11019885226781448, -0.35362193299208372);
		EXPECT_TRUE(Heightfield.Raycast(Start, Dir, 2097152.0000000000, 0, TOI, Position, Normal, FaceIdx));
	}

	void RaycastVariousWalkOnHeightField()
	{
		{
			constexpr int32 Rows = 64;
			constexpr int32 Columns = Rows;
			FVec3 Scale(1.0, 1.0, 1.0);
			TArray<FReal> Heights;
			Heights.AddZeroed(Rows * Columns);

			// Add a mountain on the diagonal
			for (int32 Index = 0; Index < Columns; ++Index)
			{
				Heights[Index * Columns + Index] = 20;
			}
			constexpr int32 BigMountainIndex = Columns / 2;
			Heights[BigMountainIndex * Columns + BigMountainIndex] = 40;

			TArray<FReal> HeightsCopy = Heights;

			FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, Scale);
			const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

			FReal TOI;
			FVec3 Position, Normal;
			int32 FaceIdx = 0;

			// Hit the diagonal, used directly the three walks (LowRes / Fast / Slow)
			{
				const FVec3 Start(2.0, 0.0, 10.0);
				FVec3 Dir(0.0, 1.0, 0.0);
				Dir.Normalize();
				const bool bResult = Heightfield.Raycast(Start, Dir, 100.0, 0, TOI, Position, Normal, FaceIdx);
				EXPECT_TRUE(bResult);
			}
			// Miss the diagonal, used the three walks but miss them  (LowRes / Fast / Slow) (worse case)
			{
				const FVec3 Start(0.6, 0.0, 17.0);
				FVec3 Dir(1.0, 1.0, 0.0);
				Dir.Normalize();
				const bool bResult = Heightfield.Raycast(Start, Dir, 100.0, 0, TOI, Position, Normal, FaceIdx);
				EXPECT_FALSE(bResult);
			}
			// Hit the diagonal in the middle used Low Res for few steps then Fast walk for few step and Slow walks (optimal use case)
			{
				const FVec3 Start(64.0, 0.0, 10.0);
				FVec3 Dir(-1.0, 1.0, 0.0);
				Dir.Normalize();
				const bool bResult = Heightfield.Raycast(Start, Dir, 100.0, 0, TOI, Position, Normal, FaceIdx);
				EXPECT_TRUE(bResult);
			}
			// Miss the diagonal in the middle used Low Res for few steps then Fast walk and Slow walks, then miss, then use LowRes until the end
			{
				const FVec3 Start(63.0, 0.0, 36.0);
				FVec3 Dir(-1.0, 1.0, 0.0);
				Dir.Normalize();
				const bool bResult = Heightfield.Raycast(Start, Dir, 100.0, 0, TOI, Position, Normal, FaceIdx);
				EXPECT_FALSE(bResult);
			}
			// Hit at the HeightField end boundary testing the LowRes Heightfield not fully filled  
			// (HeightField size) % LowResolution =>  64 % 6 = 4
			{
				const FVec3 Start(20.0, 63.0, 10.0);
				FVec3 Dir(1.0, 0.0, 0.0);
				Dir.Normalize();
				const bool bResult = Heightfield.Raycast(Start, Dir, 100.0, 0, TOI, Position, Normal, FaceIdx);
				EXPECT_TRUE(bResult);
			}
			// Along the diagonal without hitting, navigate in FastWalk
			{
				const FVec3 Start(4.0, 0.0, 10.0);
				FVec3 Dir(1.0, 1.0, 0.0);
				Dir.Normalize();
				const bool bResult = Heightfield.Raycast(Start, Dir, 100.0, 0, TOI, Position, Normal, FaceIdx);
				EXPECT_FALSE(bResult);
			}

		}
		{
			constexpr int32 Rows = 64;
			constexpr int32 Columns = Rows;
			FVec3 Scale(1.0, 1.0, 1.0);
			TArray<FReal> Heights;
			Heights.AddZeroed(Rows * Columns);

			// Add a mountain close to the edge
			for (int32 Index = 0; Index < Columns; ++Index)
			{
				Heights[Index * Columns + 62] = 20;
			}

			TArray<FReal> HeightsCopy = Heights;

			FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, Scale);
			const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

			FReal TOI;
			FVec3 Position, Normal;
			int32 FaceIdx = 0;

			// Hit at the HeightField end boundary testing the LowRes Heightfield not fully filled  
			// (HeightField size) % LowResolution =>  64 % 6 = 4
			// Jira UE-162256
			{
				const FVec3 Start(58.0, 63.0, 10.0);
				FVec3 Dir(1.0, 0.0, 0.0);
				Dir.Normalize();
				const bool bResult = Heightfield.Raycast(Start, Dir, 100.0, 0, TOI, Position, Normal, FaceIdx);
				EXPECT_TRUE(bResult);
			}

		}
		// Bug found in Fortnite: The raycast is falling in infinite loop if raycast in mode WalkFast 
		// and leave the bounding volume of the HeightField
		{
			constexpr int32 Rows = 64;
			constexpr int32 Columns = Rows;
			FVec3 Scale(1.0, 1.0, 1.0);
			TArray<FReal> Heights;
			Heights.AddZeroed(Rows * Columns);

			// Add a mountain in the middle
			for (int32 Index = 0; Index < Columns; ++Index)
			{
				Heights[Index * Columns + 32] = 20;
			}

			TArray<FReal> HeightsCopy = Heights;

			FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, Scale);
			const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

			FReal TOI;
			FVec3 Position, Normal;
			int32 FaceIdx = 0;
			{
				const FVec3 Start(34.0, 0.0, 10.0);
				FVec3 Dir(0.0, 1.0, 0.0);
				Dir.Normalize();

				const bool bResult = Heightfield.Raycast(Start, Dir, 100.0, 0, TOI, Position, Normal, FaceIdx);
				EXPECT_FALSE(bResult);
			}

		}
	}


	void EditHeights()
	{
		const int32 Columns = 10;
		const int32 Rows = 10;
		const uint16 InitialHeight = 32768; // Real height = 0, (half of uint16 max of 65535)

		TArray<uint16> Heights;
		Heights.AddZeroed(Rows * Columns);

		// Stolen from Heightfield.cpp
		TUniqueFunction<FReal(const uint16)> ConversionFunc = [](const FReal InVal) -> FReal
		{
			return (FReal)((int32)InVal - 32768);
		};



		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Col = 0; Col < Columns; ++Col)
			{
				Heights[Row * Columns + Col] = InitialHeight;
			}
		}


		FVec3 Scale(1, 1, 1);
		FHeightField Heightfield(MoveTemp(Heights),TArray<uint8>(),Rows,Columns,Scale);

		// Test is intended to catch trivial regressions in EditGeomData,
		// specifically handling Landscape module providing buffer with column index inverted from heightfield whe editing.

		int32 InRows = 1;
		int32 InCols = 3;
		TArray<uint16> ModifiedHeights;
		ModifiedHeights.Init(InitialHeight, InRows * InCols);

		int32 Row = 0;
		int32 Col = 0;
		ModifiedHeights[Row * (InCols) + Col] = 35000;
		Col = 1;
		ModifiedHeights[Row * (InCols) + Col] = 40000;
		Col = 2;
		ModifiedHeights[Row * (InCols) + Col] = 45000;

		FReal ExpectedMaxRealHeight = ConversionFunc(45000);
		FReal ExpectedMinRealHeight = ConversionFunc(InitialHeight);
		FReal ExpectedRange = ExpectedMaxRealHeight - ExpectedMinRealHeight;

		int32 RowBegin = 3;
		int32 ColBegin = 4;

		// Expectation is that all values are at default, and ModifiedHeights is applied to heightfield
		// starting at (ColBegin, RowBegin) to (ColBegin + InCols, RowBegin + InRows).
		// Landscape module provides buffer with col idx inverted, so they are expected to be written reverse order over columns within this range.
		// The Begin indices however are not inverted. These match heightfield.
		// If this seems confusing, that's because it is. -MaxW
		Heightfield.EditHeights(ModifiedHeights, RowBegin, ColBegin, InRows, InCols);


		auto& GeomData = Heightfield.GeomData;


		// Validate heights using 2d iteration scheme
		for (int32 RowIdx = 0; RowIdx < Rows; ++RowIdx)
		{
			for (int32 ColIdx = 0; ColIdx < Columns; ++ColIdx)
			{
				if (RowIdx >= RowBegin && RowIdx < RowBegin + InRows && ColIdx >= ColBegin && ColIdx < ColBegin + InCols)
				{
					// This is in modified range
					int32 ModifiedRowIdx = RowIdx - RowBegin;
					int32 ModifiedColIdx = ColIdx - ColBegin;
					int32 ModifiedIdx = ModifiedRowIdx * InCols + ModifiedColIdx;

					int32 HeightIdx = RowIdx * Columns + (Columns - 1 - ColIdx); // Remember that modified heights buffer uses inverted col index
					FReal HeightReal = GeomData.MinValue + GeomData.Heights[HeightIdx] * GeomData.HeightPerUnit;

					uint16 ModifiedHeight = ModifiedHeights[ModifiedIdx];
					FReal ModifiedHeightReal = ConversionFunc(ModifiedHeight);
					EXPECT_NEAR(ModifiedHeightReal, HeightReal, 1);
				}
				else
				{
					int32 HeightIdx = RowIdx * Columns + (Columns - 1 - ColIdx); // Remember that modified heights buffer uses inverted col index
					float HeightReal = GeomData.MinValue + GeomData.Heights[HeightIdx] * GeomData.HeightPerUnit;

					float InitialHeightReal = ConversionFunc(InitialHeight);
					EXPECT_NEAR(InitialHeightReal, HeightReal, 0.0001f);
				}
			}
		}

		EXPECT_EQ(GeomData.MinValue, ExpectedMinRealHeight);
		EXPECT_EQ(GeomData.MaxValue, ExpectedMaxRealHeight);
		EXPECT_EQ(GeomData.Range, ExpectedRange); 
		EXPECT_EQ(GeomData.HeightPerUnit, (ExpectedRange) / TNumericLimits<uint16>::Max()); // Range over uint16 max (65535)
	}


	void SweepSmallSphereTest()
	{
		const int32 Columns = 10;
		const int32 Rows = 10;
		const FReal CountToWorldScale = 1;


		TSphere<FReal, 3> Sphere(FVec3(0.0, 0.0, 0.0), 1.0);
		{

			TArray<FReal> Heights;
			Heights.AddZeroed(Rows * Columns);

			FReal Count = 0;
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				for (int32 Col = 0; Col < Columns; ++Col, ++Count)
				{
					Heights[Row * Columns + Col] = CountToWorldScale * Count;
				}
			}

			auto AlongZTest = [&](const FVec3& Scale)
			{
				TArray<FReal> HeightsCopy = Heights;
				FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, Scale);
				const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

				//test straight down sweep
				Count = 0;
				FReal TOI;
				FVec3 Position, Normal, FaceNormal;
				int32 FaceIdx;

				int32 ExpectedFaceIdx = 0;
				for (int32 Row = 0; Row < Rows-1; ++Row)
				{
					for (int32 Col = 0; Col < Columns-1; ++Col)
					{
						const FVec3 Start(Col * Scale[0] + 0.5, Row * Scale[1]+0.5, 1000 * Scale[2]);
						FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
						FVec3 Dir(0, 0, -1);

						bool Result = Heightfield.SweepGeom(Sphere, StartTM, Dir, 2000 * Scale[2], TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
						EXPECT_TRUE(Result);
					}
				}
			};

			AlongZTest(FVec3(1));
			AlongZTest(FVec3(1, 1, 3));
			AlongZTest(FVec3(1, 1, .3));


			auto AlongXTest = [&](const FVec3& Scale)
			{
				TArray<FReal> HeightsCopy = Heights;
				FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, Scale);
				const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

				//test straight down sweep
				Count = 0;
				FReal TOI;
				FVec3 Position, Normal, FaceNormal;
				int32 FaceIdx;

				int32 ExpectedFaceIdx = 0;
				for (int32 Row = 0; Row < Rows - 1; ++Row)
				{
					for (int32 Col = 0; Col < Columns - 1; ++Col)
					{

						const FVec3 Start(-Scale[0], Row * Scale[1], Heights[Row * Columns + Col] * Scale[2] + 0.01 * Scale[2]);
						FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
						FVec3 Dir(1, 0, 0);

						bool Result = Heightfield.SweepGeom(Sphere, StartTM, Dir, 2000 * Scale[0], TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
						EXPECT_TRUE(Result);
					}
				}
			};

			AlongXTest(FVec3(1));
			AlongXTest(FVec3(1, 1, 3));
			AlongXTest(FVec3(1, 1, .3));
		}
	}

	void SweepBigSphereTest()
	{
		const int32 Columns = 10;
		const int32 Rows = 10;
		const FReal CountToWorldScale = 100;


		TSphere<FReal, 3> Sphere(FVec3(0.0, 0.0, 0.0), 250.0);

		TArray<FReal> Heights;
		Heights.AddZeroed(Rows * Columns);

		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Col = 0; Col < Columns; ++Col)
			{
				if (Col == 5)
				{
					Heights[Row * Columns + Col] = 10;
				}
				else
				{
					Heights[Row * Columns + Col] = 0;
				}
			}
		}

		TArray<FReal> HeightsCopy = Heights;
		FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, FVec3(100, 100, 100));
		const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

		FReal TOI;
		FVec3 Position, Normal, FaceNormal;
		int32 FaceIdx;
		{
			const FVec3 Start(200, 500, 700);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
			FVec3 Dir(1.0, -0.1, 0.0);
			Dir.Normalize();

			bool Result = Heightfield.SweepGeom(Sphere, StartTM, Dir, 50, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_TRUE(Result);
		}
		// Test extent in the opposite direction of the sweep
		{
			const FVec3 Start(350, 500, 700);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
			FVec3 Dir(-1.0, 0.1, 0.0);
			Dir.Normalize();

			bool Result = Heightfield.SweepGeom(Sphere, StartTM, Dir, 50, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_TRUE(Result);
		}
		{
			const FVec3 Start(180, 500, 700);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
			FVec3 Dir(1.0, 0.0, -1.0);
			Dir.Normalize();

			bool Result = Heightfield.SweepGeom(Sphere, StartTM, Dir, 50, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_TRUE(Result);
		}
		// Test extent in the opposite direction of the sweep
		{
			const FVec3 Start(220, 500, 700);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
			FVec3 Dir(-1.0, 0.0, 1.0);
			Dir.Normalize();

			bool Result = Heightfield.SweepGeom(Sphere, StartTM, Dir, 50, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_TRUE(Result);
		}

	}

	void SweepSmallBoxTest()
	{
		const int32 Columns = 10;
		const int32 Rows = 10;
		const FReal CountToWorldScale = 1;

		TBox<FReal, 3> Box(FVec3(-0.05, -0.05, -0.05), FVec3(0.05, 0.05, 0.05));
		{

			TArray<FReal> Heights;
			Heights.AddZeroed(Rows * Columns);

			FReal Count = 0;
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				for (int32 Col = 0; Col < Columns; ++Col, ++Count)
				{
					Heights[Row * Columns + Col] = CountToWorldScale * Count;
				}
			}

			auto AlongZTest = [&](const FVec3& Scale)
			{
				TArray<FReal> HeightsCopy = Heights;
				FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, Scale);
				const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

				//test straight down sweep
				Count = 0;
				FReal TOI;
				FVec3 Position, Normal, FaceNormal;
				int32 FaceIdx;

				int32 ExpectedFaceIdx = 0;
				for (int32 Row = 0; Row < Rows - 1; ++Row)
				{
					for (int32 Col = 0; Col < Columns - 1; ++Col)
					{
						const FVec3 Start(Col * Scale[0] + 0.5, Row * Scale[1] + 0.5, 1000 * Scale[2]);
						FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
						FVec3 Dir(0, 0, -1);

						bool Result = Heightfield.SweepGeom(Box, StartTM, Dir, 2000 * Scale[2], TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
						EXPECT_TRUE(Result);
					}
				}
			};

			AlongZTest(FVec3(1));
			AlongZTest(FVec3(1, 1, 3));
			AlongZTest(FVec3(1, 1, .3));


			auto AlongXTest = [&](const FVec3& Scale)
			{
				TArray<FReal> HeightsCopy = Heights;
				FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, Scale);
				const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

				//test straight down sweep
				Count = 0;
				FReal TOI;
				FVec3 Position, Normal, FaceNormal;
				int32 FaceIdx;

				int32 ExpectedFaceIdx = 0;
				for (int32 Row = 0; Row < Rows - 1; ++Row)
				{
					for (int32 Col = 0; Col < Columns - 1; ++Col)
					{

						const FVec3 Start(-Scale[0], Row * Scale[1], Heights[Row * Columns + Col] * Scale[2] + 0.01 * Scale[2]);
						FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
						FVec3 Dir(1, 0, 0);

						bool Result = Heightfield.SweepGeom(Box, StartTM, Dir, 2000 * Scale[0], TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
						EXPECT_TRUE(Result);
					}
				}
			};

			AlongXTest(FVec3(1));
			AlongXTest(FVec3(1, 1, 3));
			AlongXTest(FVec3(1, 1, .3));
		}
	}

	TArray<FReal> CreateMountain(int32 Columns, int32 Rows)
	{
		TArray<FReal> Heights;
		Heights.AddZeroed(Rows * Columns);
		// Plane
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Col = 0; Col < Columns; ++Col)
			{
				Heights[Row * Columns + Col] = 0.0;
			}
		}

		// Mountain
		for (int32 Row = Rows / 2 - 1; Row <= Rows / 2 + 1; ++Row)
		{
			for (int32 Col = Columns / 2 - 1; Col <= Columns / 2 + 1; ++Col)
			{
				Heights[Row * Columns + Col] = 5.0;
			}
		}
		// Summit
		Heights[(Rows / 2) * Columns + Columns / 2] = 10.0;

		return Heights;
	}

	void SweepBoxTest()
	{
		const FReal CountToWorldScale = 1;
		const int32 Columns = 10;
		const int32 Rows = 10;

		TBox<FReal, 3> Box(FVec3(-1.0, -1.0, -1.0), FVec3(1.0, 1.0, 1.0));

		TArray<FReal> Heights = CreateMountain(Columns, Rows);

		TArray<FReal> HeightsCopy = Heights;
		FVec3 Scale(1.0, 1.0, 1.0);
		FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, Scale);
		const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

		FReal TOI;
		FVec3 Position, Normal, FaceNormal;
		int32 FaceIdx;
		int32 ExpectedFaceIdx = 0;
		{
			// Miss on one side of the mountain
			const FVec3 Start(0.0, 3.5, 10.0);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
			FVec3 Dir(1, 0, 0);
			bool Result = Heightfield.SweepGeom(Box, StartTM, Dir, 100, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_FALSE(Result);
		}
		{
			// Miss on the other side of the mountain
			const FVec3 Start(0.0, 7.0, 10.0);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
			FVec3 Dir(1, 0, 0);
			bool Result = Heightfield.SweepGeom(Box, StartTM, Dir, 100, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_FALSE(Result);
		}
		// TODO This unit test is not working, to investigate
		{
			// Same missing in Y 
			const FVec3 Start(3.0, 0.0, 10.0);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
			FVec3 Dir(0, 1, 0);
			bool Result = Heightfield.SweepGeom(Box, StartTM, Dir, 100, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_FALSE(Result);
		}
		{
			const FVec3 Start(7.0, 0.0, 10.0);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
			FVec3 Dir(0, 1, 0);
			bool Result = Heightfield.SweepGeom(Box, StartTM, Dir, 100, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_FALSE(Result);
		}
		// Hit on the side of the box
		{
			const FVec3 Start(0.0, 4.0, 10.0);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
			FVec3 Dir(1, 0, 0);
			bool Result = Heightfield.SweepGeom(Box, StartTM, Dir, 100, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_TRUE(Result);
		}
		{
			const FVec3 Start(0.0, 6.0, 10.0);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
			FVec3 Dir(1, 0, 0);
			bool Result = Heightfield.SweepGeom(Box, StartTM, Dir, 100, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_TRUE(Result);
		}
		{
			const FVec3 Start(4.0, 0.0, 10.0);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>());
			FVec3 Dir(0, 1, 0);
			bool Result = Heightfield.SweepGeom(Box, StartTM, Dir, 100, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_TRUE(Result);
		}
		{
			const FVec3 Start(6.0, 0.0, 10.0);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
			FVec3 Dir(0, 1, 0);
			bool Result = Heightfield.SweepGeom(Box, StartTM, Dir, 100, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_TRUE(Result);
		}
		// Hit in Diagonal
		{

			const FVec3 Start(0.5, 0.5, 10.0);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
			FVec3 Dir(1, 1, 0);
			Dir.SafeNormalize();
			bool Result = Heightfield.SweepGeom(Box, StartTM, Dir, 100, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_TRUE(Result);
		}
		{
			const FVec3 Start(10.5, 10.5, 10.0);
			FRigidTransform3 StartTM(Start, TRotation<FReal, 3>::Identity);
			FVec3 Dir(-1, -1, 0);
			Dir.SafeNormalize();
			bool Result = Heightfield.SweepGeom(Box, StartTM, Dir, 100, TOI, Position, Normal, FaceIdx, FaceNormal, 0.0, true);
			EXPECT_TRUE(Result);
		}
	}

	void SweepTest()
	{
		SweepSmallSphereTest();
		SweepBigSphereTest();
		SweepSmallBoxTest();
		SweepBoxTest();
	}


	void OverlapConsistentTest()
	{
		const FReal CountToWorldScale = 1;
		const int32 Columns = 10;
		const int32 Rows = 10;

		TArray<FReal> Heights = CreateMountain(Columns, Rows);

		TArray<FReal> HeightsCopy = Heights;
		FVec3 Scale(1.0, 1.0, 1.0);
		FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, Scale);
		const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds
		{
			TBox<FReal, 3> Box(FVec3(-0.5, -0.5, -0.5), FVec3(0.5, 0.5, 0.5));
			FCapsule Capsule(FVec3(0.0, 0.0, 0.0), FVec3(0.0, 0.0, 9.0), 1.14);
			Chaos::FSphere Sphere1(FVec3(0.0, 0.0, -2.0), 0.6);

			FMTDInfo OutBoxMTD;
			FMTDInfo OutCapsuleMTD;
			FMTDInfo OutSphereMTD;

			for (int32 Row = 0; Row < Rows; ++Row)
			{
				for (int32 Col = 0; Col < Columns; ++Col)
				{
					for (FReal Height = 0; Height < 12; Height+=0.5)
					{

						const FVec3 Translation(Col, Row, Height);
						FRigidTransform3 QueryTM(Translation, TRotation<FReal, 3>::Identity);
						FVec3 Dir(1, 0, 0);
						bool BoxResult = Heightfield.OverlapGeom(Box, QueryTM, 0.0, nullptr);
						bool BoxResultMTD = Heightfield.OverlapGeom(Box, QueryTM, 0.0, &OutBoxMTD);
						EXPECT_EQ(BoxResult, BoxResultMTD);

						bool CapsuleResult = Heightfield.OverlapGeom(Capsule, QueryTM, 0.0, nullptr);
						bool CapsuleResultMTD = Heightfield.OverlapGeom(Capsule, QueryTM, 0.0, &OutCapsuleMTD);
						EXPECT_EQ(CapsuleResult, CapsuleResultMTD);

						bool SphereResult = Heightfield.OverlapGeom(Sphere1, QueryTM, 0.0, nullptr);
						bool SphereResultMTD = Heightfield.OverlapGeom(Sphere1, QueryTM, 0.0, &OutSphereMTD);
						EXPECT_EQ(SphereResult, SphereResultMTD);
					}
				}
			}
		}
		// Testing capsule upside down
		{
			FCapsule Capsule(FVec3(0.0, 0.0, 9.0), FVec3(0.0, 0.0, 0.0), 1.14);
			FMTDInfo OutCapsuleMTD;

			for (int32 Row = 0; Row < Rows; ++Row)
			{
				for (int32 Col = 0; Col < Columns; ++Col)
				{
					for (FReal Height = 0; Height < 12; Height += 0.5)
					{

						const FVec3 Translation(Col, Row, Height);
						FRigidTransform3 QueryTM(Translation, TRotation<FReal, 3>::Identity);
						FVec3 Dir(1, 0, 0);

						bool CapsuleResult = Heightfield.OverlapGeom(Capsule, QueryTM, 0.0, nullptr);
						bool CapsuleResultMTD = Heightfield.OverlapGeom(Capsule, QueryTM, 0.0, &OutCapsuleMTD);
						EXPECT_EQ(CapsuleResult, CapsuleResultMTD);
					}
				}
			}
		}
	}

	void OverlapTest()
	{
		const FReal CountToWorldScale = 1;
		const int32 Columns = 10;
		const int32 Rows = 10;

		TArray<FReal> Heights = CreateMountain(Columns, Rows);

		TArray<FReal> HeightsCopy = Heights;
		FVec3 Scale(1.0, 1.0, 1.0);
		FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, Scale);
		const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds
		// Long box
		{
			TBox<FReal, 3> Box(FVec3(-1.0, -1.0, -1.0), FVec3(1.0, 1.0, 10.0));
			FCapsule Capsule(FVec3(0.0, 0.0, 0.0), FVec3(0.0, 0.0, 9.0), 1.14);
			Chaos::FSphere Sphere1(FVec3(0.0, 0.0, -2.0), 0.6);

			for (int32 Row = 0; Row < Rows; ++Row)
			{
				for (int32 Col = 0; Col < Columns; ++Col)
				{
					const FVec3 Translation(Col, Row, 1.5);
					FRigidTransform3 QueryTM(Translation, TRotation<FReal, 3>::Identity);
					FVec3 Dir(1, 0, 0);
					bool BoxResult = Heightfield.OverlapGeom(Box, QueryTM, 0.0, nullptr);
					bool CapsuleResult = Heightfield.OverlapGeom(Capsule, QueryTM, 0.0, nullptr);
					bool SphereResult = Heightfield.OverlapGeom(Sphere1, QueryTM, 0.0, nullptr);
					// No collision on the side of the mountain
					if ((Col < 3 || Col > 7) && (Row < 3 || Row > 7))
					{
						EXPECT_FALSE(BoxResult);
						EXPECT_FALSE(CapsuleResult);
						// Sphere on the floor
						EXPECT_TRUE(SphereResult);
					}
					// Collision with the mountain
					else if ((Col >= 3 && Col <= 7) && (Row >= 3 && Row <= 7))
					{
						EXPECT_TRUE(BoxResult);
						EXPECT_TRUE(CapsuleResult);
					}
					else
					{
						EXPECT_FALSE(BoxResult);
					}
					// Inside the mountain the sphere shouldn't collide
					if ((Col > 3 && Col < 7) && (Row > 3 && Row < 7))
					{
						EXPECT_FALSE(SphereResult);
					}
				}
			}
		}
		// Box rotated in X
		{
			TBox<FReal, 3> Box(FVec3(-1.0, -1.0, -20.0), FVec3(1.0, 1.0, 20.0));
			FCapsule Capsule(FVec3(0.0, 0.0, -19.0), FVec3(0.0, 0.0, 19.0), 1.14);
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				for (int32 Col = 0; Col < Columns; ++Col)
				{
					const FVec3 Translation(Col, Row, 2.0);
					FRigidTransform3 QueryTM(Translation, TRotation<FReal, 3>(UE::Math::TQuat<FReal>(TVector<FReal, 3>(1.0, 0.0, 0.0), 3.14159/2.0)));
					FVec3 Dir(1, 0, 0);
					bool BoxResult = Heightfield.OverlapGeom(Box, QueryTM, 0.0, nullptr);
					bool CapsuleResult = Heightfield.OverlapGeom(Capsule, QueryTM, 0.0, nullptr);
					// Collision with the mountain
					if (Col >= 3 && Col <= 7)
					{
						EXPECT_TRUE(BoxResult);
						EXPECT_TRUE(CapsuleResult);
					}
					else
					{
						EXPECT_FALSE(BoxResult);
						EXPECT_FALSE(CapsuleResult);
					}
				}
			}
		}
		// Box rotated in Y
		{
			TBox<FReal, 3> Box(FVec3(-1.0, -1.0, -20.0), FVec3(1.0, 1.0, 20.0));
			FCapsule Capsule(FVec3(0.0, 0.0, -19.0), FVec3(0.0, 0.0, 19.0), 1.14);
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				for (int32 Col = 0; Col < Columns; ++Col)
				{
					const FVec3 Translation(Col, Row, 2.0);
					FRigidTransform3 QueryTM(Translation, TRotation<FReal, 3>(UE::Math::TQuat<FReal>(TVector<FReal, 3>(0.0, 1.0, 0.0), 3.14159 / 2.0)));
					FVec3 Dir(1, 0, 0);
					bool BoxResult = Heightfield.OverlapGeom(Box, QueryTM, 0.0, nullptr);
					bool CapsuleResult = Heightfield.OverlapGeom(Capsule, QueryTM, 0.0, nullptr);
					// Collision with the mountain
					if (Row >= 3 && Row <= 7)
					{
						EXPECT_TRUE(BoxResult);
						EXPECT_TRUE(CapsuleResult);
					}
					else
					{
						EXPECT_FALSE(BoxResult);
						EXPECT_FALSE(CapsuleResult);
					}
				}
			}
		}
		// Thin Box
		{
			TBox<FReal, 3> Box(FVec3(-10.0, -10.0, -0.001), FVec3(10.0, 10.0, 0.001));
			FCapsule Capsule(FVec3(0.0, 0.0, -0.001), FVec3(0.0, 0.0, 0.001), 11.4);
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				for (int32 Col = 0; Col < Columns; ++Col)
				{
					const FVec3 Translation(Col, Row, 2.0);
					FRigidTransform3 QueryTM(Translation, TRotation<FReal, 3>(UE::Math::TQuat<FReal>(TVector<FReal, 3>(1.0, 0.0, 0.0), 0.0)));
					FVec3 Dir(1, 0, 0);
					bool BoxResult = Heightfield.OverlapGeom(Box, QueryTM, 0.0, nullptr);
					bool CapsuleResult = Heightfield.OverlapGeom(Capsule, QueryTM, 0.0, nullptr);
					EXPECT_TRUE(BoxResult);
					EXPECT_TRUE(CapsuleResult);
				}
			}
		}
		// Thin Box on the top
		{
			TBox<FReal, 3> Box(FVec3(-10.0, -10.0, -0.001), FVec3(10.0, 10.0, 0.001));
			FCapsule Capsule(FVec3(0.0, 0.0, -0.001), FVec3(0.0, 0.0, 0.001), 11.4);
			for (int32 Row = 0; Row < Rows; ++Row)
			{
				for (int32 Col = 0; Col < Columns; ++Col)
				{
					const FVec3 Translation(Col, Row, 9.5);
					FRigidTransform3 QueryTM(Translation, TRotation<FReal, 3>(UE::Math::TQuat<FReal>(TVector<FReal, 3>(1.0, 0.0, 0.0), 0.0)));
					FVec3 Dir(1, 0, 0);
					bool BoxResult = Heightfield.OverlapGeom(Box, QueryTM, 0.0, nullptr);
					bool CapsuleResult = Heightfield.OverlapGeom(Capsule, QueryTM, 0.0, nullptr);
					EXPECT_TRUE(BoxResult);
					EXPECT_TRUE(CapsuleResult);
				}
			}
		}

		// Inclined Box
		{
			TBox<FReal, 3> Box(FVec3(-1.0, -1.0, -0.0001), FVec3(1.0, 1.0, 10.0));
			FCapsule Capsule(FVec3(0.0, 0.0, 1.0-0.0001), FVec3(0.0, 0.0, 9.0), 1.0);
			const FVec3 Translation(5.0, 0.0, 15.0);
			FRigidTransform3 QueryTM(Translation, TRotation<FReal, 3>(UE::Math::TQuat<FReal>(TVector<FReal, 3>(1.0, 0.0, 0.0), -3*3.1415926 / 4.0)));
			FVec3 Dir(1, 0, 0);
			bool BoxResult = Heightfield.OverlapGeom(Box, QueryTM, 0.0, nullptr);
			bool CapsuleResult = Heightfield.OverlapGeom(Capsule, QueryTM, 0.0, nullptr);
			EXPECT_TRUE(BoxResult);
			EXPECT_TRUE(CapsuleResult);
		}

		HeightsCopy = Heights;
		const float UniformScale = 10.0f;
		FHeightField HeightfieldScaled(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, FVec3(UniformScale, UniformScale, UniformScale));
		const auto& Bounds2 = HeightfieldScaled.BoundingBox();	//Current API forces us to do this to cache the bounds
		// Small sphere intersecting plane area of mountain with a scaled heightfield
		{
			Chaos::FSphere Sphere1(FVec3(0.0, 0.0, 0.0), 0.2);
			int32 Row = 2;
			int32 Col = 2;
			
			const FVec3 Translation(Col* UniformScale, Row * UniformScale, 0.1f);
			FRigidTransform3 QueryTM(Translation, TRotation<FReal, 3>::Identity);
			bool SphereResult = HeightfieldScaled.OverlapGeom(Sphere1, QueryTM, 0.0, nullptr);
			EXPECT_TRUE(SphereResult);
		}

		// Small convex intersecting plane area of mountain with a scaled heightfield
		{
			//Tetrahedron
			TArray<FConvex::FVec3Type> HullParticles;
			HullParticles.SetNum(4);
			HullParticles[0] = { -1,-1,-1 };
			HullParticles[1] = { 1,-1,-1 };
			HullParticles[2] = { 0,1,-1 };
			HullParticles[3] = { 0,0,1 };
			FConvex Tet(HullParticles, 0.0f);
			
			int32 Row = 2;
			int32 Col = 2;

			const FVec3 Translation(Col * UniformScale, Row * UniformScale, 1.1f);
			FRigidTransform3 QueryTM(Translation, TRotation<FReal, 3>::Identity);
			bool TetResult = HeightfieldScaled.OverlapGeom(Tet, QueryTM, 0.2, nullptr);
			EXPECT_TRUE(TetResult);
		}
	}
	

	TEST(ChaosTests, Heightfield)
	{
		ChaosTest::Raycast();
		ChaosTest::RaycastOnFlatHeightField();
		ChaosTest::RaycastVariousWalkOnHeightField();
		ChaosTest::SweepTest();
		ChaosTest::OverlapTest();
		ChaosTest::OverlapConsistentTest();
		EditHeights();
		SUCCEED();
	}

	// A flat heightfield and a sphere swept downwards using the CCD API.
	// Check that the IgnoreThreshold is being used to ignore sweeps that have a penetration depth
	// less that that threshold at T=1.
	GTEST_TEST(HeightfieldCCDTests, TestSphere)
	{
		int32 Rows = 64;
		int32 Columns = 64;
		FVec3 Scale(100.0, 100.0, 100.0);
		TArray<FReal> Heights;
		Heights.AddZeroed(Rows * Columns);

		TArray<FReal> HeightsCopy = Heights;
		FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, Scale);
		const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

		TSphere<FReal, 3> Sphere(FVec3(0.0, 0.0, 0.0), 50.0);

		FReal TOI, Phi;
		FVec3 Position, Normal, FaceNormal;
		int32 FaceIdx = 0;

		const FRigidTransform3 Start(FVec3(0, 0, 60), FRotation3::FromIdentity());
		const FVec3 Dir(0, 0, -1);
		const FReal CCDIgnorePenetration = 30;
		const FReal CCDTargetPenetration = 0;

		// The first sweep should be less than the ignore threshold so get no hit
		const FReal CCDSweepLengthA = 30;
		const bool bHitA = Heightfield.SweepGeomCCD(Sphere, Start, Dir, CCDSweepLengthA, CCDIgnorePenetration, CCDTargetPenetration, TOI, Phi, Position, Normal, FaceIdx, FaceNormal);
		EXPECT_FALSE(bHitA);

		// The second sweep should be greater than the ignore threshold so we get a hit
		const FReal CCDSweepLengthB = 50;
		const bool bHitB = Heightfield.SweepGeomCCD(Sphere, Start, Dir, CCDSweepLengthB, CCDIgnorePenetration, CCDTargetPenetration, TOI, Phi, Position, Normal, FaceIdx, FaceNormal);
		EXPECT_TRUE(bHitB);
		EXPECT_NEAR(TOI, 0.2, UE_KINDA_SMALL_NUMBER);	// 10 / 50
		EXPECT_NEAR(Phi, 0.0, UE_KINDA_SMALL_NUMBER);	// We are not initially penetrating
	}

	// Same as TestSphere except the sphere has a local offset built in.
	// CCD sweeps have an early out if the depth at T=1 is less than some threshold, and there was a bug in this.
	// related to the fact that the sweep is effectively an AABB sweep to find the overlapping heightfield cells, 
	// and a Shape sweep to find the shape-triangle overlaps. The bug was that the AABB sweep position was being 
	// used in the early-rejection code that should be using the shape position.
	GTEST_TEST(HeightfieldCCDTests, TestSphereWithOffset)
	{
		int32 Rows = 64;
		int32 Columns = 64;
		FVec3 Scale(100.0, 100.0, 100.0);
		TArray<FReal> Heights;
		Heights.AddZeroed(Rows * Columns);

		TArray<FReal> HeightsCopy = Heights;
		FHeightField Heightfield(MoveTemp(HeightsCopy), TArray<uint8>(), Rows, Columns, Scale);
		const auto& Bounds = Heightfield.BoundingBox();	//Current API forces us to do this to cache the bounds

		// NOTE: Sphere center 50cm up
		TSphere<FReal, 3> Sphere(FVec3(0.0, 0.0, 50.0), 50.0);

		FReal TOI, Phi;
		FVec3 Position, Normal, FaceNormal;
		int32 FaceIdx = 0;

		// NOTE: Start height at 10, rather than 60 in TestSphere
		const FRigidTransform3 Start(FVec3(0, 0, 10), FRotation3::FromIdentity());
		const FVec3 Dir(0, 0, -1);
		const FReal CCDIgnorePenetration = 30;
		const FReal CCDTargetPenetration = 0;
		
		// The first sweep should be less than the ignore threshold so get no hit
		const FReal CCDSweepLengthA = 30;
		const bool bHitA = Heightfield.SweepGeomCCD(Sphere, Start, Dir, CCDSweepLengthA, CCDIgnorePenetration, CCDTargetPenetration, TOI, Phi, Position, Normal, FaceIdx, FaceNormal);
		EXPECT_FALSE(bHitA);

		// The second sweep should be greater than the ignore threshold so we get a hit
		const FReal CCDSweepLengthB = 50;
		const bool bHitB = Heightfield.SweepGeomCCD(Sphere, Start, Dir, CCDSweepLengthB, CCDIgnorePenetration, CCDTargetPenetration, TOI, Phi, Position, Normal, FaceIdx, FaceNormal);
		EXPECT_TRUE(bHitB);
		EXPECT_NEAR(TOI, 0.2, UE_KINDA_SMALL_NUMBER);	// 10 / 50
		EXPECT_NEAR(Phi, 0.0, UE_KINDA_SMALL_NUMBER);	// We are not initially penetrating
	}
}