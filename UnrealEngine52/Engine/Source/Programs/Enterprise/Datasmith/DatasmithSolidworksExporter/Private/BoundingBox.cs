// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
    public class FBoundingBox
    {
		public FVec3 Min { get; private set; }
		public FVec3 Max { get; private set; }

		public FVec3 Center { get; private set; }
        public float Size { get; private set; }
        public float Volume { get; private set; }

		public FBoundingBox()
		{
			Min = new FVec3(float.MaxValue, float.MaxValue, float.MaxValue);
			Max = new FVec3(float.MinValue, float.MinValue, float.MinValue);
		}

		public FBoundingBox(FVec3 InMin, FVec3 InMax)
		{
			Min = InMin;
			Max = InMax;
		}

        public void Add(FBoundingBox InOther)
        {
            Add(InOther.Min);
			Add(InOther.Max);
        }

        public void Add(FVec3 InPoint)
        {
			Min.X = Math.Min(Min.X, InPoint.X);
			Min.Y = Math.Min(Min.Y, InPoint.Y);
			Min.Z = Math.Min(Min.Z, InPoint.Z);

			Max.X = Math.Max(Max.X, InPoint.X);
			Max.Y = Math.Max(Max.Y, InPoint.Y);
			Max.Z = Math.Max(Max.Z, InPoint.Z);

			double DimX = Max.X - Min.X;
			double DimY = Max.Y - Min.Y;
			double DimZ = Max.Z - Min.Z;

			Center = 0.5f * (Min + Max);

			Size = (float)Math.Max(DimX, Math.Max(DimY, DimZ));
			Volume = (float)(DimX * DimY * DimZ);
        }

        public bool Contains(FVec3 InPoint)
        {
            return
                InPoint.X >= Min.X && InPoint.X <= Max.X &&
                InPoint.Y >= Min.Y && InPoint.Y <= Max.Y &&
                InPoint.Z >= Min.Z && InPoint.Z <= Max.Z;
        }

        public bool Contains(FVec3 InPoint, double InTolerance)
        {
			return
				InPoint.X >= (Min.X - InTolerance) && InPoint.X <= (Max.X + InTolerance) &&
				InPoint.Y >= (Min.Y - InTolerance) && InPoint.Y <= (Max.Y + InTolerance) &&
				InPoint.Z >= (Min.Z - InTolerance) && InPoint.Z <= (Max.Z + InTolerance);
        }

        public FBoundingBox Transform(FMatrix4 InMatrix)
        {
            FVec3 OMin = InMatrix.TransformPoint(Min);
            FVec3 OMax = InMatrix.TransformPoint(Max);

			FVec3 ResMin = new FVec3();
			FVec3 ResMax = new FVec3();

            ResMin.X = Math.Min(OMin.X, OMax.X);
            ResMin.Y = Math.Min(OMin.Y, OMax.Y);
            ResMin.Z = Math.Min(OMin.Z, OMax.Z);

            ResMax.X = Math.Max(OMin.X, OMax.X);
			ResMax.Y = Math.Max(OMin.Y, OMax.Y);
			ResMax.Z = Math.Max(OMin.Z, OMax.Z);

            return new FBoundingBox(ResMin, ResMax);
        }

        public static bool operator ==(FBoundingBox A, FBoundingBox B)
        {
			return (A == null && B == null) || A.Equals(B);
        }

        public static bool operator !=(FBoundingBox A, FBoundingBox B)
        {
            return !(A == B);
        }

        public override bool Equals(object InOther)
        {
            FBoundingBox Other = InOther as FBoundingBox;

            if (Other == null)
			{
                return false;
			}

			return Min == Other.Min && Max == Other.Max;
        }

        public override int GetHashCode()
        {
            int Hash = 47;
			Hash = Hash * 17 + Min.GetHashCode();
			Hash = Hash * 17 + Max.GetHashCode();
            return Hash;
        }
    }
}
