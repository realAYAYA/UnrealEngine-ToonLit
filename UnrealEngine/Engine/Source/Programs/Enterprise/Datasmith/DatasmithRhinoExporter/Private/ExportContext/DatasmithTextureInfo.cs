// Copyright Epic Games, Inc. All Rights Reserved.

using DatasmithRhino.Utils;
using Rhino.DocObjects;
using Rhino.Geometry;
using System;
using System.Collections.Generic;

namespace DatasmithRhino.ExportContext
{
	public class DatasmithTextureInfo : DatasmithInfoBase
	{
		public Texture RhinoTexture { get { return RhinoCommonObject as Texture; } }
		public FDatasmithFacadeTexture ExportedTexture { get { return ExportedElement as FDatasmithFacadeTexture; } }
		public string FilePath { get; private set; }

		/// <summary>
		/// Holds the Ids of all the Rhino `Texture` objects using that DatasmithTexture.
		/// </summary>
		public HashSet<Guid> TextureIds { get; } = new HashSet<Guid>();

		public DatasmithTextureInfo(Texture InRhinoTexture, string InName, string InFilePath)
			: base(InRhinoTexture, FDatasmithFacadeElement.GetStringHash(InName), InName, InName)
		{
			FilePath = InFilePath;
		}

		public bool IsSupported()
		{
			return DatasmithRhinoUtilities.IsTextureSupported(RhinoTexture);
		}
	}

}