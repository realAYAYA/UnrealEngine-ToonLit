// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
    public class FVertex
    {
        public FVec3 P { get; set; }
        public FVec3 N { get; set; }
        public FVec2 UV { get; set; }
        public int Index { get; set; }

        public FVertex(FVec3 InPoint, FVec3 InNormal, FVec2 InUV, int InIndex)
        {
            P = InPoint;
            N = InNormal;
            UV = InUV;
            Index = InIndex;
        }

        public override int GetHashCode()
        {
            int Hash = Index;
            Hash = Hash * 17 + P.GetHashCode();
            Hash = Hash * 17 + N.GetHashCode();
            Hash = Hash * 17 + UV.GetHashCode();
            return Hash;
        }

        public static bool operator < (FVertex V1, FVertex V2)
        {
			return (V1.P < V2.P) || ((V1.P == V2.P) && (V1.N < V2.N));
        }

        public static bool operator > (FVertex V1, FVertex V2)
        {
			return (V1.P > V2.P) || ((V1.P == V2.P) && (V1.N > V2.N));
        }

        public override bool Equals(object InObj)
        {
            if (InObj == null)
            {
                return false;
            }
            if (GetType() != InObj.GetType())
            {
                return false;
            }
            return Equals((FVertex)InObj);
        }

        public bool Equals(FVertex InVertex)
        {
            if (InVertex == null)
			{
                return false;
			}

			if (ReferenceEquals(this, InVertex))
			{
                return true;
			}

            if (GetHashCode() != InVertex.GetHashCode())
			{
                return false;
			}

            if (!base.Equals(InVertex))
			{
                return false;
			}

            return (this == InVertex);
        }
    }
}
