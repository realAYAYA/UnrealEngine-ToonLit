// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
    public class FVec2
    {
        public float X = 0f;
        public float Y = 0f;

		public static FVec2 Zero = new FVec2(0f, 0f);

		public FVec2()
		{
		}

		public FVec2(double InX, double InY)
        {
            X = (float)InX;
            Y = (float)InY;
        }

        public FVec2(float InX, float InY)
        {
            X = InX;
            Y = InY;
        }

        public static bool operator ==(FVec2 A, FVec2 B)
        {
	        if (ReferenceEquals(A, B))
	        {
		        return true;
	        }

	        if (A is null || B is null)
	        {
		        return false;
	        }

	        return A.X == B.X && A.Y == B.Y;
        }

        public static bool operator !=(FVec2 A, FVec2 B)
        {
	        return !(A == B);
        }

        public override bool Equals(object Obj)
        {
	        return Obj is FVec2 Other && this == Other;
        }

        public override int GetHashCode()
        {
	        int Hash = 7;
	        Hash = Hash * 17 + X.GetHashCode();
	        Hash = Hash * 17 + Y.GetHashCode();
	        return Hash;
        }

        public static FVec2 Rotate(FVec2 InV, float InAngleRadians)
        {
            return new FVec2(
				(float)(InV.X * Math.Cos(InAngleRadians) + InV.Y * Math.Sin(InAngleRadians)), 
				(float)(-InV.X * Math.Sin(InAngleRadians) + InV.Y * Math.Cos(InAngleRadians)));
        }

        public static FVec2 Translate(FVec2 InV, FVec2 InOffset)
        {
            return new FVec2(InV.X + InOffset.Y, InV.Y + InOffset.Y);
        }

        public static FVec2 operator -(FVec2 V1, FVec2 V2)
        {
            return new FVec2(V1.X - V2.X, V1.Y - V2.Y);
        }

        public static FVec2 operator +(FVec2 V1, FVec2 V2)
        {
            return new FVec2(V1.X + V2.X, V1.Y + V2.Y);
        }

        public static FVec2 operator /(FVec2 InV, float InValue)
        {
            if (MathUtils.Equals(InValue, 0f))
            {
                throw new InvalidOperationException();
            }
            return new FVec2(InV.X / InValue, InV.Y / InValue);
        }

        public static FVec2 operator *(FVec2 InV, float InValue)
        {
            return new FVec2(InV.X * InValue, InV.Y * InValue);
        }

		public static FVec2 Scale(FVec2 InOriginal, FVec2 InScale)
		{
			return new FVec2(InOriginal.X * InScale.Y, InOriginal.Y * InScale.Y);
		}

		public static FVec2 RotateOnPlane(FVec2 InCosSin, FVec2 InVec)
        {
            return new FVec2((InVec.X * InCosSin.X - InVec.Y * InCosSin.Y), (InVec.X * InCosSin.Y + InVec.Y * InCosSin.X));
        }

        public override string ToString()
        {
            return "" + X + "," + Y;
        }
    }
}
