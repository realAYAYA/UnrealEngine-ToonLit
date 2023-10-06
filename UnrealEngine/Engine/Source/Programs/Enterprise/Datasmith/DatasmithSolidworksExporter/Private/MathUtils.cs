// Copyright Epic Games, Inc. All Rights Reserved.

using SolidWorks.Interop.sldworks;
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
    public static class MathUtils
    {
		public static float Rad2Deg { get { return (float)(180.0 / Math.PI); } }
		public static float Deg2Rad { get { return (float)(Math.PI / 180.0); } }

		public static bool Equals(float F1, float F2)
		{
			return (Math.Abs(F2 - F1) <= 0.000001f);
		}

		public static bool Equals(double A, float B)
		{
			return (Math.Abs(B - A) <= 0.00000001);
		}

		public static FVec3 BarycentricToPoint(FVec3 InBary, FVec3 V1, FVec3 V2, FVec3 V3)
		{
			return new FVec3(
				(InBary.X * V1.X) + (InBary.Y * V2.X) + (InBary.Z * V3.X),
				(InBary.X * V1.Y) + (InBary.Y * V2.Y) + (InBary.Z * V3.Y),
				(InBary.X * V1.Z) + (InBary.Y * V2.Z) + (InBary.Z * V3.Z));
		}

		public static FVec2 BarycentricToPoint(FVec3 InBary, FVec2 V1, FVec2 V2, FVec2 V3)
		{
			return new FVec2(
				(InBary.X * V1.X) + (InBary.Y * V2.X) + (InBary.Z * V3.X), 
				(InBary.X * V1.Y) + (InBary.Y * V2.Y) + (InBary.Z * V3.Y));
		}

		// Explicit version of the omonimous function in Vec2.
		//
		public static void RotateOnPlane(float InCos, float InSin, ref float InOutU, ref float InOutV)
		{
			float U = InOutU;
			float V = InOutV;
			InOutU = (U * InCos - V * InSin);
			InOutV = (U * InSin + V * InCos);
		}

		// datasmith matrix:
		// 0 4 8  12 (tx)
		// 1 5 9  13 (ty)
		// 2 6 10 14 (tz)
		// 3(0.0) 7(0.0) 11(0.0) 15(1.0)
		public static FConvertedTransform ConvertFromSolidworksTransform(MathTransform InSwTransform, float InGeomScale)
		{
			float[] Result = new float[16];
			double[] InputMatrix = (double[])InSwTransform.ArrayData;

			Result[0] = (float)InputMatrix[0];
			Result[1] = (float)InputMatrix[1];
			Result[2] = (float)InputMatrix[2];
			Result[3] = 0f;
			Result[4] = (float)InputMatrix[3];
			Result[5] = (float)InputMatrix[4];
			Result[6] = (float)InputMatrix[5];
			Result[7] = 0f;
			Result[8] = (float)InputMatrix[6];
			Result[9] = (float)InputMatrix[7];
			Result[10] = (float)InputMatrix[8];
			Result[11] = 0f;
			Result[15] = 1f;
			Result[12] = (float)InputMatrix[9] * InGeomScale;
			Result[13] = (float)InputMatrix[10] * InGeomScale;
			Result[14] = (float)InputMatrix[11] * InGeomScale;

			return new FConvertedTransform(Result);
		}

		public static FConvertedTransform LookAt(FVec3 InDirection, FVec3 InFromPoint, float InGeomScale)
		{
			float[] Ret = new float[16];

			if (InDirection != null)
			{
				FVec3 Forward = InDirection.Normalized();
				FVec3 Right = - FVec3.Cross(FVec3.YAxis, Forward);
				FVec3 Up = - FVec3.Cross(Forward, Right);

				Ret[0] = Forward.X;
				Ret[1] = Forward.Y;
				Ret[2] = Forward.Z;

				Ret[4] = Right.X;
				Ret[5] = Right.Y;
				Ret[6] = Right.Z;

				Ret[8] = Up.X;
				Ret[9] = Up.Y;
				Ret[10] = Up.Z;

				Ret[3] = 0f;
				Ret[7] = 0f;
				Ret[11] = 0f;
				Ret[15] = 1f;

				if (InFromPoint != null)
				{
					Ret[12] = InFromPoint.X * InGeomScale;
					Ret[13] = InFromPoint.Y * InGeomScale;
					Ret[14] = InFromPoint.Z * InGeomScale;
				}
			}

			return new FConvertedTransform(Ret);
		}

		public static FConvertedTransform Translation(FVec3 InTranslation, float InGeomScale)
		{
			float[] Ret = new float[16];

			Ret[0] = 1f;
			Ret[1] = 0f;
			Ret[2] = 0f;

			Ret[4] = 0f;
			Ret[5] = 1f;
			Ret[6] = 0f;

			Ret[8] = 0f;
			Ret[9] = 0f;
			Ret[10] = 1f;

			Ret[3] = 0f;
			Ret[7] = 0f;
			Ret[11] = 0f;
			Ret[15] = 1f;

			Ret[12] = InTranslation.X * InGeomScale;
			Ret[13] = InTranslation.Y * InGeomScale;
			Ret[14] = InTranslation.Z * InGeomScale;

			return new FConvertedTransform(Ret);
		}

		public static bool TransformsAreEqual(FConvertedTransform InTransformA, FConvertedTransform InTransformB)
		{
			Debug.Assert(InTransformA.Matrix.Length == 16 && InTransformA.Matrix.Length == InTransformB.Matrix.Length);

			for (int Idx = 0; Idx < 16; ++Idx)
			{
				if (!Equals(InTransformA.Matrix[Idx], InTransformB.Matrix[Idx]))
				{
					return false;
				}
			}

			return true;
		}

		public static MathVector CreateVector(double InX, double InY, double InZ)
		{
			MathUtility MUtil = Addin.Instance.SolidworksApp.IGetMathUtility();
			return (MathVector)MUtil.CreateVector(new[] { InX, InY, InZ }); ;
		}

		public static MathVector CreateVector(float InX, float InY, float InZ)
		{
			MathUtility MUtil = Addin.Instance.SolidworksApp.IGetMathUtility();
			return (MathVector)MUtil.CreateVector(new[] { InX, InY, InZ }); ;
		}

		public static MathPoint CreatePoint(double InX, double InY, double InZ)
		{
			MathUtility MUtil = Addin.Instance.SolidworksApp.IGetMathUtility();
			return (MathPoint)MUtil.CreatePoint(new[] { InX, InY, InZ }); ;
		}

		public static MathPoint CreatePoint(float InX, float InY, float InZ)
		{
			MathUtility MUtil = Addin.Instance.SolidworksApp.IGetMathUtility();
			return (MathPoint)MUtil.CreatePoint(new[] { InX, InY, InZ }); ;
		}

		public static FVec3 ToEuler(FMatrix4 Mat)
		{
			// See UE::Math::TMatrix<T>::Rotator()

			FVec3 XAxis = Mat.XBasis;
			FVec3 YAxis = Mat.YBasis;
			FVec3 ZAxis = Mat.ZBasis;

			double Pitch = Math.Atan2(XAxis.Z, Math.Sqrt(XAxis.X*XAxis.X + XAxis.Y*XAxis.Y));
			double Yaw = Math.Atan2(XAxis.Y, XAxis.X);

			FVec3 SYAxis = new FVec3(-Math.Sin(Yaw), Math.Cos(Yaw), 0);

			double Roll = Math.Atan2(FVec3.Dot(ZAxis, SYAxis), FVec3.Dot(YAxis, SYAxis));

			// Y <-> Pitch,  Z <-> Yaw, X <-> Roll
			return new FVec3(MathUtils.Rad2Deg * Roll, MathUtils.Rad2Deg * Pitch, MathUtils.Rad2Deg * Yaw);
		}
    }
}
