// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Autodesk.Revit.DB;
using Autodesk.Revit.DB.Architecture;
using Autodesk.Revit.DB.Mechanical;
using Autodesk.Revit.DB.Plumbing;
using Autodesk.Revit.DB.Structure;
using Autodesk.Revit.DB.Visual;

namespace DatasmithRevitExporter
{
	public class OrientatedBoundingBox
	{
		int tlf = 0;
		int trf = 1;
		int blf = 2;
		int brf = 3;
		int tlb = 4;
		int trb = 5;
		int blb = 6;
		int brb = 7;
		
		double Epsilon = 0.000001;

		//  Z  
		//  | Y
		//  |/
		//  o---X
		//
		//     tlb-------MAX
		//     /|        /|
		//    / |       / |
		//  tlf-------trf |
		//   |  |      |  |     
		//   | blb-----|-brb
		//   | /       | /
		//   |/        |/
		//  MIN-------brf
		//
		// Vertices[0] := tlf: Top Left Front
		// Vertices[1] := trf: Top Right Front
		// Vertices[2] := blf: Bottom Left Front	=> min
		// Vertices[3] := brf: Bottom Right Front
		// Vertices[4] := tlb: Top Left Back
		// Vertices[5] := trb: Top Right Back		=> max
		// Vertices[6] := blb: Bottom Left Back
		// Vertices[7] := brb: Bottom Right Back

		public XYZ[] Vertices = new XYZ[8]; //8 vertices // corners
		
		double SideXDistance;
		double SideYDistance;
		double SideZDistance;

		public XYZ AxisAllignedMin;
		public XYZ AxisAllignedMax;

		Plane SideX0, SideX1;
		Plane SideY0, SideY1;
		Plane SideZ0, SideZ1;

		public bool bIsValidData;

		public OrientatedBoundingBox(Transform InTransform, XYZ MIN, XYZ MAX, bool InCalculatePlanes = false)
		{
			SideXDistance = (MAX.X - MIN.X) + Epsilon;
			SideYDistance = (MAX.Y - MIN.Y) + Epsilon;
			SideZDistance = (MAX.Z - MIN.Z) + Epsilon;

			bIsValidData = SideXDistance > Epsilon && SideYDistance > Epsilon && SideZDistance > Epsilon;

			if (!bIsValidData)
			{
				return;
			}

			Vertices[tlf] = new XYZ(MIN.X, MIN.Y, MAX.Z);
			Vertices[trf] = new XYZ(MAX.X, MIN.Y, MAX.Z);
			Vertices[brf] = new XYZ(MAX.X, MIN.Y, MIN.Z);
			Vertices[blf] = MIN;
			Vertices[tlb] = new XYZ(MIN.X, MAX.Y, MAX.Z);
			Vertices[trb] = MAX;
			Vertices[brb] = new XYZ(MAX.X, MAX.Y, MIN.Z);
			Vertices[blb] = new XYZ(MIN.X, MAX.Y, MIN.Z);

			double MinX = double.MaxValue;
			double MinY = double.MaxValue;
			double MinZ = double.MaxValue;
			double MaxX = -double.MaxValue;
			double MaxY = -double.MaxValue;
			double MaxZ = -double.MaxValue;

			for (int Index = 0; Index <= brb; Index++)
			{
				Vertices[Index] = InTransform.OfPoint(Vertices[Index]);

				if (Vertices[Index].X < MinX) MinX = Vertices[Index].X;
				if (Vertices[Index].Y < MinY) MinY = Vertices[Index].Y;
				if (Vertices[Index].Z < MinZ) MinZ = Vertices[Index].Z;
				if (Vertices[Index].X > MaxX) MaxX = Vertices[Index].X;
				if (Vertices[Index].Y > MaxY) MaxY = Vertices[Index].Y;
				if (Vertices[Index].Z > MaxZ) MaxZ = Vertices[Index].Z;
			}

			AxisAllignedMax = new XYZ(MaxX, MaxY, MaxZ);
			AxisAllignedMin = new XYZ(MinX, MinY, MinZ);

			if (InCalculatePlanes)
			{
				//We only need the planes for the SectionBox:
				CalculatePlanes();
			}
		}

		private void CalculatePlanes()
		{
			SideX0 = Plane.CreateByThreePoints(Vertices[tlf], Vertices[blb], Vertices[tlb]);
			SideX1 = Plane.CreateByThreePoints(Vertices[trf], Vertices[trb], Vertices[brb]);

			SideY0 = Plane.CreateByThreePoints(Vertices[tlf], Vertices[trf], Vertices[brf]);
			SideY1 = Plane.CreateByThreePoints(Vertices[tlb], Vertices[brb], Vertices[trb]);

			SideZ0 = Plane.CreateByThreePoints(Vertices[blf], Vertices[brb], Vertices[blb]);
			SideZ1 = Plane.CreateByThreePoints(Vertices[tlf], Vertices[tlb], Vertices[trb]);
		}

		private bool IsPointWithin(Plane Plane1, Plane Plane2, double Distance, XYZ Point)
		{
			UV Uv;
			double Distance1;
			double Distance2;

			Plane1.Project(Point, out Uv, out Distance1);

			if (Distance1 > Distance)
			{
				return false;
			}

			Plane2.Project(Point, out Uv, out Distance2);

			if (Distance2 > Distance)
			{
				return false;
			}

			return true;
		}

		private bool IsPointInside(XYZ Point)
		{
			return IsPointWithin(SideX0, SideX1, SideXDistance, Point)
				&& IsPointWithin(SideY0, SideY1, SideYDistance, Point)
				&& IsPointWithin(SideZ0, SideZ1, SideZDistance, Point);
		}

		// If self contains Candidate it returns false:
		public bool DoesIntersect(OrientatedBoundingBox Candidate)
		{
			int VertexWithinCounter = 0;
			int VertexOutsideCounter = 0;
			for (int Index = 0; Index < Candidate.Vertices.Length; Index++)
			{
				if (IsPointInside(Candidate.Vertices[Index]))
				{
					VertexWithinCounter++;
				}
				else
				{
					VertexOutsideCounter++;
				}

				if (VertexOutsideCounter > 0 && VertexWithinCounter > 0)
				{
					return true;
				}
			}

			//It returns true only if we have at least 1 vertex within and at least 1 vertex outside
			//every other scenario should return false
			//as in:
			//	- all vertex outside := false
			//	- all vertex inside := false => geometries with completely contained bounding boxes won't get clipped.
			return false;
		}
	}
	public class FDocumentData
	{
		// This class reflects the child -> super component relationship in Revit into the exported hierarchy (children under super components actors).
		class FSuperComponentOptimizer
		{
			private Dictionary<FBaseElementData, Element> ElementDataToElementMap = new Dictionary<FBaseElementData, Element>();
			private Dictionary<ElementId, FBaseElementData> ElementIdToElementDataMap = new Dictionary<ElementId, FBaseElementData>();

			public void UpdateCache(FBaseElementData ParentElement, FBaseElementData ChildElement)
			{
				if (!ElementDataToElementMap.ContainsKey(ParentElement))
				{
					Element Elem = null;

					if (ParentElement.GetType() == typeof(FElementData))
					{
						Elem = ((FElementData)ParentElement).CurrentElement;
					}
					else if (ChildElement.GetType() == typeof(FElementData))
					{
						Elem = ((FElementData)ChildElement).CurrentElement;
					}

					if (Elem != null)
					{
						ElementDataToElementMap[ParentElement] = Elem;
						ElementIdToElementDataMap[Elem.Id] = ParentElement;
					}
				}
			}

			public void Optimize()
			{
				foreach (var KV in ElementDataToElementMap)
				{
					FBaseElementData ElemData = KV.Key;
					Element Elem = KV.Value;

					if ((Elem as FamilyInstance) != null)
					{
						Element Parent = (Elem as FamilyInstance).SuperComponent;

						if (Parent != null)
						{
							FBaseElementData SuperParent = null;
							bool bGot = ElementIdToElementDataMap.TryGetValue(Parent.Id, out SuperParent);

							if (bGot && SuperParent != ElemData.Parent)
							{
								if (ElemData.Parent != null)
								{
									ElemData.Parent.ElementActor.RemoveChild(ElemData.ElementActor);
									ElemData.Parent.ChildElements.Remove(ElemData);
								}

								SuperParent.ChildElements.Add(ElemData);
								SuperParent.ElementActor.AddChild(ElemData.ElementActor);
							}
						}
					}
				}
			}
		};

		public struct FPolymeshFace
		{
			public int V1;
			public int V2;
			public int V3;
			public int MaterialIndex;

			public FPolymeshFace(int InVertex1, int InVertex2, int InVertex3, int InMaterialIndex = 0)
			{
				V1 = InVertex1;
				V2 = InVertex2;
				V3 = InVertex3;
				MaterialIndex = InMaterialIndex;
			}
		}

		public class FDatasmithPolymesh
		{
			public List<XYZ> Vertices = new List<XYZ>();
			public List<XYZ> Normals = new List<XYZ>();
			public List<FPolymeshFace> Faces = new List<FPolymeshFace>();
			public List<UV> UVs = new List<UV>();
		}

		public class FBaseElementData
		{
			public ElementType					BaseElementType;
			public FDatasmithPolymesh			DatasmithPolymesh = null;
			public FDatasmithFacadeMeshElement	DatasmithMeshElement = null;
			public FDatasmithFacadeActor		ElementActor = null;
			public FDatasmithFacadeMetaData		ElementMetaData = null;
			public FDocumentData				DocumentData = null;
			public bool							bOptimizeHierarchy = true;
			public bool							bIsModified = true;
			public bool							bAllowMeshInstancing = true;
			public bool							bIsDecalElement = false;

			public Dictionary<string, int>		MeshMaterialsMap = new Dictionary<string, int>();

			public Transform WorldTransform;

			public List<FBaseElementData>	ChildElements = new List<FBaseElementData>();

			public FBaseElementData			Parent = null;

			public bool bOwnedByParent = false; // Lifetime is controlled by parent Element

			public FBaseElementData(
				ElementType InElementType, FDocumentData InDocumentData
			)
			{
				BaseElementType = InElementType;
				DocumentData = InDocumentData;
			}

			public FBaseElementData(FDatasmithFacadeActor InElementActor, FDatasmithFacadeMetaData InElementMetaData, FDocumentData InDocumentData)
			{
				ElementActor = InElementActor;
				ElementMetaData = InElementMetaData;
				DocumentData = InDocumentData;
			}

			void CopyActorData(FDatasmithFacadeActor InFromActor, FDatasmithFacadeActor InToActor)
			{
				InToActor.SetLabel(InFromActor.GetLabel());

				double X, Y, Z, W;
				InFromActor.GetTranslation(out X, out Y, out Z);
				InToActor.SetTranslation(X, Y, Z);
				InFromActor.GetScale(out X, out Y, out Z);
				InToActor.SetScale(X, Y, Z);
				InFromActor.GetRotation(out X, out Y, out Z, out W);
				InToActor.SetRotation(X, Y, Z, W);

				InToActor.SetLayer(InFromActor.GetLayer());

				for (int TagIndex = 0; TagIndex < InFromActor.GetTagsCount(); ++TagIndex)
				{
					InToActor.AddTag(InFromActor.GetTag(TagIndex));
				}

				InToActor.SetIsComponent(InFromActor.IsComponent());
				InToActor.SetVisibility(InFromActor.GetVisibility());

				for (int ChildIndex = 0; ChildIndex < InFromActor.GetChildrenCount(); ++ChildIndex)
				{
					InToActor.AddChild(InFromActor.GetChild(ChildIndex));
				}

				ElementMetaData?.SetAssociatedElement(InToActor);
			}

			// Return element when this 'ElementData' is associated with an Element
			// todo: Probably worth separating FBaseElementData into different entity, calling it now "Element" data is confusing as it's also used to hold for non-Element datasmith actors
			protected virtual Element GetElement()
			{
				return null;
			}

			public void AddToScene(FDatasmithFacadeScene InScene, FBaseElementData InParent, bool bInSkipChildren, bool bInForceAdd = false)
			{
				Element ThisElement = GetElement();

				if (!bInSkipChildren)
				{
					foreach (FBaseElementData CurrentChild in ChildElements)
					{
						// Stairs get special treatment: elements of stairs (strings, landings etc.) can be duplicated,
						// meaning that the same element id can exist multiple times under the same parent.
						bool bIsStairsElement = (ThisElement != null) && (ThisElement.GetType() == typeof(Stairs));

						bool bIsInstance = (ThisElement == null);
						bool bForceAdd = (bIsInstance && bIsModified) || (bIsStairsElement && bIsModified);

						CurrentChild.AddToScene(InScene, this, false, bForceAdd);
					}
				}

				bool bIsCached =
					ThisElement != null &&
					ThisElement.IsValidObject &&
					(DocumentData.DirectLink?.IsElementCached(ThisElement) ?? false);

				// Check if actor type has changed for this element (f.e. static mesh actor -> regular actor),
				// and re-created if needed.
				if (bIsCached && bIsModified && ElementActor != null)
				{
					FDatasmithFacadeActor CachedActor = DocumentData.DirectLink.GetCachedActor(ElementActor.GetName());

					if (CachedActor != null && CachedActor.GetType() != ElementActor.GetType())
					{
						InScene.RemoveActor(CachedActor);
						DocumentData.DirectLink.CacheActorType(ElementActor);
						bIsCached = false;
					}
				}

				if ((!bIsCached && bIsModified) || bInForceAdd)
				{
					if (InParent == null)
					{
						InScene.AddActor(ElementActor);
					}
					else
					{
						InParent.ElementActor.AddChild(ElementActor);
					}

					if (!DocumentData.bSkipMetadataExport && ElementMetaData != null)
					{
						InScene.AddMetaData(ElementMetaData);
					}

					if (ThisElement != null)
					{
						DocumentData.DirectLink?.CacheElement(DocumentData.CurrentDocument, ThisElement, this);
					}
				}

				bIsModified = false;
			}

			// Returns true if element's actor is simple(plain actor without descendants)
			public bool Optimize()
			{
				// Replace MeshActor without geometry by a dummy Actor
				if (ElementActor is FDatasmithFacadeActorMesh MeshActor && MeshActor.GetMeshName().Length == 0)
				{
					ElementActor = new FDatasmithFacadeActor(MeshActor.GetName());
					CopyActorData(MeshActor, ElementActor);
				}

				// Optimize and remove children whose actors are simple
				List<FBaseElementData> SimpleChildren = ChildElements.Where(Child => Child.Optimize()).ToList(); // Build a list of elements to remove after enumeration
				foreach (FBaseElementData Child in SimpleChildren)
				{
					ChildElements.Remove(Child);
				}

				bool bIsSimpleActor = !(ElementActor is FDatasmithFacadeActorMesh || ElementActor is FDatasmithFacadeActorLight || ElementActor is FDatasmithFacadeActorCamera);
				return bIsSimpleActor && (ChildElements.Count == 0) && bOptimizeHierarchy;
			}

			public void UpdateMeshName()
			{
				FDatasmithFacadeActorMesh MeshActor = ElementActor as FDatasmithFacadeActorMesh;
				if (MeshActor == null && DocumentData.DirectLink != null)
				{
					// We have valid mesh but the actor is not a mesh actor -- the type of element has changed (DirectLink).
					MeshActor = new FDatasmithFacadeActorMesh(ElementActor.GetName());
					CopyActorData(ElementActor, MeshActor);
					ElementActor = MeshActor;
				}
				MeshActor?.SetMesh(DatasmithMeshElement.GetName());
				bOptimizeHierarchy = false;
			}
		}

		public class FElementData : FBaseElementData
		{
			public Element		CurrentElement = null;
			public Transform	MeshPointsTransform = null;

			public Stack<FBaseElementData> InstanceDataStack = new Stack<FBaseElementData>();

			public FElementData(
				Element InElement,
				Transform InWorldTransform,
				FDocumentData InDocumentData
			)
				: base(InElement.Document.GetElement(InElement.GetTypeId()) as ElementType, InDocumentData)
			{
				CurrentElement = InElement;
			}

			protected override Element GetElement() 
			{
				return CurrentElement;
			}

			public void InitializePivotPlacement(ref Transform InOutWorldTransform)
			{
				// If element has location, use it as a transform in order to have better pivot placement.
				Transform PivotTransform = GetPivotTransform(CurrentElement);
				if (PivotTransform != null)
				{
					if (!InOutWorldTransform.IsIdentity)
					{
						InOutWorldTransform = InOutWorldTransform * PivotTransform;
					}
					else
					{
						InOutWorldTransform = PivotTransform;
					}

					if (CurrentElement.GetType() == typeof(Wall)
						|| CurrentElement.GetType() == typeof(ModelText)
						|| CurrentElement.GetType().IsSubclassOf(typeof(MEPCurve))
						|| CurrentElement.GetType() == typeof(StructuralConnectionHandler)
						|| CurrentElement.GetType() == typeof(Floor)
						|| CurrentElement.GetType() == typeof(Ceiling)
						|| CurrentElement.GetType() == typeof(RoofBase)
						|| CurrentElement.GetType().IsSubclassOf(typeof(RoofBase)))
					{
						MeshPointsTransform = PivotTransform.Inverse;
					}
				}
			}

			// Compute orthonormal basis, given the X vector.
			static private void ComputeBasis(XYZ BasisX, ref XYZ BasisY, ref XYZ BasisZ)
			{
				BasisY = XYZ.BasisZ.CrossProduct(BasisX);
				if (BasisY.GetLength() < 0.0001)
				{
					// BasisX is aligned with Z, use dot product to take direction in account
					BasisY = BasisX.CrossProduct(BasisX.DotProduct(XYZ.BasisZ) * XYZ.BasisX).Normalize();
					BasisZ = BasisX.CrossProduct(BasisY).Normalize();
				}
				else
				{
					BasisY = BasisY.Normalize();
					BasisZ = BasisX.CrossProduct(BasisY).Normalize();
				}
			}

			private Transform GetPivotTransform(Element InElement)
			{
				if ((InElement as FamilyInstance) != null)
				{
					return null;
				}

				XYZ Translation = null;
				XYZ BasisX = new XYZ();
				XYZ BasisY = new XYZ();
				XYZ BasisZ = new XYZ();

				// Get pivot translation

				if (InElement.GetType() == typeof(Railing))
				{
					// Railings don't have valid location, so instead we need to get location from its path.
					IList<Curve> Paths = (InElement as Railing).GetPath();
					if (Paths.Count > 0 && Paths[0].IsBound)
					{
						Translation = Paths[0].GetEndPoint(0);
					}
				}
				else if (InElement.GetType() == typeof(StructuralConnectionHandler))
				{
					Translation = (InElement as StructuralConnectionHandler).GetOrigin();
				}
				else if (InElement.GetType() == typeof(Floor)
					|| InElement.GetType() == typeof(Ceiling)
					|| InElement.GetType() == typeof(RoofBase)
					|| InElement.GetType().IsSubclassOf(typeof(RoofBase)))
				{
					BoundingBoxXYZ BoundingBox = InElement.get_BoundingBox(InElement.Document.ActiveView);
					if (BoundingBox != null)
					{
						Translation = BoundingBox.Min;
					}
				}
				else if (InElement.Location != null)
				{
					if (InElement.Location.GetType() == typeof(LocationCurve))
					{
						LocationCurve CurveLocation = InElement.Location as LocationCurve;
						if (CurveLocation.Curve != null && CurveLocation.Curve.IsBound)
						{
							Translation = CurveLocation.Curve.GetEndPoint(0);
						}
					}
					else if (InElement.Location.GetType() == typeof(LocationPoint))
					{
						Translation = (InElement.Location as LocationPoint).Point;
					}
				}

				if (Translation == null)
				{
					return null; // Cannot get valid translation
				}

				// Get pivot basis

				if (InElement.GetType() == typeof(Wall))
				{
					// In rare cases, wall may not support orientation.
					// If this happens, we need to use the direction of its Curve property and
					// derive orientation from there.
					try
					{
						BasisY = (InElement as Wall).Orientation.Normalize();
						BasisX = BasisY.CrossProduct(XYZ.BasisZ).Normalize();
						BasisZ = BasisX.CrossProduct(BasisY).Normalize();
					}
					catch
					{
						if (InElement.Location.GetType() == typeof(LocationCurve))
						{
							LocationCurve CurveLocation = InElement.Location as LocationCurve;

							if (CurveLocation.Curve.GetType() == typeof(Line))
							{
								BasisX = (CurveLocation.Curve as Line).Direction;
								ComputeBasis(BasisX, ref BasisY, ref BasisZ);
							}
							else if (CurveLocation.Curve.IsBound)
							{
								Transform Derivatives = CurveLocation.Curve.ComputeDerivatives(0f, true);
								BasisX = Derivatives.BasisX.Normalize();
								BasisY = Derivatives.BasisY.Normalize();
								BasisZ = Derivatives.BasisZ.Normalize();
							}
							else
							{
								BasisX = XYZ.BasisX;
								BasisY = XYZ.BasisY;
								BasisZ = XYZ.BasisZ;
							}
						}
					}
				}
				else if (InElement.GetType() == typeof(Railing))
				{
					IList<Curve> Paths = (InElement as Railing).GetPath();
					if (Paths.Count > 0)
					{
						Curve FirstPath = Paths[0];
						if (FirstPath.GetType() == typeof(Line))
						{
							BasisX = (FirstPath as Line).Direction.Normalize();
							ComputeBasis(BasisX, ref BasisY, ref BasisZ);
						}
						else if (FirstPath.GetType() == typeof(Arc) && FirstPath.IsBound)
						{
							Transform Derivatives = (FirstPath as Arc).ComputeDerivatives(0f, true);
							BasisX = Derivatives.BasisX.Normalize();
							BasisY = Derivatives.BasisY.Normalize();
							BasisZ = Derivatives.BasisZ.Normalize();
						}
					}
				}
				else if (InElement.GetType() == typeof(StructuralConnectionHandler))
				{
					BasisX = XYZ.BasisX;
					BasisY = XYZ.BasisY;
					BasisZ = XYZ.BasisZ;
				}
				else if (InElement.GetType() == typeof(ModelText))
				{
					// Model text has no direction information!
					BasisX = XYZ.BasisX;
					BasisY = XYZ.BasisY;
					BasisZ = XYZ.BasisZ;
				}
				else if (InElement.GetType() == typeof(FlexDuct))
				{
					BasisX = (InElement as FlexDuct).StartTangent;
					ComputeBasis(BasisX, ref BasisY, ref BasisZ);
				}
				else if (InElement.GetType() == typeof(FlexPipe))
				{
					BasisX = (InElement as FlexPipe).StartTangent;
					ComputeBasis(BasisX, ref BasisY, ref BasisZ);
				}
				else if (InElement.GetType() == typeof(Floor)
					|| InElement.GetType() == typeof(Ceiling)
					|| InElement.GetType() == typeof(RoofBase)
					|| InElement.GetType().IsSubclassOf(typeof(RoofBase)))
				{
					BasisX = XYZ.BasisX;
					BasisY = XYZ.BasisY;
					BasisZ = XYZ.BasisZ;
				}
				else if (InElement.Location.GetType() == typeof(LocationCurve))
				{
					LocationCurve CurveLocation = InElement.Location as LocationCurve;

					if (CurveLocation.Curve.GetType() == typeof(Line))
					{
						BasisX = (CurveLocation.Curve as Line).Direction;
						ComputeBasis(BasisX, ref BasisY, ref BasisZ);
					}
					else if (CurveLocation.Curve.IsBound)
					{
						Transform Derivatives = CurveLocation.Curve.ComputeDerivatives(0f, true);
						BasisX = Derivatives.BasisX.Normalize();
						BasisY = Derivatives.BasisY.Normalize();
						BasisZ = Derivatives.BasisZ.Normalize();
					}
					else
					{
						return null;
					}
				}
				else
				{
					return null; // Cannot get valid basis
				}

				Transform PivotTransform = Transform.CreateTranslation(Translation);
				PivotTransform.BasisX = BasisX;
				PivotTransform.BasisY = BasisY;
				PivotTransform.BasisZ = BasisZ;

				return PivotTransform;
			}

			public FBaseElementData PushInstance(
				ElementType InInstanceType,
				Transform InWorldTransform,
				bool bInAllowMeshInstancing
			)
			{
				FBaseElementData InstanceData = new FBaseElementData(InInstanceType, DocumentData);
				InstanceData.bOwnedByParent = true;

				FamilyInstance CurrentFamilyInstance = CurrentElement as FamilyInstance;
				if (CurrentFamilyInstance != null && CurrentFamilyInstance.HasModifiedGeometry())
				{
					//In case the FamilyInstance has a modified geometry, then don't instantiate the original mesh,
					//the onPolymesh will provide the customized geometry instead.
					bInAllowMeshInstancing = false;
				}

				InstanceData.bAllowMeshInstancing = bInAllowMeshInstancing;
				InstanceDataStack.Push(InstanceData);

				InitializeElement(InWorldTransform, InstanceData);

				// The Datasmith instance actor is a component in the hierarchy.
				InstanceData.ElementActor.SetIsComponent(true);

				return InstanceData;
			}

			public FBaseElementData PopInstance()
			{
				FBaseElementData Instance = InstanceDataStack.Pop();
				Instance.bIsModified = true;
				return Instance;
			}

			public FDatasmithFacadeMeshElement GetCurrentMeshElement()
			{
				if (InstanceDataStack.Count > 0)
				{
					return InstanceDataStack.Peek().DatasmithMeshElement;
				}
				return DatasmithMeshElement;
			}

			public void AddLightActor(
				Transform InWorldTransform,
				Asset InLightAsset
			)
			{
				// Create a new Datasmith light actor.
				// Hash the Datasmith light actor name to shorten it.
				string HashedActorName = FDatasmithFacadeElement.GetStringHash("L:" + GetActorName(true));
				FDatasmithFacadeActorLight LightActor = FDatasmithRevitLight.CreateLightActor(CurrentElement, HashedActorName);
				LightActor.SetLabel(GetActorLabel());

				// Set the world transform of the Datasmith light actor.
				DocumentData.SetActorTransform(InWorldTransform, LightActor);

				// Set the base properties of the Datasmith light actor.
				string LayerName = Category.GetCategory(CurrentElement.Document, BuiltInCategory.OST_LightingFixtureSource)?.Name ?? "Light Sources";
				SetActorProperties(LayerName, LightActor);

				FDatasmithFacadeMetaData LightMetaData = null;

				if (!DocumentData.bSkipMetadataExport)
				{
					LightMetaData = GetActorMetaData(LightActor);
				}

				// Set the Datasmith light actor layer to its predefined name.
				string CategoryName = Category.GetCategory(CurrentElement.Document, BuiltInCategory.OST_LightingFixtureSource)?.Name ?? "Light Sources";
				LightActor.SetLayer(CategoryName);

				// Set the specific properties of the Datasmith light actor.
				FDatasmithRevitLight.SetLightProperties(InLightAsset, CurrentElement, LightActor);

				// Add the light actor to the Datasmith actor hierarchy.
				AddChildActor(LightActor, LightMetaData, false, true);
			}

			public bool AddRPCActor(
				Transform InWorldTransform,
				Asset InRPCAsset,
				FMaterialData InMaterialData,
				out FDatasmithFacadeMesh OutDatasmithMesh,
				out FDatasmithFacadeMeshElement OutDatasmithMeshElement
			)
			{
				// Create a new Datasmith RPC mesh.
				// Hash the Datasmith RPC mesh name to shorten it.
				string HashedName = FDatasmithFacadeElement.GetStringHash("RPCM:" + GetActorName(false));
				FDatasmithFacadeMesh RPCMesh = new FDatasmithFacadeMesh();
				RPCMesh.SetName(HashedName);
				Transform AffineTransform = Transform.Identity;

				LocationPoint RPCLocationPoint = CurrentElement.Location as LocationPoint;

				if (RPCLocationPoint != null)
				{
					if (RPCLocationPoint.Rotation != 0.0)
					{
						AffineTransform = AffineTransform.Multiply(Transform.CreateRotation(XYZ.BasisZ, -RPCLocationPoint.Rotation));
						AffineTransform = AffineTransform.Multiply(Transform.CreateTranslation(RPCLocationPoint.Point.Negate()));
					}
					else
					{
						AffineTransform = Transform.CreateTranslation(RPCLocationPoint.Point.Negate());
					}
				}

				int TotalVertexCount = 0;
				int TotalTriangleCount = 0;
				List<Mesh> GeometryObjectList = new List<Mesh>();
				foreach (GeometryObject RPCGeometryObject in CurrentElement.get_Geometry(new Options()))
				{
					GeometryInstance RPCGeometryInstance = RPCGeometryObject as GeometryInstance;
					if (RPCGeometryInstance == null)
					{
						continue;
					}

					foreach (GeometryObject RPCInstanceGeometryObject in RPCGeometryInstance.GetInstanceGeometry())
					{
						Mesh RPCInstanceGeometryMesh = RPCInstanceGeometryObject as Mesh;
						if (RPCInstanceGeometryMesh == null || RPCInstanceGeometryMesh.NumTriangles < 1)
						{
							continue;
						}

						TotalVertexCount += RPCInstanceGeometryMesh.Vertices.Count;
						TotalTriangleCount += RPCInstanceGeometryMesh.NumTriangles;
						GeometryObjectList.Add(RPCInstanceGeometryMesh);
					}
				}

				RPCMesh.SetVerticesCount(TotalVertexCount);
				RPCMesh.SetFacesCount(TotalTriangleCount * 2); // Triangles are added twice for RPC meshes: CW & CCW

				int MeshMaterialIndex = 0;
				int VertexIndexOffset = 0;
				int TriangleIndexOffset = 0;
				foreach (Mesh RPCInstanceGeometryMesh in GeometryObjectList)
				{
					// RPC geometry does not have normals nor UVs available through the Revit Mesh interface.
					int VertexCount = RPCInstanceGeometryMesh.Vertices.Count;
					int TriangleCount = RPCInstanceGeometryMesh.NumTriangles;

					// Add the RPC geometry vertices to the Datasmith RPC mesh.
					for (int VertexIndex = 0; VertexIndex < RPCInstanceGeometryMesh.Vertices.Count; ++VertexIndex)
					{
						XYZ PositionedVertex = AffineTransform.OfPoint(RPCInstanceGeometryMesh.Vertices[VertexIndex]);
						RPCMesh.SetVertex(VertexIndexOffset + VertexIndex, (float)PositionedVertex.X, (float)PositionedVertex.Y, (float)PositionedVertex.Z);
					}

					// Add the RPC geometry triangles to the Datasmith RPC mesh.
					for (int TriangleNo = 0, BaseTriangleIndex = 0; TriangleNo < TriangleCount; TriangleNo++, BaseTriangleIndex += 2)
					{
						MeshTriangle Triangle = RPCInstanceGeometryMesh.get_Triangle(TriangleNo);

						try
						{
							int Index0 = VertexIndexOffset + Convert.ToInt32(Triangle.get_Index(0));
							int Index1 = VertexIndexOffset + Convert.ToInt32(Triangle.get_Index(1));
							int Index2 = VertexIndexOffset + Convert.ToInt32(Triangle.get_Index(2));

							// Add triangles for both the front and back faces.
							RPCMesh.SetFace(TriangleIndexOffset + BaseTriangleIndex, Index0, Index1, Index2, MeshMaterialIndex);
							RPCMesh.SetFace(TriangleIndexOffset + BaseTriangleIndex + 1, Index2, Index1, Index0, MeshMaterialIndex);
						}
						catch (OverflowException)
						{
							continue;
						}
					}

					VertexIndexOffset += VertexCount;
					TriangleIndexOffset += TriangleCount * 2;
				}

				// Create a new Datasmith RPC mesh actor.
				// Hash the Datasmith RPC mesh actor name to shorten it.
				string HashedActorName = FDatasmithFacadeElement.GetStringHash("RPC:" + GetActorName(true));
				FDatasmithFacadeActor FacadeActor;
				if (RPCMesh.GetVerticesCount() > 0 && RPCMesh.GetFacesCount() > 0)
				{
					FDatasmithFacadeActorMesh RPCMeshActor = new FDatasmithFacadeActorMesh(HashedActorName);
					RPCMeshActor.SetMesh(RPCMesh.GetName());
					FacadeActor = RPCMeshActor;

					OutDatasmithMesh = RPCMesh;
					OutDatasmithMeshElement = new FDatasmithFacadeMeshElement(HashedName);
					OutDatasmithMeshElement.SetLabel(GetActorLabel());
					OutDatasmithMeshElement.SetMaterial(InMaterialData.MaterialInstance.GetName(), MeshMaterialIndex);
				}
				else
				{
					//Create an empty actor instead of a static mesh actor with no mesh.
					FacadeActor = new FDatasmithFacadeActor(HashedActorName);

					OutDatasmithMesh = null;
					OutDatasmithMeshElement = null;
				}
				FacadeActor.SetLabel(GetActorLabel());

				// Set the world transform of the Datasmith RPC mesh actor.
				DocumentData.SetActorTransform(InWorldTransform, FacadeActor);

				// Set the base properties of the Datasmith RPC mesh actor.
				string LayerName = GetCategoryName();
				SetActorProperties(LayerName, FacadeActor);

				// Add a Revit element RPC tag to the Datasmith RPC mesh actor.
				FacadeActor.AddTag("Revit.Element.RPC");

				// Add some Revit element RPC metadata to the Datasmith RPC mesh actor.
				AssetProperty RPCTypeId = InRPCAsset.FindByName("RPCTypeId");
				AssetProperty RPCFilePath = InRPCAsset.FindByName("RPCFilePath");

				FDatasmithFacadeMetaData ElementMetaData = new FDatasmithFacadeMetaData(FacadeActor.GetName() + "_DATA");
				ElementMetaData.SetLabel(FacadeActor.GetLabel());
				ElementMetaData.SetAssociatedElement(FacadeActor);

				if (RPCTypeId != null)
				{
					ElementMetaData.AddPropertyString("Type*RPCTypeId", (RPCTypeId as AssetPropertyString).Value);
				}

				if (RPCFilePath != null)
				{
					ElementMetaData.AddPropertyString("Type*RPCFilePath", (RPCFilePath as AssetPropertyString).Value);
				}

				// Add the RPC mesh actor to the Datasmith actor hierarchy.
				AddChildActor(FacadeActor, ElementMetaData, false, true);

				return OutDatasmithMesh != null;
			}

			public void AddChildActor(
				FBaseElementData InChildActor
			)
			{
				FBaseElementData ParentElement = (InstanceDataStack.Count == 0) ? this : InstanceDataStack.Peek();

				ParentElement.ChildElements.Add(InChildActor);
				InChildActor.Parent = ParentElement;
			}

			public void AddChildActor(
				FDatasmithFacadeActor ChildActor,
				FDatasmithFacadeMetaData MetaData,
				bool bOptimizeHierarchy,
				bool bOwned // Make its lifetime controlled by this ElementData
			)
			{
				FBaseElementData ElementData = new FBaseElementData(ChildActor, MetaData, DocumentData);
				ElementData.bOptimizeHierarchy = bOptimizeHierarchy;

				FBaseElementData Parent = (InstanceDataStack.Count == 0) ? this : InstanceDataStack.Peek();

				Parent.ChildElements.Add(ElementData);
				ElementData.Parent = Parent;
				ElementData.bOwnedByParent = bOwned;
			}

			public void InitializeElement(
					Transform InWorldTransform,
					FBaseElementData InElement
			)
			{
				InElement.WorldTransform = InWorldTransform;

				// Create a new Datasmith mesh.
				// Hash the Datasmith mesh name to shorten it.
				string HashedMeshName = FDatasmithFacadeElement.GetStringHash("M:" + GetMeshName());
				InElement.DatasmithPolymesh = new FDatasmithPolymesh();
				InElement.DatasmithMeshElement = new FDatasmithFacadeMeshElement(HashedMeshName);
				InElement.DatasmithMeshElement.SetLabel(GetActorLabel());

				if (InElement.ElementActor == null)
				{
					// Create a new Datasmith mesh actor.
					// Hash the Datasmith mesh actor name to shorten it.
					string HashedActorName = FDatasmithFacadeElement.GetStringHash("A:" + GetActorName(true));

					if (BaseElementType != null && BaseElementType.FamilyName == "Decal")
					{
						bOptimizeHierarchy = false;
						bIsDecalElement = true;
						InElement.ElementActor = new FDatasmithFacadeActorDecal(HashedActorName);
					}
					else
					{
						InElement.ElementActor = new FDatasmithFacadeActorMesh(HashedActorName);
					}

					InElement.ElementActor.SetLabel(GetActorLabel());
				}

				// Set the world transform of the Datasmith mesh actor.
				DocumentData.SetActorTransform(InWorldTransform, InElement.ElementActor);

				// Set the base properties of the Datasmith mesh actor.
				string LayerName = GetCategoryName();
				SetActorProperties(LayerName, InElement.ElementActor);

				if (!DocumentData.bSkipMetadataExport)
				{
					InElement.ElementMetaData = GetActorMetaData(InElement.ElementActor);
				}
			}

			public string GetCategoryName()
			{
				return BaseElementType?.Category?.Name ?? CurrentElement.Category?.Name;
			}

			public bool IgnoreElementGeometry()
			{
				// Ignore elements that have unwanted geometry, such as level symbols.
				return (BaseElementType as LevelType) != null;
			}

			public FDatasmithPolymesh GetCurrentPolymesh()
			{
				if (InstanceDataStack.Count == 0)
				{
					return DatasmithPolymesh;
				}
				else
				{
					return InstanceDataStack.Peek().DatasmithPolymesh;
				}
			}

			public FBaseElementData PeekInstance()
			{
				return InstanceDataStack.Count > 0 ? InstanceDataStack.Peek() : null;
			}

			public FBaseElementData GetCurrentActor()
			{
				if (InstanceDataStack.Count == 0)
				{
					return this;
				}
				else
				{
					return InstanceDataStack.Peek();
				}
			}

			public void Log(
				FDatasmithFacadeLog InDebugLog,
				string InLinePrefix,
				int InLineIndentation
			)
			{
				if (InDebugLog != null)
				{
					if (InLineIndentation < 0)
					{
						InDebugLog.LessIndentation();
					}

					Element SourceElement = (InstanceDataStack.Count == 0) ? CurrentElement : InstanceDataStack.Peek().BaseElementType;

					InDebugLog.AddLine($"{InLinePrefix} {SourceElement.Id.IntegerValue} '{SourceElement.Name}' {SourceElement.GetType()}: '{GetActorLabel()}'");

					if (InLineIndentation > 0)
					{
						InDebugLog.MoreIndentation();
					}
				}
			}

			private string GetActorName(bool bEnsureUnique)
			{
				return DocumentData.GetActorName(this);
			}

			public string GenerateUniqueInstanceNameSuffix()
			{
				// GenerateUniqueInstanceName is being called when generating a name for instance.
				// After the call, the intance is added as a child to its parent.
				// Next time the method gets called for the next instance, ChildElements.Count will be different/incremented.

				// To add uniqueness to the generated name, we construct a string with child counts from
				// current parent instance, up to the root:
				// Elem->Instance->Instance->Instance can produce something like: "1:5:3" for example.
				// However, this is not enough because elsewhere we might encounter the same sequence in terms of child counts,
				// but adding the CurrentElement unique id ensures we get unique name string in the end.

				StringBuilder ChildCounts = new StringBuilder();

				for (int ElemIndex = 1; ElemIndex < InstanceDataStack.Count; ++ElemIndex)
				{
					FBaseElementData Elem = InstanceDataStack.ElementAt(ElemIndex);
					ChildCounts.AppendFormat(":{0}", Elem.ChildElements.Count);
				}

				// Add child count for the root element (parent of all instances)
				ChildCounts.AppendFormat(":{0}", ChildElements.Count);

				return $"I:{ChildCounts}";
			}

			private string GetMeshName()
			{
				if (InstanceDataStack.Count == 0)
				{
					return $"{DocumentData.DocumentId}:{CurrentElement.UniqueId}";
				}

				FBaseElementData Instance = InstanceDataStack.Peek();

				if (Instance.bAllowMeshInstancing)
				{
					// Generate instanced mesh name using Instance geometry UniqueId
					return $"{DocumentData.DocumentId}:{Instance.BaseElementType.UniqueId}";
				}

				return GetActorName(true);  // Use unique element's actor name to base mesh name on
			}

			private string GetActorLabel()
			{
				string CategoryName = GetCategoryName();
				string FamilyName = BaseElementType?.FamilyName;
				string TypeName = BaseElementType?.Name;
				string InstanceName = (InstanceDataStack.Count > 1) ? InstanceDataStack.Peek().BaseElementType?.Name : null;

				string ActorLabel = "";

				if (CurrentElement as Level != null)
				{
					ActorLabel += string.IsNullOrEmpty(FamilyName) ? "" : FamilyName + "*";
					ActorLabel += string.IsNullOrEmpty(TypeName) ? "" : TypeName + "*";
					ActorLabel += CurrentElement.Name;
				}
				else
				{
					ActorLabel += string.IsNullOrEmpty(CategoryName) ? "" : CategoryName + "*";
					ActorLabel += string.IsNullOrEmpty(FamilyName) ? "" : FamilyName + "*";
					ActorLabel += string.IsNullOrEmpty(TypeName) ? CurrentElement.Name : TypeName;
					ActorLabel += string.IsNullOrEmpty(InstanceName) ? "" : "*" + InstanceName;
				}

				DocumentData.Context.LogDebug($"GetActorLabel(CategoryName='{CategoryName}', FamilyName='{FamilyName}', TypeName='{TypeName}', InstanceName='{InstanceName}') -> '{ActorLabel}'");
				return ActorLabel;
			}

			private void SetActorProperties(
				string InLayerName,
				FDatasmithFacadeActor IOActor
			)
			{
				// Set the Datasmith actor layer to the element type category name.
				IOActor.SetLayer(InLayerName);

				// Add the Revit element ID and Unique ID tags to the Datasmith actor.
				IOActor.AddTag($"Revit.Element.Id.{CurrentElement.Id.IntegerValue}");
				IOActor.AddTag($"Revit.Element.UniqueId.{CurrentElement.UniqueId}");

				// For an hosted Revit family instance, add the host ID, Unique ID and Mirrored/Flipped flags as tags to the Datasmith actor.
				FamilyInstance CurrentFamilyInstance = CurrentElement as FamilyInstance;
				if (CurrentFamilyInstance != null)
				{
					IOActor.AddTag($"Revit.DB.FamilyInstance.Mirrored.{CurrentFamilyInstance.Mirrored}");
					IOActor.AddTag($"Revit.DB.FamilyInstance.HandFlipped.{CurrentFamilyInstance.HandFlipped}");
					IOActor.AddTag($"Revit.DB.FamilyInstance.FaceFlipped.{CurrentFamilyInstance.FacingFlipped}");

					if (CurrentFamilyInstance.Host != null)
					{
						IOActor.AddTag($"Revit.Host.Id.{CurrentFamilyInstance.Host.Id.IntegerValue}");
						IOActor.AddTag($"Revit.Host.UniqueId.{CurrentFamilyInstance.Host.UniqueId}");
					}
				}
			}

			private FDatasmithFacadeMetaData GetActorMetaData(FDatasmithFacadeActor IOActor)
			{
				FDatasmithFacadeMetaData ElementMetaData = new FDatasmithFacadeMetaData(IOActor.GetName() + "_DATA");
				ElementMetaData.SetLabel(IOActor.GetLabel());
				ElementMetaData.SetAssociatedElement(IOActor);

				// Add the Revit element category name metadata to the Datasmith actor.
				string CategoryName = GetCategoryName();
				if (!string.IsNullOrEmpty(CategoryName))
				{
					ElementMetaData.AddPropertyString("Element*Category", CategoryName);
				}

				// Add the Revit element family name metadata to the Datasmith actor.
				string FamilyName = BaseElementType?.FamilyName;
				if (!string.IsNullOrEmpty(FamilyName))
				{
					ElementMetaData.AddPropertyString("Element*Family", FamilyName);
				}

				// Add the Revit element type name metadata to the Datasmith actor.
				string TypeName = BaseElementType?.Name;
				if (!string.IsNullOrEmpty(TypeName))
				{
					ElementMetaData.AddPropertyString("Element*Type", TypeName);
				}

				// Add Revit element metadata to the Datasmith actor.
				FUtils.AddActorMetadata(CurrentElement, "Element*", ElementMetaData, DocumentData.CurrentSettings);

				if (BaseElementType != null)
				{
					// Add Revit element type metadata to the Datasmith actor.
					FUtils.AddActorMetadata(BaseElementType, "Type*", ElementMetaData, DocumentData.CurrentSettings);
				}

				return ElementMetaData;
			}
		}

		private string GetActorName(FElementData InElementData)
		{
			// GetActorName should be called only for the current processed element
			// so InElementData serves internally the only purpose to validate the call
			Debug.Assert(InElementData == ElementDataStack.Peek());

			string ActorName = Context.GetActorName(this);
			if (InElementData.InstanceDataStack.Count == 0)
			{
				return ActorName;
			}

			
			// Instance actor name (when InstanceDataStack is not empty), using current element's encountered instance count as instance's identification
			// (to to OnElementBegin could follow multiple OnInstanceBegin/End calls, meaning an element can have multiple instances nested right under it)
			// And each instance should have a separate unique name name identified by element path from root plus instances index(calling GetActorName for next
			// instance in the same element will have InstanceDataStack.Count increased)
			return ActorName + InElementData.GenerateUniqueInstanceNameSuffix();
		}

		public Dictionary<string, Tuple<FDatasmithFacadeMeshElement, Task<bool>>>
														MeshMap = new Dictionary<string, Tuple<FDatasmithFacadeMeshElement, Task<bool>>>();
		public Dictionary<ElementId, FBaseElementData>	ActorMap = new Dictionary<ElementId, FDocumentData.FBaseElementData>();
		public Dictionary<string, FMaterialData>		MaterialDataMap = null;
		public Dictionary<string, FMaterialData>		NewMaterialsMap = new Dictionary<string, FMaterialData>();

		public Dictionary<ElementId, FElementData>		DecalElementsMap = new Dictionary<ElementId, FElementData>();
		public Dictionary<ElementId, FDecalMaterial>	DecalMaterialsMap = new Dictionary<ElementId, FDecalMaterial>();

		private Stack<FElementData>						ElementDataStack = new Stack<FElementData>();
		private string									CurrentMaterialName = null;
		private List<string>							MessageList = null;

		private FSettings								CurrentSettings = null;

		// Apply world offset to elements
		public	FSettings.EInsertionPoint				InsertionPoint { get; set; } = FSettings.EInsertionPoint.Default;

		private XYZ										ProjectSurveyPoint = null;
		private XYZ										ProjectBasePoint = null;

		public FDatasmithRevitExportContext				Context = null;

		public string									DocumentId { get; private set; } = "";

		public bool										bSkipMetadataExport { get; private set; } = false;
		public Document									CurrentDocument { get; private set; } = null;
		public FDirectLink								DirectLink { get; private set; } = null;

		public Outline									SectionBoxOutline = null;
		private OrientatedBoundingBox					SectionBox = null;

		public FDocumentData(
			Document InDocument,
			FSettings InSettings,
			ref List<string> InMessageList,
			FDirectLink InDirectLink,
			string InDocumentId
		)
		{
			
			CurrentSettings = InSettings;
			DirectLink = InDirectLink;
			CurrentDocument = InDocument;
			MessageList = InMessageList;
			// With DirectLink, we delay export of metadata for a faster initial export.
			bSkipMetadataExport = (DirectLink != null);


			DocumentId = InDocumentId;
		}

		public void Reset(FDatasmithRevitExportContext InContext)
		{
			Context = InContext; // Make currently processing document aware of the current export context (currently context is recreated on each Sync)
			MeshMap = new Dictionary<string, Tuple<FDatasmithFacadeMeshElement, Task<bool>>>();
			ActorMap = new Dictionary<ElementId, FBaseElementData>();
			MaterialDataMap = null;
			NewMaterialsMap = new Dictionary<string, FMaterialData>();
			DecalElementsMap = new Dictionary<ElementId, FElementData>();
			DecalMaterialsMap = new Dictionary<ElementId, FDecalMaterial>();

			ElementDataStack = new Stack<FElementData>();
			CurrentMaterialName = null;

			InsertionPoint = FSettings.EInsertionPoint.Default;

			ProjectSurveyPoint = null;
			ProjectBasePoint = null;

			SectionBoxOutline = null;
			SectionBox = null;

			if (DirectLink != null)
			{
				MaterialDataMap = DirectLink.MaterialDataMap;
			}
			else
			{
				MaterialDataMap = new Dictionary<string, FMaterialData>();
			}

			InsertionPoint = CurrentSettings?.InsertionPoint ?? FSettings.EInsertionPoint.Default;

			// Cache document section boxes
			if (CurrentDocument.ActiveView != null)
			{
				View3D CurrentView3d = (GetElement(CurrentDocument.ActiveView.Id) as View3D);

				SectionBox = null;
				SectionBoxOutline = null;

				if (CurrentView3d != null && CurrentView3d.IsSectionBoxActive)
				{
					BoundingBoxXYZ BBox = CurrentView3d.GetSectionBox();


#if REVIT_API_2023
					if (BBox.IsSet && BBox.Enabled)
#else
					if (BBox.Enabled)
#endif
					{
						SectionBox = new OrientatedBoundingBox(BBox.Transform, BBox.Min, BBox.Max, true);
						if (SectionBox.bIsValidData)
						{
							SectionBoxOutline = new Outline(SectionBox.AxisAllignedMin, SectionBox.AxisAllignedMax);
						}
						else
						{
							SectionBox = null;
						}
					}
				}
			}
		}

		public Element GetElement(
			ElementId InElementId
		)
		{
			return (InElementId != ElementId.InvalidElementId) ? CurrentDocument.GetElement(InElementId) : null;
		}

		public bool ContainsMesh(string MeshName)
		{
			return MeshMap.ContainsKey(MeshName);
		}

		public bool PushElement(
			Element InElement,
			Transform InWorldTransform
		)
		{
#if REVIT_API_2023
			if (DirectLink != null && InElement.Category != null && InElement.Category.BuiltInCategory != BuiltInCategory.OST_Levels)
#else
			if (DirectLink != null && InElement.Category != null && (BuiltInCategory)InElement.Category.Id.IntegerValue != BuiltInCategory.OST_Levels)
#endif
			{
				//Check if any of its children is Decal:
				//If so track them, so that we can use the owner object elementId to update the Decals (when modified, for ep: changing its location)
				foreach (ElementId DependentElementId in FUtils.GetAllDependentElements(InElement))
				{
					if (FUtils.IsElementIdDecal(InElement.Document, DependentElementId))
					{
						if (DependentElementId != InElement.Id && !DirectLink.DecalIdToOwnerObjectIdMap.ContainsKey(DependentElementId))
						{
							DirectLink.DecalIdToOwnerObjectIdMap.Add(DependentElementId, InElement.Id);
						}
					}
				}
			}

			DirectLink?.MarkForExport(InElement);

			FElementData ElementData = null;

			if (ActorMap.ContainsKey(InElement.Id))
			{
				if (DirectLink != null && DirectLink.IsElementCached(InElement))
				{
					return false;
				}
				ElementData = ActorMap[InElement.Id] as FElementData;
			}

			if (ElementData == null)
			{
				if (DirectLink?.IsElementCached(InElement) ?? false)
				{
					ElementData = (FElementData)DirectLink.GetCachedElement(InElement);

					bool bIsLinkedDocument = InElement.GetType() == typeof(RevitLinkInstance);

					if (bIsLinkedDocument || DirectLink.IsElementModified(InElement))
					{
						FDatasmithFacadeActor ExistingActor = ElementData.ElementActor;

						// Remove children that are instances: they will be re-created;
						// The reason is that we cannot uniquely identify family instances (no id) and when element changes,
						// we need to export all of its child instances anew.
						if (ExistingActor != null)
						{
							if (ElementData.ChildElements.Count > 0)
							{
								List<FBaseElementData> ChildrenToRemove = new List<FBaseElementData>();

								for(int ChildIndex = 0; ChildIndex < ElementData.ChildElements.Count; ++ChildIndex)
								{
									FBaseElementData ChildElement = ElementData.ChildElements[ChildIndex];

									bool bIsFamilyIntance =
										((ChildElement as FElementData) == null) &&
										ChildElement.ElementActor.IsComponent();

									if (bIsFamilyIntance)
									{
										ChildrenToRemove.Add(ChildElement);
									}
								}

								foreach (FBaseElementData Child in ChildrenToRemove)
								{
									ExistingActor.RemoveChild(Child.ElementActor);
									ElementData.ChildElements.Remove(Child);
								}
							}

							ExistingActor.ResetTags();
							(ExistingActor as FDatasmithFacadeActorMesh)?.SetMesh(null);
						}

						ElementData.BaseElementType = InElement.Document.GetElement(InElement.GetTypeId()) as ElementType;
						ElementData.CurrentElement = InElement;
						ElementData.MeshMaterialsMap.Clear();
					}
					else
					{
						ActorMap[InElement.Id] = ElementData;
						return false; // We have up to date cache for this element.
					}
				}
				else
				{
					ElementData = new FElementData(InElement, InWorldTransform, this);
				}

				ElementDataStack.Push(ElementData);

				// Initialize element after pushing it on the stack(to unify this with other calls to element's methods)
				// GetActorName depends on this 
				ElementData.InitializePivotPlacement(ref InWorldTransform);
				ElementData.InitializeElement(InWorldTransform, ElementData);

				if (ElementData.bIsDecalElement)
				{
					if (!DecalElementsMap.ContainsKey(InElement.Id))
					{
						DecalElementsMap.Add(InElement.Id, ElementData);
					}
				}

			}
			else
			{
				ElementDataStack.Push(ElementData);
			}

			ElementDataStack.Peek().ElementActor.AddTag("IsElement");

			return true;
		}

		public void PopElement(FDatasmithFacadeScene InDatasmithScene)
		{
			FElementData ElementData = ElementDataStack.Pop();
			FDatasmithPolymesh DatasmithPolymesh = ElementData.DatasmithPolymesh;

			if(DatasmithPolymesh.Vertices.Count > 0 && DatasmithPolymesh.Faces.Count > 0)
			{
				ElementData.UpdateMeshName();
			}

			CollectMesh(ElementData.DatasmithPolymesh, ElementData.DatasmithMeshElement, InDatasmithScene);

			DirectLink?.ClearModified(ElementData.CurrentElement);

			ElementData.bIsModified = true;

			ElementId ElemId = ElementData.CurrentElement.Id;

			if (ElementDataStack.Count == 0)
			{
				if (ActorMap.ContainsKey(ElemId) && ActorMap[ElemId] != ElementData)
				{
					// Handle the spurious case of Revit Custom Exporter calling back more than once for the same element.
					// These extra empty actors will be cleaned up later by the Datasmith actor hierarchy optimization.
					ActorMap[ElemId].ChildElements.Add(ElementData);
					ElementData.Parent = ActorMap[ElemId];
				}
				else
				{
					// Collect the element mesh actor into the Datasmith actor dictionary.
					ActorMap[ElemId] = ElementData;
				}
			}
			else
			{
				if (!ActorMap.ContainsKey(ElemId))
				{
					// Add the element mesh actor to the Datasmith actor hierarchy.
					ElementDataStack.Peek().AddChildActor(ElementData);
				}
			}
		}

		private static FDatasmithFacadeActor DuplicateBaseActor(FDatasmithFacadeActor SourceActor)
		{
			FDatasmithFacadeActor CloneActor = new FDatasmithFacadeActor(SourceActor.GetName());
			CloneActor.SetLabel(SourceActor.GetLabel());

			double X, Y, Z, W;
			SourceActor.GetTranslation(out X, out Y, out Z);
			CloneActor.SetTranslation(X, Y, Z);
			SourceActor.GetScale(out X, out Y, out Z);
			CloneActor.SetScale(X, Y, Z);
			SourceActor.GetRotation(out X, out Y, out Z, out W);
			CloneActor.SetRotation(X, Y, Z, W);

			CloneActor.SetLayer(SourceActor.GetLayer());

			for (int TagIndex = 0; TagIndex < SourceActor.GetTagsCount(); ++TagIndex)
			{
				CloneActor.AddTag(SourceActor.GetTag(TagIndex));
			}

			CloneActor.SetIsComponent(SourceActor.IsComponent());
			CloneActor.SetVisibility(SourceActor.GetVisibility());

			for (int ChildIndex = 0; ChildIndex < SourceActor.GetChildrenCount(); ++ChildIndex)
			{
				CloneActor.AddChild(SourceActor.GetChild(ChildIndex));
			}

			return CloneActor;
		}

		public void PushInstance(
			ElementType InInstanceType,
			Transform InWorldTransform
		)
		{
			// Check if this instance intersects any section box.
			// If so, we can't instance its mesh--will be considered unique.

			bool bIntersectedBySectionBox = false;

			if (SectionBox != null)
			{
				BoundingBoxXYZ InstanceBoundingBox = InInstanceType.get_BoundingBox(CurrentDocument.ActiveView);
				
				if (InstanceBoundingBox != null)
				{
					OrientatedBoundingBox InstanceOrientatedBoundingBox = new OrientatedBoundingBox(InWorldTransform, InstanceBoundingBox.Min, InstanceBoundingBox.Max);
					if (InstanceOrientatedBoundingBox.bIsValidData)
					{
						Outline InstanceOutline = new Outline(InstanceOrientatedBoundingBox.AxisAllignedMin, InstanceOrientatedBoundingBox.AxisAllignedMax);

						bIntersectedBySectionBox = SectionBox.DoesIntersect(InstanceOrientatedBoundingBox);
					}
				}
			}

			FElementData CurrentElementData = ElementDataStack.Peek();
			FBaseElementData NewInstance = CurrentElementData.PushInstance(InInstanceType, InWorldTransform, !bIntersectedBySectionBox);
		}

		public void PopInstance(FDatasmithFacadeScene InDatasmithScene)
		{
			FElementData CurrentElement = ElementDataStack.Peek();
			FBaseElementData InstanceData = CurrentElement.PopInstance();
			FDatasmithPolymesh DatasmithPolymesh = InstanceData.DatasmithPolymesh;

			if (ContainsMesh(InstanceData.DatasmithMeshElement.GetName()) || (DatasmithPolymesh.Vertices.Count > 0 && DatasmithPolymesh.Faces.Count > 0))
			{
				InstanceData.UpdateMeshName();
			}
			else
			{
				/* Instance has no mesh.
				 * Handle the case where instance has valid transform, but parent element has valid mesh (exported after instance gets finished),
				 * in which case we want to apply the instance transform as a pivot transform.
				 * This is a case currently encountered for steel beams.
				 */
				bool bElementHasMesh = ContainsMesh(CurrentElement.DatasmithMeshElement.GetName()) || (CurrentElement.DatasmithPolymesh.Vertices.Count > 0 && CurrentElement.DatasmithPolymesh.Faces.Count > 0);

				if (CurrentElement.CurrentElement.GetType() == typeof(FamilyInstance) && !bElementHasMesh)
				{
					if (!CurrentElement.WorldTransform.IsIdentity)
					{
						CurrentElement.MeshPointsTransform = (CurrentElement.WorldTransform.Inverse * InstanceData.WorldTransform).Inverse;
					}
					else
					{
						CurrentElement.MeshPointsTransform = InstanceData.WorldTransform.Inverse;
					}

					CurrentElement.WorldTransform = InstanceData.WorldTransform;
					SetActorTransform(CurrentElement.WorldTransform, CurrentElement.ElementActor);
				}
			}

			// Collect the element Datasmith mesh into the mesh dictionary.
			CollectMesh(InstanceData.DatasmithPolymesh, InstanceData.DatasmithMeshElement, InDatasmithScene);

			// Add the instance mesh actor to the Datasmith actor hierarchy.
			CurrentElement.AddChildActor(InstanceData);
		}

		public void AddLocationActors(
			Transform InWorldTransform
		)
		{
			// Add a new Datasmith placeholder actor for this document site location.
			AddSiteLocation(CurrentDocument.SiteLocation);

			// Add new Datasmith placeholder actors for the project base point and survey points.
			// A project has one base point and at least one survey point. Linked documents also have their own points.
			AddPointLocations(InWorldTransform);
		}

		public void AddLightActor(
			Transform InWorldTransform,
			Asset InLightAsset
		)
		{
			ElementDataStack.Peek().AddLightActor(InWorldTransform, InLightAsset);
		}

		public void AddRPCActor(
			Transform InWorldTransform,
			Asset InRPCAsset,
			FDatasmithFacadeScene InDatasmithScene
		)
		{
			// Create a simple fallback material for the RPC mesh.
			string RPCCategoryName = ElementDataStack.Peek().GetCategoryName();
			bool isRPCPlant = !string.IsNullOrEmpty(RPCCategoryName) && RPCCategoryName == Category.GetCategory(CurrentDocument, BuiltInCategory.OST_Planting)?.Name;
			string RPCMaterialName = isRPCPlant ? "RPC_Plant" : "RPC_Material";
			string RPCHashedMaterialName = FDatasmithFacadeElement.GetStringHash(RPCMaterialName);

			if (!MaterialDataMap.ContainsKey(RPCHashedMaterialName))
			{
				// Color reference: https://www.color-hex.com/color-palette/70002
				Color RPCColor = isRPCPlant ? /* green */ new Color(88, 126, 96) : /* gray */ new Color(128, 128, 128);

				// Keep track of a new RPC master material.
				MaterialDataMap[RPCHashedMaterialName] = new FMaterialData(RPCHashedMaterialName, RPCMaterialName, RPCColor);
				NewMaterialsMap[RPCHashedMaterialName] = MaterialDataMap[RPCHashedMaterialName];
			}

			FMaterialData RPCMaterialData = MaterialDataMap[RPCHashedMaterialName];

			if (ElementDataStack.Peek().AddRPCActor(InWorldTransform, InRPCAsset, RPCMaterialData, out FDatasmithFacadeMesh RPCMesh, out FDatasmithFacadeMeshElement RPCMeshElement))
			{
				// Collect the RPC mesh into the Datasmith mesh dictionary.
				CollectMesh(RPCMesh, RPCMeshElement, InDatasmithScene);
			}
		}

		public bool SetMaterial(
			MaterialNode InMaterialNode,
			IList<string> InExtraTexturePaths
		)
		{
			Material CurrentMaterial = GetElement(InMaterialNode.MaterialId) as Material;

			if (InMaterialNode.HasOverriddenAppearance)
			{
				IList<KeyValuePair<ElementId, Asset>> ReferencingDecalIdAndAssetPairs = FDecalMaterial.GetDecalElementIdAndAppearancePairList(InMaterialNode);
				//Assets in on the .Value seem to be always unique, regardless if they are "instances" of the same DecalType:
				if (ReferencingDecalIdAndAssetPairs.Count > 0)
				{
					IList<FDecalMaterial> DecalMaterials = new List<FDecalMaterial>();
					foreach (KeyValuePair<ElementId, Asset> DecalIdAndAssetPair in ReferencingDecalIdAndAssetPairs)
					{
						if (DecalMaterialsMap.ContainsKey(DecalIdAndAssetPair.Key))
						{
							continue;
						}

						FDecalMaterial DecalMaterial = FDecalMaterial.Create(InMaterialNode, CurrentMaterial, DecalMaterials.Count, DecalIdAndAssetPair.Value);

						if (DecalMaterial != null)
						{
							//unique DecalMaterial:
							FDecalMaterial ExistingDecalMaterial = null;
							foreach (FDecalMaterial ExistingDecalMaterialCandidate in DecalMaterials)
							{
								if (ExistingDecalMaterialCandidate.CheckRenderValueEquiality(DecalMaterial))
								{
									ExistingDecalMaterial = ExistingDecalMaterialCandidate;
									break;
								}
							}

							if (ExistingDecalMaterial != null)
							{
								DecalMaterialsMap.Add(DecalIdAndAssetPair.Key, ExistingDecalMaterial);
							}
							else
							{
								DecalMaterialsMap.Add(DecalIdAndAssetPair.Key, DecalMaterial);
								DecalMaterials.Add(DecalMaterial);
							}
						}
					}
				}
			}

			CurrentMaterialName = FMaterialData.GetMaterialName(InMaterialNode, CurrentMaterial);

			if (!MaterialDataMap.ContainsKey(CurrentMaterialName) || (CurrentMaterial != null && DirectLink != null && DirectLink.IsMaterialDirty(CurrentMaterial)))
			{
				// Keep track of a new Datasmith master material.
				MaterialDataMap[CurrentMaterialName] = new FMaterialData(InMaterialNode, CurrentMaterial, InExtraTexturePaths);
				NewMaterialsMap[CurrentMaterialName] = MaterialDataMap[CurrentMaterialName];

				DirectLink?.SetMaterialClean(CurrentMaterial);

				// A new Datasmith master material was created.
				return true;
			}

			// No new Datasmith master material created.
			return false;
		}

		public bool IgnoreElementGeometry()
		{
			bool bIgnore = ElementDataStack.Peek().IgnoreElementGeometry();

			if (!bIgnore)
			{
				// Check for instanced meshes.
				// For mesh to be reused, it must not be cutoff by a section box.
				FBaseElementData CurrentInstance = ElementDataStack.Peek().PeekInstance();

				if (CurrentInstance != null && CurrentInstance.bAllowMeshInstancing && CurrentInstance.DatasmithMeshElement != null)
				{
					bIgnore = MeshMap.ContainsKey(CurrentInstance.DatasmithMeshElement.GetName());
				}
			}

			return bIgnore;
		}

		public FDatasmithPolymesh GetCurrentPolymesh()
		{
			return ElementDataStack.Peek().GetCurrentPolymesh();
		}

		public FDatasmithFacadeMeshElement GetCurrentMeshElement()
		{
			return ElementDataStack.Peek().GetCurrentMeshElement();
		}

		public Transform GetCurrentMeshPointsTransform()
		{
			return ElementDataStack.Peek().MeshPointsTransform;
		}

		public int GetCurrentMaterialIndex()
		{
			FElementData ElemData = ElementDataStack.Peek();
			FBaseElementData InstanceData = ElemData.PeekInstance();
			FBaseElementData CurrentElement = InstanceData != null ? InstanceData : ElemData;

			if (!CurrentElement.MeshMaterialsMap.ContainsKey(CurrentMaterialName))
			{
				int NewMaterialIndex = CurrentElement.MeshMaterialsMap.Count;
				CurrentElement.MeshMaterialsMap[CurrentMaterialName] = NewMaterialIndex;
				CurrentElement.DatasmithMeshElement.SetMaterial(CurrentMaterialName, NewMaterialIndex);
			}

			return CurrentElement.MeshMaterialsMap[CurrentMaterialName];
		}

		public FBaseElementData GetCurrentActor()
		{
			return ElementDataStack.Peek().GetCurrentActor();
		}

		public Element GetCurrentElement()
		{
			return ElementDataStack.Count > 0 ? ElementDataStack.Peek().CurrentElement : null;
		}

		private FBaseElementData OptimizeElementRecursive(FBaseElementData InElementData, FDatasmithFacadeScene InDatasmithScene, FSuperComponentOptimizer SuperComponentOptimizer)
		{
			List<FBaseElementData> RemoveChildren = new List<FBaseElementData>();
			List<FBaseElementData> AddChildren = new List<FBaseElementData>();

			for (int ChildIndex = 0; ChildIndex < InElementData.ChildElements.Count; ChildIndex++)
			{
				FBaseElementData ChildElement = InElementData.ChildElements[ChildIndex];

				// Optimize the Datasmith child actor.
				FBaseElementData ResultElement = OptimizeElementRecursive(ChildElement, InDatasmithScene, SuperComponentOptimizer);

				if (ChildElement != ResultElement)
				{
					RemoveChildren.Add(ChildElement);

					if (ResultElement != null)
					{
						AddChildren.Add(ResultElement);

						SuperComponentOptimizer.UpdateCache(ResultElement, ChildElement);
					}
				}
			}

			foreach (FBaseElementData Child in RemoveChildren)
			{
				Child.Parent = null;
				InElementData.ChildElements.Remove(Child);
				InElementData.ElementActor.RemoveChild(Child.ElementActor);
			}
			foreach (FBaseElementData Child in AddChildren)
			{
				Child.Parent = InElementData;
				InElementData.ChildElements.Add(Child);
				InElementData.ElementActor.AddChild(Child.ElementActor);
			}

			if (InElementData.bOptimizeHierarchy)
			{
				int ChildrenCount = InElementData.ElementActor.GetChildrenCount();

				if (ChildrenCount == 0)
				{
					// This Datasmith actor can be removed by optimization.
					return null;
				}

				if (ChildrenCount == 1)
				{
					Debug.Assert(InElementData.ChildElements.Count == 1);

					// This intermediate Datasmith actor can be removed while keeping its single child actor.
					FBaseElementData SingleChild = InElementData.ChildElements[0];

					// Make sure the single child actor will not become a dangling component in the actor hierarchy.
					if (!InElementData.ElementActor.IsComponent() && SingleChild.ElementActor.IsComponent())
					{
						SingleChild.ElementActor.SetIsComponent(false);
					}

					return SingleChild;
				}
			}

			return InElementData;
		}

		public void OptimizeActorHierarchy(FDatasmithFacadeScene InDatasmithScene)
		{
			FSuperComponentOptimizer SuperComponentOptimizer = new FSuperComponentOptimizer();

			foreach (var ElementEntry in ActorMap)
			{
				FBaseElementData ElementData = ElementEntry.Value;
				FBaseElementData ResultElementData = OptimizeElementRecursive(ElementData, InDatasmithScene, SuperComponentOptimizer);

				if (ResultElementData != ElementData)
				{
					if (ResultElementData == null)
					{
						InDatasmithScene.RemoveActor(ElementData.ElementActor, FDatasmithFacadeScene.EActorRemovalRule.RemoveChildren);
					}
					else
					{
						InDatasmithScene.RemoveActor(ElementData.ElementActor, FDatasmithFacadeScene.EActorRemovalRule.KeepChildrenAndKeepRelativeTransform);

						if (ElementData.ChildElements.Count == 1)
						{
							SuperComponentOptimizer.UpdateCache(ElementData.ChildElements[0], ElementData);
						}
					}
				}
			}

			SuperComponentOptimizer.Optimize();
		}

		public void WrapupLink(
			FDatasmithFacadeScene InDatasmithScene,
			FBaseElementData InLinkActor,
			HashSet<string> UniqueTextureNameSet
		)
		{
			// Add the collected meshes from the Datasmith mesh dictionary to the Datasmith scene.
			AddCollectedMeshes(InDatasmithScene);

			// Factor in the Datasmith actor hierarchy the Revit document host hierarchy.
			AddHostHierarchy();

			// Factor in the Datasmith actor hierarchy the Revit document level hierarchy.
			AddLevelHierarchy();

			if (ActorMap.Count > 0)
			{
				// Prevent the Datasmith link actor from being removed by optimization.
				InLinkActor.bOptimizeHierarchy = false;

				// Add the collected actors from the Datasmith actor dictionary as children of the Datasmith link actor.
				foreach (var Actor in ActorMap.Values)
				{
					InLinkActor.ChildElements.Add(Actor);
					Actor.Parent = InLinkActor;
				}
			}

			// Add the collected master materials from the material data dictionary to the Datasmith scene.
			AddCollectedMaterials(InDatasmithScene, UniqueTextureNameSet);
		}

		public void WrapupScene(
			FDatasmithFacadeScene InDatasmithScene,
			HashSet<string> UniqueTextureNameSet
		)
		{
			AddCollectedDecals(InDatasmithScene);

			// Add the collected meshes from the Datasmith mesh dictionary to the Datasmith scene.
			AddCollectedMeshes(InDatasmithScene);

			// Factor in the Datasmith actor hierarchy the Revit document host hierarchy.
			AddHostHierarchy();

			// Factor in the Datasmith actor hierarchy the Revit document level hierarchy.
			AddLevelHierarchy();

			List<ElementId> OptimizedAwayElements = new List<ElementId>();

			foreach (var CollectedActor in ActorMap)
			{
				if (CollectedActor.Value.Optimize())
				{
					OptimizedAwayElements.Add(CollectedActor.Key);
				}
			}

			foreach (ElementId Element in  OptimizedAwayElements)
			{
				ActorMap.Remove(Element);
			}

			// Add the collected actors from the Datasmith actor dictionary to the Datasmith scene.
			foreach (var ActorEntry in ActorMap)
			{
				Element CollectedElement = CurrentDocument.GetElement(ActorEntry.Key);

				if (DirectLink != null && CollectedElement.GetType() == typeof(RevitLinkInstance))
				{
					Document LinkedDoc = (CollectedElement as RevitLinkInstance).GetLinkDocument();
					if (LinkedDoc != null)
					{
						DirectLink.OnBeginLinkedDocument(CollectedElement);
						foreach (FBaseElementData CurrentChild in ActorEntry.Value.ChildElements)
						{
							CurrentChild.AddToScene(InDatasmithScene, ActorEntry.Value, false);
						}
						DirectLink.OnEndLinkedDocument();
						ActorEntry.Value.AddToScene(InDatasmithScene, null, true);
					}
					else
					{
						ActorEntry.Value.AddToScene(InDatasmithScene, null, false);
					}
				}
				else
				{
					ActorEntry.Value.AddToScene(InDatasmithScene, null, false);
				}
			}

			// Add the collected master materials from the material data dictionary to the Datasmith scene.
			AddCollectedMaterials(InDatasmithScene, UniqueTextureNameSet);
		}

		public void LogElement(
			FDatasmithFacadeLog InDebugLog,
			string InLinePrefix,
			int InLineIndentation
		)
		{
			ElementDataStack.Peek().Log(InDebugLog, InLinePrefix, InLineIndentation);
		}

		public void LogMaterial(
			MaterialNode InMaterialNode,
			FDatasmithFacadeLog InDebugLog,
			string InLinePrefix
		)
		{
			if (MaterialDataMap.ContainsKey(CurrentMaterialName))
			{
				MaterialDataMap[CurrentMaterialName].Log(InMaterialNode, InDebugLog, InLinePrefix);
			}
		}

		private void AddSiteLocation(
			SiteLocation InSiteLocation
		)
		{
			if (InSiteLocation == null || !InSiteLocation.IsValidObject)
			{
				return;
			}

			FDatasmithFacadeActor SiteLocationActor = null;
			FBaseElementData ElementData = null;

			DirectLink?.MarkForExport(InSiteLocation);

			if (DirectLink?.IsElementCached(InSiteLocation) ?? false)
			{
				if (!DirectLink.IsElementModified(InSiteLocation))
				{
					return;
				}

				ElementData = DirectLink.GetCachedElement(InSiteLocation);
				SiteLocationActor = ElementData.ElementActor;
				SiteLocationActor.ResetTags();
			}
			else
			{
				// Create a new Datasmith placeholder actor for the site location.
				// Hash the Datasmith placeholder actor name to shorten it.
				string NameHash = FDatasmithFacadeElement.GetStringHash("SiteLocation");
				SiteLocationActor = new FDatasmithFacadeActor(NameHash);
				SiteLocationActor.SetLabel("Site Location");
			}

			// Set the Datasmith placeholder actor layer to the site location category name.
			SiteLocationActor.SetLayer(InSiteLocation.Category.Name);

			// Add the Revit element ID and Unique ID tags to the Datasmith placeholder actor.
			SiteLocationActor.AddTag($"Revit.Element.Id.{InSiteLocation.Id.IntegerValue}");
			SiteLocationActor.AddTag($"Revit.Element.UniqueId.{InSiteLocation.UniqueId}");

			// Add a Revit element site location tag to the Datasmith placeholder actor.
			SiteLocationActor.AddTag("Revit.Element.SiteLocation");

			FDatasmithFacadeMetaData SiteLocationMetaData = new FDatasmithFacadeMetaData(SiteLocationActor.GetName() + "_DATA");
			SiteLocationMetaData.SetLabel(SiteLocationActor.GetLabel());
			SiteLocationMetaData.SetAssociatedElement(SiteLocationActor);

			// Add site location metadata to the Datasmith placeholder actor.
			const double RadiansToDegrees = 180.0 / Math.PI;
			SiteLocationMetaData.AddPropertyFloat("SiteLocation*Latitude", (float)(InSiteLocation.Latitude * RadiansToDegrees));
			SiteLocationMetaData.AddPropertyFloat("SiteLocation*Longitude", (float)(InSiteLocation.Longitude * RadiansToDegrees));
			SiteLocationMetaData.AddPropertyFloat("SiteLocation*Elevation", (float)InSiteLocation.Elevation);
			SiteLocationMetaData.AddPropertyFloat("SiteLocation*TimeZone", (float)InSiteLocation.TimeZone);
			SiteLocationMetaData.AddPropertyString("SiteLocation*Place", InSiteLocation.PlaceName);

			// Collect the site location placeholder actor into the Datasmith actor dictionary.
			if (ElementData == null)
			{
				ElementData = new FBaseElementData(SiteLocationActor, null, this);
				// Prevent the Datasmith placeholder actor from being removed by optimization.
				ElementData.bOptimizeHierarchy = false;
			}
			else
			{
				ElementData.ElementMetaData = SiteLocationMetaData;
			}

			ActorMap[InSiteLocation.Id] = ElementData;

			DirectLink?.CacheElement(CurrentDocument, InSiteLocation, ElementData);
		}

		private void AddPointLocations(
			Transform InWorldTransform
		)
		{
			FilteredElementCollector Collector = new FilteredElementCollector(CurrentDocument);
			ICollection<Element> PointLocations = Collector.OfClass(typeof(BasePoint)).ToElements();

			foreach (Element PointLocation in PointLocations)
			{
				BasePoint BasePointLocation = PointLocation as BasePoint;

				if (BasePointLocation != null)
				{
					// Since BasePoint.Location is not a location point we cannot get a position from it; so we use a bounding box approach.
					// Note that, as of Revit 2020, BasePoint has 2 new properties: Position for base point and SharedPosition for survey point.
					BoundingBoxXYZ BasePointBoundingBox = BasePointLocation.get_BoundingBox(CurrentDocument.ActiveView);
					if (BasePointBoundingBox == null)
					{
						continue;
					}

					string ActorName = BasePointLocation.IsShared ? "SurveyPoint" : "BasePoint";
					string ActorLabel = BasePointLocation.IsShared ? "Survey Point" : "Base Point";

					FDatasmithFacadeActor BasePointActor = null;
					FBaseElementData BasePointElement = null;

					DirectLink?.MarkForExport(PointLocation);

					if (DirectLink?.IsElementCached(PointLocation) ?? false)
					{
						if (!DirectLink.IsElementModified(PointLocation))
						{
							continue;
						}

						BasePointElement = DirectLink.GetCachedElement(PointLocation);
						BasePointActor = BasePointElement.ElementActor;
						BasePointActor.ResetTags();
					}
					else
					{
						// Create a new Datasmith placeholder actor for the base point.
						// Hash the Datasmith placeholder actor name to shorten it.
						string HashedActorName = FDatasmithFacadeElement.GetStringHash(ActorName);
						BasePointActor = new FDatasmithFacadeActor(HashedActorName);
						BasePointActor.SetLabel(ActorLabel);
					}

					// Set the world transform of the Datasmith placeholder actor.
					XYZ BasePointPosition = BasePointBoundingBox.Min;

					if (BasePointLocation.IsShared)
					{
						ProjectSurveyPoint = BasePointPosition;
					}
					else
					{
						ProjectBasePoint = BasePointPosition;
					}

					Transform TranslationMatrix = Transform.CreateTranslation(BasePointPosition);
					// Don't apply offset since basepoints aren't yet initialized
					SetActorTransform(TranslationMatrix.Multiply(InWorldTransform), BasePointActor, false);


					// Set the Datasmith placeholder actor layer to the base point category name.
					BasePointActor.SetLayer(BasePointLocation.Category.Name);

					// Add the Revit element ID and Unique ID tags to the Datasmith placeholder actor.
					BasePointActor.AddTag($"Revit.Element.Id.{BasePointLocation.Id.IntegerValue}");
					BasePointActor.AddTag($"Revit.Element.UniqueId.{BasePointLocation.UniqueId}");

					// Add a Revit element base point tag to the Datasmith placeholder actor.
					BasePointActor.AddTag("Revit.Element." + ActorName);

					// Add base point metadata to the Datasmith actor.
					string MetadataPrefix = BasePointLocation.IsShared ? "SurveyPointLocation*" : "BasePointLocation*";

					FDatasmithFacadeMetaData BasePointMetaData = new FDatasmithFacadeMetaData(BasePointActor.GetName() + "_DATA");
					BasePointMetaData.SetLabel(BasePointActor.GetLabel());
					BasePointMetaData.SetAssociatedElement(BasePointActor);

					BasePointMetaData.AddPropertyVector(MetadataPrefix + "Location", $"{BasePointPosition.X} {BasePointPosition.Y} {BasePointPosition.Z}");

					if (!bSkipMetadataExport)
					{
						FUtils.AddActorMetadata(BasePointLocation, MetadataPrefix, BasePointMetaData, CurrentSettings);
					}

					if (BasePointElement == null)
					{
						// Collect the base point placeholder actor into the Datasmith actor dictionary.
						BasePointElement = new FBaseElementData(BasePointActor, BasePointMetaData, this);
						BasePointElement.bOptimizeHierarchy = false;
					}
					else
					{
						BasePointElement.ElementMetaData = BasePointMetaData;
					}

					ActorMap[BasePointLocation.Id] = BasePointElement;

					DirectLink?.CacheElement(CurrentDocument, PointLocation, BasePointElement);
				}
			}
		}

		private void CollectMesh(
			FDatasmithFacadeMesh InMesh,
			FDatasmithFacadeMeshElement InMeshElement,
			FDatasmithFacadeScene InDatasmithScene
		)
		{
			if (InDatasmithScene != null && InMesh.GetVerticesCount() > 0 && InMesh.GetFacesCount() > 0)
			{
				string MeshName = InMesh.GetName();

				if (!MeshMap.ContainsKey(MeshName))
				{
					// Export the DatasmithMesh in a task while we parse the rest of the document.
					// The task result indicates if the export was successful and if the associated FDatasmithFacadeMeshElement can be added to the scene.
					MeshMap[MeshName] = new Tuple<FDatasmithFacadeMeshElement, Task<bool>>(InMeshElement, Task.Run<bool>(() => InDatasmithScene.ExportDatasmithMesh(InMeshElement, InMesh)));
				}
			}
		}

		private void CollectMesh(
			FDatasmithPolymesh InPolymesh,
			FDatasmithFacadeMeshElement InMeshElement,
			FDatasmithFacadeScene InDatasmithScene
		)
		{
			if (InDatasmithScene != null && InPolymesh.Vertices.Count > 0 && InPolymesh.Faces.Count > 0)
			{
				string MeshName = InMeshElement.GetName();

				if (!MeshMap.ContainsKey(MeshName))
				{
					// Export the DatasmithMesh in a task while we parse the rest of the document.
					// The task result indicates if the export was successful and if the associated FDatasmithFacadeMeshElement can be added to the scene.
					MeshMap[MeshName] = new Tuple<FDatasmithFacadeMeshElement, Task<bool>>(InMeshElement, Task.Run<bool>(
						() =>
						{
							using (FDatasmithFacadeMesh DatasmithMesh = ParsePolymesh(InPolymesh, MeshName))
							{
								return InDatasmithScene.ExportDatasmithMesh(InMeshElement, DatasmithMesh);
							}
						}
					));
				}
			}
		}

		private FDatasmithFacadeMesh ParsePolymesh(FDatasmithPolymesh InPolymesh, string MeshName)
		{
			FDatasmithFacadeMesh DatasmithMesh = new FDatasmithFacadeMesh();
			DatasmithMesh.SetName(MeshName);
			DatasmithMesh.SetVerticesCount(InPolymesh.Vertices.Count);
			DatasmithMesh.SetFacesCount(InPolymesh.Faces.Count);
			DatasmithMesh.SetUVChannelsCount(1);
			DatasmithMesh.SetUVCount(0, InPolymesh.UVs.Count);
			const int UVChannelIndex = 0;

			// Add the vertex points (in right-handed Z-up coordinates) to the Datasmith mesh.
			for (int VertexIndex = 0; VertexIndex < InPolymesh.Vertices.Count; ++VertexIndex)
			{
				XYZ Point = InPolymesh.Vertices[VertexIndex];
				DatasmithMesh.SetVertex(VertexIndex, (float)Point.X, (float)Point.Y, (float)Point.Z);
			}

			// Add the vertex UV texture coordinates to the Datasmith mesh.
			for (int UVIndex = 0; UVIndex < InPolymesh.UVs.Count; ++UVIndex)
			{
				UV CurrentUV = InPolymesh.UVs[UVIndex];
				DatasmithMesh.SetUV(UVChannelIndex, UVIndex, CurrentUV.U, CurrentUV.V);
			}

			// Add the triangle vertex indexes to the Datasmith mesh.
			for (int FacetIndex = 0; FacetIndex < InPolymesh.Faces.Count; ++FacetIndex)
			{
				FDocumentData.FPolymeshFace Face = InPolymesh.Faces[FacetIndex];
				DatasmithMesh.SetFace(FacetIndex, Face.V1, Face.V2, Face.V3, Face.MaterialIndex);
				DatasmithMesh.SetFaceUV(FacetIndex, UVChannelIndex, Face.V1, Face.V2, Face.V3);
			}

			for (int NormalIndex = 0; NormalIndex < InPolymesh.Normals.Count; ++NormalIndex)
			{
				XYZ Normal = InPolymesh.Normals[NormalIndex];
				DatasmithMesh.SetNormal(NormalIndex, (float)Normal.X, (float)Normal.Y, (float)Normal.Z);
			}

			return DatasmithMesh;
		}

		private void AddCollectedDecals(FDatasmithFacadeScene InDatasmithScene)
		{
			foreach (KeyValuePair<ElementId, FDecalMaterial> DecalMaterialPair in DecalMaterialsMap)
			{
				FElementData DecalElement = null;
				if (!DecalElementsMap.TryGetValue(DecalMaterialPair.Key, out DecalElement))
				{
					continue;
				}

				FDecalMaterial DecalMaterial = DecalMaterialPair.Value;

				FDatasmithFacadeMaterialInstance DatasmithMaterial = new FDatasmithFacadeMaterialInstance(DecalMaterial.MaterialName);
				DatasmithMaterial.SetMaterialType(FDatasmithFacadeMaterialInstance.EMaterialInstanceType.Decal);

				if (!string.IsNullOrEmpty(DecalMaterial.DiffuseTexturePath))
				{
					FDatasmithFacadeTexture DiffuseTexture = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(DecalMaterial.DiffuseTexturePath);
					DiffuseTexture.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
					DiffuseTexture.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Diffuse);
					DiffuseTexture.SetFile(DecalMaterial.DiffuseTexturePath);

					DatasmithMaterial.AddTexture("ColorMap", DiffuseTexture);

					InDatasmithScene.AddTexture(DiffuseTexture);
				}

				if (!string.IsNullOrEmpty(DecalMaterial.BumpTexturePath))
				{
					FDatasmithFacadeTexture BumpTexture = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(DecalMaterial.BumpTexturePath);
					BumpTexture.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
					BumpTexture.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Bump);
					BumpTexture.SetFile(DecalMaterial.BumpTexturePath);

					DatasmithMaterial.AddTexture("NormalMap", BumpTexture);
					DatasmithMaterial.AddFloat("NormalMapAmount", (float)DecalMaterial.BumpAmount);

					InDatasmithScene.AddTexture(BumpTexture);
				}

				if (!string.IsNullOrEmpty(DecalMaterial.CutoutTexturePath))
				{
					FDatasmithFacadeTexture CutoutTexture = FDatasmithFacadeMaterialsUtils.CreateSimpleTextureElement(DecalMaterial.CutoutTexturePath);
					CutoutTexture.SetSRGB(FDatasmithFacadeTexture.EColorSpace.sRGB);
					CutoutTexture.SetTextureMode(FDatasmithFacadeTexture.ETextureMode.Other);
					CutoutTexture.SetFile(DecalMaterial.CutoutTexturePath);

					DatasmithMaterial.AddBoolean("UseCustomOpacityMap", true);
					DatasmithMaterial.AddTexture("OpacityMap", CutoutTexture);

					InDatasmithScene.AddTexture(CutoutTexture);
				}

				if (DecalMaterial.Luminance > 0f)
				{
					DatasmithMaterial.AddFloat("LuminanceAmount", (float)DecalMaterial.Luminance);
				}

				if (DecalMaterial.Transparency > 0f)
				{
					DatasmithMaterial.AddFloat("Opacity", (float)DecalMaterial.Transparency);
				}

				InDatasmithScene.AddMaterial(DatasmithMaterial);

				Transform DecalTransform = null;
				XYZ DecalDimensions = null;
				FUtils.GetDecalSpatialParams(DecalElement.CurrentElement, ref DecalTransform, ref DecalDimensions);

				if (DecalTransform == null || DecalDimensions == null)
				{
					continue;
				}

				FDatasmithFacadeActorDecal DecalActor = DecalElement.ElementActor as FDatasmithFacadeActorDecal;
				DecalActor.SetDimensions(DecalDimensions.Z, DecalDimensions.X, DecalDimensions.Y);
				DecalActor.SetDecalMaterialPathName(DatasmithMaterial.GetName());

				SetActorTransform(DecalTransform, DecalActor);
			}
		}

		private void AddCollectedMeshes(
			FDatasmithFacadeScene InDatasmithScene
		)
		{
			// Add the collected meshes from the Datasmith mesh dictionary to the Datasmith scene.
			foreach (var MeshElementExportResultTuple in MeshMap.Values)
			{
				// Wait for the export to complete and add the Mesh element on success.
				if (MeshElementExportResultTuple.Item2.Result)
				{
					InDatasmithScene.AddMesh(MeshElementExportResultTuple.Item1);
				}
			}
		}

		private void AddHostHierarchy()
		{
			AddParentElementHierarchy(GetHostElement);
		}

		private void AddLevelHierarchy()
		{
			AddParentElementHierarchy(GetLevelElement);
		}

		private void AddCollectedMaterials(
			FDatasmithFacadeScene InDatasmithScene,
			HashSet<string> UniqueTextureNameSet
		)
		{
			UniqueTextureNameSet = (DirectLink != null) ? DirectLink.UniqueTextureNameSet : UniqueTextureNameSet;

			// Add the collected master materials from the material data dictionary to the Datasmith scene.
			foreach (FMaterialData CollectedMaterialData in NewMaterialsMap.Values)
			{
				InDatasmithScene.AddMaterial(CollectedMaterialData.MaterialInstance);

				foreach(FDatasmithFacadeTexture CurrentTexture in CollectedMaterialData.CollectedTextures)
				{
					string TextureName = CurrentTexture.GetName();
					if (!UniqueTextureNameSet.Contains(TextureName))
					{
						UniqueTextureNameSet.Add(TextureName);
						InDatasmithScene.AddTexture(CurrentTexture);
					}
				}

				if (CollectedMaterialData.MessageList.Count > 0)
				{
					MessageList.AddRange(CollectedMaterialData.MessageList);
				}
			}
		}

		private Element GetHostElement(
			ElementId InElementId
		)
		{
			Element SourceElement = CurrentDocument.GetElement(InElementId);
			Element HostElement = null;

			if (SourceElement as FamilyInstance != null)
			{
				HostElement = (SourceElement as FamilyInstance).Host;
			}
			else if (SourceElement as Wall != null)
			{
				HostElement = CurrentDocument.GetElement((SourceElement as Wall).StackedWallOwnerId);
			}
			else if (SourceElement as ContinuousRail != null)
			{
				HostElement = CurrentDocument.GetElement((SourceElement as ContinuousRail).HostRailingId);
			}
			else if (SourceElement.GetType().IsSubclassOf(typeof(InsulationLiningBase)))
			{
				HostElement = CurrentDocument.GetElement((SourceElement as InsulationLiningBase).HostElementId);
			}

			// DirectLink: if host is hidden, go up the hierarchy (NOTE this does not apply for linked documents)
			if (DirectLink != null &&
				HostElement != null &&
				CurrentDocument.ActiveView != null &&
				HostElement.IsHidden(CurrentDocument.ActiveView))
			{
				return GetHostElement(HostElement.Id);
			}

			return HostElement;
		}

		private Element GetLevelElement(
			ElementId InElementId
		)
		{
			Element SourceElement = CurrentDocument.GetElement(InElementId);

			return (SourceElement == null) ? null : CurrentDocument.GetElement(SourceElement.LevelId);
		}

		private void AddParentElementHierarchy(
			Func<ElementId, Element> InGetParentElement
		)
		{
			Context.LogDebug("AddParentElementHierarchy");
			Queue<ElementId> ElementIdQueue = new Queue<ElementId>(ActorMap.Keys);

			// Make sure the Datasmith actor dictionary contains actors for all the Revit parent elements.
			while (ElementIdQueue.Count > 0)
			{
				Element ParentElement = InGetParentElement(ElementIdQueue.Dequeue());

				if (ParentElement == null)
				{
					continue;
				}

				ElementId ParentElementId = ParentElement.Id;

				if (ActorMap.ContainsKey(ParentElementId))
				{
					continue;
				}

				if (DirectLink?.IsElementCached(ParentElement) ?? false)
				{
					// Move parent actor out of cache.
					DirectLink.MarkForExport(ParentElement);
					ActorMap[ParentElementId] =  DirectLink.GetCachedElement(ParentElement);
				}
				else
				{
					Context.LogDebug("  Add element");
					const FDatasmithFacadeScene NullScene = null;
					PushElement(ParentElement, Transform.Identity);
					PopElement(NullScene);
				}

				ElementIdQueue.Enqueue(ParentElementId);
			}

			// Add the parented actors as children of the parent Datasmith actors.
			foreach (ElementId ElemId in new List<ElementId>(ActorMap.Keys))
			{
				Element ParentElement = InGetParentElement(ElemId);

				if (ParentElement == null)
				{
					continue;
				}

				Element SourceElement = CurrentDocument.GetElement(ElemId);

				if ((SourceElement as FamilyInstance != null && ParentElement as Truss != null) ||
					(SourceElement as Mullion != null) ||
					(SourceElement as Panel != null) ||
					(SourceElement as ContinuousRail != null))
				{
					// The Datasmith actor is a component in the hierarchy.
					ActorMap[ElemId].ElementActor.SetIsComponent(true);
				}

				ElementId ParentElementId = ParentElement.Id;

				// Add the parented actor as child of the parent Datasmith actor.

				FBaseElementData ElementData = ActorMap[ElemId];
				FBaseElementData ParentElementData = ActorMap[ParentElementId];

				if (!ParentElementData.ChildElements.Contains(ElementData))
				{
					ParentElementData.ChildElements.Add(ElementData);
					ElementData.Parent = ParentElementData;
				}

				// Prevent the parent Datasmith actor from being removed by optimization.
				ParentElementData.bOptimizeHierarchy = false;
			}

			// Remove the parented child actors from the Datasmith actor dictionary.
			foreach (ElementId ElemId in new List<ElementId>(ActorMap.Keys))
			{
				Element ParentElement = InGetParentElement(ElemId);

				if (ParentElement == null)
				{
					continue;
				}

				// Remove the parented child actor from the Datasmith actor dictionary.
				ActorMap.Remove(ElemId);
			}
		}

		private void SetActorTransform(
			Transform InWorldTransform,
			FDatasmithFacadeActor IOActor,
			bool bInApplyOffset = true
		)
		{
			XYZ transformBasisX = InWorldTransform.BasisX;
			XYZ transformBasisY = InWorldTransform.BasisY;
			XYZ transformBasisZ = InWorldTransform.BasisZ;
			XYZ transformOrigin = InWorldTransform.Origin;

			// Check if need to apply world offset to element transform
			if (bInApplyOffset && InsertionPoint != FSettings.EInsertionPoint.Default)
			{
				switch (InsertionPoint)
				{
					case FSettings.EInsertionPoint.BasePoint:
					{
						if (ProjectBasePoint != null)
						{
							transformOrigin -= ProjectBasePoint;
						}
					}
					break;

					case FSettings.EInsertionPoint.SurveyPoint:
					{
						if (ProjectSurveyPoint != null)
						{
							transformOrigin -= ProjectSurveyPoint;
						}
					}
					break;
				}
			}

			double[] worldMatrix = new double[16];

			worldMatrix[0] = transformBasisX.X;
			worldMatrix[1] = transformBasisX.Y;
			worldMatrix[2] = transformBasisX.Z;
			worldMatrix[3] = 0.0;
			worldMatrix[4] = transformBasisY.X;
			worldMatrix[5] = transformBasisY.Y;
			worldMatrix[6] = transformBasisY.Z;
			worldMatrix[7] = 0.0;
			worldMatrix[8] = transformBasisZ.X;
			worldMatrix[9] = transformBasisZ.Y;
			worldMatrix[10] = transformBasisZ.Z;
			worldMatrix[11] = 0.0;
			worldMatrix[12] = transformOrigin.X;
			worldMatrix[13] = transformOrigin.Y;
			worldMatrix[14] = transformOrigin.Z;
			worldMatrix[15] = 1.0;

			// Set the world transform of the Datasmith actor.
			IOActor.SetWorldTransform(worldMatrix);
		}

		/// <summary>
		/// Combine whole element stack in the document hierarchy to build unique path to the element in the document
		/// </summary>
		public string GetElementStackName()
		{
			return string.Join(", ", ElementDataStack.Select(Data => $"{Data.CurrentElement.UniqueId}"));
		}

	}
}
