// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading.Tasks;

namespace DatasmithSolidworks
{
	[ComVisible(false)]
	public class FStripGeometryBody
	{
		public FBoundingBox Bounds { get; set; } = null;
		public List<FStripGeometryFace> Faces { get; set; } = new List<FStripGeometryFace>();

		public void ExtractGeometry(List<FGeometryChunk> InChunks)
		{
			foreach (var Face in Faces)
			{
				FGeometryChunk Chunk = Face.ExtractGeometry(this);
				if (Chunk != null)
				{
					InChunks.Add(Chunk);
				}
			}
		}
	}

	[ComVisible(false)]
	public class FStripGeometryFace
	{
		public FMaterial Material { get; set; } = null;
		public FTriangleStrip Strip { get; set; } = null;

		public FGeometryChunk ExtractGeometry(FStripGeometryBody InBody)
		{
			float[] Vertices = Strip?.Vertices.Floats;
			if (Vertices == null)
			{
				return null;
			}

			int StripVertexCount = Strip.NumVertices;
			int[] Indices = Strip.BuildIndices();
			float[] Normals = Strip.Normals.Floats;
			float[] TexCoords = Strip.TexCoords.Floats;

			int NumFaces = Strip.TrianglesInIndexBuffer;
			int BaseOffset = Strip.StripOffsets[0];

			FGeometryChunk Chunk = new FGeometryChunk(
				BaseOffset,
				Vertices,
				Normals,
				TexCoords,
				Indices,
				InBody.Bounds,
				Material);

			return Chunk;
		}
	}

	[ComVisible(false)]
	public class FStripGeometry
	{
		private ConcurrentBag<FStripGeometryBody> Bodies { get; set; } = new ConcurrentBag<FStripGeometryBody>();

		private List<FGeometryChunk> ExtractGeometry()
		{
			List<FGeometryChunk> Chunks = new List<FGeometryChunk>();
			try
			{
				foreach (FStripGeometryBody Body in Bodies)
				{
					Body.ExtractGeometry(Chunks);
				}
			}
			catch { }

			return Chunks;
		}

		public static FMeshData CreateMeshData(ConcurrentBag<FBody> InBodies, FObjectMaterials InMaterials)
		{
			if (InBodies == null || InBodies.Count == 0)
			{
				return null;
			}

			FStripGeometry StripGeom = new FStripGeometry();

			Parallel.ForEach(InBodies, Body =>
			{
				FStripGeometryBody StripBody = new FStripGeometryBody();
				StripBody.Bounds = Body.Bounds;
				StripGeom.Bodies.Add(StripBody);

				foreach (FBody.FBodyFace Face in Body.Faces)
				{
					FStripGeometryFace StripFace = new FStripGeometryFace();
					StripFace.Strip = Face.ExtractGeometry();
					StripFace.Material = InMaterials?.GetMaterial(Face.Face);
					StripBody.Faces.Add(StripFace);
				}
			});

			FMeshData MeshData = null;
			try
			{
				List<FGeometryChunk> Chunks = StripGeom.ExtractGeometry();
				if (Chunks != null && Chunks.Count > 0)
				{
					MeshData = FMeshData.Create(Chunks);
				}
			}
			catch { }

			return MeshData;
		}
	}
}
