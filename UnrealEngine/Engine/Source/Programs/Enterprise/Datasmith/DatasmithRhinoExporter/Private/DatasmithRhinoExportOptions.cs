// Copyright Epic Games, Inc. All Rights Reserved.
using Rhino;
using Rhino.FileIO;
using Rhino.Geometry;

namespace DatasmithRhino
{
	public class DatasmithRhinoExportOptions
	{
		public const string UntitledSceneName = "";

		public bool bWriteSelectedObjectsOnly { get; private set; } = false;
		public Transform Xform { get; private set; } = Transform.Identity;
		public RhinoDoc RhinoDocument { get; private set; }
		public FDatasmithFacadeScene DatasmithScene { get; private set; }
		public bool bSkipHidden { get; private set; }
		public UnitSystem ModelUnitSystem { get; set; }

		public DatasmithRhinoExportOptions(RhinoDoc InRhinoDocument, FDatasmithFacadeScene InDatasmithScene, bool bInSkipHidden)
		{
			RhinoDocument = InRhinoDocument;
			ModelUnitSystem = RhinoDocument.ModelUnitSystem;
			DatasmithScene = InDatasmithScene;
			bSkipHidden = bInSkipHidden;
		}

		public DatasmithRhinoExportOptions(FileWriteOptions RhinoFileWriteOptions, RhinoDoc InRhinoDocument, FDatasmithFacadeScene InDatasmithScene, bool bInSkipHidden)
			: this(InRhinoDocument, InDatasmithScene, bInSkipHidden)
		{
			bWriteSelectedObjectsOnly = RhinoFileWriteOptions.WriteSelectedObjectsOnly;
			Xform = RhinoFileWriteOptions.Xform;
		}
	}
}
