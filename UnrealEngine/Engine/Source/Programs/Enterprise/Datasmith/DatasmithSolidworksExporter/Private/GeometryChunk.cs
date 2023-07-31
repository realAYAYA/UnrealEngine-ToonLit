// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
    [ComVisible(false)]
    public class FGeometryChunk
    {
        public FVec3[] Vertices { get; set; } = null;
        public FVec3[] Normals { get; set; } = null;
        public FVec2[] TexCoords { get; set; } = null;
        public FTriangle[] Triangles = null;

        public float ModelSize { get; private set; }
        public FVec3 ModelCenter { get; private set; }

        public FGeometryChunk()
        {
        }

        public FGeometryChunk(int InBaseOffset, float[] InVertices, float[] InNormals, float[] InTexCoords, int[] InTriangleIndices, FBoundingBox InBounds, FMaterial InMaterial)
        {
            int VertexCount = (InVertices.Length - InBaseOffset) / 3;
            int NumFaces = InTriangleIndices.Length / 3;
            Vertices = new FVec3[VertexCount];
            Normals = new FVec3[VertexCount];
            Triangles = new FTriangle[NumFaces];

            ModelSize = InBounds.Size;
            ModelCenter = InBounds.Center;

            for (int I = InBaseOffset, J = 0; I < InVertices.Length; I += 3, J++)
            {
                Vertices[J] = new FVec3(InVertices[I], InVertices[I + 1], InVertices[I + 2]);
                Normals[J] = new FVec3(InNormals[I], InNormals[I + 1], InNormals[I + 2]);
            }

            int MatID = InMaterial != null ? InMaterial.ID : -1;

            for (int I = 0, J = 0; I < InTriangleIndices.Length; I += 3, J++)
            {
                Triangles[J] = new FTriangle(
					InTriangleIndices[I + 0], 
					InTriangleIndices[I + 1], 
					InTriangleIndices[I + 2], MatID);
            }

            // UV mapping

            if (InMaterial != null)
            {
                FConstructionGeometry CG = new FConstructionGeometry(this);

				switch (InMaterial.UVMappingType)
				{
					case FMaterial.EMappingType.TYPE_SPHERICAL:
					{
						CG.ComputeSphericalUV(InMaterial);
					}
					break;

					case FMaterial.EMappingType.TYPE_PROJECTION:
					case FMaterial.EMappingType.TYPE_AUTOMATIC:
					{
						CG.ComputePlanarUV(InMaterial);
					}
					break;

					case FMaterial.EMappingType.TYPE_CYLINDRICAL:
					{
						CG.ComputeCylindricalUV(InMaterial);
					}
					break;

					/*
                    case SwMaterial.MappingType.TYPE_SURFACE:
					{
						//GetSurfaceUVMapping(material, triStripInfo, swFace);
                    }
                    break;
                    */
				}
			}
		}
	}
}
