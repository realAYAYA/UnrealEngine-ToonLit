// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using Autodesk.Revit.DB;

namespace DatasmithRevitExporter
{
	public static class FUtils
	{
		private static string GetCategoryName(Element InElement)
		{
			ElementType Type = GetElementType(InElement);
			return Type?.Category?.Name ?? InElement.Category?.Name;
		}

		private static ElementType GetElementType(Element InElement)
		{
			return InElement.Document.GetElement(InElement.GetTypeId()) as ElementType;
		}

		public static void AddActorMetadata(Element InElement, FDatasmithFacadeMetaData ActorMetadata, FSettings InSettings)
		{
			// Add the Revit element category name metadata to the Datasmith actor.
			string CategoryName = GetCategoryName(InElement);
			if (!string.IsNullOrEmpty(CategoryName))
			{
				ActorMetadata.AddPropertyString("Element*Category", CategoryName);
			}

			// Add the Revit element family name metadata to the Datasmith actor.
			ElementType ElemType = GetElementType(InElement);
			string FamilyName = ElemType?.FamilyName;
			if (!string.IsNullOrEmpty(FamilyName))
			{
				ActorMetadata.AddPropertyString("Element*Family", FamilyName);
			}

			// Add the Revit element type name metadata to the Datasmith actor.
			string TypeName = ElemType?.Name;
			if (!string.IsNullOrEmpty(TypeName))
			{
				ActorMetadata.AddPropertyString("Element*Type", TypeName);
			}

			// Add Revit element metadata to the Datasmith actor.
			AddActorMetadata(InElement, "Element*", ActorMetadata, InSettings);

			if (ElemType != null)
			{
				// Add Revit element type metadata to the Datasmith actor.
				AddActorMetadata(ElemType, "Type*", ActorMetadata, InSettings);
			}
		}

		public static void AddActorMetadata(
			Element InSourceElement,
			string InMetadataPrefix,
			FDatasmithFacadeMetaData ElementMetaData,
			FSettings InSettings
		)
		{
			IList<Parameter> Parameters = InSourceElement.GetOrderedParameters();

			if (Parameters != null)
			{
				foreach (Parameter Parameter in Parameters)
				{
					try
					{
						if (Parameter.HasValue)
						{
							if (InSettings != null && !InSettings.MatchParameterByMetadata(Parameter))
							{
								continue; // Skip export of this param
							}

							string ParameterValue = Parameter.AsValueString();

							if (string.IsNullOrEmpty(ParameterValue))
							{
								switch (Parameter.StorageType)
								{
									case StorageType.Integer:
									ParameterValue = Parameter.AsInteger().ToString();
									break;
									case StorageType.Double:
									ParameterValue = Parameter.AsDouble().ToString();
									break;
									case StorageType.String:
									ParameterValue = Parameter.AsString();
									break;
									case StorageType.ElementId:
									ParameterValue = Parameter.AsElementId().ToString();
									break;
								}
							}

							if (!string.IsNullOrEmpty(ParameterValue))
							{
								string MetadataKey = InMetadataPrefix + Parameter.Definition.Name;
								ElementMetaData.AddPropertyString(MetadataKey, ParameterValue);
							}
						}
					}
					catch { }
				}
			}
		}

		public static void GetDecalSpatialParams(Element InDecalElement, ref Transform OutDecalTransform, ref XYZ OutDecalDimensions)
		{
			List<Line> DecalQuad = new List<Line>();

			GeometryElement GeomElement = InDecalElement.get_Geometry(new Options());

			List<XYZ> MidPointsToTest = new List<XYZ>();

			foreach (GeometryObject GeomObj in GeomElement)
			{
				if (GeomObj is Line QuadLine)
				{
					DecalQuad.Add(QuadLine);
				}
				else if (GeomObj is Curve QuadCurve)
				{
					try
					{
						XYZ StartPoint = QuadCurve.GetEndPoint(0);
						XYZ EndPoint = QuadCurve.GetEndPoint(1);
						Line BoundLine = Line.CreateBound(StartPoint, EndPoint);
						DecalQuad.Add(BoundLine);

						MidPointsToTest.AddRange(QuadCurve.Tessellate());
					}
					catch {}
				}
			}

			if (DecalQuad.Count != 4)
			{
				return;
			}

			XYZ TopLeft = DecalQuad[0].Origin;
			XYZ TopRight = DecalQuad[1].Origin;
			XYZ BottomRight = DecalQuad[2].Origin;
			XYZ BottomLeft = DecalQuad[3].Origin;

			XYZ BasisY = (TopRight - TopLeft).Normalize();
			XYZ BasisZ = (TopLeft - BottomLeft).Normalize();
			XYZ BasisX = BasisZ.CrossProduct(BasisY).Normalize();

			XYZ Origin = (TopLeft + BottomRight) * 0.5f;

			OutDecalTransform = Transform.Identity;
			OutDecalTransform.BasisX = BasisX;
			OutDecalTransform.BasisY = BasisZ;
			OutDecalTransform.BasisZ = BasisY;
			OutDecalTransform.Origin = Origin;

			const float CENTIMETERS_PER_FOOT = 30.48F;

			Plane TestPlane = Plane.CreateByThreePoints(TopLeft, TopRight, BottomRight);
			UV TestUv;

			double FinalDistance = 0;
			TestPlane.Project(BottomLeft, out TestUv, out FinalDistance);

			foreach (XYZ MidPoint in MidPointsToTest)
			{
				double Distance = 0;
				TestPlane.Project(MidPoint, out TestUv, out Distance);

				if (FinalDistance < Distance)
				{
					FinalDistance = Distance;
				}
			}

			FinalDistance *= CENTIMETERS_PER_FOOT * 1.3;	// leniency multiplier of 15% in each direction
															// seems to be needed in order to circumvent UE's decal projection imprecisions
															// (issue would be visible via blending on big curvatured surfaces without the leniency multiplier)

			double DimensionZ = 2.0;
			if (DimensionZ < FinalDistance)
			{
				DimensionZ = FinalDistance;
			}

			OutDecalDimensions = new XYZ(DecalQuad[0].Length * CENTIMETERS_PER_FOOT * 0.5, DecalQuad[1].Length * CENTIMETERS_PER_FOOT * 0.5, DimensionZ);
		}

		public static bool IsElementIdDecal(Document InDocument, ElementId InElementId)
		{
			Element Element = InDocument.GetElement(InElementId);
			return IsElementDecal(Element);
		}

		public static bool IsElementDecal(Element InElement)
		{
			if (InElement == null)
			{
				return false;
			}

			ElementType ElementType = InElement.Document.GetElement(InElement.GetTypeId()) as ElementType;

			if (ElementType != null && (ElementType.FamilyName == "Decal"))
			{
				return true;
			}

			return false;
		}

		public static IList<ElementId> GetAllDependentElements(Element ElementInView)
		{
			// Revit 2018 doesn't support passing 'null' to GetDependentElements so just make a simple filter
			return ElementInView.GetDependentElements(new ExclusionFilter(new List<ElementId>(){ElementInView.Id}));
		}
	}
}
