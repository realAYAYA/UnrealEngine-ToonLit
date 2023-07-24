// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
    public class FUVPlane
    {
        public FVec3 UDirection { get; set; }
        public FVec3 VDirection { get; set; }
        public FVec3 Normal { get; set; }
        public FVec3 Offset { get; set; }
    }
}
