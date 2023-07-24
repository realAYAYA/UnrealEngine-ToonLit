// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3Sharp's CachingMeshSDFImplicit

#pragma once

#include "BoxTypes.h"
#include "MathUtil.h"
#include "VectorTypes.h"
#include "IntVectorTypes.h"

namespace UE
{
namespace Geometry
{


/**
 * Tri-linear interpolant for a 3D dense Grid. Supports Grid translation
 * via GridOrigin, but does not support scaling or rotation. If you need those,
 * you can wrap this in something that does the xform.
 *
 * GridType must have a GetValue() that returns a value to interpolate at a given FVector3i location -- (w/ locations ranging from [0,0,0] to Dimensions (exclusive))
 */
template <class GridType>
class TTriLinearGridInterpolant /*: public BoundedImplicitFunction3d (TODO: consider add ImplicitFunction3d interface concept once more implicit functions are available*/
{
public:
	GridType* Grid;
	FVector3d GridOrigin;
	double CellSize;
	FVector3i Dimensions;

	// value to return if query point is outside Grid (in an SDF
	// outside is usually positive). Need to do math with this value,
	// and cast this value to/from float; use FMathf::MaxReal to avoid overflow
	double Outside = FMathf::MaxReal;

	TTriLinearGridInterpolant(GridType* Grid, FVector3d GridOrigin, double CellSize, FVector3i Dimensions) : Grid(Grid), GridOrigin(GridOrigin), CellSize(CellSize), Dimensions(Dimensions)
	{
	}

	FAxisAlignedBox3d Bounds() const
	{
		return FAxisAlignedBox3d(
			{ GridOrigin.X, GridOrigin.Y, GridOrigin.Z },
			{ GridOrigin.X + CellSize * Dimensions.X,
			  GridOrigin.Y + CellSize * Dimensions.Y,
			  GridOrigin.Z + CellSize * Dimensions.Z });
	}

	FVector3i Cell(const FVector3d& Pt) const
	{
		// compute integer coordinates
		FVector3i CellCoords;
		CellCoords.X = (int)((Pt.X - GridOrigin.X) / CellSize);
		CellCoords.Y = (int)((Pt.Y - GridOrigin.Y) / CellSize);
		CellCoords.Z = (int)((Pt.Z - GridOrigin.Z) / CellSize);

		return CellCoords;
	}

	double Value(const FVector3d& Pt) const
	{
		FVector3d gridPt(
			((Pt.X - GridOrigin.X) / CellSize),
			((Pt.Y - GridOrigin.Y) / CellSize),
			((Pt.Z - GridOrigin.Z) / CellSize));

		// compute integer coordinates
		int X0 = (int)gridPt.X;
		int Y0 = (int)gridPt.Y, Y1 = Y0 + 1;
		int Z0 = (int)gridPt.Z, Z1 = Z0 + 1;

		// clamp to Grid
		if (X0 < 0 || (X0 + 1) >= Dimensions.X ||
			Y0 < 0 || Y1 >= Dimensions.Y ||
			Z0 < 0 || Z1 >= Dimensions.Z)
		{
			return Outside;
		}

		// convert double coords to [0,1] range
		double fAx = gridPt.X - (double)X0;
		double fAy = gridPt.Y - (double)Y0;
		double fAz = gridPt.Z - (double)Z0;
		double OneMinusfAx = 1.0 - fAx;

		// compute trilinear interpolant. The code below tries to do this with the fewest 
		// number of variables, in hopes that optimizer will be clever about re-using registers, etc.
		// Commented code at bottom is fully-expanded version.
		// [TODO] it is possible to implement lerps here as A+(B-A)*t, saving a multiply and a variable.
		//   This is numerically worse, but since the Grid values are floats and
		//   we are computing in doubles, does it matter?
		double xa, xb;

		get_value_pair(X0, Y0, Z0, xa, xb);
		double yz = (1 - fAy) * (1 - fAz);
		double sum = (OneMinusfAx * xa + fAx * xb) * yz;

		get_value_pair(X0, Y0, Z1, xa, xb);
		yz = (1 - fAy) * (fAz);
		sum += (OneMinusfAx * xa + fAx * xb) * yz;

		get_value_pair(X0, Y1, Z0, xa, xb);
		yz = (fAy) * (1 - fAz);
		sum += (OneMinusfAx * xa + fAx * xb) * yz;

		get_value_pair(X0, Y1, Z1, xa, xb);
		yz = (fAy) * (fAz);
		sum += (OneMinusfAx * xa + fAx * xb) * yz;

		return sum;

		// fV### is Grid cell corner index
		//return
		//    fV000 * (1 - fAx) * (1 - fAy) * (1 - fAz) +
		//    fV001 * (1 - fAx) * (1 - fAy) * (fAz) +
		//    fV010 * (1 - fAx) * (fAy) * (1 - fAz) +
		//    fV011 * (1 - fAx) * (fAy) * (fAz) +
		//    fV100 * (fAx) * (1 - fAy) * (1 - fAz) +
		//    fV101 * (fAx) * (1 - fAy) * (fAz) +
		//    fV110 * (fAx) * (fAy) * (1 - fAz) +
		//    fV111 * (fAx) * (fAy) * (fAz);
	}


protected:
	void get_value_pair(int I, int J, int K, double& A, double& B) const
	{
		A = (double)Grid->GetValue(FVector3i(I, J, K));
		B = (double)Grid->GetValue(FVector3i(I + 1, J, K));
	}


public:
	FVector3d Gradient(const FVector3d& Pt) const
	{
		FVector3d gridPt = FVector3d(
			((Pt.X - GridOrigin.X) / CellSize),
			((Pt.Y - GridOrigin.Y) / CellSize),
			((Pt.Z - GridOrigin.Z) / CellSize));

		// clamp to Grid
		if (gridPt.X < 0 || gridPt.X >= Dimensions.X - 1 ||
			gridPt.Y < 0 || gridPt.Y >= Dimensions.Y - 1 ||
			gridPt.Z < 0 || gridPt.Z >= Dimensions.Z - 1)
		{
			return FVector3d::Zero();
		}

		// compute integer coordinates
		int X0 = (int)gridPt.X;
		int Y0 = (int)gridPt.Y, Y1 = Y0 + 1;
		int Z0 = (int)gridPt.Z, Z1 = Z0 + 1;

		// convert double coords to [0,1] range
		double fAx = gridPt.X - (double)X0;
		double fAy = gridPt.Y - (double)Y0;
		double fAz = gridPt.Z - (double)Z0;

		double fV000, fV100;
		get_value_pair(X0, Y0, Z0, fV000, fV100);
		double fV010, fV110;
		get_value_pair(X0, Y1, Z0, fV010, fV110);
		double fV001, fV101;
		get_value_pair(X0, Y0, Z1, fV001, fV101);
		double fV011, fV111;
		get_value_pair(X0, Y1, Z1, fV011, fV111);

		// [TODO] can re-order this to vastly reduce number of ops!
		double gradX =
			-fV000 * (1 - fAy) * (1 - fAz) +
			-fV001 * (1 - fAy) * (fAz)+
			-fV010 * (fAy) * (1 - fAz) +
			-fV011 * (fAy) * (fAz)+
			fV100 * (1 - fAy) * (1 - fAz) +
			fV101 * (1 - fAy) * (fAz)+
			fV110 * (fAy) * (1 - fAz) +
			fV111 * (fAy) * (fAz);

		double gradY =
			-fV000 * (1 - fAx) * (1 - fAz) +
			-fV001 * (1 - fAx) * (fAz)+
			fV010 * (1 - fAx) * (1 - fAz) +
			fV011 * (1 - fAx) * (fAz)+
			-fV100 * (fAx) * (1 - fAz) +
			-fV101 * (fAx) * (fAz)+
			fV110 * (fAx) * (1 - fAz) +
			fV111 * (fAx) * (fAz);

		double gradZ =
			-fV000 * (1 - fAx) * (1 - fAy) +
			fV001 * (1 - fAx) * (1 - fAy) +
			-fV010 * (1 - fAx) * (fAy)+
			fV011 * (1 - fAx) * (fAy)+
			-fV100 * (fAx) * (1 - fAy) +
			fV101 * (fAx) * (1 - fAy) +
			-fV110 * (fAx) * (fAy)+
			fV111 * (fAx) * (fAy);

		return FVector3d(gradX, gradY, gradZ);
	}
};


} // end namespace UE::Geometry
} // end namespace UE