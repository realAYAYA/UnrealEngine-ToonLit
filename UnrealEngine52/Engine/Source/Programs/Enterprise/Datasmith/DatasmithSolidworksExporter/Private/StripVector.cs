// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
    public class FStripVector
    {
        private FStripUnion BaseArray;
        private int DataStartIndex;
        private int DataSubarrays;
        private int[] SubarrayVertexOffset;

        public int NumberOfStrips { get { return BaseArray.NumStrips; } }
        public int TotalBackingElements
        {
            get
            {
                if (SubarrayVertexOffset == null)
                {
                    // Just a normal array backing us, no header
                    return BaseArray.Floats.Length;
                }
                else
                {
                    // We're in solidworks data mode, skip header data
                    return (BaseArray.Floats.Length - 1 - NumberOfStrips);
                }

            }
        }
        public int[] StripVertices { get { return BaseArray.StripCounts; } }
        public int[] StripVertexOffsets { get { return BaseArray.StripOffsets; } }
        public float[] BaseFloatArray { get { return BaseArray.Floats; } }
        public int[] BaseIntArray { get { return BaseArray.Ints; } }

        public FStripVector()
        {
        }

        public static FStripVector BuildStripVector(FStripUnion InUnion)
        {
            FStripVector V = new FStripVector();
            V.BaseArray = InUnion;
            V.DataSubarrays = V.BaseArray.NumStrips;
            V.SubarrayVertexOffset = V.BaseArray.StripOffsets;
            V.DataStartIndex = V.BaseArray.StartOffset;
            return V;
        }

        public FStripVector(int InDimensions, int InElements)
        {
            BaseArray = new FStripUnion(InDimensions * InElements);
            DataSubarrays = 0;
            SubarrayVertexOffset = null;
            DataStartIndex = 0;
        }

        public FVec2 GetVector2(int InIndex)
        {
			return new FVec2(
				BaseArray.Floats[(InIndex << 1) + 0 + DataStartIndex],
				BaseArray.Floats[(InIndex << 1) + 1 + DataStartIndex]);
		}

		public FVec2 GetVector2(int InStrip, int InIndex)
        {
            return GetVector2(InIndex + SubarrayVertexOffset[InStrip]);
        }

        public FVec3 GetVector3(int InIndex)
        {
            return new FVec3(
				BaseArray.Floats[(InIndex * 3) + 0 + DataStartIndex],
				BaseArray.Floats[(InIndex * 3) + 1 + DataStartIndex],
				BaseArray.Floats[(InIndex * 3) + 2 + DataStartIndex]);
        }

        public FVec3 GetVector3(int InStrip, int InIndex)
        {
            return GetVector3(InIndex + SubarrayVertexOffset[InStrip]);
        }

        public void setTriangleIndices(uint InTriangleIndex, FTriangle InIndicies)
        {
            BaseArray.Ints[(InTriangleIndex * 3) + 0 + DataStartIndex] = InIndicies.Index1;
            BaseArray.Ints[(InTriangleIndex * 3) + 1 + DataStartIndex] = InIndicies.Index2;
            BaseArray.Ints[(InTriangleIndex * 3) + 2 + DataStartIndex] = InIndicies.Index3;
        }
    }
}
