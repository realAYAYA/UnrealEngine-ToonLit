// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;
using System.Linq;
using System.Collections.Concurrent;
using DatasmithSolidworks.Names;

namespace DatasmithSolidworks
{
	public class FObjectMaterials
	{
		// Note: part document might be a different document than the owner one:
		// for example, when loading component materials, owner will be the document 
		// that component resides in, while part doc will be the document that component references!
		public PartDoc PartDocument {  get; private set; }
		public FDocumentTracker OwnerDoc {  get; private set; }

		public int ComponentMaterialID { get; private set; } = -1;
		public int PartMaterialID { get; private set; } = -1;

		public Dictionary<string, int> BodyMaterialsMap { get; private set; } = new Dictionary<string, int>();
		public Dictionary<string, int> FaceMaterialsMap { get; private set; } = new Dictionary<string, int>();
		public Dictionary<string, int> FeatureMaterialsMap { get; private set; } = new Dictionary<string, int>();

		public ConcurrentDictionary<int, FMaterial> GlobalMaterialsMap { get; private set; } = null;

		public FObjectMaterials(FDocumentTracker InOwnerDoc, PartDoc InPartDocument, ref ConcurrentDictionary<int, FMaterial> InOutMaterialsMap)
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

		public override string ToString()
		{
			return $"FObjectMaterials:ComponentMaterialID='{ComponentMaterialID}', PartMaterialID='{PartMaterialID}', BodyMaterialsMap='{ToString(BodyMaterialsMap)}', FeatureMaterialsMap='{ToString(FeatureMaterialsMap)}', FaceMaterialsMap='{ToString(FaceMaterialsMap)}'";
		}

		private static string ToString(Dictionary<string, int> Map)
		{
			return string.Join(", ", Map.Select(KVP => $"{KVP.Key}:{KVP.Value}" ));
		}

		public FMaterial GetMaterial(Face2 InFace)
		{
			if (ComponentMaterialID != -1) // Highest priority
			{
				return GetMaterial(ComponentMaterialID);
			}

			uint FaceId = unchecked((uint)(InFace?.GetFaceId() ?? 0));
			if (FDocumentTracker.IsValidFaceId(FaceId) && FaceMaterialsMap.ContainsKey(FaceId.ToString()))
			{
				int MatId = FaceMaterialsMap[FaceId.ToString()];
				return GetMaterial(MatId);
			}

			// Face does not have material assigned, try one level up
			Feature Feat = InFace?.GetFeature();
			Body2 Body = InFace?.GetBody();

			if (Feat != null)
			{
				string FeatId = FPartDocumentTracker.GetFeaturePath(PartDocument, Feat);
				if (FeatureMaterialsMap.ContainsKey(FeatId))
				{
					return GetMaterial(FeatureMaterialsMap[FeatId]);
				}
			}

			if (Body != null)
			{
				string BodyId = FPartDocumentTracker.GetBodyPath(PartDocument, Body);
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

		public static FObjectMaterials LoadPartMaterials(FDocumentTracker InOwnerDoc, PartDoc InPartDoc, swDisplayStateOpts_e InDisplayState, string[] InDisplayStateNames)
		{
			Addin.Instance.LogDebug($"LoadPartMaterials: '{(InPartDoc as ModelDoc2).GetPathName()}', {InDisplayState}, {(InDisplayStateNames == null ? "<null>" : string.Join(", ", InDisplayStateNames))}");

			ModelDoc2 Doc = InPartDoc as ModelDoc2;

			if (Doc == null)
			{
				return null;
			}

			IModelDocExtension Ext = Doc.Extension;
			int NumMaterials = Ext.GetRenderMaterialsCount2((int)InDisplayState, InDisplayStateNames);

			Addin.Instance.LogDebug($"    GetRenderMaterialsCount2 -> {NumMaterials}");

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
				Addin.Instance.LogDebug($"  FileName: {RenderMat.FileName}");
				Addin.Instance.LogDebug($"    Users({NumUsers}):");

				if (NumUsers == 0)
				{
					continue;
				}

				object[] ObjUsers = RenderMat.GetEntities();
				foreach (object ObjUser in ObjUsers)
				{
					switch (ObjUser)
					{
						case IPartDoc _:
							Addin.Instance.LogDebug($"    PartDoc '{(ObjUser as ModelDoc2).GetPathName()}'");
							PartMaterials.SetPartMaterial(RenderMat, Doc);
							continue;
						case IBody2 Body:
						{
							Addin.Instance.LogDebug($"    Body '{Body.Name}'");

							string BodyId = FPartDocumentTracker.GetBodyPath(InPartDoc as PartDoc, Body);
							PartMaterials.RegisterMaterial(PartMaterials.BodyMaterialsMap, Doc, RenderMat, BodyId);
							continue;
						}
						case IFace2 Face:
						{
							uint FaceId = PartMaterials.OwnerDoc.GetFaceId(Face);
							Addin.Instance.LogDebug($"    Face '{Face.GetFaceId()}'");
							PartMaterials.RegisterMaterial(PartMaterials.FaceMaterialsMap, Doc, RenderMat, FaceId.ToString());
							continue;
						}
						case IFeature Feat:
						{
							Addin.Instance.LogDebug($"    Feature '{Feat.Name}'");
							string FeatureId = FPartDocumentTracker.GetFeaturePath(InPartDoc as PartDoc, Feat);
							PartMaterials.RegisterMaterial(PartMaterials.FeatureMaterialsMap, Doc, RenderMat, FeatureId);
							continue;
						}
						default:
						{
							Addin.Instance.LogDebug($"    <unknown>'");
							break;
						}
					}
				}
			}

			// Parse Part entities material properties to workaround issue with SW api
			//
			// Solidworks API gives away render materials IModelDocExtension.GetRenderMaterialsCount2
			// and entities these materials are assigned to are retrieved using RenderMaterial.GetEntities[Count] method
			// But this has a flaw that when a Part has multiple configurationa those entities returned are only for the configuration
			// that was active when this Part file was saved. Not matter if we switch configuration - still same list of entities returned.
			// E.g. in Config C0, feature F0 was visible and assigned material M0, in config C1, feature F0 is not visible
			// but rather feature F1  is revealed and has this material M0. If model was saved with C0 active
			// M0.GetEntities returns only F0, even if we switch to C1 after part is loaded
			// But get_DisplayStateSpecMaterialPropertyValues seems to return proper material properties for proper(i.e. visible in currently active config/display state) features
			{
				DisplayStateSetting swDSS = Ext.GetDisplayStateSetting((int)swDisplayStateOpts_e.swThisDisplayState);
				swDSS.Option = (int)swDisplayStateOpts_e.swThisDisplayState;

				// Check Bodies
				object[] BodiesArray = InPartDoc.GetBodies((int)swBodyType_e.swSolidBody);
				if (BodiesArray != null)
				{
					swDSS.Entities = BodiesArray.Cast<Body2>().ToArray();
					// Get appearances for entities array
					object[] Appearances = (object[])Ext.get_DisplayStateSpecMaterialPropertyValues(swDSS);

					for (int SettingIndex = 0; SettingIndex < Appearances.Length; SettingIndex++)
					{
						Body2 Body = swDSS.Entities[SettingIndex];
						AppearanceSetting Appearance = Appearances[SettingIndex] as AppearanceSetting;
						
						string BodyId = FPartDocumentTracker.GetBodyPath(InPartDoc as PartDoc, Body);
						if (PartMaterials.BodyMaterialsMap.TryGetValue(BodyId, out int AssignedMaterialId))
						{
							Addin.Instance.LogDebug($"  Body '{Body.Name}' already has material assigned {PartMaterials.GetMaterial(AssignedMaterialId).Name}");
							continue;
						}

						Addin.Instance.LogDebug($"  Body '{Body.Name}' has no material assigned, searching material matching its Appearance..");

						foreach (KeyValuePair<int, FMaterial> KVP in PartMaterials.GlobalMaterialsMap)
						{
							FMaterial Material = KVP.Value;

							if (Material.EqualsAppearance(Appearance))
							{
								Addin.Instance.LogDebug($"Matching material for Body '{Body.Name}'  is {Material.Name}");

								PartMaterials.BodyMaterialsMap[BodyId] = KVP.Key; 
								break;
							}
						}
					}
				}

				// Check Features
				Stack<Feature> Features = new Stack<Feature>();

				// Collect features
				{
					Feature Feat = InPartDoc.FirstFeature();
					while (Feat != null)
					{
						Features.Push(Feat);

						Feature SubFeature = Feat.GetFirstSubFeature();
						while (SubFeature != null)
						{
							Features.Push(SubFeature);
							SubFeature = SubFeature.GetNextSubFeature();
						}

						Feat = Feat.GetNextFeature();
					}
				}

				if (Features.Count > 0)
				{
					swDSS.Entities = Features.ToArray();

					// Get appearances for entities array
					object[] Appearances = (object[])Ext.get_DisplayStateSpecMaterialPropertyValues(swDSS);

					for (int SettingIndex = 0; SettingIndex < Appearances.Length; SettingIndex++)
					{
						Feature Feat = swDSS.Entities[SettingIndex];
						AppearanceSetting Appearance = Appearances[SettingIndex] as AppearanceSetting;
						
						string FeatureId = FPartDocumentTracker.GetFeaturePath(InPartDoc as PartDoc, Feat);
						if (PartMaterials.FeatureMaterialsMap.TryGetValue(FeatureId, out int AssignedMaterialId))
						{
							Addin.Instance.LogDebug($"  Feature '{Feat.Name}' already has material assigned {PartMaterials.GetMaterial(AssignedMaterialId).Name}");
							continue;
						}

						Addin.Instance.LogDebug($"  Feature '{Feat.Name}' has no material assigned, searching material matching its Appearance..");

						foreach (KeyValuePair<int, FMaterial> KVP in PartMaterials.GlobalMaterialsMap)
						{
							FMaterial Material = KVP.Value;

							if (Material.EqualsAppearance(Appearance))
							{
								Addin.Instance.LogDebug($"Matching material for Feature '{Feat.Name}'  is {Material.Name}");

								PartMaterials.FeatureMaterialsMap[FeatureId] = KVP.Key; 
								break;
							}
						}
					}
				}
			}			

			return PartMaterials;
		}

		public static FObjectMaterials LoadComponentMaterials(FDocumentTracker InComponentOwner, Component2 InComponent, swDisplayStateOpts_e InDisplayState, string[] InDisplayStateNames)
		{
			Addin.Instance.LogDebug($"LoadComponentMaterials: '{InComponent.Name2}', '{InComponentOwner.GetPathName()}', {InDisplayState}, {(InDisplayStateNames == null ? "<null>" : string.Join(", ", InDisplayStateNames))}");

			ModelDoc2 ComponentDoc = InComponent.GetModelDoc2() as ModelDoc2;

			if (ComponentDoc == null || !(ComponentDoc is PartDoc))
			{
				// Component's model doc might be null if component is suppressed or lightweight (in which case we treat it as hidden)
				return null;
			}

			Addin.Instance.LogDebug($"  GetRenderMaterialsCount2({InDisplayState}, {InDisplayStateNames})");
			int NumMaterials = InComponent.GetRenderMaterialsCount2((int)InDisplayState, InDisplayStateNames);
			Addin.Instance.LogDebug($"    GetRenderMaterialsCount2 -> {NumMaterials}");

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

				Addin.Instance.LogDebug($"  FileName: {RenderMat.FileName}");
				Addin.Instance.LogDebug($"    Users({NumUsers}):");

				if (NumUsers == 0)
				{
					continue;
				}

				object[] ObjUsers = RenderMat.GetEntities();
				foreach (object ObjUser in ObjUsers)
				{
					switch (ObjUser)
					{
						case IBody2 Body:
						{
							Addin.Instance.LogDebug($"    Body '{Body.Name}'");
							string BodyId = FPartDocumentTracker.GetBodyPath(ComponentDoc as PartDoc, Body);
							ComponentMaterials.RegisterMaterial(ComponentMaterials.BodyMaterialsMap, InComponentOwner.SwDoc, RenderMat, BodyId);
							break;
						}
						case IFace2 Face:
						{
							// note: plugins sets FaceId on each face to identify per-face materials later in CreateMeshData
							uint FaceId = ComponentMaterials.OwnerDoc.GetFaceId(Face);
							Addin.Instance.LogDebug($"    Face '{Face.GetFaceId()}'");
							ComponentMaterials.RegisterMaterial(ComponentMaterials.FaceMaterialsMap, InComponentOwner.SwDoc, RenderMat, FaceId.ToString());
							break;
						}
						case IFeature Feat:
						{
							Addin.Instance.LogDebug($"    Feature '{Feat.Name}'");
							string FeatureId = FPartDocumentTracker.GetFeaturePath(ComponentDoc as PartDoc, Feat);
							ComponentMaterials.RegisterMaterial(ComponentMaterials.FeatureMaterialsMap, InComponentOwner.SwDoc, RenderMat, FeatureId);
							break;
						}
						case IPartDoc Doc:
						{
							Addin.Instance.LogDebug($"    Part Doc '{(Doc as ModelDoc2).GetPathName()}'");
							ComponentMaterials.SetPartMaterial(RenderMat, InComponentOwner.SwDoc);
							break;
						}
						default:
						{
							Addin.Instance.LogDebug($"    <unknown>'");
							break;
						}
					}
				}
			}

			return ComponentMaterials;
		}

		public static ConcurrentDictionary<FComponentName, FObjectMaterials> LoadAssemblyMaterials(FAssemblyDocumentTracker InAsmDoc, HashSet<FComponentName> InComponentsSet, swDisplayStateOpts_e InDisplayState, string[] InDisplayStateNames)
		{
			Addin.Instance.LogDebug($"LoadAssemblyMaterials: {InAsmDoc.GetPathName()}, {InDisplayState}, [{(InDisplayStateNames == null ? "" : string.Join(",", InDisplayStateNames))}]");
			Addin.Instance.LogDebug($"  for components: [{string.Join(", ", InComponentsSet.OrderBy(Name => Name.GetString()))}]");

			IModelDocExtension Ext = InAsmDoc.SwDoc.Extension;
			int NumMaterials = Ext.GetRenderMaterialsCount2((int)InDisplayState, InDisplayStateNames);

			Addin.Instance.LogDebug($"  Assembly materials={NumMaterials}");

			ConcurrentDictionary<FComponentName, FObjectMaterials> DocMaterials = new ConcurrentDictionary<FComponentName, FObjectMaterials>();

			if (NumMaterials > 0)
			{
				object[] ObjMaterials = Ext.GetRenderMaterials2((int)InDisplayState, InDisplayStateNames);

				foreach (object ObjMat in ObjMaterials)
				{
					RenderMaterial RenderMat = ObjMat as RenderMaterial;
					
					Addin.Instance.LogDebug($"    Material: '{RenderMat.FileName}' ");

					int NumUsers = RenderMat.GetEntitiesCount();

					Addin.Instance.LogDebug($"    NumUsers: '{NumUsers}' ");

					if (NumUsers == 0)
					{
						continue;
					}

					object[] ObjUsers = RenderMat.GetEntities();
					foreach (object ObjUser in ObjUsers)
					{
						if (ObjUser is IEntity Entity)
						{
							Addin.Instance.LogDebug($"    User: type '{Entity.GetType()}' ");
							
						}
						
						switch (ObjUser)
						{
							case Component2 Comp:
							{
								Addin.Instance.LogDebug($"      Component '{Comp.Name2}' ");
								if (Comp.GetModelDoc2() is PartDoc CompDoc)
								{
									Addin.Instance.LogDebug($"        registering material for Part Component in DocMaterials");
									FObjectMaterials ComponentMaterials = new FObjectMaterials(InAsmDoc, CompDoc, ref InAsmDoc.ExportedMaterialsMap);

									Addin.Instance.LogIndent();
									ComponentMaterials.SetComponentMaterial(RenderMat, InAsmDoc.SwDoc);
									Addin.Instance.LogDedent();

									DocMaterials[new FComponentName(Comp)] = ComponentMaterials;
								}
								break;
							}
							case IPartDoc Doc:
							{
								Addin.Instance.LogDebug($"      Part Doc at '{(Doc as ModelDoc2).GetPathName()}' ");

								Addin.Instance.LogIndent();
								FObjectMaterials PartMaterials = LoadPartMaterials(InAsmDoc, Doc as PartDoc, InDisplayState, InDisplayStateNames);
								Addin.Instance.LogDedent();

								if (PartMaterials != null)
								{
									// todo: why trying to present PathName as component name?
									//  how it can be a PartDoc in Assembly function
									DocMaterials[FComponentName.FromCustomString((Doc as ModelDoc2).GetPathName())] = PartMaterials;
								}
								break;
							}
						}
					}
				}
			}

			Addin.Instance.LogDebug($"Load Each Component's materials");
			Addin.Instance.LogIndent();
			foreach(FComponentName CompName in InComponentsSet.OrderBy(Name => Name.GetString()))
			{
				Component2 Comp = null;

				Addin.Instance.LogDebug($"Component '{CompName}' - adding DocMaterials");
				Addin.Instance.LogIndent();

				if (InAsmDoc.SyncState.ExportedComponentsMap.TryGetValue(CompName, out Comp))
				{
					if (!DocMaterials.ContainsKey(CompName))
					{
						Addin.Instance.LogDebug($"Adding to DocMaterials");

						Addin.Instance.LogIndent();
						FObjectMaterials ComponentMaterials = FObjectMaterials.LoadComponentMaterials(InAsmDoc, Comp, swDisplayStateOpts_e.swThisDisplayState, null);
						Addin.Instance.LogDedent();

						if (ComponentMaterials == null && Comp.GetModelDoc2() is PartDoc)
						{
							Addin.Instance.LogIndent();
							ComponentMaterials = FObjectMaterials.LoadPartMaterials(InAsmDoc, Comp.GetModelDoc2() as PartDoc, swDisplayStateOpts_e.swThisDisplayState, null);
							Addin.Instance.LogDedent();
						}
						if (ComponentMaterials != null)
						{
							DocMaterials.TryAdd(CompName, ComponentMaterials);
						}
					}
					else
					{
						Addin.Instance.LogDebug($"...already in DocMaterials");
					}
				}
				else
				{
					Addin.Instance.LogDebug($"...not in ExportedComponentsMap");
				}

				Addin.Instance.LogDedent();
			}
			Addin.Instance.LogDedent();

			return DocMaterials.Count > 0 ? DocMaterials : null;
		}
	}
}
