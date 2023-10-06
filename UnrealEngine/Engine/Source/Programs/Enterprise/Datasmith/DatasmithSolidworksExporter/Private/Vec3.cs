// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using SolidWorks.Interop.sldworks;
using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
    public class FVec3
    {
        public float X = 0f;
        public float Y = 0f;
        public float Z = 0f;

        public static readonly FVec3 Zero = new FVec3(0f, 0f, 0f);
        public static readonly FVec3 One = new FVec3(1f, 1f, 1f);
        public static readonly FVec3 XAxis = new FVec3(1f, 0f, 0f);
        public static readonly FVec3 YAxis = new FVec3(0f, 1f, 0f);
        public static readonly FVec3 ZAxis = new FVec3(0f, 0f, 1f);

		public FVec3()
		{
		}

        public FVec3(double InX, double InY, double InZ)
        {
            X = (float)InX;
            Y = (float)InY;
            Z = (float)InZ;
        }

        public FVec3(float InX, float InY, float InZ)
        {
            X = InX;
			Y = InY;
			Z = InZ;
        }

        public FVec3(FVec3 InOther)
        {
            X = InOther.X;
            Y = InOther.Y;
            Z = InOther.Z;
        }

        public FVec3(double[] InOther)
        {
            X = (float)InOther[0];
            Y = (float)InOther[1];
            Z = (float)InOther[2];
        }

        public FVec3(MathVector MathVec)
        {
            double[] Data = MathVec.ArrayData();
			X = (float)Data[0];
			Y = (float)Data[1];
			Z = (float)Data[2];
        }

        public static bool operator ==(FVec3 A, FVec3 B)
        {
	        if (ReferenceEquals(A, B))
	        {
				return true;
	        }

	        if (A is null || B is null)
	        {
		        return false;
	        }

	        return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
        }

        public static bool operator !=(FVec3 A, FVec3 B)
        {
	        return !(A == B);
        }

        public override bool Equals(object Obj)
        {
            return Obj is FVec3 Other && this == Other;
        }

        public override int GetHashCode()
        {
            int Hash = 1;
            Hash = Hash * 17 + X.GetHashCode();
            Hash = Hash * 17 + Y.GetHashCode();
            Hash = Hash * 17 + Z.GetHashCode();
            return Hash;
        }

        public bool IsZero()
        {
            return (X == 0f && Y == 0f && Z == 0f);
        }

        public float SquareMagnitude()
        {
            return (X * X) + (Y * Y) + (Z * Z);
        }

        public float Magnitude()
        {
            return (float)Math.Sqrt(SquareMagnitude());
        }

        public FVec3 Normalized()
        {
            FVec3 N = new FVec3(this);
            float Mag = Magnitude();
            if (Mag != 0.0f)
            {
                Mag = 1.0f / Mag;
                N.X *= Mag;
                N.Y *= Mag;
                N.Z *= Mag;
            }
            return N;
        }

        public FVec3 Cleared()
        {
            return new FVec3(
                (X == float.NaN) ? 0.0f : X,
                (Y == float.NaN) ? 0.0f : Y,
                (Z == float.NaN) ? 0.0f : Z);
        }

        public static bool operator <(FVec3 V1, FVec3 V2)
        {
			return
				V1.X < V2.X &&
				V1.Y < V2.Y &&
				V1.Z < V2.Z;
        }

        public static bool operator >(FVec3 V1, FVec3 V2)
        {
			return
				V1.X > V2.X &&
				V1.Y > V2.Y &&
				V1.Z > V2.Z;
		}

		public static float Dot(FVec3 V1, FVec3 V2)
        {
            return V1.X * V2.X + V1.Y * V2.Y + V1.Z * V2.Z;
        }

        public static FVec3 Cross(FVec3 V1, FVec3 V2)
        {
            return new FVec3(
                V1.Z * V2.Y - V1.Y * V2.Z,
                V1.X * V2.Z - V1.Z * V2.X,
                V1.Y * V2.X - V1.X * V2.Y);
        }

        public static FVec3 operator * (FVec3 V, float M)
        {
            return new FVec3(V.X * M, V.Y * M, V.Z * M);
        }

		public static FVec3 operator *(float M, FVec3 V)
		{
			return new FVec3(V.X * M, V.Y * M, V.Z * M);
		}

		public static FVec3 operator *(FVec3 V, FVec3 M)
        {
            return new FVec3(V.X * M.X, V.Y * M.Y, V.Z * M.Z);
        }

        public static FVec3 operator /(FVec3 V, float M)
        {
            return new FVec3(V.X / M, V.Y / M, V.Z / M);
        }

        public static FVec3 operator -(FVec3 V, float M)
        {
            return new FVec3(V.X - M, V.Y - M, V.Z - M);
        }

        public static FVec3 operator -(FVec3 V, FVec3 M)
        {
            return new FVec3(V.X - M.X, V.Y - M.Y, V.Z - M.Z);
        }

        public static FVec3 operator -(FVec3 V)
        {
	        return new FVec3(-V.X, -V.Y, -V.Z);
        }

        public static FVec3 operator +(FVec3 V, float M)
        {
            return new FVec3(V.X + M, V.Y + M, V.Z + M);
        }

        public static FVec3 operator +(FVec3 V, FVec3 M)
        {
            return new FVec3(V.X + M.X, V.Y + M.Y, V.Z + M.Z);
        }

        public override string ToString()
        {
            return "" + X + "," + Y + "," + Z;
        }

        public MathPoint ToMathPoint()
        {
            return MathUtils.CreatePoint(X, Y, Z);
        }

        public MathVector ToMathVector()
        {
            return MathUtils.CreateVector(X, Y, Z);
        }
    }
}
