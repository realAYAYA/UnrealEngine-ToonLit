// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
    public class FMeshData
    {
        public FVec3[] Vertices { get; set; } = null;
        public FVec3[] Normals { get; set; } = null;
        public FVec2[] TexCoords { get; set; } = null;
        public FTriangle[] Triangles { get; set; } = null;

		public static FMeshData Create(List<FGeometryChunk> InChunks)
		{
			List<FTriangle> AllTriangles = new List<FTriangle>();

			List<FVertex> Verts = new List<FVertex>();

			int VertexOffset = 0;
			foreach (FGeometryChunk Chunk in InChunks)
			{
				bool bHasUv = Chunk.TexCoords != null;
				for (int Idx = 0; Idx < Chunk.Vertices.Length; Idx++)
				{
					Verts.Add(
						new FVertex(Chunk.Vertices[Idx] * 100f /*SwSingleton.GeometryScale*/,
						Chunk.Normals[Idx],
						bHasUv ? Chunk.TexCoords[Idx] : FVec2.Zero,
						VertexOffset + Idx));

				}
				foreach (FTriangle Triangle in Chunk.Triangles)
				{
					FTriangle OffsetTriangle = Triangle.Offset(VertexOffset);
					AllTriangles.Add(new FTriangle(OffsetTriangle[0], OffsetTriangle[1], OffsetTriangle[2], Triangle.MaterialID));
				}
				VertexOffset += Chunk.Vertices.Length;
			}

			FMeshData MeshData = new FMeshData();

			MeshData.Vertices = new FVec3[Verts.Count];
			MeshData.TexCoords = new FVec2[Verts.Count];
			MeshData.Normals = new FVec3[AllTriangles.Count * 3];

			for (int Idx = 0; Idx < Verts.Count; Idx++)
			{
				MeshData.Vertices[Idx] = Verts[Idx].P;
				MeshData.TexCoords[Idx] = Verts[Idx].UV;
			}

			for (int I = 0; I < AllTriangles.Count; I++)
			{
				FTriangle Triangle = AllTriangles[I];
				int Idx = I * 3;

				MeshData.Normals[Idx + 0] = Verts[Triangle.Index1].N;
				MeshData.Normals[Idx + 1] = Verts[Triangle.Index2].N;
				MeshData.Normals[Idx + 2] = Verts[Triangle.Index3].N;
			}

			MeshData.Triangles = AllTriangles.ToArray();

			return MeshData;
		}
	}
}
