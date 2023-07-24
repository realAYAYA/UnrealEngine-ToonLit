// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Math/VectorRegister.h"
#include "Chaos/VectorUtility.h"

namespace Chaos
{
	template <bool CalculatExtraInformation>
	FORCEINLINE VectorRegister4Float VectorLineSimplexFindOrigin(VectorRegister4Float* RESTRICT Simplex, VectorRegister4Int& RESTRICT NumVerts, VectorRegister4Float& RESTRICT OutBarycentric, VectorRegister4Float* RESTRICT A, VectorRegister4Float* RESTRICT B)
	{
		const VectorRegister4Float& X0 = Simplex[0];
		const VectorRegister4Float& X1 = Simplex[1];
		const VectorRegister4Float X0ToX1 = VectorSubtract(X1, X0);

		//Closest Point = (-X0 dot X1-X0) / ||(X1-X0)||^2 * (X1-X0)

		const VectorRegister4Float X0ToOrigin = VectorNegate(X0);
		const VectorRegister4Float Dot = VectorDot3(X0ToOrigin, X0ToX1);

		const VectorRegister4Float IsX0 = VectorCompareGE(VectorZeroFloat(), Dot);

		constexpr VectorRegister4Float OutBarycentricIfX0OrX1 = GlobalVectorConstants::Float1000;
		const VectorRegister4Float X0ToX1Squared = VectorDot3(X0ToX1, X0ToX1);
		const VectorRegister4Float DotBigger = VectorCompareGE(Dot, X0ToX1Squared);

		const VectorRegister4Float MinLimt = MakeVectorRegisterFloat(std::numeric_limits<FRealSingle>::min() , std::numeric_limits<FRealSingle>::min(), std::numeric_limits<FRealSingle>::min(), std::numeric_limits<FRealSingle>::min());
		const VectorRegister4Float X0ToX1SquaredSmall = VectorCompareGE(MinLimt, X0ToX1Squared);
		const VectorRegister4Float IsX1 = VectorBitwiseOr(DotBigger, X0ToX1SquaredSmall);

		Simplex[0] = VectorSelect(IsX1, Simplex[1], Simplex[0]);

		if (CalculatExtraInformation)
		{
			A[0] = VectorSelect(IsX1, A[1], A[0]);
			B[0] = VectorSelect(IsX1, B[1], B[0]);
		}

		VectorRegister4Float Ratio = VectorDivide(Dot, X0ToX1Squared);

		Ratio = VectorMax(Ratio, VectorZeroFloat());
		Ratio = VectorMin(Ratio, GlobalVectorConstants::FloatOne);

		VectorRegister4Float Closest = VectorMultiplyAdd(Ratio, X0ToX1, X0);

		const VectorRegister4Float OneMinusRatio = VectorSubtract(GlobalVectorConstants::FloatOne, Ratio);
		const VectorRegister4Float OutBarycentricOtherwise = VectorUnpackLo(OneMinusRatio, Ratio);

		Closest = VectorSelect(IsX0, X0, VectorSelect(IsX1, X1, Closest));

		const VectorRegister4Float IsX0OrX1 = VectorBitwiseOr(IsX0, IsX1);
		const VectorRegister4Int IsX0OrX1Int = VectorCast4FloatTo4Int(IsX0OrX1);
		NumVerts = VectorIntSelect(IsX0OrX1Int, GlobalVectorConstants::IntOne, NumVerts);

		if (CalculatExtraInformation)
		{
			OutBarycentric = VectorSelect(IsX0OrX1, OutBarycentricIfX0OrX1, OutBarycentricOtherwise);
		}

		return Closest;
	}

	// Based on an algorithm in Real Time Collision Detection - Ericson (very close to that)
	// Using the same variable name conventions for easy reference
	template <bool CalculatExtraInformation>
	FORCEINLINE_DEBUGGABLE VectorRegister4Float  TriangleSimplexFindOriginFast(VectorRegister4Float* RESTRICT Simplex, VectorRegister4Int& RESTRICT NumVerts, VectorRegister4Float& RESTRICT OutBarycentric, VectorRegister4Float* RESTRICT As, VectorRegister4Float* RESTRICT Bs)
	{
		const VectorRegister4Float& A = Simplex[0];
		const VectorRegister4Float& B = Simplex[1];
		const VectorRegister4Float& C = Simplex[2];

		const VectorRegister4Float AB = VectorSubtract(B, A);
		const VectorRegister4Float AC = VectorSubtract(C, A);


		// Handle degenerate triangle
		const VectorRegister4Float TriNormal = VectorCross(AB, AC);
		const VectorRegister4Float TriNormal2 = VectorDot3(TriNormal, TriNormal);
		const VectorRegister4Float MinFloat = MakeVectorRegisterFloatConstant(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min());
		const VectorRegister4Float AMin = VectorMultiply(A, MinFloat);
		const VectorRegister4Float Eps2 = VectorDot3(AMin, AMin);
		const VectorRegister4Float Eps2GENormal2 = VectorCompareGE(Eps2, TriNormal2);
		if (VectorMaskBits(Eps2GENormal2))
		{
			constexpr VectorRegister4Int Two = MakeVectorRegisterIntConstant(2, 2, 2, 2);
			NumVerts = Two;
			return VectorLineSimplexFindOrigin<CalculatExtraInformation>(Simplex, NumVerts, OutBarycentric, As, Bs);
		}

		// Vertex region A
		const VectorRegister4Float AO = VectorNegate(A);

		const VectorRegister4Float d1 = VectorDot3(AB, AO);
		const VectorRegister4Float d2 = VectorDot3(AC, AO);

		const VectorRegister4Float IsD1SEZero = VectorCompareGE(VectorZeroFloat(), d1);
		const VectorRegister4Float IsD2SEZero = VectorCompareGE(VectorZeroFloat(), d2);
		const VectorRegister4Float IsA = VectorBitwiseAnd(IsD1SEZero, IsD2SEZero);

		if (VectorMaskBits(IsA))
		{
			NumVerts = GlobalVectorConstants::IntOne;
			if (CalculatExtraInformation)
			{
				OutBarycentric = GlobalVectorConstants::Float1000;
			}
			return A;
		}

		//Vertex region B
		const VectorRegister4Float BO = VectorNegate(B);
		const VectorRegister4Float d3 = VectorDot3(AB, BO);
		const VectorRegister4Float d4 = VectorDot3(AC, BO);

		const VectorRegister4Float IsD3GEZero = VectorCompareGE(d3, VectorZeroFloat());
		const VectorRegister4Float IsD3GED4 = VectorCompareGE(d3, d4);
		const VectorRegister4Float IsB = VectorBitwiseAnd(IsD3GEZero, IsD3GED4);

		if (VectorMaskBits(IsB))
		{
			NumVerts = GlobalVectorConstants::IntOne;
			if (CalculatExtraInformation)
			{
				OutBarycentric = GlobalVectorConstants::Float1000;
			}
			Simplex[0] = B;
			if (CalculatExtraInformation)
			{
				As[0] = As[1];
				Bs[0] = Bs[1];
			}
			return B;
		}

		// Edge AB
		const VectorRegister4Float d1d4 = VectorMultiply(d1, d4);
		const VectorRegister4Float vc = VectorNegateMultiplyAdd(d3, d2, d1d4);
		const VectorRegister4Float NormalizationDenominatorAB = VectorSubtract(d1, d3);

		const VectorRegister4Float IsZeroGEvc = VectorCompareGE(VectorZeroFloat(), vc);
		const VectorRegister4Float IsD1GEZero = VectorCompareGE(d1, VectorZeroFloat());
		const VectorRegister4Float IsZeroGED3= VectorCompareGE(VectorZeroFloat(), d3);
		const VectorRegister4Float IsNDABGTZero = VectorCompareGT(NormalizationDenominatorAB, VectorZeroFloat());
		const VectorRegister4Float IsAB = VectorBitwiseAnd(VectorBitwiseAnd(IsZeroGEvc, IsD1GEZero), VectorBitwiseAnd(IsZeroGED3, IsNDABGTZero));

		if (VectorMaskBits(IsAB))
		{
			constexpr VectorRegister4Int two = MakeVectorRegisterIntConstant(2, 2, 2, 2);
			NumVerts = two;

			const VectorRegister4Float v = VectorDivide(d1, NormalizationDenominatorAB);
			const VectorRegister4Float OneMinusV = VectorSubtract(GlobalVectorConstants::FloatOne, v);
			// b0	a1	a2	a3
			if (CalculatExtraInformation)
			{
				OutBarycentric = VectorUnpackLo(OneMinusV, v);
			}
			return VectorMultiplyAdd(v, AB, A);
		}

		// Vertex C
		const VectorRegister4Float CO = VectorNegate(C);
		const VectorRegister4Float d5 = VectorDot3(AB, CO);
		const VectorRegister4Float d6 = VectorDot3(AC, CO);
		const VectorRegister4Float IsD6GEZero = VectorCompareGE(d6, VectorZeroFloat());
		const VectorRegister4Float IsD6GED5 = VectorCompareGE(d6, d5);
		const VectorRegister4Float IsC = VectorBitwiseAnd(IsD6GEZero, IsD6GED5);

		if (VectorMaskBits(IsC))
		{
			NumVerts = GlobalVectorConstants::IntOne;
			if (CalculatExtraInformation)
			{
				OutBarycentric = GlobalVectorConstants::Float1000;
			}

			Simplex[0] = C;
			if (CalculatExtraInformation)
			{
				As[0] = As[2];
				Bs[0] = Bs[2];
			}
			return C;
		}

		// Edge AC
		const VectorRegister4Float d5d2 = VectorMultiply(d5, d2);
		const VectorRegister4Float vb = VectorNegateMultiplyAdd(d1, d6, d5d2);
		const VectorRegister4Float NormalizationDenominatorAC = VectorSubtract(d2, d6);

		const VectorRegister4Float IsZeroGEvb = VectorCompareGE(VectorZeroFloat(), vb);
		const VectorRegister4Float IsD2GEZero = VectorCompareGE(d2, VectorZeroFloat());
		const VectorRegister4Float IsZeroGED6 = VectorCompareGE(VectorZeroFloat(), d6);
		const VectorRegister4Float IsNDACGTZero = VectorCompareGT(NormalizationDenominatorAC, VectorZeroFloat());
		const VectorRegister4Float IsAC = VectorBitwiseAnd(VectorBitwiseAnd(IsZeroGEvb, IsD2GEZero), VectorBitwiseAnd(IsZeroGED6, IsNDACGTZero));

		if (VectorMaskBits(IsAC))
		{
			const VectorRegister4Float w = VectorDivide(d2, NormalizationDenominatorAC);
			constexpr VectorRegister4Int two = MakeVectorRegisterIntConstant(2, 2, 2, 2);
			NumVerts = two;
			const VectorRegister4Float OneMinusW = VectorSubtract(GlobalVectorConstants::FloatOne, w);
			// b0	a1	a2	a3
			if (CalculatExtraInformation)
			{
				OutBarycentric = VectorUnpackLo(OneMinusW, w);
			}
			Simplex[1] = C;
			if (CalculatExtraInformation)
			{
				As[1] = As[2];
				Bs[1] = Bs[2];
			}
			return VectorMultiplyAdd(w, AC, A);
		}

		// Edge BC
		const VectorRegister4Float d3d6 = VectorMultiply(d3, d6);
		const VectorRegister4Float va = VectorNegateMultiplyAdd(d5, d4, d3d6);
		const VectorRegister4Float d4MinusD3 = VectorSubtract(d4, d3);
		const VectorRegister4Float d5MinusD6 = VectorSubtract(d5, d6);
		const VectorRegister4Float NormalizationDenominatorBC = VectorAdd(d4MinusD3, d5MinusD6);

		const VectorRegister4Float IsZeroGEva = VectorCompareGE(VectorZeroFloat(), va);
		const VectorRegister4Float IsD4MinusD3GEZero = VectorCompareGE(d4MinusD3, VectorZeroFloat());
		const VectorRegister4Float IsD5MinusD6GEZero = VectorCompareGE(d5MinusD6, VectorZeroFloat());
		const VectorRegister4Float IsNDBCGTZero = VectorCompareGT(NormalizationDenominatorBC, VectorZeroFloat());
		const VectorRegister4Float IsBC = VectorBitwiseAnd(VectorBitwiseAnd(IsZeroGEva, IsD4MinusD3GEZero), VectorBitwiseAnd(IsD5MinusD6GEZero, IsNDBCGTZero));

		if (VectorMaskBits(IsBC))
		{
			constexpr VectorRegister4Int two = MakeVectorRegisterIntConstant(2, 2, 2, 2);
			NumVerts = two;
			const VectorRegister4Float w = VectorDivide(d4MinusD3, NormalizationDenominatorBC);
			if (CalculatExtraInformation)
			{
				const VectorRegister4Float OneMinusW = VectorSubtract(GlobalVectorConstants::FloatOne, w);
				// b0	a1	a2	a3
				OutBarycentric = VectorUnpackLo(OneMinusW, w);
			}
			const VectorRegister4Float CMinusB = VectorSubtract(C, B);
			const VectorRegister4Float Result = VectorMultiplyAdd(w, CMinusB, B);
			Simplex[0] = B;
			Simplex[1] = C;
			if (CalculatExtraInformation)
			{
				As[0] = As[1];
				Bs[0] = Bs[1];
				As[1] = As[2];
				Bs[1] = Bs[2];
			}
			return Result;
		}

		// Inside triangle
		const VectorRegister4Float denom = VectorDivide(VectorOne(), VectorAdd(va, VectorAdd(vb, vc)));
		VectorRegister4Float v = VectorMultiply(vb, denom);
		VectorRegister4Float w = VectorMultiply(vc, denom);
		constexpr VectorRegister4Int three = MakeVectorRegisterIntConstant(3, 3, 3, 3);
		NumVerts = three;

		if (CalculatExtraInformation)
		{
			const VectorRegister4Float OneMinusVMinusW = VectorSubtract(VectorSubtract(GlobalVectorConstants::FloatOne, v), w);
			// b0	a1	a2	a3
			const VectorRegister4Float OneMinusVMinusW_W = VectorUnpackLo(OneMinusVMinusW, w);
			// a0	b0	a1	b1
			OutBarycentric = VectorUnpackLo(OneMinusVMinusW_W, v);
		}

		// We know that we are inside the triangle so we can use the projected point we calculated above. 
		// The closest point can also be derived from the barycentric coordinates, but it will contain 
		// numerical error from the determinant calculation  and can cause GJK to terminate with a poor solution.
		// (E.g., this caused jittering when walking on box with dimensions of 100000cm or more).
		// This fix the unit test TestSmallCapsuleLargeBoxGJKRaycast_Vertical
		// Previously was return VectorMultiplyAdd(AC, w, VectorMultiplyAdd(AB, v, A));
		const VectorRegister4Float TriNormalOverSize2 = VectorDivide(TriNormal, TriNormal2);
		const VectorRegister4Float SignedDistance = VectorDot3(A, TriNormalOverSize2);
		return VectorMultiply(TriNormal, SignedDistance);
	}

	FORCEINLINE bool VectorSignMatch(VectorRegister4Float A, VectorRegister4Float B)
	{
		VectorRegister4Float OneIsZero = VectorMultiply(A, B);
		OneIsZero = VectorCompareEQ(OneIsZero, VectorZeroFloat());

		const bool IsZero = static_cast<bool>(VectorMaskBits(OneIsZero));
		const int32 MaskA = VectorMaskBits(A);
		const int32 MaskB = VectorMaskBits(B);
		return (MaskA == MaskB) && !IsZero;
	}

	template <bool CalculatExtraInformation>
	FORCEINLINE_DEBUGGABLE VectorRegister4Float VectorTetrahedronSimplexFindOrigin(VectorRegister4Float* RESTRICT Simplex, VectorRegister4Int& RESTRICT NumVerts, VectorRegister4Float& RESTRICT OutBarycentric, VectorRegister4Float* RESTRICT A, VectorRegister4Float* RESTRICT B)
	{
		const VectorRegister4Float& X0 = Simplex[0];
		const VectorRegister4Float& X1 = Simplex[1];
		const VectorRegister4Float& X2 = Simplex[2];
		const VectorRegister4Float& X3 = Simplex[3];

		//Use signed volumes to determine if origin is inside or outside
		/*
			M = [X0x X1x X2x X3x;
				 X0y X1y X2y X3y;
				 X0z X1z X2z X3z;
				 1   1   1   1]
		*/

		VectorRegister4Float Cofactors[4];
		Cofactors[0] = VectorNegate(VectorDot3(X1, VectorCross(X2, X3)));
		Cofactors[1] = VectorDot3(X0, VectorCross(X2, X3));
		Cofactors[2] = VectorNegate(VectorDot3(X0, VectorCross(X1, X3)));
		Cofactors[3] = VectorDot3(X0, VectorCross(X1, X2));
		VectorRegister4Float DetM = VectorAdd(VectorAdd(Cofactors[0], Cofactors[1]), VectorAdd(Cofactors[2], Cofactors[3]));

		bool bSignMatch[4];
		// constexpr int32 SubIdxs[4][3] = { {1,2,3}, {0,2,3}, {0,1,3}, {0,1,2} };
		constexpr VectorRegister4Int ThreeInt = MakeVectorRegisterIntConstant(3, 3, 3, 3);
		VectorRegister4Int SubNumVerts[4] = { ThreeInt, ThreeInt, ThreeInt, ThreeInt };
		VectorRegister4Float SubSimplices[4][3] = { {Simplex[1], Simplex[2], Simplex[3]}, {Simplex[0], Simplex[2], Simplex[3]}, {Simplex[0], Simplex[1], Simplex[3]}, {Simplex[0], Simplex[1], Simplex[2]} };
		VectorRegister4Float SubAs[4][3];
		VectorRegister4Float SubBs[4][3];
		if (CalculatExtraInformation)
		{
			//SubAs = { {A[1], A[2], A[3]}, {A[0], A[2], A[3]}, {A[0], A[1], A[3]}, {A[0], A[1], A[2]} };
			//SubBs = { {B[1], B[2], B[3]}, {B[0], B[2], B[3]}, {B[0], B[1], B[3]}, {B[0], B[1], B[2]} };
			SubAs[0][0] = A[1]; SubAs[0][1] = A[2]; SubAs[0][2] = A[3];
			SubAs[1][0] = A[0]; SubAs[1][1] = A[2]; SubAs[1][2] = A[3];
			SubAs[2][0] = A[0]; SubAs[2][1] = A[1]; SubAs[2][2] = A[3];
			SubAs[3][0] = A[0]; SubAs[3][1] = A[1]; SubAs[3][2] = A[2];

			SubBs[0][0] = B[1]; SubBs[0][1] = B[2]; SubBs[0][2] = B[3];
			SubBs[1][0] = B[0]; SubBs[1][1] = B[2]; SubBs[1][2] = B[3];
			SubBs[2][0] = B[0]; SubBs[2][1] = B[1]; SubBs[2][2] = B[3];
			SubBs[3][0] = B[0]; SubBs[3][1] = B[1]; SubBs[3][2] = B[2];
		}
		VectorRegister4Float ClosestPointSub[4];
		VectorRegister4Float SubBarycentric[4];
		constexpr VectorRegister4Int IndexNone = MakeVectorRegisterIntConstant(INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE);
		VectorRegister4Int ClosestTriangleIdx = IndexNone;
		VectorRegister4Float MinTriangleDist2 = VectorZeroFloat();


		constexpr VectorRegister4Int IdxSimd[4] = { GlobalVectorConstants::IntZero,
													GlobalVectorConstants::IntOne,
													MakeVectorRegisterIntConstant(2, 2, 2, 2),
													MakeVectorRegisterIntConstant(3, 3, 3, 3)
		};

		VectorRegister4Float Eps = VectorMultiply(GlobalVectorConstants::KindaSmallNumber, VectorDivide(DetM, VectorSet1(4.0f)));

		bool bInside = true;
		for (int Idx = 0; Idx < 4; ++Idx)
		{
			bSignMatch[Idx] = VectorSignMatch(DetM, Cofactors[Idx]);
			if (!bSignMatch[Idx])
			{
				bInside = false;
				ClosestPointSub[Idx] = TriangleSimplexFindOriginFast<CalculatExtraInformation>(SubSimplices[Idx], SubNumVerts[Idx], SubBarycentric[Idx], SubAs[Idx], SubBs[Idx]);

				const VectorRegister4Float Dist2 = VectorDot3(ClosestPointSub[Idx], ClosestPointSub[Idx]);

				const VectorRegister4Float MinGTDist = VectorCompareGT(MinTriangleDist2, Dist2);
				const VectorRegister4Int ClsoestEq = VectorIntCompareEQ(ClosestTriangleIdx, IndexNone);

				const VectorRegister4Float FindClosest = VectorBitwiseOr(MinGTDist, VectorCast4IntTo4Float(ClsoestEq));
				const VectorRegister4Int FindClosestInt = VectorCast4FloatTo4Int(FindClosest);

				MinTriangleDist2 = VectorSelect(FindClosest, Dist2, MinTriangleDist2);
				ClosestTriangleIdx = VectorIntSelect(FindClosestInt, IdxSimd[Idx], ClosestTriangleIdx);
			}
		}

		if (bInside)
		{
			if (CalculatExtraInformation)
			{
				VectorRegister4Float OutBarycentricVectors[4];
				const VectorRegister4Float  InvDetM = VectorDivide(GlobalVectorConstants::FloatOne, DetM);
				OutBarycentricVectors[0] = VectorMultiply(Cofactors[0], InvDetM);
				OutBarycentricVectors[1] = VectorMultiply(Cofactors[1], InvDetM);
				OutBarycentricVectors[2] = VectorMultiply(Cofactors[2], InvDetM);
				OutBarycentricVectors[3] = VectorMultiply(Cofactors[3], InvDetM);
				// a0	b0	a1	b1
				const VectorRegister4Float OutBarycentric0101 = VectorUnpackLo(OutBarycentricVectors[0], OutBarycentricVectors[1]);
				const VectorRegister4Float OutBarycentric2323 = VectorUnpackLo(OutBarycentricVectors[2], OutBarycentricVectors[3]);
				// a0	a1	b0	b1
				OutBarycentric = VectorMoveLh(OutBarycentric0101, OutBarycentric2323);
			}

			return VectorZeroFloat();
		}

		alignas(16) int32 ClosestTriangleIdxInts[4];
		VectorIntStoreAligned(ClosestTriangleIdx, ClosestTriangleIdxInts);
		int32 ClosestTriangleIdxInt = ClosestTriangleIdxInts[0];
		NumVerts = SubNumVerts[ClosestTriangleIdxInt];
		if (CalculatExtraInformation)
		{
			OutBarycentric = SubBarycentric[ClosestTriangleIdxInt];
		}

		for (int i = 0; i < 3; i++)
		{
			Simplex[i] = SubSimplices[ClosestTriangleIdxInt][i];
			if (CalculatExtraInformation)
			{
				A[i] = SubAs[ClosestTriangleIdxInt][i];
				B[i] = SubBs[ClosestTriangleIdxInt][i];
			}
		}

		return ClosestPointSub[ClosestTriangleIdxInt];
	}


	// CalculatExtraHitInformation : Should we calculate the BaryCentric coordinates, As and Bs?
	template <bool CalculatExtraInformation = true>
	FORCEINLINE_DEBUGGABLE VectorRegister4Float VectorSimplexFindClosestToOrigin(VectorRegister4Float* RESTRICT Simplex, VectorRegister4Int& RESTRICT NumVerts, VectorRegister4Float& RESTRICT OutBarycentric, VectorRegister4Float* RESTRICT A, VectorRegister4Float* RESTRICT B)
	{
		VectorRegister4Float ClosestPoint;
		alignas(16) int32 NumVertsInt[4];
		VectorIntStoreAligned(NumVerts, NumVertsInt);
		switch (NumVertsInt[0])
		{
		case 1:
			if (CalculatExtraInformation)
			{
				OutBarycentric = GlobalVectorConstants::Float1000;
			}
			ClosestPoint = Simplex[0]; 
			break;
		case 2:
		{
			ClosestPoint = VectorLineSimplexFindOrigin<CalculatExtraInformation>(Simplex, NumVerts, OutBarycentric, A, B);
			break;
		}
		case 3:
		{
			ClosestPoint = TriangleSimplexFindOriginFast<CalculatExtraInformation>(Simplex, NumVerts, OutBarycentric, A, B);
			break;
		}
		case 4:
		{
			ClosestPoint = VectorTetrahedronSimplexFindOrigin<CalculatExtraInformation>(Simplex, NumVerts, OutBarycentric, A, B);
			break;
		}
		default:
			ensure(false);
			ClosestPoint = VectorZeroFloat();
		}

		return ClosestPoint;
	}
}
