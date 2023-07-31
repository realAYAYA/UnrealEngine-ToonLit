// Copyright Epic Games, Inc. All Rights Reserved.

using Rhino.DocObjects;
using System.Collections.Generic;

namespace DatasmithRhino.ExportContext
{
	public class DatasmithMaterialInfo : DatasmithInfoBase
	{
		public Material RhinoMaterial { get { return RhinoCommonObject as Material; } }
		public FDatasmithFacadeUEPbrMaterial ExportedMaterial { get { return ExportedElement as FDatasmithFacadeUEPbrMaterial; } }

		/// <summary>
		/// Set of rhino material indexes that are represented by this DatasmithMaterialInfo
		/// </summary>
		public HashSet<int> MaterialIndexes { get; } = new HashSet<int>();

		private List<string> InternalTextureHashes;
		public IReadOnlyList<string> TextureHashes { get => InternalTextureHashes; }

		public DatasmithMaterialInfo(Material InRhinoMaterial, string InName, string InUniqueLabel, string InBaseLabel, List<string> InTextureHashes)
			: base(InRhinoMaterial, InName, InUniqueLabel, InBaseLabel)
		{
			InternalTextureHashes = new List<string>(InTextureHashes);
		}

		public override void ApplyDiffs(DatasmithInfoBase OtherInfo)
		{
			base.ApplyDiffs(OtherInfo);

			if (OtherInfo is DatasmithMaterialInfo OtherMaterialInfo)
			{
				//Rhino "replaces" object instead of modifying them, we must update the our reference to the object.
				RhinoCommonObject = OtherMaterialInfo.RhinoCommonObject;

				MaterialIndexes.Clear();
				MaterialIndexes.UnionWith(OtherMaterialInfo.MaterialIndexes);

				InternalTextureHashes.Clear();
				InternalTextureHashes.AddRange(OtherMaterialInfo.TextureHashes);
			}
			else
			{
				System.Diagnostics.Debug.Fail("OtherInfo does not derive from DatasmithMaterialInfo");
			}
		}
	}
}