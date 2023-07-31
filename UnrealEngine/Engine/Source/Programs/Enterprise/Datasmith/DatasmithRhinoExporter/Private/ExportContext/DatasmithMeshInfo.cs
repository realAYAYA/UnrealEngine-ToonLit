// Copyright Epic Games, Inc. All Rights Reserved.

using Rhino.Geometry;
using System.Collections.Generic;

namespace DatasmithRhino.ExportContext
{
	public class DatasmithMeshInfo : DatasmithInfoBase
	{
		public FDatasmithFacadeMeshElement ExportedMesh { get { return ExportedElement as FDatasmithFacadeMeshElement; } }

		public List<Mesh> RhinoMeshes { get; private set; }
		public Transform OffsetTransform { get; set; }
		public List<int> MaterialIndices { get; set; }
		public List<DatasmithTextureMappingData> TextureMappings { get; set; }

		public DatasmithMeshInfo(IEnumerable<Mesh> InRhinoMeshes, Transform InOffset, List<int> InMaterialIndexes, List<DatasmithTextureMappingData> InTextureMappings, string InName, string InUniqueLabel, string InBaseLabel)
			: base(null, InName, InUniqueLabel, InBaseLabel)
		{
			RhinoMeshes = new List<Mesh>(InRhinoMeshes);
			OffsetTransform = InOffset;
			MaterialIndices = InMaterialIndexes;
			TextureMappings = InTextureMappings;
		}

		public override void ApplyDiffs(DatasmithInfoBase OtherInfo)
		{
			base.ApplyDiffs(OtherInfo);

			if (OtherInfo is DatasmithMeshInfo OtherMeshInfo)
			{
				RhinoMeshes = OtherMeshInfo.RhinoMeshes;
				OffsetTransform = OtherMeshInfo.OffsetTransform;
				MaterialIndices = OtherMeshInfo.MaterialIndices;
				TextureMappings = OtherMeshInfo.TextureMappings;
			}
			else
			{
				System.Diagnostics.Debug.Fail("OtherInfo does not derive from DatasmithMeshInfo");
			}
		}
	}

	public class DatasmithTextureMappingData
	{
		public int ChannelId;
		public Rhino.Render.TextureMapping RhinoTextureMapping;
		public Transform ObjectTransform;

		public DatasmithTextureMappingData(int InChannelId, Rhino.Render.TextureMapping InTextureMapping, Transform InTransform)
		{
			ChannelId = InChannelId;
			RhinoTextureMapping = InTextureMapping;
			ObjectTransform = InTransform;
		}
	}
}