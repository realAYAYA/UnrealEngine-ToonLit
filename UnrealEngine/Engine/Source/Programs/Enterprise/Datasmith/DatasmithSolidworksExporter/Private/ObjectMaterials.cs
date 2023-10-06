// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;
using System.Linq;
using System.Collections.Concurrent;
using DatasmithSolidworks.Names;
using static DatasmithSolidworks.Addin;
using static System.Windows.Forms.VisualStyles.VisualStyleElement;

namespace DatasmithSolidworks
{
	// Contains material assignment structure for a Component in assembly or a Part document
	// Materials(appearances) in SW can be assigned on different levels - document(top or sub-assembly/part), component, body, feature
	// Highest priority has appearance assigned to Component(top-level assembly document). Then  Face, Feature, Body, Part (from highest to lowest priority).
	// see "Appearance Hierarchy" in SW docs
	public class FObjectMaterials
	{
		// Note: part document might be a different document than the owner one:
		// for example, when loading component materials, owner will be the document 
		// that component resides in, while part doc will be the document that component references!
		public PartDoc PartDocument;

		public int ComponentMaterialID { get; private set; } = -1;
		public int PartMaterialID { get; private set; } = -1;

		public Dictionary<string, int> BodyMaterialsMap { get; private set; } = new Dictionary<string, int>();
		public Dictionary<string, int> FaceMaterialsMap { get; private set; } = new Dictionary<string, int>();
		public Dictionary<string, int> FeatureMaterialsMap { get; private set; } = new Dictionary<string, int>();

		private readonly Dictionary<int, FMaterial> GlobalMaterialsMap;  // All collected materials

		public FObjectMaterials(PartDoc InPartDocument, Dictionary<int, FMaterial> InOutMaterialsMap)
		{
			GlobalMaterialsMap = InOutMaterialsMap;
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
				GlobalMaterialsMap[MatID] = new FMaterial(InRenderMat, InDoc.Extension);
			}
			ComponentMaterialID = MatID;
		}

		public void SetPartMaterial(RenderMaterial InRenderMat, ModelDoc2 InDoc)
		{
			int MatID = MaterialUtils.GetMaterialID(InRenderMat);
			if (!GlobalMaterialsMap.ContainsKey(MatID))
			{
				FMaterial Mat = new FMaterial(InRenderMat, InDoc.Extension);
				GlobalMaterialsMap[MatID] = Mat;
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
				GlobalMaterialsMap[MaterialID] = new FMaterial(RenderMat, InDoc.Extension);
			}
		}

		private static bool CompareMaterialMaps(Dictionary<string, int> InDict1, Dictionary<string, int> InDict2)
		{
			return InDict1.OrderBy(KVP => KVP.Key).SequenceEqual(InDict2.OrderBy(KVP => KVP.Key));
		}

		public static FObjectMaterials LoadPartMaterials(FDocumentTracker InOwnerDoc, PartDoc InPartDoc, swDisplayStateOpts_e InDisplayState, string[] InDisplayStateNames, Dictionary<int, FMaterial> MaterialsMap)
		{
			LogDebug($"LoadPartMaterials: '{(InPartDoc as ModelDoc2).GetPathName()}', {InDisplayState}, {(InDisplayStateNames == null ? "<null>" : string.Join(", ", InDisplayStateNames))}");

			ModelDoc2 Doc = InPartDoc as ModelDoc2;

			if (Doc == null)
			{
				return null;
			}

			IModelDocExtension Ext = Doc.Extension;
			int NumMaterials = Ext.GetRenderMaterialsCount2((int)InDisplayState, InDisplayStateNames);

			LogDebug($"    GetRenderMaterialsCount2 -> {NumMaterials}");

			if (NumMaterials == 0)
			{
				return null;
			}

			object[] ObjMaterials = Ext.GetRenderMaterials2((int)InDisplayState, InDisplayStateNames);

			FObjectMaterials PartMaterials = new FObjectMaterials(InPartDoc, MaterialsMap);

			foreach (object ObjMat in ObjMaterials)
			{
				RenderMaterial RenderMat = ObjMat as RenderMaterial;
				int NumUsers = RenderMat.GetEntitiesCount();
				LogDebug($"  FileName: {RenderMat.FileName}");
				LogDebug($"    Users({NumUsers}):");

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
							LogDebug($"    PartDoc '{(ObjUser as ModelDoc2).GetPathName()}'");
							PartMaterials.SetPartMaterial(RenderMat, Doc);
							continue;
						case IBody2 Body:
						{
							LogDebug($"    Body '{Body.Name}'");

							string BodyId = FPartDocumentTracker.GetBodyPath(InPartDoc as PartDoc, Body);
							PartMaterials.RegisterMaterial(PartMaterials.BodyMaterialsMap, Doc, RenderMat, BodyId);
							continue;
						}
						case IFace2 Face:
						{
							uint FaceId = InOwnerDoc.GetFaceId(Face);
							LogDebug($"    Face '{Face.GetFaceId()}'");
							PartMaterials.RegisterMaterial(PartMaterials.FaceMaterialsMap, Doc, RenderMat, FaceId.ToString());
							continue;
						}
						case IFeature Feat:
						{
							LogDebug($"    Feature '{Feat.Name}'");
							string FeatureId = FPartDocumentTracker.GetFeaturePath(InPartDoc as PartDoc, Feat);
							PartMaterials.RegisterMaterial(PartMaterials.FeatureMaterialsMap, Doc, RenderMat, FeatureId);
							continue;
						}
						default:
						{
							LogDebug($"    <unknown>'");
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
							LogDebug($"  Body '{Body.Name}' already has material assigned {PartMaterials.GetMaterial(AssignedMaterialId).Name}");
							continue;
						}

						LogDebug($"  Body '{Body.Name}' has no material assigned, searching material matching its Appearance..");

						foreach (KeyValuePair<int, FMaterial> KVP in MaterialsMap)
						{
							FMaterial Material = KVP.Value;

							if (Material.EqualsAppearance(Appearance))
							{
								LogDebug($"Matching material for Body '{Body.Name}'  is {Material.Name}");

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
							LogDebug($"  Feature '{Feat.Name}' already has material assigned {PartMaterials.GetMaterial(AssignedMaterialId).Name}");
							continue;
						}

						LogDebug($"  Feature '{Feat.Name}' has no material assigned, searching material matching its Appearance..");

						foreach (KeyValuePair<int, FMaterial> KVP in MaterialsMap)
						{
							FMaterial Material = KVP.Value;

							if (Material.EqualsAppearance(Appearance))
							{
								LogDebug($"Matching material for Feature '{Feat.Name}'  is {Material.Name}");

								PartMaterials.FeatureMaterialsMap[FeatureId] = KVP.Key; 
								break;
							}
						}
					}
				}
			}			

			return PartMaterials;
		}

		public static FObjectMaterials LoadComponentMaterials(FDocumentTracker InComponentOwner, Component2 InComponent,
			swDisplayStateOpts_e InDisplayState, string[] InDisplayStateNames,
			Dictionary<int, FMaterial> MaterialsMap)
		{
			LogDebug($"LoadComponentMaterials: '{InComponent.Name2}', '{InComponentOwner.GetPathName()}', {InDisplayState}, {(InDisplayStateNames == null ? "<null>" : string.Join(", ", InDisplayStateNames))}");

			ModelDoc2 ComponentDoc = InComponent.GetModelDoc2() as ModelDoc2;

			if (ComponentDoc == null || !(ComponentDoc is PartDoc))
			{
				// Component's model doc might be null if component is suppressed or lightweight (in which case we treat it as hidden)
				return null;
			}

			LogDebug($"  GetRenderMaterialsCount2({InDisplayState}, {InDisplayStateNames})");
			int NumMaterials = InComponent.GetRenderMaterialsCount2((int)InDisplayState, InDisplayStateNames);
			LogDebug($"    GetRenderMaterialsCount2 -> {NumMaterials}");

			if (NumMaterials == 0)
			{
				return null;
			}

			object[] ObjMaterials = InComponent.GetRenderMaterials2((int)InDisplayState, InDisplayStateNames);

			FObjectMaterials ComponentMaterials = new FObjectMaterials(ComponentDoc as PartDoc, MaterialsMap);

			if (ObjMaterials != null)
			{
				foreach (object ObjMat in ObjMaterials)
				{
					RenderMaterial RenderMat = ObjMat as RenderMaterial;
					int NumUsers = RenderMat.GetEntitiesCount();

					LogDebug($"  FileName: {RenderMat.FileName}");
					LogDebug($"    Users({NumUsers}):");

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
								LogDebug($"    Body '{Body.Name}'");
								string BodyId = FPartDocumentTracker.GetBodyPath(ComponentDoc as PartDoc, Body);
								ComponentMaterials.RegisterMaterial(ComponentMaterials.BodyMaterialsMap,
									InComponentOwner.SwDoc, RenderMat, BodyId);
								break;
							}
							case IFace2 Face:
							{
								// note: plugins sets FaceId on each face to identify per-face materials later in CreateMeshData
								uint FaceId = InComponentOwner.GetFaceId(Face);
								LogDebug($"    Face '{Face.GetFaceId()}'");
								ComponentMaterials.RegisterMaterial(ComponentMaterials.FaceMaterialsMap,
									InComponentOwner.SwDoc, RenderMat, FaceId.ToString());
								break;
							}
							case IFeature Feat:
							{
								LogDebug($"    Feature '{Feat.Name}'");
								string FeatureId = FPartDocumentTracker.GetFeaturePath(ComponentDoc as PartDoc, Feat);
								ComponentMaterials.RegisterMaterial(ComponentMaterials.FeatureMaterialsMap,
									InComponentOwner.SwDoc, RenderMat, FeatureId);
								break;
							}
							case IPartDoc Doc:
							{
								LogDebug($"    Part Doc '{(Doc as ModelDoc2).GetPathName()}'");
								ComponentMaterials.SetPartMaterial(RenderMat, InComponentOwner.SwDoc);
								break;
							}
							default:
							{
								LogDebug($"    <unknown>'");
								break;
							}
						}
					}
				}
			}			

			return ComponentMaterials;
		}

		// Load all materials in the assembly - including subassemblies and parts within
		// returns: material assignment for each component
		public static Dictionary<FComponentName, FObjectMaterials> LoadAssemblyMaterials(FAssemblyDocumentTracker InAsmDoc, HashSet<FComponentName> InComponentsSet, swDisplayStateOpts_e InDisplayState, string[] InDisplayStateNames)
		{
			LogDebug($"LoadAssemblyMaterials: {InAsmDoc.GetPathName()}, {InDisplayState}, [{(InDisplayStateNames == null ? "" : string.Join(",", InDisplayStateNames))}]");
			LogDebug($"  for components: [{string.Join(", ", InComponentsSet.OrderBy(Name => Name.GetString()))}]");

			Dictionary<int, FMaterial> MaterialsMap = InAsmDoc.ExportedMaterialsMap;
			Dictionary<FComponentName, FObjectMaterials> DocMaterials = new Dictionary<FComponentName, FObjectMaterials>();

			LoadMaterialsWithUsers(InAsmDoc, InDisplayState, InDisplayStateNames, DocMaterials, MaterialsMap);

			LogDebug($"Load Each Component's materials");
			LogIndent();

			foreach (FComponentName CompName in InComponentsSet.OrderBy(Name => Name.GetString()))
			{
				UpdateComponentMaterials(InAsmDoc, CompName, DocMaterials, MaterialsMap);
			}

			LogDedent();

			return DocMaterials.Count > 0 ? DocMaterials : null;
		}

		public static IEnumerable<bool> LoadAssemblyMaterialsEnum(FAssemblyDocumentTracker InAsmDoc, HashSet<FComponentName> InComponentsSet, Dictionary<FComponentName, FObjectMaterials> CurrentDocMaterialsMap, Dictionary<int, FMaterial> MaterialsMap, List<FComponentName> InvalidComponents)
		{
			FObjectMaterials.LoadMaterialsWithUsers(InAsmDoc, swDisplayStateOpts_e.swThisDisplayState, null,
				CurrentDocMaterialsMap, MaterialsMap);
			yield return true;

			foreach (FComponentName CompName in InComponentsSet.OrderBy(Name => Name.GetString()))
			{
				bool bIsComponentValid = FObjectMaterials.UpdateComponentMaterials(InAsmDoc, CompName, CurrentDocMaterialsMap,
					MaterialsMap);
				if (!bIsComponentValid)
				{
					InvalidComponents.Add(CompName);
				}
				yield return true;
			}
		}

		/// Returns whether component was found valid for export(not suppressed/not deleted)
		public static bool UpdateComponentMaterials(FAssemblyDocumentTracker InAsmDoc, FComponentName CompName,
			Dictionary<FComponentName, FObjectMaterials> DocMaterials, Dictionary<int, FMaterial> MaterialsMap)
		{
			LogDebug($"Component '{CompName}' - adding DocMaterials");
			LogIndent();

			if (InAsmDoc.SyncState.CollectedComponentsMap.TryGetValue(CompName, out Component2 Comp))
			{
				if (!DocMaterials.ContainsKey(CompName))
				{
					LogDebug($"Adding to DocMaterials");

					if (Comp.GetModelDoc2() == null)
					{
						// Component's model doc might be null if component is suppressed/lightweight (in which case we treat it as hidden)
						// or has already been deleted
						return false;
					}

					LogIndent();
					FObjectMaterials ComponentMaterials = FObjectMaterials.LoadComponentMaterials(InAsmDoc,
						Comp, swDisplayStateOpts_e.swThisDisplayState, null, MaterialsMap);

					LogDedent();

					if (ComponentMaterials == null)
					{
						if (Comp.GetModelDoc2() is PartDoc)
						{
							LogIndent();
							ComponentMaterials = FObjectMaterials.LoadPartMaterials(InAsmDoc,
								Comp.GetModelDoc2() as PartDoc, swDisplayStateOpts_e.swThisDisplayState, null,
								MaterialsMap);
							LogDedent();
						}
					}

					if (ComponentMaterials != null)
					{
						DocMaterials[CompName] = ComponentMaterials;
					}
				}
				else
				{
					LogDebug($"...already in DocMaterials");
				}
			}
			else
			{
				LogDebug($"...not in CollectedComponentsMap");
			}

			LogDedent();

			return true;
		}

		// Parse materials within document and return components using them
		public static void LoadMaterialsWithUsers(FAssemblyDocumentTracker InAsmDoc,
			swDisplayStateOpts_e InDisplayState,
			string[] InDisplayStateNames, Dictionary<FComponentName, FObjectMaterials> DocMaterials,
			Dictionary<int, FMaterial> MaterialsMap)
		{
			IModelDocExtension Ext = InAsmDoc.SwDoc.Extension;
			int NumMaterials = Ext.GetRenderMaterialsCount2((int)InDisplayState, InDisplayStateNames);

			LogDebug($"  Assembly materials={NumMaterials}");

			if (NumMaterials > 0)
			{
				object[] ObjMaterials = Ext.GetRenderMaterials2((int)InDisplayState, InDisplayStateNames);

				foreach (object ObjMat in ObjMaterials)
				{
					RenderMaterial RenderMat = ObjMat as RenderMaterial;

					LogDebug($"    Material: '{RenderMat.FileName}' ");

					int NumUsers = RenderMat.GetEntitiesCount();

					LogDebug($"    NumUsers: '{NumUsers}' ");

					if (NumUsers == 0)
					{
						continue;
					}

					object[] ObjUsers = RenderMat.GetEntities();
					foreach (object ObjUser in ObjUsers)
					{
						if (ObjUser is IEntity Entity)
						{
							LogDebug($"    User: type '{Entity.GetType()}' ");
						}

						switch (ObjUser)
						{
							case Component2 Comp:
							{
								LogDebug($"      Component '{Comp.Name2}' ");
								if (Comp.GetModelDoc2() is PartDoc CompDoc)
								{
									LogDebug($"        registering material for Part Component in DocMaterials");
									FObjectMaterials ComponentMaterials = new FObjectMaterials(CompDoc, MaterialsMap);

									LogIndent();
									ComponentMaterials.SetComponentMaterial(RenderMat, InAsmDoc.SwDoc);
									LogDedent();

									DocMaterials[new FComponentName(Comp)] = ComponentMaterials;
								}

								break;
							}
							case IPartDoc Doc:
							{
								LogDebug($"      Part Doc at '{(Doc as ModelDoc2).GetPathName()}' ");

								LogIndent();
								FObjectMaterials PartMaterials = LoadPartMaterials(InAsmDoc, Doc as PartDoc, InDisplayState,
									InDisplayStateNames, MaterialsMap);
								LogDedent();

								if (PartMaterials != null)
								{
									// todo: why trying to present PathName as component name?
									//  how it can be a PartDoc in Assembly function
									DocMaterials[FComponentName.FromCustomString((Doc as ModelDoc2).GetPathName())] =
										PartMaterials;
								}

								break;
							}
						}
					}
				}
			}
		}

	}
}
