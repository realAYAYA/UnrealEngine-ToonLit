// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.InteropServices;
using System.Collections.Generic;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;
using System.Linq;
using System.Collections.Concurrent;
using System.Threading.Tasks;

namespace DatasmithSolidworks
{
	[ComVisible(false)]
	public class FObjectMaterials
	{
		// Note: part document might be a different document than the owner one:
		// for example, when loading component materials, owner will be the document 
		// that component resides in, while part doc will be the document that component references!
		public PartDoc PartDocument {  get; private set; }
		public FDocument OwnerDoc {  get; private set; }

		public int ComponentMaterialID { get; private set; } = -1;
		public int PartMaterialID { get; private set; } = -1;

		public Dictionary<string, int> BodyMaterialsMap { get; private set; } = new Dictionary<string, int>();
		public Dictionary<string, int> FaceMaterialsMap { get; private set; } = new Dictionary<string, int>();
		public Dictionary<string, int> FeatureMaterialsMap { get; private set; } = new Dictionary<string, int>();

		public ConcurrentDictionary<int, FMaterial> GlobalMaterialsMap { get; private set; } = null;

		public FObjectMaterials(FDocument InOwnerDoc, PartDoc InPartDocument, ref ConcurrentDictionary<int, FMaterial> InOutMaterialsMap)
		{
			GlobalMaterialsMap = InOutMaterialsMap;
			OwnerDoc = InOwnerDoc;
			PartDocument = InPartDocument;
		}

		public FMaterial GetMaterial(int InMaterialID)
		{
			FMaterial Result = null;
			if (InMaterialID >= 1 && GlobalMaterialsMap.TryGetValue(InMaterialID, out Result))
			{
				return Result;
			}
			return null;
		}

		public void SetComponentMaterial(RenderMaterial InRenderMat, ModelDoc2 InDoc)
		{
			int MatID = MaterialUtils.GetMaterialID(InRenderMat);
			if (!GlobalMaterialsMap.ContainsKey(MatID))
			{
				GlobalMaterialsMap.TryAdd(MatID, new FMaterial(InRenderMat, InDoc.Extension));
			}
			ComponentMaterialID = MatID;
		}

		public void SetPartMaterial(RenderMaterial InRenderMat, ModelDoc2 InDoc)
		{
			int MatID = MaterialUtils.GetMaterialID(InRenderMat);
			if (!GlobalMaterialsMap.ContainsKey(MatID))
			{
				FMaterial Mat = new FMaterial(InRenderMat, InDoc.Extension);
				GlobalMaterialsMap.TryAdd(MatID, Mat);
			}
			PartMaterialID = MatID;
		}

		public FMaterial GetComponentMaterial()
		{
			return GetMaterial(ComponentMaterialID);
		}

		public FMaterial GetPartMaterial()
		{
			return GetMaterial(PartMaterialID);
		}

		public FMaterial GetMaterial(Face2 InFace)
		{
			if (ComponentMaterialID != -1) // Highest priority
			{
				return GetMaterial(ComponentMaterialID);
			}

			uint FaceId = unchecked((uint)(InFace?.GetFaceId() ?? 0));
			if (FDocument.IsValidFaceId(FaceId) && FaceMaterialsMap.ContainsKey(FaceId.ToString()))
			{
				int MatId = FaceMaterialsMap[FaceId.ToString()];
				return GetMaterial(MatId);
			}

			// Face does not have material assigned, try one level up
			Feature Feat = InFace?.GetFeature();
			Body2 Body = InFace?.GetBody();

			if (Feat != null)
			{
				string FeatId = FPartDocument.GetFeaturePath(PartDocument, Feat);
				if (FeatureMaterialsMap.ContainsKey(FeatId))
				{
					return GetMaterial(FeatureMaterialsMap[FeatId]);
				}
			}

			if (Body != null)
			{
				string BodyId = FPartDocument.GetBodyPath(PartDocument, Body);
				if (BodyMaterialsMap.ContainsKey(BodyId))
				{
					return GetMaterial(BodyMaterialsMap[BodyId]);
				}
			}

			if (PartMaterialID >= 1)
			{
				return GetMaterial(PartMaterialID);
			}

			return null;
		}

		public bool EqualMaterials(FObjectMaterials InOther)
		{
			if (ComponentMaterialID != InOther.ComponentMaterialID)
			{
				return false;
			}
			if (PartMaterialID != InOther.PartMaterialID)
			{
				return false;
			}
			if (!CompareMaterialMaps(BodyMaterialsMap, InOther.BodyMaterialsMap))
			{
				return false;
			}
			if (!CompareMaterialMaps(FeatureMaterialsMap, InOther.FeatureMaterialsMap))
			{
				return false;
			}
			if (!CompareMaterialMaps(FaceMaterialsMap, InOther.FaceMaterialsMap))
			{
				return false;
			}
			return true;
		}

		private void RegisterMaterial(Dictionary<string, int> MaterialsDict, ModelDoc2 InDoc, RenderMaterial RenderMat, string ObjectID)
		{
			int MaterialID = MaterialUtils.GetMaterialID(RenderMat);

			if (!MaterialsDict.ContainsKey(ObjectID))
			{
				MaterialsDict[ObjectID] = MaterialID;
			}

			if (!GlobalMaterialsMap.ContainsKey(MaterialID))
			{
				GlobalMaterialsMap.TryAdd(MaterialID, new FMaterial(RenderMat, InDoc.Extension));
			}
		}

		private static bool CompareMaterialMaps(Dictionary<string, int> InDict1, Dictionary<string, int> InDict2)
		{
			return InDict1.OrderBy(KVP => KVP.Key).SequenceEqual(InDict2.OrderBy(KVP => KVP.Key));
		}

		public static FObjectMaterials LoadPartMaterials(FDocument InOwnerDoc, PartDoc InPartDoc, swDisplayStateOpts_e InDisplayState, string[] InDisplayStateNames)
		{
			ModelDoc2 Doc = InPartDoc as ModelDoc2;
			if (Doc == null)
			{
				return null;
			}

			IModelDocExtension Ext = Doc.Extension;
			int NumMaterials = Ext.GetRenderMaterialsCount2((int)InDisplayState, InDisplayStateNames);
			if (NumMaterials == 0)
			{
				return null;
			}

			object[] ObjMaterials = Ext.GetRenderMaterials2((int)InDisplayState, InDisplayStateNames);

			FObjectMaterials PartMaterials = new FObjectMaterials(InOwnerDoc, InPartDoc, ref InOwnerDoc.ExportedMaterialsMap);

			foreach (object ObjMat in ObjMaterials)
			{
				RenderMaterial RenderMat = ObjMat as RenderMaterial;
				int NumUsers = RenderMat.GetEntitiesCount();

				if (NumUsers == 0)
				{
					continue;
				}

				object[] ObjUsers = RenderMat.GetEntities();
				foreach (object ObjUser in ObjUsers)
				{
					if (ObjUser is IPartDoc)
					{
						PartMaterials.SetPartMaterial(RenderMat, Doc);
						continue;
					}

					if (ObjUser is IBody2 Body)
					{
						string BodyId = FPartDocument.GetBodyPath(InPartDoc as PartDoc, Body);
						PartMaterials.RegisterMaterial(PartMaterials.BodyMaterialsMap, Doc, RenderMat, BodyId);
						continue;
					}

					if (ObjUser is IFace2 Face)
					{
						uint FaceId = PartMaterials.OwnerDoc.GetFaceId(Face);
						PartMaterials.RegisterMaterial(PartMaterials.FaceMaterialsMap, Doc, RenderMat, FaceId.ToString());
						continue;
					}

					if (ObjUser is IFeature Feat)
					{
						string FeatureId = FPartDocument.GetFeaturePath(InPartDoc as PartDoc, Feat);
						PartMaterials.RegisterMaterial(PartMaterials.FeatureMaterialsMap, Doc, RenderMat, FeatureId);
						continue;
					}
				}
			}

			return PartMaterials;
		}

		public static FObjectMaterials LoadComponentMaterials(FDocument InComponentOwner, Component2 InComponent, swDisplayStateOpts_e InDisplayState, string[] InDisplayStateNames)
		{
			ModelDoc2 ComponentDoc = InComponent.GetModelDoc2() as ModelDoc2;
			if (ComponentDoc == null || !(ComponentDoc is PartDoc))
			{
				// Component's model doc might be null if component is suppressed or lightweight (in which case we treat it as hidden)
				return null;
			}

			int NumMaterials = InComponent.GetRenderMaterialsCount2((int)InDisplayState, InDisplayStateNames);
			if (NumMaterials == 0)
			{
				return null;
			}

			object[] ObjMaterials = InComponent.GetRenderMaterials2((int)InDisplayState, InDisplayStateNames);

			FObjectMaterials ComponentMaterials = new FObjectMaterials(InComponentOwner, ComponentDoc as PartDoc, ref InComponentOwner.ExportedMaterialsMap);

			foreach (object ObjMat in ObjMaterials)
			{
				RenderMaterial RenderMat = ObjMat as RenderMaterial;
				int NumUsers = RenderMat.GetEntitiesCount();

				if (NumUsers == 0)
				{
					continue;
				}

				object[] ObjUsers = RenderMat.GetEntities();
				foreach (object ObjUser in ObjUsers)
				{
					if (ObjUser is IBody2 Body)
					{
						string BodyId = FPartDocument.GetBodyPath(ComponentDoc as PartDoc, Body);
						ComponentMaterials.RegisterMaterial(ComponentMaterials.BodyMaterialsMap, InComponentOwner.SwDoc, RenderMat, BodyId);
						continue;
					}

					if (ObjUser is IFace2 Face)
					{
						uint FaceId = ComponentMaterials.OwnerDoc.GetFaceId(Face);
						ComponentMaterials.RegisterMaterial(ComponentMaterials.FaceMaterialsMap, InComponentOwner.SwDoc, RenderMat, FaceId.ToString());
						continue;
					}

					if (ObjUser is IFeature Feat)
					{
						string FeatureId = FPartDocument.GetFeaturePath(ComponentDoc as PartDoc, Feat);
						ComponentMaterials.RegisterMaterial(ComponentMaterials.FeatureMaterialsMap, InComponentOwner.SwDoc, RenderMat, FeatureId);
						continue;
					}
					if (ObjUser is IPartDoc Doc)
					{
						ComponentMaterials.SetPartMaterial(RenderMat, InComponentOwner.SwDoc);
					}
				}
			}

			return ComponentMaterials;
		}

		public static ConcurrentDictionary<FComponentName, FObjectMaterials> LoadAssemblyMaterials(FAssemblyDocument InAsmDoc, HashSet<FComponentName> InComponentsSet, swDisplayStateOpts_e InDisplayState, string[] InDisplayStateNames)
		{
			IModelDocExtension Ext = InAsmDoc.SwDoc.Extension;
			int NumMaterials = Ext.GetRenderMaterialsCount2((int)InDisplayState, InDisplayStateNames);

			ConcurrentDictionary<FComponentName, FObjectMaterials> DocMaterials = new ConcurrentDictionary<FComponentName, FObjectMaterials>();

			if (NumMaterials > 0)
			{
				object[] ObjMaterials = Ext.GetRenderMaterials2((int)InDisplayState, InDisplayStateNames);

				foreach (object ObjMat in ObjMaterials)
				{
					RenderMaterial RenderMat = ObjMat as RenderMaterial;
					int NumUsers = RenderMat.GetEntitiesCount();

					if (NumUsers == 0)
					{
						continue;
					}

					object[] ObjUsers = RenderMat.GetEntities();
					foreach (object ObjUser in ObjUsers)
					{
						if (ObjUser is Component2 Comp)
						{
							PartDoc CompDoc = Comp.GetModelDoc2() as PartDoc;
							if (CompDoc != null)
							{
								FObjectMaterials ComponentMaterials = new FObjectMaterials(InAsmDoc, CompDoc, ref InAsmDoc.ExportedMaterialsMap);
								ComponentMaterials.SetComponentMaterial(RenderMat, InAsmDoc.SwDoc);
								DocMaterials[new FComponentName(Comp)] = ComponentMaterials;
							}
							continue;
						}

						if (ObjUser is IPartDoc Doc)
						{
							FObjectMaterials PartMaterials = LoadPartMaterials(InAsmDoc, Doc as PartDoc, InDisplayState, InDisplayStateNames);
							if (PartMaterials != null)
							{
								// xxx: why trying to present PahtName as component name?
								// xxx: how it can be a PartDoc in Assembly function
								DocMaterials[FComponentName.FromCustomString((Doc as ModelDoc2).GetPathName())] = PartMaterials;
							}
							continue;
						}
					}
				}
			}

			// Check for materials that are not per component (but instead per face, feature, part etc.)
			Parallel.ForEach(InComponentsSet, CompName =>
			{
				Component2 Comp = null;

				if (InAsmDoc.SyncState.ExportedComponentsMap.TryGetValue(CompName, out Comp))
				{
					if (!DocMaterials.ContainsKey(CompName))
					{
						FObjectMaterials ComponentMaterials = FObjectMaterials.LoadComponentMaterials(InAsmDoc, Comp, swDisplayStateOpts_e.swThisDisplayState, null);

						if (ComponentMaterials == null && Comp.GetModelDoc2() is PartDoc)
						{
							ComponentMaterials = FObjectMaterials.LoadPartMaterials(InAsmDoc, Comp.GetModelDoc2() as PartDoc, swDisplayStateOpts_e.swThisDisplayState, null);
						}
						if (ComponentMaterials != null)
						{
							DocMaterials.TryAdd(CompName, ComponentMaterials);
						}
					}
				}
			});

			return DocMaterials.Count > 0 ? DocMaterials : null;
		}
	}
}
