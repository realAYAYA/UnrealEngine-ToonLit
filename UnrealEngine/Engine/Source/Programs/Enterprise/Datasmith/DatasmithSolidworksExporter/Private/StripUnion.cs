// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
    [ComVisible(false)]
    [StructLayout(LayoutKind.Explicit)]
    public struct FStripUnion
    {
        [FieldOffset(sizeof(ulong))]
        public readonly int[] Ints;

        [FieldOffset(sizeof(ulong))]
        public readonly float[] Floats;

        public FStripUnion(float[] V) : this()
        {
            Floats = V;
        }

        public FStripUnion(int[] V) : this()
        {
            Ints = V;
        }

        public FStripUnion(int InElements) : this()
        {
            Ints = new int[InElements];
        }

		public int NumStrips { get { return Ints[0]; } }

        public int[] StripCounts
        {
            get
            {
                int[] Counts = new int[NumStrips];
                for (int Idx = 0; Idx < NumStrips; Idx++)
				{
                    Counts[Idx] = Ints[Idx + 1];
				}
                return Counts;
            }
        }

        public int[] StripOffsets
        {
            get
            {
                int[] Offsets = new int[NumStrips];
                int Offset = 0;
                for (int Idx = 0; Idx < NumStrips; Idx++)
                {
                    Offsets[Idx] = Offset;
                    Offset += Ints[Idx + 1];
                }
                return Offsets;
            }
        }

        public int StartOffset { get { return NumStrips + 1; } }
    }
}
