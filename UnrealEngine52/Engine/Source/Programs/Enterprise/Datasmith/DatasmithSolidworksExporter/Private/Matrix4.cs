// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
	public class FMatrix4
	{
		public FVec3 XBasis { get { return new FVec3(Data[0], Data[1], Data[2]); } }
		public FVec3 YBasis { get { return new FVec3(Data[4], Data[5], Data[6]); } }
		public FVec3 ZBasis { get { return new FVec3(Data[8], Data[9], Data[10]); } }

		private static Func<float, float> Sin = AngleDegrees => (float)Math.Sin(AngleDegrees * MathUtils.Deg2Rad);
		private static Func<float, float> Cos = AngleDegrees => (float)Math.Cos(AngleDegrees * MathUtils.Deg2Rad);

		public const int Size4x4 = 16;
		public const int Size4x3 = 12;

		private float[] Data;

		public FMatrix4()
		{
			Data = Identity;
		}

		public FMatrix4(float[] InData)
		{
			Data = InData;
		}

		public override int GetHashCode()
		{
			int Hash = 1;
			if (Data != null)
			{
				Hash = (Hash * 17) + Data.Length;
				foreach (float val in Data)
				{
					Hash *= 17;
					Hash = Hash + val.GetHashCode();
				}
			}
			return Hash;
		}

		public static implicit operator float[] (FMatrix4 InMatrix)
		{
			return InMatrix.Data;
		}

		public float this[int Index]
		{
			get => Data[Index];
			set => Data[Index] = value;
		}

		public static FMatrix4 Identity
		{
			get
			{
				return new FMatrix4(new float[Size4x4]
				{
					1f, 0f, 0f, 0f,
					0f, 1f, 0f, 0f,
					0f, 0f, 1f, 0f,
					0f, 0f, 0f, 1f
				});
			}
		}

		public static FMatrix4 FromSolidWorks(double[] InSwMatrix, float InGeomScale)
		{
			return new FMatrix4(new float[Size4x4]
			{
				(float)InSwMatrix[0], (float)InSwMatrix[3], (float)InSwMatrix[6], (float)InSwMatrix[9] * InGeomScale,
				(float)InSwMatrix[1], (float)InSwMatrix[4], (float)InSwMatrix[7], (float)InSwMatrix[10] * InGeomScale,
				(float)InSwMatrix[2], (float)InSwMatrix[5], (float)InSwMatrix[8], (float)InSwMatrix[11] * InGeomScale,
				0f, 0f, 0f, (float)InSwMatrix[12]
			});
		}

		public static FMatrix4 From3x3(float[] In3x3Matrix)
		{
			return new FMatrix4(new float[Size4x4]
			{
				In3x3Matrix[0], In3x3Matrix[1], In3x3Matrix[2], 0.0f,
				In3x3Matrix[3], In3x3Matrix[4], In3x3Matrix[5], 0.0f,
				In3x3Matrix[6], In3x3Matrix[7], In3x3Matrix[8], 0.0f,
				0f, 0f, 0f, 1.0f
			});
		}

		public static FMatrix4 From4x3(float[] In4x3Matrix)
		{
			return new FMatrix4(new float[Size4x4]
			{
				In4x3Matrix[0], In4x3Matrix[1], In4x3Matrix[2], In4x3Matrix[3],
				In4x3Matrix[4], In4x3Matrix[5], In4x3Matrix[6], In4x3Matrix[7],
				In4x3Matrix[8], In4x3Matrix[9], In4x3Matrix[10], In4x3Matrix[11],
				0f, 0f, 0f, 1.0f
			});
		}

		public static FMatrix4 FromRotationX(float AngleDegrees)
		{
			return new FMatrix4(new float[Size4x4]
			{
				1f, 0f, 0f, 0f,
				0f, Cos(AngleDegrees), -Sin(AngleDegrees), 0f,
				0f, Sin(AngleDegrees), Cos(AngleDegrees), 0f,
				0f, 0f, 0f, 1f
			});
		}

		public static FMatrix4 FromRotationY(float AngleDegrees)
		{
			return new FMatrix4(new float[Size4x4]
			{
				Cos(AngleDegrees), 0f, Sin(AngleDegrees), 0f,
				0f, 1f, 0f, 0f,
				-Sin(AngleDegrees), 0f, Cos(AngleDegrees), 0f,
				0f, 0f, 0f, 1f
			});
		}

		public static FMatrix4 FromRotationZ(float AngleDegrees)
		{
			return new FMatrix4(new float[Size4x4]
			{
				Cos(AngleDegrees), -Sin(AngleDegrees), 0f, 0f,
				Sin(AngleDegrees), Cos(AngleDegrees), 0f, 0f,
				0f, 0f, 1f, 0f,
				0f, 0f, 0f, 1f
			});
		}

		public static FMatrix4 Translation(FVec3 InVector)
		{
			return FromTranslation(InVector.X, InVector.Y, InVector.Z);
		}

		public static FMatrix4 FromTranslation(float InX, float InY, float InZ)
		{
			FMatrix4 Result = Identity;
			Result.SetTranslation(InX, InY, InZ);
			return Result;
		}

		public static FMatrix4 FromScale(float InX, float InY, float InZ)
		{
			FMatrix4 Result = Identity;
			Result.Data[0] = InX;
			Result.Data[5] = InY;
			Result.Data[10] = InZ;
			return Result;
		}

		public static float[] FMatrix4x4Multiply(float[] InMatrixA, float[] InMatrixB)
		{
			float[] Ret = null;

			if (InMatrixA.Length == Size4x4 && InMatrixB.Length == Size4x4)
			{
				Ret = new float[Size4x4];
				for (var I = 0; I < 4; I++)
				{
					for (var J = 0; J < 4; J++)
					{
						var Val = 0f;
						for (var Pos = 0; Pos < 4; Pos++)
						{
							Val += InMatrixA[I * 4 + Pos] * InMatrixB[Pos * 4 + J];
						}

						Ret[I * 4 + J] = Val;
					}
				}
			}

			return Ret;
		}

		public override string ToString()
		{
			string Str =
				$"\n( { Data[0] }, { Data[1] }, { Data[2] } )( { Data[3] } )" +
				$"\n( { Data[4] }, { Data[5] }, { Data[6] } )( { Data[7] } )" +
				$"\n( { Data[8] }, { Data[9] }, { Data[10] } )( {Data[11] } )";
			return Str;
		}

		public float[] To4x3()
		{
			float[] M = new float[Size4x3];

			for (int i = 0; i < Size4x3; i++)
			{
				M[i] = Data[i];
			}

			return M;
		}

		public void SetTranslation(float InX, float InY, float InZ)
		{
			Data[3] = InX;
			Data[7] = InY;
			Data[11] = InZ;
		}

		public void SetTranslation(FVec3 InVec)
		{
			Data[3] = InVec.X;
			Data[7] = InVec.Y;
			Data[11] = InVec.Z;
		}

		public static FMatrix4 FromRotationAxisAngle(FVec3 InAxis, float InAngleDegrees)
		{
			FMatrix4 Matrix = new FMatrix4();
			Matrix.SetRotationAxisAngle(InAxis, InAngleDegrees);
			return Matrix;
		}

		public void SetRotationAxisAngle(FVec3 InAxis, float InAngleDegrees)
		{
			FVec3 V = InAxis.Normalized();

			float CosA = Cos(InAngleDegrees);
			float SinA = Sin(InAngleDegrees);
			float OneMinusCosA = 1f - CosA;

			Data[0] = ((V.X * V.X) * OneMinusCosA) + CosA;
			Data[1] = ((V.X * V.Y) * OneMinusCosA) + (V.Z * SinA);
			Data[2] = ((V.X * V.Z) * OneMinusCosA) - (V.Y * SinA);
			Data[3] = 0f;

			Data[4] = ((V.Y * V.X) * OneMinusCosA) - (V.Z * SinA);
			Data[5] = ((V.Y * V.Y) * OneMinusCosA) + CosA;
			Data[6] = ((V.Y * V.Z) * OneMinusCosA) + (V.X * SinA);
			Data[7] = 0f;

			Data[8] = ((V.Z * V.X) * OneMinusCosA) + (V.Y * SinA);
			Data[9] = ((V.Z * V.Y) * OneMinusCosA) - (V.X * SinA);
			Data[10] = ((V.Z * V.Z) * OneMinusCosA) + CosA;
			Data[11] = 0f;

			Data[12] = 0f;
			Data[13] = 0f;
			Data[14] = 0f;
			Data[15] = 1f;
		}

		public FVec3 TransformPoint(FVec3 InPoint)
		{
			float X = InPoint.X * Data[0] + InPoint.Y * Data[1] + InPoint.Z * Data[2] + Data[3];
			float Y = InPoint.X * Data[4] + InPoint.Y * Data[5] + InPoint.Z * Data[6] + Data[7];
			float Z = InPoint.X * Data[8] + InPoint.Y * Data[9] + InPoint.Z * Data[10] + Data[11];
			float W = InPoint.X * Data[12] + InPoint.Y * Data[13] + InPoint.Z * Data[14] + Data[15];

			if (W != 1)
			{
				return new FVec3(X / W, Y / W, Z / W);
			}
			else
			{
				return new FVec3(X, Y, Z);
			}
		}

		public FVec3 TransformVector(FVec3 InVec)
		{
			float X = InVec.X * Data[0] + InVec.Y * Data[1] + InVec.Z * Data[2];
			float Y = InVec.X * Data[4] + InVec.Y * Data[5] + InVec.Z * Data[6];
			float Z = InVec.X * Data[8] + InVec.Y * Data[9] + InVec.Z * Data[10];

			return new FVec3(X, Y, Z);
		}

		public FMatrix4 Transposed()
		{
			FMatrix4 T = new FMatrix4();

			T.Data[0] = Data[0]; T.Data[1] = Data[4]; T.Data[2] = Data[8]; T.Data[3] = Data[12];
			T.Data[4] = Data[1]; T.Data[5] = Data[5]; T.Data[6] = Data[9]; T.Data[7] = Data[13];
			T.Data[8] = Data[2]; T.Data[9] = Data[6]; T.Data[10] = Data[10]; T.Data[11] = Data[14];
			T.Data[12] = Data[3]; T.Data[13] = Data[7]; T.Data[14] = Data[11]; T.Data[15] = Data[15];

			return T;
		}

		public FMatrix4 Inverse()
		{
			float S0 = Data[0] * Data[5] - Data[1] * Data[4];
			float S1 = Data[0] * Data[9] - Data[1] * Data[8];
			float S2 = Data[0] * Data[13] - Data[1] * Data[12];
			float S3 = Data[4] * Data[9] - Data[5] * Data[8];
			float S4 = Data[4] * Data[13] - Data[5] * Data[12];
			float S5 = Data[8] * Data[13] - Data[9] * Data[12];

			float C0 = Data[2] * Data[7] - Data[3] * Data[6];
			float C1 = Data[2] * Data[11] - Data[3] * Data[10];
			float C2 = Data[2] * Data[15] - Data[3] * Data[14];
			float C3 = Data[6] * Data[11] - Data[7] * Data[10];
			float C4 = Data[6] * Data[15] - Data[7] * Data[14];
			float C5 = Data[10] * Data[15] - Data[11] * Data[14];

			float DI = 1.0f / (S0 * C5 - S1 * C4 + S2 * C3 + S3 * C2 - S4 * C1 + S5 * C0);

			FMatrix4 Inv = new FMatrix4();

			Inv.Data[0] = (Data[5] * C5 - Data[9] * C4 + Data[13] * C3) * DI;
			Inv.Data[4] = (-Data[4] * C5 + Data[8] * C4 - Data[12] * C3) * DI;
			Inv.Data[8] = (Data[7] * S5 - Data[11] * S4 + Data[15] * S3) * DI;
			Inv.Data[12] = (-Data[6] * S5 + Data[10] * S4 - Data[14] * S3) * DI;

			Inv.Data[1] = (-Data[1] * C5 + Data[9] * C2 - Data[13] * C1) * DI;
			Inv.Data[5] = (Data[0] * C5 - Data[8] * C2 + Data[12] * C1) * DI;
			Inv.Data[9] = (-Data[3] * S5 + Data[11] * S2 - Data[15] * S1) * DI;
			Inv.Data[13] = (Data[2] * S5 - Data[10] * S2 + Data[14] * S1) * DI;

			Inv.Data[2] = (Data[1] * C4 - Data[5] * C2 + Data[13] * C0) * DI;
			Inv.Data[6] = (-Data[0] * C4 + Data[4] * C2 - Data[12] * C0) * DI;
			Inv.Data[10] = (Data[3] * S4 - Data[7] * S2 + Data[15] * S0) * DI;
			Inv.Data[14] = (-Data[2] * S4 + Data[6] * S2 - Data[14] * S0) * DI;

			Inv.Data[3] = (-Data[1] * C3 + Data[5] * C1 - Data[9] * C0) * DI;
			Inv.Data[7] = (Data[0] * C3 - Data[4] * C1 + Data[8] * C0) * DI;
			Inv.Data[11] = (-Data[3] * S3 + Data[7] * S1 - Data[11] * S0) * DI;
			Inv.Data[15] = (Data[2] * S3 - Data[6] * S1 + Data[10] * S0) * DI;

			return Inv;
		}

		public static FMatrix4 RotateVectorByAxis(FVec3 InCenterPoint, FMatrix4 InRotationMatrix, FVec3 InAxis, double InAngle)
		{
			FMatrix4 Ret = InRotationMatrix;
			if (!MathUtils.Equals(InAngle, 0.0))
			{
				FMatrix4 RotateMatrix = FMatrix4.FromRotationAxisAngle(InAxis, (float)InAngle);
				RotateMatrix.SetTranslation(InAxis.X, InAxis.Y, InAxis.Z);
				Ret = (Ret != null ? RotateMatrix * Ret : RotateMatrix);
			}
			return Ret;
		}

		public static FMatrix4 operator *(FMatrix4 InA, FMatrix4 InB)
		{
			return new FMatrix4(FMatrix4x4Multiply(InA, InB));
		}

		public override bool Equals(object InObj)
		{
			return base.Equals(InObj);
		}
	}
}
