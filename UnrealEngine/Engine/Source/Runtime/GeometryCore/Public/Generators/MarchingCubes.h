// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3Sharp MarchingCubesPro

#pragma once

#include "Async/ParallelFor.h"
#include "BoxTypes.h"
#include "CompGeom/PolygonTriangulation.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"
#include "IndexTypes.h"
#include "IntBoxTypes.h"
#include "IntVectorTypes.h"
#include "Math/UnrealMathSSE.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "MathUtil.h"
#include "MeshShapeGenerator.h"
#include "Misc/AssertionMacros.h"
#include "Misc/ScopeLock.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Spatial/BlockedDenseGrid3.h"
#include "Spatial/DenseGrid3.h"
#include "Templates/Function.h"
#include "Templates/TypeHash.h"
#include "Templates/UnrealTemplate.h"
#include "Util/IndexUtil.h"
#include "VectorTypes.h"

#include <atomic>

namespace UE
{
namespace Geometry
{

using namespace UE::Math;

enum class /*GEOMETRYCORE_API*/ ERootfindingModes
{
	SingleLerp,
	LerpSteps,
	Bisection
};

class /*GEOMETRYCORE_API*/ FMarchingCubes : public FMeshShapeGenerator
{
public:
	/**
	*  this is the function we will evaluate
	*/
	TFunction<double(TVector<double>)> Implicit;

	/**
	*  mesh surface will be at this isovalue. Normally 0 unless you want
	*  offset surface or field is not a distance-field.
	*/
	double IsoValue = 0;

	/** bounding-box we will mesh inside of. We use the min-corner and
	 *  the width/height/depth, but do not clamp vertices to stay within max-corner,
	 *  we may spill one cell over
	 */
	TAxisAlignedBox3<double> Bounds;

	/**
	 *  Length of edges of cubes that are marching.
	 *  currently, # of cells along axis = (int)(bounds_dimension / CellSize) + 1
	 */
	double CubeSize = 0.1;

	/**
	 *  Use multi-threading? Generally a good idea unless problem is very small or
	 *  you are multi-threading at a higher level (which may be more efficient)
	 */
	bool bParallelCompute = true;

	/**
	 * If true, code will assume that Implicit() is expensive enough that it is worth it to cache
	 * evaluations when possible. For something simple like evaluation of an SDF defined by a discrete
	 * grid, this is generally not worth the overhead.
	 */
	bool bEnableValueCaching = true;

	/**
	 * Max number of cells on any dimension; if exceeded, CubeSize will be automatically increased to fix
	 */
	int SafetyMaxDimension = 4096;

	/**
	 *  Which rootfinding method will be used to converge on surface along edges
	 */
	ERootfindingModes RootMode = ERootfindingModes::SingleLerp;

	/**
	 *  number of iterations of rootfinding method (ignored for SingleLerp)
	 */
	int RootModeSteps = 5;


	/** if this function returns true, we should abort calculation */
	TFunction<bool(void)> CancelF = []() { return false; };

	/*
	 * Outputs
	 */

	// cube indices range from [Origin,CellDimensions)   
	FVector3i CellDimensions;


	FMarchingCubes()
	{
		Bounds = TAxisAlignedBox3<double>(TVector<double>::Zero(), 8);
		CubeSize = 0.25;
	}

	virtual ~FMarchingCubes()
	{
	}

	bool Validate()
	{
		return CubeSize > 0 && FMath::IsFinite(CubeSize) && !Bounds.IsEmpty() && FMath::IsFinite(Bounds.MaxDim());
	}

	/**
	*  Run MC algorithm and generate Output mesh
	*/
	FMeshShapeGenerator& Generate() override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Geometry_MCMesh_Generate);

		if (!ensure(Validate()))
		{
			return *this;
		}

		SetDimensions();
		GridBounds = FAxisAlignedBox3i(FVector3i::Zero(), CellDimensions - FVector3i(1,1,1)); // grid bounds are inclusive

		if (bEnableValueCaching)
		{
			BlockedCornerValuesGrid.Reset(CellDimensions.X + 1, CellDimensions.Y + 1, CellDimensions.Z + 1, FMathf::MaxReal);
		}
		InitHashTables();
		ResetMesh();

		if (bParallelCompute) 
		{
			generate_parallel();
		} 
		else 
		{
			generate_basic();
		}

		// finalize mesh
		BuildMesh();

		return *this;
	}


	FMeshShapeGenerator& GenerateContinuation(TArrayView<const FVector3d> Seeds)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Geometry_MCMesh_GenerateContinuation);

		if (!ensure(Validate()))
		{
			return *this;
		}

		SetDimensions();
		GridBounds = FAxisAlignedBox3i(FVector3i::Zero(), CellDimensions - FVector3i(1,1,1)); // grid bounds are inclusive

		InitHashTables();
		ResetMesh();

		if (LastGridBounds != GridBounds)
		{
			if (bEnableValueCaching)
			{
				BlockedCornerValuesGrid.Reset(CellDimensions.X + 1, CellDimensions.Y + 1, CellDimensions.Z + 1, FMathf::MaxReal);
			}
			if (bParallelCompute)
			{
				BlockedDoneCells.Reset(CellDimensions.X, CellDimensions.Y, CellDimensions.Z, 0);
			}
		}
		else
		{
			if (bEnableValueCaching)
			{
				BlockedCornerValuesGrid.Resize(CellDimensions.X + 1, CellDimensions.Y + 1, CellDimensions.Z + 1);
			}
			if (bParallelCompute)
			{
				BlockedDoneCells.Resize(CellDimensions.X, CellDimensions.Y, CellDimensions.Z);
			}
		}

		if (bParallelCompute) 
		{
			generate_continuation_parallel(Seeds);
		} 
		else 
		{
			generate_continuation(Seeds);
		}

		// finalize mesh
		BuildMesh();

		LastGridBounds = GridBounds;

		return *this;
	}


protected:


	FAxisAlignedBox3i GridBounds;
	FAxisAlignedBox3i LastGridBounds;


	// we pass Cells around, this makes code cleaner
	struct FGridCell
	{
		// TODO we do not actually need to store i, we just need the min-corner!
		FVector3i i[8];    // indices of corners of cell
		double f[8];      // field values at corners
	};

	void SetDimensions()
	{
		int NX = (int)(Bounds.Width() / CubeSize) + 1;
		int NY = (int)(Bounds.Height() / CubeSize) + 1;
		int NZ = (int)(Bounds.Depth() / CubeSize) + 1;
		int MaxDim = FMath::Max3(NX, NY, NZ);
		if (!ensure(MaxDim <= SafetyMaxDimension))
		{
			CubeSize = Bounds.MaxDim() / double(SafetyMaxDimension - 1);
			NX = (int)(Bounds.Width() / CubeSize) + 1;
			NY = (int)(Bounds.Height() / CubeSize) + 1;
			NZ = (int)(Bounds.Depth() / CubeSize) + 1;
		}
		CellDimensions = FVector3i(NX, NY, NZ);
	}

	void corner_pos(const FVector3i& IJK, TVector<double>& Pos)
	{
		Pos.X = Bounds.Min.X + CubeSize * IJK.X;
		Pos.Y = Bounds.Min.Y + CubeSize * IJK.Y;
		Pos.Z = Bounds.Min.Z + CubeSize * IJK.Z;
	}
	TVector<double> corner_pos(const FVector3i& IJK)
	{
		return TVector<double>(Bounds.Min.X + CubeSize * IJK.X,
			Bounds.Min.Y + CubeSize * IJK.Y,
			Bounds.Min.Z + CubeSize * IJK.Z);
	}
	FVector3i cell_index(const TVector<double>& Pos)
	{
		return FVector3i(
			(int)((Pos.X - Bounds.Min.X) / CubeSize),
			(int)((Pos.Y - Bounds.Min.Y) / CubeSize),
			(int)((Pos.Z - Bounds.Min.Z) / CubeSize));
	}



	//
	// corner and edge hash functions, these pack the coordinate
	// integers into 16-bits, so max of 65536 in any dimension.
	//


	int64 corner_hash(const FVector3i& Idx)
	{
		return ((int64)Idx.X&0xFFFF) | (((int64)Idx.Y&0xFFFF) << 16) | (((int64)Idx.Z&0xFFFF) << 32);
	}
	int64 corner_hash(int X, int Y, int Z)
	{
		return ((int64)X & 0xFFFF) | (((int64)Y & 0xFFFF) << 16) | (((int64)Z & 0xFFFF) << 32);
	}

	const int64 EDGE_X = int64(1) << 60;
	const int64 EDGE_Y = int64(1) << 61;
	const int64 EDGE_Z = int64(1) << 62;

	int64 edge_hash(const FVector3i& Idx1, const FVector3i& Idx2)
	{
		if ( Idx1.X != Idx2.X )
		{
			int xlo = FMath::Min(Idx1.X, Idx2.X);
			return corner_hash(xlo, Idx1.Y, Idx1.Z) | EDGE_X;
		}
		else if ( Idx1.Y != Idx2.Y )
		{
			int ylo = FMath::Min(Idx1.Y, Idx2.Y);
			return corner_hash(Idx1.X, ylo, Idx1.Z) | EDGE_Y;
		}
		else
		{
			int zlo = FMath::Min(Idx1.Z, Idx2.Z);
			return corner_hash(Idx1.X, Idx1.Y, zlo) | EDGE_Z;
		}
	}



	//
	// Hash table for edge vertices
	//

	const int64 NumEdgeVertexSections = 64;
	TArray<TMap<int64, int>> EdgeVertexSections;
	TArray<FCriticalSection> EdgeVertexSectionLocks;
	
	int FindVertexID(int64 hash)
	{
		int32 SectionIndex = (int32)(hash % (NumEdgeVertexSections - 1));
		FScopeLock Lock(&EdgeVertexSectionLocks[SectionIndex]);
		int* Found = EdgeVertexSections[SectionIndex].Find(hash);
		return (Found != nullptr) ? *Found : IndexConstants::InvalidID;
	}

	int AppendOrFindVertexID(int64 hash, TVector<double> Pos)
	{
		int32 SectionIndex = (int32)(hash % (NumEdgeVertexSections - 1));
		FScopeLock Lock(&EdgeVertexSectionLocks[SectionIndex]);
		int* FoundVID = EdgeVertexSections[SectionIndex].Find(hash);
		if (FoundVID != nullptr)
		{
			return *FoundVID;
		}
		int NewVID = append_vertex(Pos, hash);
		EdgeVertexSections[SectionIndex].Add(hash, NewVID);
		return NewVID;
	}


	int edge_vertex_id(const FVector3i& Idx1, const FVector3i& Idx2, double F1, double F2)
	{
		int64 hash = edge_hash(Idx1, Idx2);

		int foundvid = FindVertexID(hash);
		if (foundvid != IndexConstants::InvalidID)
		{
			return foundvid;
		}

		// ok this is a bit messy. We do not want to lock the entire hash table 
		// while we do find_iso. However it is possible that during this time we
		// are unlocked we have re-entered with the same edge. So when we
		// re-acquire the lock we need to check again that we have not already
		// computed this edge, otherwise we will end up with duplicate vertices!

		TVector<double> pa = TVector<double>::Zero(), pb = TVector<double>::Zero();
		corner_pos(Idx1, pa);
		corner_pos(Idx2, pb);
		TVector<double> Pos = TVector<double>::Zero();
		find_iso(pa, pb, F1, F2, Pos);

		return AppendOrFindVertexID(hash, Pos);
	}








	//
	// store corner values in pre-allocated grid that has
	// FMathf::MaxReal as sentinel. 
	// (note this is float grid, not double...)
	//

	FBlockedDenseGrid3f BlockedCornerValuesGrid;

	double corner_value_grid_parallel(const FVector3i& Idx)
	{
		// note: it's possible to have a race here, where multiple threads might both
		// GetValue, see that the value is invalid, and compute and set it. Since Implicit(V)
		// is (intended to be) determinstic, they will compute the same value, so this doesn't cause an error, 
		// it just wastes a bit of computation time. Since it is common for multiple corners to be
		// in the same grid-block, and locking is on the block level, it is (or was in some testing)
		// better to not lock the entire block while Implicit(V) computed, at the cost of
		// some wasted evals in some cases.

		float CurrentValue = BlockedCornerValuesGrid.GetValueThreadSafe(Idx.X, Idx.Y, Idx.Z);
		if (CurrentValue != FMathf::MaxReal)
		{
			return (double)CurrentValue;
		}

		TVector<double> V = corner_pos(Idx);
		CurrentValue = (float)Implicit(V);

		BlockedCornerValuesGrid.SetValueThreadSafe(Idx.X, Idx.Y, Idx.Z, CurrentValue);

		return (double)CurrentValue;
	}
	double corner_value_grid(const FVector3i& Idx)
	{
		if (bParallelCompute)
		{
			return corner_value_grid_parallel(Idx);
		}

		float CurrentValue = BlockedCornerValuesGrid.GetValue(Idx.X, Idx.Y, Idx.Z);
		if (CurrentValue != FMathf::MaxReal)
		{
			return (double)CurrentValue;
		}

		TVector<double> V = corner_pos(Idx);
		CurrentValue = (float)Implicit(V);

		BlockedCornerValuesGrid.SetValue(Idx.X, Idx.Y, Idx.Z, CurrentValue);

		return (double)CurrentValue;
	}

	void initialize_cell_values_grid(FGridCell& Cell, bool Shift)
	{
		if (Shift)
		{
			Cell.f[1] = corner_value_grid(Cell.i[1]);
			Cell.f[2] = corner_value_grid(Cell.i[2]);
			Cell.f[5] = corner_value_grid(Cell.i[5]);
			Cell.f[6] = corner_value_grid(Cell.i[6]);
		}
		else
		{
			for (int i = 0; i < 8; ++i)
			{
				Cell.f[i] = corner_value_grid(Cell.i[i]);
			}
		}
	}



	//
	// explicitly compute corner values as necessary
	//
	//

	double corner_value_nohash(const FVector3i& Idx) 
	{
		TVector<double> V = corner_pos(Idx);
		return Implicit(V);
	}
	void initialize_cell_values_nohash(FGridCell& Cell, bool Shift)
	{
		if (Shift)
		{
			Cell.f[1] = corner_value_nohash(Cell.i[1]);
			Cell.f[2] = corner_value_nohash(Cell.i[2]);
			Cell.f[5] = corner_value_nohash(Cell.i[5]);
			Cell.f[6] = corner_value_nohash(Cell.i[6]);
		}
		else
		{
			for (int i = 0; i < 8; ++i)
			{
				Cell.f[i] = corner_value_nohash(Cell.i[i]);
			}
		}
	}



	/**
	*  compute 3D corner-positions and field values for cell at index
	*/
	void initialize_cell(FGridCell& Cell, const FVector3i& Idx)
	{
		Cell.i[0] = FVector3i(Idx.X + 0, Idx.Y + 0, Idx.Z + 0);
		Cell.i[1] = FVector3i(Idx.X + 1, Idx.Y + 0, Idx.Z + 0);
		Cell.i[2] = FVector3i(Idx.X + 1, Idx.Y + 0, Idx.Z + 1);
		Cell.i[3] = FVector3i(Idx.X + 0, Idx.Y + 0, Idx.Z + 1);
		Cell.i[4] = FVector3i(Idx.X + 0, Idx.Y + 1, Idx.Z + 0);
		Cell.i[5] = FVector3i(Idx.X + 1, Idx.Y + 1, Idx.Z + 0);
		Cell.i[6] = FVector3i(Idx.X + 1, Idx.Y + 1, Idx.Z + 1);
		Cell.i[7] = FVector3i(Idx.X + 0, Idx.Y + 1, Idx.Z + 1);

		if (bEnableValueCaching)
		{
			initialize_cell_values_grid(Cell, false);
		}
		else
		{
			initialize_cell_values_nohash(Cell, false);
		}
	}


	// assume we just want to slide cell at XIdx-1 to cell at XIdx, while keeping
	// yi and ZIdx constant. Then only x-coords change, and we have already 
	// computed half the values
	void shift_cell_x(FGridCell& Cell, int XIdx)
	{
		Cell.f[0] = Cell.f[1];
		Cell.f[3] = Cell.f[2];
		Cell.f[4] = Cell.f[5];
		Cell.f[7] = Cell.f[6];

		Cell.i[0].X = XIdx; Cell.i[1].X = XIdx+1; Cell.i[2].X = XIdx+1; Cell.i[3].X = XIdx;
		Cell.i[4].X = XIdx; Cell.i[5].X = XIdx+1; Cell.i[6].X = XIdx+1; Cell.i[7].X = XIdx;

		if (bEnableValueCaching)
		{
			initialize_cell_values_grid(Cell, true);
		}
		else
		{
			initialize_cell_values_nohash(Cell, true);
		}
	}


	void InitHashTables()
	{
		EdgeVertexSections.Reset();
		EdgeVertexSections.SetNum((int32)NumEdgeVertexSections);
		EdgeVertexSectionLocks.Reset();
		EdgeVertexSectionLocks.SetNum((int32)NumEdgeVertexSections);
	}


	bool parallel_mesh_access = false;


	/**
	*  processing z-slabs of cells in parallel
	*/
	void generate_parallel()
	{
		parallel_mesh_access = true;

		// [TODO] maybe shouldn't alway use Z axis here?
		ParallelFor(CellDimensions.Z, [this](int32 ZIdx)
		{
			FGridCell Cell;
			int vertTArray[12];
			for (int yi = 0; yi < CellDimensions.Y; ++yi)
			{
				if (CancelF())
				{
					return;
				}
				// compute full cell at x=0, then slide along x row, which saves half of value computes
				FVector3i Idx(0, yi, ZIdx);
				initialize_cell(Cell, Idx);
				polygonize_cell(Cell, vertTArray);
				for (int XIdx = 1; XIdx < CellDimensions.X; ++XIdx)
				{
					shift_cell_x(Cell, XIdx);
					polygonize_cell(Cell, vertTArray);
				}
			}
		});


		parallel_mesh_access = false;
	}




	/**
	*  fully sequential version, no threading
	*/
	void generate_basic()
	{
		FGridCell Cell;
		int vertTArray[12];

		for (int ZIdx = 0; ZIdx < CellDimensions.Z; ++ZIdx)
		{
			for (int yi = 0; yi < CellDimensions.Y; ++yi)
			{
				if (CancelF())
				{
					return;
				}
				// compute full Cell at x=0, then slide along x row, which saves half of value computes
				FVector3i Idx(0, yi, ZIdx);
				initialize_cell(Cell, Idx);
				polygonize_cell(Cell, vertTArray);
				for (int XIdx = 1; XIdx < CellDimensions.X; ++XIdx)
				{
					shift_cell_x(Cell, XIdx);
					polygonize_cell(Cell, vertTArray);
				}

			}
		}
	}




	/**
	*  fully sequential version, no threading
	*/
	void generate_continuation(TArrayView<const FVector3d> Seeds)
	{
		FGridCell Cell;
		int vertTArray[12];

		BlockedDoneCells.Reset(CellDimensions.X, CellDimensions.Y, CellDimensions.Z, 0);

		TArray<FVector3i> stack;

		for (FVector3d seed : Seeds)
		{
			FVector3i seed_idx = cell_index(seed);
			if (!BlockedDoneCells.IsValidIndex(seed_idx) || BlockedDoneCells.GetValue(seed_idx.X, seed_idx.Y, seed_idx.Z) == 1)
			{
				continue;
			}
			stack.Add(seed_idx);
			BlockedDoneCells.SetValue(seed_idx.X, seed_idx.Y, seed_idx.Z, 1);

			while ( stack.Num() > 0 )
			{
				FVector3i Idx = stack[stack.Num()-1]; 
				stack.RemoveAt(stack.Num()-1);
				if (CancelF())
				{
					return;
				}

				initialize_cell(Cell, Idx);
				if ( polygonize_cell(Cell, vertTArray) )
				{     // found crossing
					for ( FVector3i o : IndexUtil::GridOffsets6 )
					{
						FVector3i nbr_idx = Idx + o;
						if (GridBounds.Contains(nbr_idx) && BlockedDoneCells.GetValue(nbr_idx.X, nbr_idx.Y, nbr_idx.Z) == 0)
						{
							stack.Add(nbr_idx);
							BlockedDoneCells.SetValue(nbr_idx.X, nbr_idx.Y, nbr_idx.Z, 1);
						}
					}
				}
			}
		}
	}




	/**
	*  parallel seed evaluation
	*/
	void generate_continuation_parallel(TArrayView<const FVector3d> Seeds)
	{
		// Parallel marching cubes based on continuation (ie surface-following / front propagation) 
		// can have quite poor multithreaded performance depending on the ordering of the region-growing.
		// For example processing each seed point in parallel can result in one thread that
		// takes significantly longer than others, if the seed point distribution is such
		// that a large part of the surface is only reachable from one seed (or gets
		// "cut off" by a thin area, etc). So we want to basically do front-marching in
		// parallel passes. However this can result in a large number of very short passes
		// if the front ends up with many small regions, etc. So, in the implementation below,
		// each "seed cell" is allowed to process up to N neighbour cells before terminating, at 
		// which point any cells remaining on the active front are added as seed cells for the next pass.
		// This seems to provide good utilization, however more profiling may be needed.
		// (In particular, if the active cell list is large, some blocks of the ParallelFor
		//  may still end up doing much more work than others)


		// set this flag so that append vertex/triangle operations will lock the mesh
		parallel_mesh_access = true;

		// maximum number of cells to process in each ParallelFor iteration
		static constexpr int MaxNeighboursPerActiveCell = 100;

		// list of active cells on the MC front to process in the next pass
		TArray<FVector3i> ActiveCells;

		// initially push list of seed-point cells onto the ActiveCells list
		for (FVector3d Seed : Seeds)
		{
			FVector3i seed_idx = cell_index(Seed);
			if ( BlockedDoneCells.IsValidIndex(seed_idx) && set_cell_if_not_done(seed_idx) )
			{
				ActiveCells.Add(seed_idx);
			}
		}

		// new active cells will be accumulated in each parallel-pass
		TArray<FVector3i> NewActiveCells;
		FCriticalSection NewActiveCellsLock;

		while (ActiveCells.Num() > 0)
		{
			// process all active cells
			ParallelFor(ActiveCells.Num(), [&](int32 Idx)
			{
				FVector3i InitialCellIndex = ActiveCells[Idx];
				if (CancelF())
				{
					return;
				}

				FGridCell TempCell;
				int TempArray[12];

				// we will process up to MaxNeighboursPerActiveCell new cells in each ParallelFor iteration
				TArray<FVector3i, TInlineAllocator<64>> LocalStack;
				LocalStack.Add(InitialCellIndex);
				int32 CellsProcessed = 0;

				while (LocalStack.Num() > 0 && CellsProcessed++ < MaxNeighboursPerActiveCell)
				{
					FVector3i CellIndex = LocalStack.Pop(EAllowShrinking::No);

					initialize_cell(TempCell, CellIndex);
					if (polygonize_cell(TempCell, TempArray))
					{
						// found crossing
						for (FVector3i GridOffset : IndexUtil::GridOffsets6)
						{
							FVector3i NbrCellIndex = CellIndex + GridOffset;
							if (GridBounds.Contains(NbrCellIndex))
							{
								if (set_cell_if_not_done(NbrCellIndex) == true)
								{ 
									LocalStack.Add(NbrCellIndex);
								}
							}
						}
					}
				}

				// if stack is not empty, ie hit MaxNeighboursPerActiveCell, add remaining cells to next-pass Active list
				if (LocalStack.Num() > 0)
				{
					NewActiveCellsLock.Lock();
					NewActiveCells.Append(LocalStack);
					NewActiveCellsLock.Unlock();
				}

			});

			ActiveCells.Reset();
			if (NewActiveCells.Num() > 0)
			{
				Swap(ActiveCells, NewActiveCells);
			}
		}

		parallel_mesh_access = false;
	}


	FBlockedDenseGrid3i BlockedDoneCells;

	bool set_cell_if_not_done(const FVector3i& Idx)
	{
		bool was_set = false;
		{
			BlockedDoneCells.ProcessValueThreadSafe(Idx.X, Idx.Y, Idx.Z, [&](int& CellValue) 
			{
				if (CellValue == 0)
				{
					CellValue = 1;
					was_set = true;
				}
			});
		}
		return was_set;
	}





	/**
	*  find edge crossings and generate triangles for this cell
	*/
	bool polygonize_cell(FGridCell& Cell, int VertIndexArray[])
	{
		// construct bits of index into edge table, where bit for each
		// corner is 1 if that value is < isovalue.
		// This tell us which edges have sign-crossings, and the int value
		// of the bitmap is an index into the edge and triangle tables
		int cubeindex = 0, Shift = 1;
		for (int i = 0; i < 8; ++i)
		{
			if (Cell.f[i] < IsoValue)
			{
				cubeindex |= Shift;
			}
			Shift <<= 1;
		}

		// no crossings!
		if (EdgeTable[cubeindex] == 0)
		{
			return false;
		}

		// check each bit of value in edge table. If it is 1, we
		// have a crossing on that edge. Look up the indices of this
		// edge and find the intersection point along it
		Shift = 1;
		TVector<double> pa = TVector<double>::Zero(), pb = TVector<double>::Zero();
		for (int i = 0; i <= 11; i++)
		{
			if ((EdgeTable[cubeindex] & Shift) != 0)
			{
				int a = EdgeIndices[i][0], b = EdgeIndices[i][1];
				VertIndexArray[i] = edge_vertex_id(Cell.i[a], Cell.i[b], Cell.f[a], Cell.f[b]);
			}
			Shift <<= 1;
		}

		int64 CellHash = corner_hash(Cell.i[0]);

		// now iterate through the set of triangles in TriTable for this cube,
		// and emit triangles using the vertices we found.
		int tri_count = 0;
		for (int i = 0; TriTable[cubeindex][i] != -1; i += 3)
		{
			int ta = TriTable[cubeindex][i];
			int tb = TriTable[cubeindex][i + 1];
			int tc = TriTable[cubeindex][i + 2];
			int a = VertIndexArray[ta], b = VertIndexArray[tb], c = VertIndexArray[tc];

			// if a corner is within tolerance of isovalue, then some triangles
			// will be degenerate, and we can skip them w/o resulting in cracks (right?)
			// !! this should never happen anymore...artifact of old hashtable impl
			if (!ensure(a != b && a != c && b != c))
			{
				continue;
			}

			append_triangle(a, b, c, CellHash);
			tri_count++;
		}

		return (tri_count > 0);
	}


	struct FIndexedVertex
	{
		int32 Index;
		FVector3d Position;
	};
	std::atomic<int32> VertexCounter;

	int64 NumVertexSections = 64;
	TArray<FCriticalSection> VertexSectionLocks;
	TArray<TArray<FIndexedVertex>> VertexSectionLists;
	int GetVertexSectionIndex(int64 hash)
	{
		return (int32)(hash % (NumVertexSections - 1));
	}

	/**
	*  add vertex to mesh, with locking if we are computing in parallel
	*/
	int append_vertex(TVector<double> V, int64 CellHash)
	{
		int SectionIndex = GetVertexSectionIndex(CellHash);
		int32 NewIndex = VertexCounter++;

		if (parallel_mesh_access)
		{
			FScopeLock Lock(&VertexSectionLocks[SectionIndex]);
			VertexSectionLists[SectionIndex].Add(FIndexedVertex{ NewIndex, V });
		}
		else
		{
			VertexSectionLists[SectionIndex].Add(FIndexedVertex{ NewIndex, V });
		}

		return NewIndex;
	}


	int64 NumTriangleSections = 64;
	TArray<FCriticalSection> TriangleSectionLocks;
	TArray<TArray<FIndex3i>> TriangleSectionLists;
	int GetTriangleSectionIndex(int64 hash)
	{
		return (int32)(hash % (NumTriangleSections - 1));
	}

	/**
	*  add triangle to mesh, with locking if we are computing in parallel
	*/
	void append_triangle(int A, int B, int C, int64 CellHash)
	{
		int SectionIndex = GetTriangleSectionIndex(CellHash);
		if (parallel_mesh_access)
		{
			FScopeLock Lock(&TriangleSectionLocks[SectionIndex]);
			TriangleSectionLists[SectionIndex].Add(FIndex3i(A, B, C));
		}
		else
		{
			TriangleSectionLists[SectionIndex].Add(FIndex3i(A, B, C));
		}
	}


	/**
	 * Reset internal mesh-assembly data structures
	 */
	void ResetMesh()
	{
		VertexSectionLocks.SetNum((int32)NumVertexSections);
		VertexSectionLists.Reset();
		VertexSectionLists.SetNum((int32)NumVertexSections);
		VertexCounter = 0;

		TriangleSectionLocks.SetNum((int32)NumTriangleSections);
		TriangleSectionLists.Reset();
		TriangleSectionLists.SetNum((int32)NumTriangleSections);
	}

	/**
	 * Populate FMeshShapeGenerator data structures from accumulated
	 * vertex/triangle sets
	 */
	void BuildMesh()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Geometry_MCMesh_BuildMesh);

		int32 NumVertices = VertexCounter;
		TArray<FVector3d> VertexBuffer;
		VertexBuffer.SetNum(NumVertices);
		for (const TArray<FIndexedVertex>& VertexList : VertexSectionLists)
		{
			for (FIndexedVertex Vtx : VertexList)
			{
				VertexBuffer[Vtx.Index] = Vtx.Position;
			}
		}
		for (int32 k = 0; k < NumVertices; ++k)
		{
			int32 vid = AppendVertex(VertexBuffer[k]);
		}

		for (const TArray<FIndex3i>& TriangleList : TriangleSectionLists)
		{
			for (FIndex3i Tri : TriangleList)
			{
				AppendTriangle(Tri.A, Tri.B, Tri.C);
			}
		}
	}



	/**
	*  root-find the intersection along edge from f(P1)=ValP1 to f(P2)=ValP2
	*/
	void find_iso(const TVector<double>& P1, const TVector<double>& P2, double ValP1, double ValP2, TVector<double>& PIso)
	{
		// Ok, this is a bit hacky but seems to work? If both isovalues
		// are the same, we just return the midpoint. If one is nearly zero, we can
		// but assume that's where the surface is. *However* if we return that point exactly,
		// we can get nonmanifold vertices, because multiple fans may connect there. 
		// Since FDynamicMesh3 disallows that, it results in holes. So we pull 
		// slightly towards the other point along this edge. This means we will get
		// repeated nearly-coincident vertices, but the mesh will be manifold.
		const double dt = 0.999999;
		if (FMath::Abs(ValP1 - ValP2) < 0.00001)
		{
			PIso = (P1 + P2) * 0.5;
			return;
		}
		if (FMath::Abs(IsoValue - ValP1) < 0.00001)
		{
			PIso = dt * P1 + (1.0 - dt) * P2;
			return;
		}
		if (FMath::Abs(IsoValue - ValP2) < 0.00001)
		{
			PIso = (dt) * P2 + (1.0 - dt) * P1;
			return;
		}

		// Note: if we don't maintain min/max order here, then numerical error means
		//   that hashing on point x/y/z doesn't work
		TVector<double> a = P1, b = P2;
		double fa = ValP1, fb = ValP2;
		if (ValP2 < ValP1)
		{
			a = P2; b = P1;
			fb = ValP1; fa = ValP2;
		}

		// converge on root
		if (RootMode == ERootfindingModes::Bisection)
		{
			for (int k = 0; k < RootModeSteps; ++k)
			{
				PIso.X = (a.X + b.X) * 0.5; PIso.Y = (a.Y + b.Y) * 0.5; PIso.Z = (a.Z + b.Z) * 0.5;
				double mid_f = Implicit(PIso);
				if (mid_f < IsoValue)
				{
					a = PIso; fa = mid_f;
				}
				else
				{
					b = PIso; fb = mid_f;
				}
			}
			PIso = Lerp(a, b, 0.5);

		}
		else
		{
			double mu = 0;
			if (RootMode == ERootfindingModes::LerpSteps)
			{
				for (int k = 0; k < RootModeSteps; ++k)
				{
					mu = FMathd::Clamp((IsoValue - fa) / (fb - fa), 0.0, 1.0);
					PIso.X = a.X + mu * (b.X - a.X);
					PIso.Y = a.Y + mu * (b.Y - a.Y);
					PIso.Z = a.Z + mu * (b.Z - a.Z);
					double mid_f = Implicit(PIso);
					if (mid_f < IsoValue)
					{
						a = PIso; fa = mid_f;
					}
					else
					{
						b = PIso; fb = mid_f;
					}
				}
			}

			// final lerp
			mu = FMathd::Clamp((IsoValue - fa) / (fb - fa), 0.0, 1.0);
			PIso.X = a.X + mu * (b.X - a.X);
			PIso.Y = a.Y + mu * (b.Y - a.Y);
			PIso.Z = a.Z + mu * (b.Z - a.Z);
		}
	}




	/*
	* Below here are standard marching-cubes tables. 
	*/


	constexpr static int EdgeIndices[12][2] = {
		{0,1}, {1,2}, {2,3}, {3,0}, {4,5}, {5,6}, {6,7}, {7,4}, {0,4}, {1,5}, {2,6}, {3,7}
	};

	constexpr static int EdgeTable[256] = {
		0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
		0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
		0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
		0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
		0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
		0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
		0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
		0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
		0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
		0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
		0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
		0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
		0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
		0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
		0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
		0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
		0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
		0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
		0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
		0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
		0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
		0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
		0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
		0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
		0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
		0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
		0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
		0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
		0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
		0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
		0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
		0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0   };


	constexpr static int TriTable[256][16] =
	{
		{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
		{3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
		{3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
		{3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
		{9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
		{9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
		{2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
		{8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
		{9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
		{4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
		{3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
		{1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
		{4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
		{4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
		{9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
		{5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
		{2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
		{9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
		{0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
		{2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
		{10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
		{4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
		{5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
		{5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
		{9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
		{0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
		{1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
		{10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
		{8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
		{2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
		{7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
		{9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
		{2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
		{11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
		{9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
		{5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
		{11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
		{11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
		{1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
		{9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
		{5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
		{2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
		{0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
		{5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
		{6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
		{3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
		{6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
		{5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
		{1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
		{10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
		{6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
		{8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
		{7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
		{3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
		{5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
		{0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
		{9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
		{8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
		{5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
		{0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
		{6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
		{10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
		{10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
		{8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
		{1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
		{3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
		{0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
		{10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
		{3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
		{6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
		{9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
		{8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
		{3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
		{6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
		{0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
		{10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
		{10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
		{2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
		{7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
		{7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
		{2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
		{1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
		{11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
		{8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
		{0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
		{7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
		{10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
		{2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
		{6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
		{7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
		{2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
		{1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
		{10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
		{10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
		{0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
		{7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
		{6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
		{8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
		{9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
		{6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
		{4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
		{10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
		{8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
		{0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
		{1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
		{8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
		{10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
		{4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
		{10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
		{5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
		{11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
		{9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
		{6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
		{7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
		{3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
		{7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
		{9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
		{3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
		{6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
		{9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
		{1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
		{4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
		{7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
		{6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
		{3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
		{0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
		{6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
		{0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
		{11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
		{6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
		{5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
		{9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
		{1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
		{1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
		{10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
		{0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
		{5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
		{10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
		{11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
		{9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
		{7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
		{2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
		{8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
		{9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
		{9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
		{1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
		{9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
		{9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
		{5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
		{0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
		{10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
		{2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
		{0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
		{0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
		{9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
		{5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
		{3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
		{5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
		{8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
		{0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
		{9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
		{0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
		{1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
		{3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
		{4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
		{9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
		{11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
		{11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
		{2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
		{9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
		{3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
		{1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
		{4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
		{4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
		{0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
		{3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
		{3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
		{0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
		{9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
		{1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
		{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
	};

};


} // end namespace UE::Geometry
} // end namespace UE
