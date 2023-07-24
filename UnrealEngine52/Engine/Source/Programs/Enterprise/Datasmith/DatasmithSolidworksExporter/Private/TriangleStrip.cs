// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
	public class FTriangleStrip
	{
		public FStripUnion Vertices { get; set; }
		public FStripUnion Normals { get; set; }
		public FStripUnion TexCoords { get; set; }

		public int NumStrips;
		public int NumVertices;
		public int NumTris;

		public int[] StripOffsets { get; set; }
		public int[] TriangleCounts { get; set; }

		public int TrianglesInIndexBuffer { get; set; } = 0;

		public FTriangleStrip(float[] InVerts, float[] InNormals)
		{
			Vertices = new FStripUnion(InVerts);
			Normals = new FStripUnion(InNormals);

			NumStrips = Vertices.NumStrips;
			if (NumStrips > 0)
			{
				StripOffsets = new int[NumStrips];
				TriangleCounts = new int[NumStrips];

				NumVertices = (Vertices.Floats.Length - 1 - NumStrips) / 3;

				StripOffsets[0] = NumStrips + 1;
				NumTris = Vertices.Ints[1] - 2;
				TriangleCounts[0] = 0;

				for (int i = 1; i < NumStrips; i++)
				{
					NumTris += Vertices.Ints[1 + i] - 2;
					StripOffsets[i] = StripOffsets[i - 1] + Vertices.Ints[i] * 3;
					TriangleCounts[i] = TriangleCounts[i - 1] + Vertices.Ints[i] - 2;
				}
			}
		}

		public int[] BuildIndices()
		{
			int[] Indices = null;

			if (NumVertices >= 3)
			{
				TrianglesInIndexBuffer = 0;

				Indices = new int[NumTris * 3];

				var StripCounts = Vertices.StripCounts;
				var StripOffsets = Vertices.StripOffsets;

				for (int Stripindex = 0; Stripindex < NumStrips; Stripindex++)
				{
					int VertexCountInStrip = StripCounts[Stripindex];
					int VertexIndexForStrip = StripOffsets[Stripindex];

					if (VertexCountInStrip >= 3)
					{
						for (int TriangleIndex = 0; TriangleIndex < (VertexCountInStrip - 2); TriangleIndex++)
						{
							int Idx = TrianglesInIndexBuffer * 3;
							TrianglesInIndexBuffer++;

							Indices[Idx + 0] = VertexIndexForStrip + TriangleIndex + 0;
							if ((TriangleIndex & 1) == 0)
							{
								Indices[Idx + 1] = VertexIndexForStrip + TriangleIndex + 1;
								Indices[Idx + 2] = VertexIndexForStrip + TriangleIndex + 2;
							}
							else
							{
								Indices[Idx + 1] = VertexIndexForStrip + TriangleIndex + 2;
								Indices[Idx + 2] = VertexIndexForStrip + TriangleIndex + 1;
							}
						}
					}
				}
			}
			return Indices;
		}

		public int GetNumVerticesInStrip(int InStripNum)
		{
			return Vertices.Ints[1 + InStripNum];
		}
	}
}
