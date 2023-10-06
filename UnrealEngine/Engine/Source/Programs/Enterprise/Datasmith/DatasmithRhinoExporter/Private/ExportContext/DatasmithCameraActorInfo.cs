// Copyright Epic Games, Inc. All Rights Reserved.

using Rhino.DocObjects;
using System;

namespace DatasmithRhino.ExportContext
{
	public class DatasmithCameraActorInfo : DatasmithActorInfo
	{
		public override Guid RhinoObjectId
		{
			get
			{
				if (RhinoCommonObject is ViewportInfo RhinoViewportInfo)
				{
					return RhinoViewportInfo.Id;
				}

				return Guid.Empty;
			}
		}

		public FDatasmithFacadeActorCamera ExportedCameraActor { get { return ExportedElement as FDatasmithFacadeActorCamera; } }

		/// <summary>
		/// Returns a hash of the properties of the ViewportInfo relevant for Datasmith export.
		/// We use this because Rhino does not provide an event for when the named view table is modified.
		/// That means every time the document is modified we must check for named view changes, the hash allows us to avoid false-positives modifications.
		/// </summary>
		public String NamedViewHash { get; private set; }

		public DatasmithCameraActorInfo(ViewportInfo InViewportInfo, string InName, string InUniqueLabel, string InBaseLabel, string InHash)
			: base(InViewportInfo, InName, InUniqueLabel, InBaseLabel)
		{
			NamedViewHash = InHash;
		}

		public override void ApplyDiffs(DatasmithInfoBase OtherInfo)
		{
			base.ApplyDiffs(OtherInfo);

			if (OtherInfo is DatasmithCameraActorInfo OtherActorCameraInfo)
			{
				NamedViewHash = OtherActorCameraInfo.NamedViewHash;
			}
			else
			{
				System.Diagnostics.Debug.Fail("OtherInfo does not derive from DatasmithActorCameraInfo");
			}
		}
	}
}