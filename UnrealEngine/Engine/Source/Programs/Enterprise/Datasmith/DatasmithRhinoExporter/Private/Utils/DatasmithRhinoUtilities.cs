// Copyright Epic Games, Inc. All Rights Reserved.

using DatasmithRhino.ExportContext;
using Rhino;
using Rhino.DocObjects;
using Rhino.Geometry;
using Rhino.Runtime;
using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace DatasmithRhino.Utils
{
	public static class DatasmithRhinoUtilities
	{
		public static string GetNamedViewHash(ViewInfo NamedView)
		{
			return ComputeHashStringFromBytes(SerializeNamedView(NamedView));
		}
		
		public static string GetMaterialHash(Material RhinoMaterial)
		{
			return ComputeHashStringFromBytes(SerializeMaterial(RhinoMaterial));
		}

		public static string GetTextureHash(Texture RhinoTexture)
		{
			string HashString;

			using (MemoryStream Stream = new MemoryStream())
			{
				using (BinaryWriter Writer = new BinaryWriter(Stream))
				{
					SerializeTexture(Writer, RhinoTexture);

				}

				HashString = ComputeHashStringFromBytes(Stream.ToArray());
			}

			return HashString;
		}

		private static string ComputeHashStringFromBytes(byte[] Data)
		{
			MD5 Md5Hash = MD5.Create();
			byte[] Hash = Md5Hash.ComputeHash(Data);

			StringBuilder Builder = new StringBuilder();
			Builder.Capacity = Hash.Length;
			for (int ByteIndex = 0, ByteCount = Hash.Length; ByteIndex < ByteCount; ++ByteIndex)
			{
				//Format the bytes hexadecimal characters.
				Builder.Append(Hash[ByteIndex].ToString("X2"));
			}

			return Builder.ToString();
		}

		private static byte[] SerializeNamedView(ViewInfo NamedView)
		{
			using (MemoryStream Stream = new MemoryStream())
			{
				using (BinaryWriter Writer = new BinaryWriter(Stream))
				{
					Writer.Write(NamedView.Name);

					ViewportInfo Viewport = NamedView.Viewport;
					Writer.Write(Viewport.FrustumAspect);
					SerializeVector3d(Writer, new Vector3d(Viewport.CameraLocation));
					SerializeVector3d(Writer, Viewport.CameraDirection);
					SerializeVector3d(Writer, Viewport.CameraUp);
					Writer.Write(Viewport.Camera35mmLensLength);
				}
				return Stream.ToArray();
			}
		}

		private static byte[] SerializeMaterial(Material RhinoMaterial)
		{
			using (MemoryStream Stream = new MemoryStream())
			{
				using (BinaryWriter Writer = new BinaryWriter(Stream))
				{
					// Hash the material properties that are used to create the Unreal material: diffuse color, transparency, shininess and texture maps
					if (RhinoMaterial.Name != null)
					{
						Writer.Write(RhinoMaterial.Name);
					}

					Writer.Write(RhinoMaterial.DiffuseColor.R);
					Writer.Write(RhinoMaterial.DiffuseColor.G);
					Writer.Write(RhinoMaterial.DiffuseColor.B);
					Writer.Write(RhinoMaterial.DiffuseColor.A);

					Writer.Write(RhinoMaterial.Transparency);
					Writer.Write(RhinoMaterial.Shine);
					Writer.Write(RhinoMaterial.Reflectivity);

					Texture[] MaterialTextures = RhinoMaterial.GetTextures();
					for (int Index = 0; Index < MaterialTextures.Length; ++Index)
					{
						if (MaterialTextures[Index] is Texture RhinoTexture)
						{
							SerializeTexture(Writer, RhinoTexture);
						}
					}
				}

				return Stream.ToArray();
			}
		}

		public static bool IsTextureSupported(Texture RhinoTexture)
		{
			return (RhinoTexture.Enabled 
				&& (RhinoTexture.TextureType == TextureType.Bitmap 
					|| RhinoTexture.TextureType == TextureType.Bump 
					|| RhinoTexture.TextureType == TextureType.Transparency));
		}

		private static void SerializeTexture(BinaryWriter Writer, Texture RhinoTexture)
		{
			if (!IsTextureSupported(RhinoTexture))
			{
				return;
			}

			string FullPath = RhinoTexture.FileReference.FullPath;
			string FilePath;
			if (FullPath.Length != 0)
			{
				FilePath = FullPath;
			}
			else
			{
				string RelativePath = RhinoTexture.FileReference.RelativePath;
				if (RelativePath.Length == 0)
				{
					//No valid path found, skip the texture.
					return;
				}
				FilePath = RelativePath;
			}

			// Hash the parameters for texture maps
			Writer.Write(FilePath);
			Writer.Write((int)RhinoTexture.TextureType);
			Writer.Write(RhinoTexture.MappingChannelId);

			double Constant, A0, A1, A2, A3;
			RhinoTexture.GetAlphaBlendValues(out Constant, out A0, out A1, out A2, out A3);
			Writer.Write(Constant);
			Writer.Write(A0);
			Writer.Write(A1);
			Writer.Write(A2);
			Writer.Write(A3);

			SerializeTransform(Writer, RhinoTexture.UvwTransform);
		}

		private static void SerializeVector3d(BinaryWriter Writer, Vector3d Vector)
		{
			Writer.Write(Vector.X);
			Writer.Write(Vector.Y);
			Writer.Write(Vector.Z);
		}

		private static void SerializeTransform(BinaryWriter Writer, Rhino.Geometry.Transform RhinoTransform)
		{
			Writer.Write(RhinoTransform.M00);
			Writer.Write(RhinoTransform.M01);
			Writer.Write(RhinoTransform.M02);
			Writer.Write(RhinoTransform.M03);
			Writer.Write(RhinoTransform.M10);
			Writer.Write(RhinoTransform.M11);
			Writer.Write(RhinoTransform.M12);
			Writer.Write(RhinoTransform.M13);
			Writer.Write(RhinoTransform.M20);
			Writer.Write(RhinoTransform.M21);
			Writer.Write(RhinoTransform.M22);
			Writer.Write(RhinoTransform.M23);
			Writer.Write(RhinoTransform.M30);
			Writer.Write(RhinoTransform.M31);
			Writer.Write(RhinoTransform.M32);
			Writer.Write(RhinoTransform.M33);
		}

		public static double RadianToDegree(double Radian)
		{
			return Radian * (180 / Math.PI);
		}

		public static Transform GetCommonObjectTransform(CommonObject InCommonObject)
		{
			switch (InCommonObject)
			{
				case InstanceObject InInstance:
					return InInstance.InstanceXform;

				case PointObject InPoint:
					Vector3d PointLocation = new Vector3d(InPoint.PointGeometry.Location);
					return Transform.Translation(PointLocation);

				case LightObject InLightObject:
					return GetLightTransform(InLightObject.LightGeometry);

				case ViewportInfo InViewportInfo:
					return Transform.Translation(new Vector3d(InViewportInfo.CameraLocation));

				default:
					return Transform.Identity;
			}
		}

		public static Transform GetLightTransform(Light LightGeometry)
		{
			if (LightGeometry.IsLinearLight || LightGeometry.IsRectangularLight)
			{
				return GetAreaLightTransform(LightGeometry);
			}
			else
			{
				Transform RotationTransform = Transform.Rotation(new Vector3d(1, 0, 0), LightGeometry.Direction, new Point3d(0, 0, 0));
				Transform TranslationTransform = Transform.Translation(new Vector3d(LightGeometry.Location));

				return TranslationTransform * RotationTransform;
			}
		}

		private static Transform GetAreaLightTransform(Light LightGeometry)
		{
			Point3d Center = LightGeometry.Location + 0.5 * LightGeometry.Length;
			if (LightGeometry.IsRectangularLight)
			{
				Center += 0.5 * LightGeometry.Width;
			}

			Vector3d InvLengthAxis = -LightGeometry.Length;
			Vector3d WidthAxis = -LightGeometry.Width;
			Vector3d InvLightAxis = Vector3d.CrossProduct(WidthAxis, InvLengthAxis);
			InvLengthAxis.Unitize();
			WidthAxis.Unitize();
			InvLightAxis.Unitize();

			Transform RotationTransform = Transform.ChangeBasis(InvLightAxis, WidthAxis, InvLengthAxis, new Vector3d(1, 0, 0), new Vector3d(0, 1, 0), new Vector3d(0, 0, 1));
			Transform TranslationTransform = Transform.Translation(new Vector3d(Center));

			return TranslationTransform * RotationTransform;
		}

		/// <summary>
		/// Center the given meshes on the pivot determined from the union of their bounding boxes. Returns the bottom center point.
		/// </summary>
		/// <param name="RhinoMeshes"></param>
		/// <returns>The bottom-center point on which the Mesh was centered</returns>
		public static Vector3d CenterMeshesOnPivot(IEnumerable<Mesh> RhinoMeshes)
		{
			BoundingBox MeshesBoundingBox = BoundingBox.Empty;

			foreach (Mesh CurrentMesh in RhinoMeshes)
			{
				const bool bAccurate = true;

				if(MeshesBoundingBox.IsValid)
				{
					MeshesBoundingBox.Union(CurrentMesh.GetBoundingBox(bAccurate));
				}
				else
				{
					MeshesBoundingBox = CurrentMesh.GetBoundingBox(bAccurate);
				}
			}

			Vector3d PivotPoint = new Vector3d(MeshesBoundingBox.Center.X, MeshesBoundingBox.Center.Y, MeshesBoundingBox.Min.Z);
			
			foreach (Mesh CurrentMesh in RhinoMeshes)
			{
				CurrentMesh.Translate(-PivotPoint);
			}

			return PivotPoint;
		}

		public static string EvaluateAttributeUserText(DatasmithActorInfo InNode, string ValueFormula)
		{
			if (!ValueFormula.StartsWith("%<") || !ValueFormula.EndsWith(">%"))
			{
				// Nothing to evaluate, just return the string as-is.
				return ValueFormula;
			}

			RhinoObject NodeObject = InNode.RhinoCommonObject as RhinoObject;
			RhinoObject ParentObject = null;
			DatasmithActorInfo CurrentNode = InNode;
			while (CurrentNode.DefinitionNode != null)
			{
				CurrentNode = CurrentNode.DefinitionNode;
				ParentObject = CurrentNode.RhinoCommonObject as RhinoObject;
			}

			// In case this is an instance of a block sub-object, the ID held in the formula may not have been updated
			// with the definition object ID. We need to replace the ID with the definition object ID, otherwise the formula
			// will not evaluate correctly.
			if (InNode.DefinitionNode != null && !InNode.DefinitionNode.bIsRoot)
			{
				int IdStartIndex = ValueFormula.IndexOf("(\"") + 2;
				int IdEndIndex = ValueFormula.IndexOf("\")");

				if (IdStartIndex >= 0 && IdEndIndex > IdStartIndex)
				{
					ValueFormula = ValueFormula.Remove(IdStartIndex, IdEndIndex - IdStartIndex);
					ValueFormula = ValueFormula.Insert(IdStartIndex, NodeObject.Id.ToString());
				}
			}

			return RhinoApp.ParseTextField(ValueFormula, NodeObject, ParentObject);
		}

		public static string GetRhinoTextureFilePath(Texture RhinoTexture)
		{
			string FileName = string.Empty;
			string FilePath = RhinoTexture.FileReference.FullPath;

			try
			{
				if (!string.IsNullOrWhiteSpace(FilePath))
				{
					FileName = Path.GetFileName(FilePath);
					if (!File.Exists(FilePath))
					{
						FilePath = string.Empty;
					}
				}

				// Rhino's full path did not work, check with Rhino's relative path starting from current path
				string RhinoFilePath = RhinoDoc.ActiveDoc.Path;
				if (!string.IsNullOrWhiteSpace(RhinoFilePath))
				{
					string CurrentPath = Path.GetFullPath(Path.GetDirectoryName(RhinoFilePath));
					if (string.IsNullOrWhiteSpace(FilePath))
					{
						string RelativePath = RhinoTexture.FileReference.RelativePath;
						if (!string.IsNullOrWhiteSpace(RelativePath))
						{
							FilePath = Path.Combine(CurrentPath, RelativePath);
							FilePath = Path.GetFullPath(FilePath);

							if (!File.Exists(FilePath))
							{
								FilePath = string.Empty;
							}
						}
					}

					// Last resort, search for the file
					if (string.IsNullOrWhiteSpace(FilePath) && !string.IsNullOrEmpty(FileName))
					{
						// Search the texture in the CurrentPath and its sub-folders
						string[] FileNames = Directory.GetFiles(CurrentPath, FileName, SearchOption.AllDirectories);
						if (FileNames.Length > 0)
						{
							FilePath = Path.GetFullPath(FileNames[0]);
						}
					}
				}
			}
			catch (Exception)
			{
				// IO operations can raise an exception for reasons outside of our control. Don't crash the export for this.
				FilePath = string.Empty;
			}

			if (string.IsNullOrWhiteSpace(FilePath))
			{
				string WarningMessage = string.Format("Texture ({0}) has an invalid filepath or its file is missing. Filepath: {1}", RhinoTexture.Id, RhinoTexture.FileReference.FullPath);
				DatasmithRhinoPlugin.Instance.LogManager.AddLog(DatasmithRhinoLogType.Warning, WarningMessage);
			}

			return FilePath;
		}

		public static bool GetRhinoTextureNameAndPath(Texture RhinoTexture, out string Name, out string TexturePath)
		{
			TexturePath = GetRhinoTextureFilePath(RhinoTexture);

			if (string.IsNullOrEmpty(TexturePath))
			{
				Name = string.Empty;
				return false;
			}

			Name = System.IO.Path.GetFileNameWithoutExtension(TexturePath);
			if (RhinoTexture.TextureType == TextureType.Bump)
			{
				Name += "_normal";
			}
			else if (RhinoTexture.TextureType == TextureType.Transparency)
			{
				Name += "_alpha";
			}

			return true;
		}
	}
}