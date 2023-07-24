// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
	public class FConstructionGeometry
	{
		public List<FVec3> CVertices;
		public List<FVec3> CNormals;
		public List<FVec2> CTexCoords;
		public List<FTriangle> CTriangles;
		private FGeometryChunk Original = null;

		public FConstructionGeometry(FGeometryChunk InChunk)
		{
			Original = InChunk;
			CVertices = Original.Vertices.ToList();
			CNormals = Original.Normals.ToList();
			CTexCoords = Enumerable.Repeat(new FVec2(0f, 0f), CVertices.Count).ToList();
			CTriangles = new List<FTriangle>();
		}

		// Remeshing

		public void AddTriangle(FVec3 InV1, FVec3 InV2, FVec3 InV3, FVec3 InN1, FVec3 InN2, FVec3 InN3, FUVPlane InPlane, FMaterial InMaterial)
		{
			int CurIndex = CVertices.Count;
			CTriangles.Add(new FTriangle(CurIndex, CurIndex + 1, CurIndex + 2, InMaterial.ID));
			CVertices.Add(InV1);
			CVertices.Add(InV2);
			CVertices.Add(InV3);
			CNormals.Add(InN1);
			CNormals.Add(InN2);
			CNormals.Add(InN3);
			CTexCoords.Add(InMaterial.ComputeVertexUV(InPlane, InV1));
			CTexCoords.Add(InMaterial.ComputeVertexUV(InPlane, InV2));
			CTexCoords.Add(InMaterial.ComputeVertexUV(InPlane, InV3));
		}

		public void AddTriangle(FTriangle InTriangle, FVec2 InUV1, FVec2 InUV2, FVec2 InUV3)
		{
			CTexCoords[InTriangle.Index1] = InUV1;
			CTexCoords[InTriangle.Index2] = InUV2;
			CTexCoords[InTriangle.Index3] = InUV3;
			CTriangles.Add(InTriangle);
		}

		public void AddTriangle(FVec3 InV1, FVec3 InV2, FVec3 InV3, FVec3 InN1, FVec3 InN2, FVec3 InN3, FVec2 InUV1, FVec2 InUV2, FVec2 InUV3, FMaterial InMaterial)
		{
			int CurIndex = CVertices.Count;
			CTriangles.Add(new FTriangle(CurIndex, CurIndex + 1, CurIndex + 2, InMaterial.ID));
			CVertices.Add(InV1);
			CVertices.Add(InV2);
			CVertices.Add(InV3);
			CNormals.Add(InN1);
			CNormals.Add(InN2);
			CNormals.Add(InN3);
			CTexCoords.Add(InUV1);
			CTexCoords.Add(InUV2);
			CTexCoords.Add(InUV3);
		}

		private void SplitTriangle(FVec3 InV1, FVec3 InV2, FVec3 InV3, FVec3 InN1, FVec3 InN2, FVec3 InN3, FUVPlane[] InPlanes, FMaterial InMaterial)
		{
			uint Splits = 0;
			bool[] SplitEdge = new bool[3];
			float U = 0.5f, V = 0.5f, W = 0.5f;

			if (InPlanes[0] != InPlanes[1])
			{
				Splits++;
				SplitEdge[0] = true;
				U = 0.5f;
			}

			if (InPlanes[1] != InPlanes[2])
			{
				Splits++;
				SplitEdge[1] = true;
				V = 0.5f;
			}

			if (InPlanes[2] != InPlanes[0])
			{
				Splits++;
				SplitEdge[2] = true;
				W = 0.5f;
			}

			float Sum = (U + V + W);
			FVec3 BaryWeights = new FVec3(U / Sum, V / Sum, W / Sum);

			if (Splits == 0)
			{
				AddTriangle(InV1, InV2, InV3, InN1, InN2, InN3, InPlanes[0], InMaterial);
			}
			else if (Splits == 2 || Splits == 3)
			{
				FVec3[] SplitVerts = new FVec3[3];
				FVec3[] SplitNorms = new FVec3[3];
				FVec3 CenterVertex = MathUtils.BarycentricToPoint(BaryWeights, InV1, InV2, InV3);
				FVec3 CenterNormal = MathUtils.BarycentricToPoint(BaryWeights, InN1, InN2, InN3).Normalized();

				float A = BaryWeights.X / (BaryWeights.X + BaryWeights.Y);
				SplitVerts[0] = (InV1 * A) + (InV2 * (1.0f - A));
				SplitNorms[0] = ((InN1 * A) + (InN2 * (1.0f - A))).Normalized();

				A = BaryWeights.Y / (BaryWeights.Y + BaryWeights.Z);
				SplitVerts[1] = (InV2 * A) + (InV3 * (1.0f - A));
				SplitNorms[1] = ((InN2 * A) + (InN3 * (1.0f - A))).Normalized();

				A = BaryWeights.Z / (BaryWeights.Z + BaryWeights.X);
				SplitVerts[2] = (InV3 * A) + (InV1 * (1.0f - A));
				SplitNorms[2] = ((InN3 * A) + (InN1 * (1.0f - A))).Normalized();

				AddTriangle(InV1, SplitVerts[0], CenterVertex, InN1, SplitNorms[0], CenterNormal, InPlanes[0], InMaterial);
				AddTriangle(SplitVerts[0], InV2, CenterVertex, SplitNorms[0], InN2, CenterNormal, InPlanes[1], InMaterial);
				AddTriangle(CenterVertex, InV2, SplitVerts[1], CenterNormal, InN2, SplitNorms[1], InPlanes[1], InMaterial);
				AddTriangle(SplitVerts[1], InV3, CenterVertex, SplitNorms[1], InN3, CenterNormal, InPlanes[2], InMaterial);
				AddTriangle(InV3, SplitVerts[2], CenterVertex, InN3, SplitNorms[2], CenterNormal, InPlanes[2], InMaterial);
				AddTriangle(SplitVerts[2], InV1, CenterVertex, SplitNorms[2], InN1, CenterNormal, InPlanes[0], InMaterial);
			}
		}

		private void AddCylindricalTriangle(FVec3[] InVertices, FVec3[] InNormals, FVec2[] InSigns, FMaterial InMaterial, FVec2 InCylinderScale, FVec2 InCylinderOffset, bool bInAngleNotZero, FVec2 InCylinderTextureRotate)
		{
			int CurIndex = CVertices.Count;
			CTriangles.Add(new FTriangle(CurIndex, CurIndex + 1, CurIndex + 2, InMaterial.ID));
			CVertices.Add(InVertices[0]);
			CVertices.Add(InVertices[1]);
			CVertices.Add(InVertices[2]);
			CNormals.Add(InNormals[0]);
			CNormals.Add(InNormals[1]);
			CNormals.Add(InNormals[2]);
			CTexCoords.Add(CapCylinderMapping(InSigns[0], InCylinderScale, InCylinderOffset, bInAngleNotZero, InCylinderTextureRotate));
			CTexCoords.Add(CapCylinderMapping(InSigns[1], InCylinderScale, InCylinderOffset, bInAngleNotZero, InCylinderTextureRotate));
			CTexCoords.Add(CapCylinderMapping(InSigns[2], InCylinderScale, InCylinderOffset, bInAngleNotZero, InCylinderTextureRotate));
		}

		public void AddCylindricalTriangleWithSplit(FVec3[] InVertices, FVec3[] InNormals, FVec2[] InSigns, FMaterial InMaterial, FVec2 InCCylinderScale, FVec2 InCylinderOffset, bool bInAngleNotZero, FVec2 InCylinderTextureRotate)
		{
			uint Splits = 0;
			bool[] SplitEdge = new bool[3];
			List<FVec3> PositiveVerts = new List<FVec3>();
			List<FVec3> PositiveNormals = new List<FVec3>();
			List<FVec2> PositiveUVs = new List<FVec2>();
			List<FVec3> NegativeVerts = new List<FVec3>();
			List<FVec3> NegativeNormals = new List<FVec3>();
			List<FVec2> NegativeUVs = new List<FVec2>();

			if (Math.Sign(InSigns[0].X) != Math.Sign(InSigns[1].Y))
			{
				if (Math.Abs(InSigns[0].X - InSigns[1].X) > 0.5f)
				{
					Splits++;
					SplitEdge[0] = true;
				}
			}

			if (Math.Sign(InSigns[1].X) != Math.Sign(InSigns[2].X))
			{
				if (Math.Abs(InSigns[1].X - InSigns[2].X) > 0.5f)
				{
					Splits++;
					SplitEdge[1] = true;
				}
			}

			if (Math.Sign(InSigns[2].X) != Math.Sign(InSigns[0].X))
			{
				if (Math.Abs(InSigns[2].X - InSigns[0].X) > 0.5f)
				{
					Splits++;
					SplitEdge[2] = true;
				}
			}

			for (uint I = 0; I < 3; I++)
			{
				if (InSigns[I].X < 0.0f)
				{
					NegativeVerts.Add(InVertices[I]);
					NegativeNormals.Add(InNormals[I]);
					NegativeUVs.Add(InSigns[I]);
				}
				else
				{
					PositiveVerts.Add(InVertices[I]);
					PositiveNormals.Add(InNormals[I]);
					PositiveUVs.Add(InSigns[I]);
				}
			}

			if (Splits == 0)
			{
				AddCylindricalTriangle(InVertices, InNormals, InSigns, InMaterial, InCCylinderScale, InCylinderOffset, bInAngleNotZero, InCylinderTextureRotate);
			}
			else if (Splits == 2)
			{
				FVec3[] SplitVerts = new FVec3[2];
				FVec3[] SplitNormals = new FVec3[2];
				FVec2[] SplitUVs = new FVec2[2];

				if (NegativeVerts.Count > PositiveVerts.Count)
				{
					SplitVerts[0] = (NegativeVerts[0] + PositiveVerts[0]) * 0.5f;
					SplitNormals[0] = (NegativeNormals[0] + PositiveNormals[0]) * 0.5f;
					SplitUVs[0] = (NegativeUVs[0] + PositiveUVs[0]) * 0.5f;

					SplitVerts[1] = (NegativeVerts[1] + PositiveVerts[0]) * 0.5f;
					SplitNormals[1] = (NegativeNormals[1] + PositiveNormals[0]) * 0.5f;
					SplitUVs[1] = (NegativeUVs[1] + PositiveUVs[0]) * 0.5f;

					AddCylindricalTriangle(
						new FVec3[] { NegativeVerts[0], SplitVerts[1], SplitVerts[0] },
						new FVec3[] { NegativeNormals[0], SplitNormals[1], SplitNormals[0] },
						new FVec2[] { NegativeUVs[0], new FVec2(-0.5f, SplitUVs[1].Y), new FVec2(-0.5f, SplitUVs[0].Y) },
						InMaterial, InCCylinderScale, InCylinderOffset, bInAngleNotZero, InCylinderTextureRotate);

					AddCylindricalTriangle(new FVec3[] { NegativeVerts[0], NegativeVerts[1], SplitVerts[1] },
											new FVec3[] { NegativeNormals[0], NegativeNormals[1], SplitNormals[1] },
											new FVec2[] { NegativeUVs[0], NegativeUVs[1], new FVec2(-0.5f, SplitUVs[1].Y) },
											InMaterial, InCCylinderScale, InCylinderOffset, bInAngleNotZero, InCylinderTextureRotate);

					AddCylindricalTriangle(new FVec3[] { PositiveVerts[0], SplitVerts[0], SplitVerts[1] },
											new FVec3[] { PositiveNormals[0], SplitNormals[0], SplitNormals[1] },
											new FVec2[] { PositiveUVs[0], new FVec2(0.5f, SplitUVs[0].Y), new FVec2(0.5f, SplitUVs[1].Y) },
											InMaterial, InCCylinderScale, InCylinderOffset, bInAngleNotZero, InCylinderTextureRotate);
				}
				else
				{
					SplitVerts[0] = (NegativeVerts[0] + PositiveVerts[0]) * 0.5f;
					SplitNormals[0] = (NegativeNormals[0] + PositiveNormals[0]) * 0.5f;
					SplitUVs[0] = (NegativeUVs[0] + PositiveUVs[0]) * 0.5f;
					SplitVerts[1] = (NegativeVerts[0] + PositiveVerts[1]) * 0.5f;
					SplitNormals[1] = (NegativeNormals[0] + PositiveNormals[1]) * 0.5f;
					SplitUVs[1] = (NegativeUVs[0] + PositiveUVs[1]) * 0.5f;

					AddCylindricalTriangle(new FVec3[] { PositiveVerts[0], SplitVerts[0], SplitVerts[1] },
											new FVec3[] { PositiveNormals[0], SplitNormals[0], SplitNormals[1] },
											new FVec2[] { PositiveUVs[0], new FVec2(0.5f, SplitUVs[0].Y), new FVec2(0.5f, SplitUVs[1].Y) },
											InMaterial, InCCylinderScale, InCylinderOffset, bInAngleNotZero, InCylinderTextureRotate);

					AddCylindricalTriangle(new FVec3[] { PositiveVerts[0], SplitVerts[1], PositiveVerts[1] },
											new FVec3[] { PositiveNormals[0], SplitNormals[1], PositiveNormals[1] },
											new FVec2[] { PositiveUVs[0], new FVec2(0.5f, SplitUVs[1].Y), PositiveUVs[1] },
											InMaterial, InCCylinderScale, InCylinderOffset, bInAngleNotZero, InCylinderTextureRotate);

					AddCylindricalTriangle(new FVec3[] { NegativeVerts[0], SplitVerts[1], SplitVerts[0] },
											new FVec3[] { NegativeNormals[0], SplitNormals[1], SplitNormals[0] },
											new FVec2[] { NegativeUVs[0], new FVec2(-0.5f, SplitUVs[1].Y), new FVec2(-0.5f, SplitUVs[0].Y) },
											InMaterial, InCCylinderScale, InCylinderOffset, bInAngleNotZero, InCylinderTextureRotate);
				}
			}
		}

		private static FVec2 CapCylinderMapping(FVec2 InAtan2, FVec2 InCylinderScale, FVec2 InCylinderOffset, bool bInAngleNotZero, FVec2 InCylinderTextureRotate)
		{
			InAtan2 = FVec2.Translate(InAtan2, new FVec2(0.5f, 0));
			InAtan2 = FVec2.Translate(FVec2.Scale(InAtan2, InCylinderScale), InCylinderOffset);
			if (bInAngleNotZero)
			{
				InAtan2 = FVec2.RotateOnPlane(InCylinderTextureRotate, InAtan2);
			}
			return InAtan2;
		}

		// UV generation

		public void ComputeSphericalUV(FMaterial InMaterial)
		{
			Original.TexCoords = new FVec2[Original.Vertices.Length];
			float Angle = (float)(-InMaterial.RotationAngle * MathUtils.Deg2Rad);
			float RotationCos = (float)Math.Cos(Angle);
			float RotationSin = (float)Math.Sin(Angle);
			float FlipU = InMaterial.MirrorHorizontal ? -1f : 1f;
			float FlipV = InMaterial.MirrorVertical ? -1f : 1f;
			bool bIsAngleNotZero = !MathUtils.Equals(InMaterial.RotationAngle, 0.0);
			float ScaleU = (float)(Original.ModelSize * FlipU / InMaterial.Width);
			float ScaleV = (float)(Original.ModelSize * FlipV / InMaterial.Height);
			float OffsetU = (float)((FlipU / InMaterial.Width) * (bIsAngleNotZero ? InMaterial.XPos * RotationCos - InMaterial.YPos * RotationSin : InMaterial.XPos));
			float OffsetV = (float)((FlipV / InMaterial.Height) * (bIsAngleNotZero ? InMaterial.XPos * RotationSin + InMaterial.YPos * RotationCos : InMaterial.YPos));

			for (var Idx = 0; Idx < Original.Vertices.Length; Idx++)
			{
				FVec3 Normal = new FVec3(Original.Vertices[Idx].X - Original.ModelCenter.X, Original.Vertices[Idx].Y - Original.ModelCenter.Y, Original.Vertices[Idx].Z - Original.ModelCenter.Z).Normalized();
				Normal = InMaterial.RotateVectorByXY(Normal, Original.ModelCenter);
				Original.TexCoords[Idx] = new FVec2((float)(0.5 + Math.Atan2(Normal.X, -Normal.Z) / (Math.PI * 2)), (float)(0.5 - Math.Asin(Normal.Y) / Math.PI));
				if (bIsAngleNotZero)
				{
					MathUtils.RotateOnPlane(RotationCos, RotationSin, ref Original.TexCoords[Idx].X, ref Original.TexCoords[Idx].Y);
				}
				Original.TexCoords[Idx].X = Original.TexCoords[Idx].X * ScaleU;
				Original.TexCoords[Idx].Y = Original.TexCoords[Idx].Y * ScaleV;
				Original.TexCoords[Idx].X = Original.TexCoords[Idx].X + OffsetU;
				Original.TexCoords[Idx].Y = Original.TexCoords[Idx].Y + OffsetV;
			}
		}

		public void ComputePlanarUV(FMaterial InMaterial)
		{
			FUVPlane[] Planes = new FUVPlane[3];
			List<FUVPlane> MatPlanes = InMaterial.ComputeUVPlanes();

			if (Original.Vertices.Length >= 3)
			{
				for (int TriIdx = 0; TriIdx < Original.Triangles.Length; TriIdx++)
				{
					FTriangle Triangle = Original.Triangles[TriIdx];
					Planes[0] = InMaterial.GetTexturePlane(MatPlanes, Original.Normals[Triangle.Index1]);
					Planes[1] = InMaterial.GetTexturePlane(MatPlanes, Original.Normals[Triangle.Index2]);
					Planes[2] = InMaterial.GetTexturePlane(MatPlanes, Original.Normals[Triangle.Index3]);

					if ((Planes[0] != Planes[1]) || (Planes[1] != Planes[2]) || (Planes[2] != Planes[0]))
					{
						SplitTriangle(
							Original.Vertices[Triangle.Index1], Original.Vertices[Triangle.Index2], Original.Vertices[Triangle.Index3], 
							Original.Normals[Triangle.Index1], Original.Normals[Triangle.Index2], Original.Normals[Triangle.Index3], Planes, InMaterial);
					}
					else
					{
						AddTriangle(Triangle,
								InMaterial.ComputeVertexUV(Planes[0], Original.Vertices[Triangle.Index1]),
								InMaterial.ComputeVertexUV(Planes[0], Original.Vertices[Triangle.Index2]),
								InMaterial.ComputeVertexUV(Planes[0], Original.Vertices[Triangle.Index3]));
					}
				}
			}
			Original.Vertices = CVertices.ToArray();
			Original.Normals = CNormals.ToArray();
			Original.TexCoords = CTexCoords.ToArray();
			Original.Triangles = CTriangles.ToArray();
		}

		public void ComputeCylindricalUV(FMaterial InMaterial)
		{
			float FlipU = InMaterial.MirrorHorizontal ? -1f : 1f;
			float FlipV = InMaterial.MirrorVertical ? -1f : 1f;
			float Angle = (float)(-InMaterial.RotationAngle * MathUtils.Deg2Rad);
			FVec2 Rotation = new FVec2((float)Math.Cos(Angle), (float)Math.Sin(FlipU * FlipV * Angle));
			bool bAngleNotZero = !MathUtils.Equals(InMaterial.RotationAngle, 0.0);
			FVec2 Scale = new FVec2((float)(Original.ModelSize * FlipU / InMaterial.Width), (float)(FlipV / InMaterial.Height));
			FVec2 Offset = new FVec2((float)(InMaterial.XPos * FlipU / InMaterial.Width), (float)(InMaterial.YPos * FlipV / InMaterial.Height));

			if (Original.Vertices.Length >= 3)
			{
				for (int TriIdx = 0; TriIdx < Original.Triangles.Length; TriIdx++)
				{
					FTriangle Triangle = Original.Triangles[TriIdx];
					FVec2[] signs = new FVec2[3] 
					{
						InMaterial.ComputeNormalAtan2(Original.Vertices[Triangle.Index1], Original.ModelCenter),
						InMaterial.ComputeNormalAtan2(Original.Vertices[Triangle.Index2], Original.ModelCenter),
						InMaterial.ComputeNormalAtan2(Original.Vertices[Triangle.Index3], Original.ModelCenter)
					};

					if ((Math.Sign(signs[0].X) != Math.Sign(signs[1].X)) || (Math.Sign(signs[1].X) != Math.Sign(signs[2].X)) || (Math.Sign(signs[2].X) != Math.Sign(signs[0].X)))
					{
						AddCylindricalTriangleWithSplit(new FVec3[] { Original.Vertices[Triangle.Index1], Original.Vertices[Triangle.Index2], Original.Vertices[Triangle.Index3] },
								new FVec3[] { Original.Normals[Triangle.Index1], Original.Normals[Triangle.Index2], Original.Normals[Triangle.Index3] },
								signs, InMaterial, Scale, Offset, bAngleNotZero, Rotation);
					}
					else
					{
						AddTriangle(Triangle,
								CapCylinderMapping(signs[0], Scale, Offset, bAngleNotZero, Rotation),
								CapCylinderMapping(signs[1], Scale, Offset, bAngleNotZero, Rotation),
								CapCylinderMapping(signs[2], Scale, Offset, bAngleNotZero, Rotation));
					}
				}
			}
			Original.Vertices = CVertices.ToArray();
			Original.Normals = CNormals.ToArray();
			Original.TexCoords = CTexCoords.ToArray();
			Original.Triangles = CTriangles.ToArray();
		}
	}
}
