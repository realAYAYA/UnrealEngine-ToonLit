// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;
using System.Collections.Concurrent;
using System.Linq;
using System.Diagnostics;

using DatasmithSolidworks.Names;
using static DatasmithSolidworks.FConfigurationTree;
using static DatasmithSolidworks.Addin;

namespace DatasmithSolidworks
{
	public class FConfigurationData
	{
		public string Name;
		public bool bIsDisplayStateConfiguration = false;
		public Dictionary<FComponentName, FConvertedTransform> ComponentTransform = new Dictionary<FComponentName, FConvertedTransform>();  // Relative(to parent) transform
		public Dictionary<FComponentName, bool> ComponentVisibility = new Dictionary<FComponentName, bool>();
		public Dictionary<FComponentName, FObjectMaterials> ComponentMaterials = new Dictionary<FComponentName, FObjectMaterials>();

		public struct FComponentGeometryVariant
		{
			public FActorName VisibleActor; // Actor enabled for this variant
			public List<FActorName> All; // All actors used for this component in variants
		}

		public Dictionary<FComponentName, FComponentGeometryVariant> ComponentGeometry = new Dictionary<FComponentName, FComponentGeometryVariant>();

		public bool IsEmpty()
		{
			return (ComponentTransform.Count == 0
			        && ComponentVisibility.Count == 0 
			        && ComponentMaterials.Count == 0
			        && ComponentGeometry.Count == 0);
		}
	};

	public class FMeshes
	{
		private readonly string MainConfigurationName;
		private Dictionary<string, FConfiguration> Configurations = new Dictionary<string, FConfiguration>();

		// Store unique meshes(identical mesh from different component will use the same FMesh object)
		// Using Dictionary instead of HashSet to be able to retrieve stored object by the key(which is a different object for identical bu different mesh)
		private Dictionary<FMesh, FMesh> Meshes = new Dictionary<FMesh, FMesh>();  
		private Dictionary<object, FMeshName> MeshNames = new Dictionary<object, FMeshName>();

		public struct FMesh
		{
			public FMeshData MeshData;

			private readonly int Hash;

			public FMesh(FMeshData InMeshData)
			{

				MeshData = InMeshData;

				Hash = 0;

				if (InMeshData == null)
				{
					return;
				}

				foreach (FVec3 Vertex in MeshData.Vertices)
				{
					Hash ^= Vertex.GetHashCode();
				}

				foreach (FVec3 Normal in MeshData.Normals)
				{
					Hash ^= Normal.GetHashCode();
				}

				foreach (FVec2 TexCoord in MeshData.TexCoords)
				{
					Hash ^= TexCoord.GetHashCode();
				}

				foreach (FTriangle Triangle in MeshData.Triangles)
				{
					Hash ^= Triangle.GetHashCode();
				}
			}

			public override bool Equals(object Obj)
			{
				return Obj is FMesh Other && Equals(Other);
			}

			public bool Equals(FMesh Other)
			{
				
				if (Hash != Other.Hash)
				{
					return false;
				}

				if (ReferenceEquals(MeshData, Other.MeshData))
				{
					return true;
				}
				
				if (!MeshData.Vertices.SequenceEqual(Other.MeshData.Vertices))
				{
					return false;
				}

				if (!MeshData.Normals.SequenceEqual(Other.MeshData.Normals))
				{
					return false;
				}

				if (!MeshData.TexCoords.SequenceEqual(Other.MeshData.TexCoords))
				{
					return false;
				}

				if (!MeshData.Triangles.SequenceEqual(Other.MeshData.Triangles))
				{
					return false;
				}

				return true;
			}

			public static bool operator ==(FMesh A, FMesh B)
			{
				return A.Equals(B);
			}

			public static bool operator !=(FMesh A, FMesh B)
			{
				return !(A == B);
			}

			public override int GetHashCode()
			{
				return Hash;
			}
		}

		public FMeshes(string InMainConfigurationName)
		{
			MainConfigurationName = InMainConfigurationName;
		}

		public FConfiguration GetMeshesConfiguration(string ConfigurationName)
		{
			if (Configurations.TryGetValue(ConfigurationName, out var Configuration))
			{
				return Configuration;
			}			

			Configuration = new FConfiguration(this, ConfigurationName, MainConfigurationName==ConfigurationName);
			Configurations.Add(ConfigurationName, Configuration);
			return Configuration;
		}

		public bool DoesComponentHaveVisibleButDifferentMeshesInConfigs(FComponentName ComponentName, string ConfigNameA, string ConfigNameB)
		{
			bool bHasMeshA = GetMeshesConfiguration(ConfigNameA).GetMeshForComponent(ComponentName, out FMesh MeshA);
			bool bHasMeshB = GetMeshesConfiguration(ConfigNameB).GetMeshForComponent(ComponentName, out FMesh MeshB);

			return (bHasMeshA && bHasMeshB) && (MeshA != MeshB);
		}

		public FMeshData GetMeshData(string ConfigName, FComponentName ComponentName)
		{
			return GetMeshesConfiguration(ConfigName).GetMeshForComponent(ComponentName, out FMesh Mesh) ? Mesh.MeshData : null;
		}

		public class FConfiguration
		{
			public class FComponentMesh
			{
				public Component2 Component;
				public FMesh Mesh = new FMesh();
				public FMeshName MeshName;
			}

			private readonly FMeshes Meshes;
			private readonly string Name;
			private readonly bool bIsMainConfiguration;
			private Dictionary<FComponentName, FComponentMesh> MeshForComponent = new Dictionary<FComponentName, FComponentMesh>();
			private readonly FConfiguration ParentConfiguration;

			public FConfiguration(FMeshes InMeshes, string InName, bool bInIsMainConfiguration, FConfiguration InParentConfiguration = null)
			{
				Meshes = InMeshes;
				Name = InName;
				bIsMainConfiguration = bInIsMainConfiguration;
				ParentConfiguration = InParentConfiguration;
			}

			public void AddComponentWhichNeedsMesh(Component2 InComponent, FComponentName InComponentName)
			{
				MeshForComponent[InComponentName] = new FComponentMesh() {Component = InComponent};
			}

			public void AddMesh(FComponentName ComponentName, FMeshData MeshData, out FMeshName MeshName)
			{
				FComponentMesh ComponentMesh;
				if (MeshForComponent.TryGetValue(ComponentName, out ComponentMesh))
				{
					// MeshName for active configuration is simplified(doesn't contain config name)
					FMeshName MeshNameDesired = bIsMainConfiguration ? new FMeshName(ComponentName) : new FMeshName(ComponentName, Name);

					// AddMesh may return mesh(and mesh name) of an identical mesh already processed - use these instead then
					Meshes.AddMesh(MeshNameDesired,  MeshData, out ComponentMesh.MeshName, out ComponentMesh.Mesh);
					MeshName = ComponentMesh.MeshName;
					return;
				}
				MeshName = new FMeshName();
				Debug.Assert(false);  // unexpected - component which can have mesh added should already be registered...
			}

			public bool GetMeshForComponent(FComponentName ComponentName, out FMesh Result)
			{
				if (MeshForComponent.TryGetValue(ComponentName, out FComponentMesh ComponentMesh))
				{
					Result = ComponentMesh.Mesh;
					return true;
				}
				Result = new FMesh(null);
				return false;
			}

			public IEnumerable<FComponentName> EnumerateComponentNames()
			{
				return MeshForComponent.Keys;
			}

			public IEnumerable<Component2> EnumerateComponents()
			{
				return MeshForComponent.Select(KVP => KVP.Value.Component);
			}

			public FMeshName GetMeshNameForComponent(FComponentName ComponentName)
			{
				if (MeshForComponent.TryGetValue(ComponentName, out FComponentMesh Mesh))
				{
					return Mesh.MeshName;
				}

				return ParentConfiguration?.GetMeshNameForComponent(ComponentName) ?? new FMeshName();
			}
		}

		// Returns true if this mesh was already there
		private bool AddMesh(FMeshName MeshName, FMeshData MeshData, out FMeshName OutMeshName, out FMesh OutMesh)
		{
			LogDebug($"FMeshes.AddMesh('{MeshName}')");
			// Store only one instance of identical mesh
			FMesh Mesh = new FMesh(MeshData);
			if (Meshes.TryGetValue(Mesh, out OutMesh))
			{
				OutMeshName = MeshNames[OutMesh];
				LogDebug($"    identical mesh found: '{OutMeshName}'");
				return true;
			}

			Meshes.Add(Mesh, Mesh);
			MeshNames.Add(Mesh, MeshName); // Record name for the instance of the mesh also

			OutMesh = Mesh;
			OutMeshName = MeshName;
			return false;;
		}

		public string GetConfigurationNameForComponentMesh(FComponentName ComponentName)
		{
			foreach (KeyValuePair<string, FConfiguration> KVP in Configurations)
			{
				if (KVP.Value.GetMeshForComponent(ComponentName, out FMesh Mesh))
				{
					return KVP.Key;
				}
			}
			return null;
		}

		public FMeshName GetMeshName(string ConfigName, FComponentName ComponentName)
		{
			return Configurations[ConfigName].GetMeshNameForComponent(ComponentName);
		}

	}

	public class FConfigurationExporter
	{
		public readonly FMeshes Meshes;
		public readonly string[] ConfigurationNames;
		private readonly string MainConfigurationName;
		private readonly bool bExportDisplayStates;
		private readonly bool bExportExplodedViews;

		public FConfigurationTree.FComponentTreeNode CombinedTree;

		private List<FDatasmithExporter.FMeshExportInfo> ExtractedMeshes = new List<FDatasmithExporter.FMeshExportInfo>();
		public Dictionary<FMeshName, List<FActorName>> ActorsForMesh = new Dictionary<FMeshName, List<FActorName>>();
		public Dictionary<FComponentName, List<FMeshName>> MeshesForComponent = new Dictionary<FComponentName, List<FMeshName>>();

		public FConfigurationExporter(FMeshes InMeshes, 
			string[] InConfigurationNames, string InMainConfigurationName,
			bool bInExportDisplayStates,
			bool bInExportExplodedViews)
		{
			Meshes = InMeshes;
			ConfigurationNames = InConfigurationNames;
			MainConfigurationName = InMainConfigurationName;
			bExportDisplayStates = bInExportDisplayStates;
			bExportExplodedViews = bInExportExplodedViews;
		}

		public List<FConfigurationData> ExportConfigurations(FDocumentTracker InDoc)
		{
			LogDebug($"FConfigurationExporter.ExportConfigurations [{string.Join(",", ConfigurationNames)}]");
			// todo: may refactor to union behavior for 'no configuration'(i.e. active) and one-to-many configurations export
			// Not sure how CfgNames might be technically null though
			if (ConfigurationNames == null) // || CfgNames.Length <= 1)
			{
				return null;
			}

			CombinedTree = new FConfigurationTree.FComponentTreeNode(null);
			CombinedTree.ComponentInfo.ComponentName = FComponentName.FromCustomString("CombinedTree");

			// Ensure recursion will not stop on the root node (this may happen if it is not explicitly marked as visible)
			CombinedTree.bVisibilitySame = true;
			CombinedTree.CommonConfig.bVisible = true;

			ConfigurationManager ConfigManager = InDoc.SwDoc.ConfigurationManager;
			IConfiguration OriginalConfiguration = ConfigManager.ActiveConfiguration;

			string OriginalConfigurationName = ConfigManager.ActiveConfiguration.Name;

			Dictionary<string, List<FVariantName>> ExportedVariantNames = new Dictionary<string, List<FVariantName>>();

			// Enumerate configurations starting with Active - to avoid redundant config switching(which takes time) and,
			// additionally, to export meshes in Active configuration without configuration name suffixes
			List<string> ConfigurationNamesActiveFirst = ConfigurationNames.ToList();
			ConfigurationNamesActiveFirst.Remove(OriginalConfigurationName);
			ConfigurationNamesActiveFirst.Insert(0, OriginalConfigurationName);

			// Keep configurations order
			Dictionary<string, int> ConfigurationOrder = ConfigurationNames.Select((Name, Index) => new { Name, Index }).ToDictionary(V => V.Name, V => V.Index);

			bool bIsActiveConfiguration = true;
			foreach (string CfgName in ConfigurationNamesActiveFirst)
			{
				LogDebug($"Configuration '{CfgName}'");
				IConfiguration swConfiguration = InDoc.SwDoc.GetConfigurationByName(CfgName) as IConfiguration;

				if (!bIsActiveConfiguration)
				{
					LogDebug($"ShowConfiguration2 '{CfgName}'");
					InDoc.SwDoc.ShowConfiguration2(CfgName);
				}
				LogDebug($"ActiveConfiguration '{ConfigManager.ActiveConfiguration.Name}'");

				List<string> ExplodedViews = InitExplodedViews(InDoc, CfgName);

				// Variant name to base on for linked display states and exploded views
				FVariantName BaseCfgVariantName = new FVariantName(swConfiguration, CfgName);  

				// Name identifying Datasmith Variant to be exported
				// variants sets consists of SW Configurations, derived configurations, display states, exploded views
				FVariantName VariantName = BaseCfgVariantName;

				string[] DisplayStates = null;
				int DisplayStateCount = 0;

				if (bExportDisplayStates)
				{
					DisplayStateCount = swConfiguration.GetDisplayStatesCount();

					// When display states are linked to configuration modify variant name to include current display state name
					if (ConfigManager.LinkDisplayStatesToConfigurations && DisplayStateCount > 1)
					{
						DisplayStates = swConfiguration.GetDisplayStates();
						VariantName = VariantName.LinkedDisplayStateVariant(DisplayStates[0]);
					}
				}

				FConfigurationTree.FComponentTreeNode ConfigNode = new FConfigurationTree.FComponentTreeNode();
				ConfigNode.ComponentInfo.ComponentName = VariantName.GetRootComponentName();

				ConfigNode.Children = new List<FConfigurationTree.FComponentTreeNode>();

				FMeshes.FConfiguration MeshesConfiguration = Meshes.GetMeshesConfiguration(ConfigurationName: CfgName);

				// Build the tree and get default materials (which aren't affected by any configuration)
				// Use GetRootComponent3() with Resolve = true to ensure suppressed components will be loaded
				// todo: docs says that Part document SW returns null. Not the case. But probably better to add a guard
				LogDebug($"Components:");
				CollectComponentsRecursive(InDoc, swConfiguration.GetRootComponent3(true), ConfigNode, MeshesConfiguration);

				ExportedVariantNames.Add(CfgName, new List<FVariantName>(){VariantName});

				LogDebug($"Materials:");
				Dictionary<FComponentName, FObjectMaterials> MaterialsMap = GetComponentMaterials(InDoc, DisplayStates?[0], swConfiguration);
				SetComponentTreeMaterials(ConfigNode, MaterialsMap, new FVariantName(), false);

				if (DisplayStates != null)
				{
					// Export display states here: they are linked to configurations.
					// Skip first as it was already exported above.
					for (int Index = 1; Index < DisplayStateCount; ++Index)
					{
						string DisplayState = DisplayStates[Index];

						FVariantName DisplayStateVariantName = BaseCfgVariantName.LinkedDisplayStateVariant(DisplayState);

						LogDebug($"Linked DisplayState '{DisplayState}' Materials for variant '{DisplayStateVariantName}'");

						ExportedVariantNames[CfgName].Add(DisplayStateVariantName);
						MaterialsMap = GetComponentMaterials(InDoc, DisplayState, swConfiguration);
						SetComponentTreeMaterials(ConfigNode, MaterialsMap, DisplayStateVariantName, false);
					}
				}

				// Add variant per exploded view within the configuration
				if (ExplodedViews != null && ExplodedViews.Count > 0)
				{
					LogIndent();


					InDoc.SetExportStatus($"Exploded Views");
					LogDebug($"Exploded Views:");
					foreach (string ExplodedViewName in ExplodedViews)
					{
						LogIndent();

						LogDebug($"View '{ExplodedViewName}'");

						// Switch document state to the exploded view
						switch (InDoc)
						{
							case FAssemblyDocumentTracker AsmDoc:
							{
								AsmDoc.SwAsmDoc?.ShowExploded2(true, ExplodedViewName);
								break;
							}
							case FPartDocumentTracker PartDoc:
							{
								PartDoc.SwPartDoc?.ShowExploded(true, ExplodedViewName);
								break;
							}
						}

						FVariantName ExplodedViewVariantName = BaseCfgVariantName.ExplodedViewVariant(ExplodedViewName);
						ExportedVariantNames[CfgName].Add(ExplodedViewVariantName);

						// Add variant to each node with transform initialized from exploded state
						foreach (FComponentTreeNode Child in ConfigNode.Children)
						{
							Child.Traverse(ConfigNode.CommonConfig, 
								(InParentConfig, InNode) =>
								{
									FComponentConfig Config = InNode.AddConfiguration(ExplodedViewVariantName, bIsDisplayState: false, bIsActiveConfiguration: false);
									ComputeNodeTransform(InParentConfig, InNode, Config);
									return Config;
								});
						}

						LogDedent();
					}

					LogDedent();
				}

				// Export materials
				InDoc.SetExportStatus($"Component Materials");
				HashSet<FComponentName> ComponentNamesToExportSet = new HashSet<FComponentName>();
				foreach (FComponentName ComponentName in MeshesConfiguration.EnumerateComponentNames())
				{
					ComponentNamesToExportSet.Add(ComponentName);
				}

				Dictionary<FComponentName, FObjectMaterials> ModifiedComponentsMaterials = InDoc.LoadDocumentMaterials(ComponentNamesToExportSet);

				if (ModifiedComponentsMaterials != null)
				{
					foreach (KeyValuePair<FComponentName, FObjectMaterials> MatKVP in ModifiedComponentsMaterials)
					{
						InDoc.AddComponentMaterials(MatKVP.Key, MatKVP.Value);
					}
				}
				
				// Export meshes
				InDoc.SetExportStatus($"Component Meshes");

				InDoc.ProcessConfigurationMeshes(ExtractedMeshes, MeshesConfiguration);

				// Combine separate scene trees into the single one with configuration-specific data
				FConfigurationTree.Merge(CombinedTree, ConfigNode, VariantName, bIsActiveConfiguration);

				bIsActiveConfiguration = false;  // Next enumerated configurations will be switched to(they weren't active on export start)
			}

			if (OriginalConfiguration != null && ConfigurationNames.Length > 1)
			{
				LogDebug($"ShowConfiguration2 '{OriginalConfiguration.Name}'");
				bool bShowConfigurationResult = InDoc.SwDoc.ShowConfiguration2(OriginalConfiguration.Name);
				Debug.Assert(bShowConfigurationResult);
			}

			// Reload components for Original configuration(without this extracting materials for Display States later won't work - old component objects simply return 0) 
			{
				Component2 RootComponent = OriginalConfiguration.GetRootComponent3(true);
				Stack<Component2> Components = new Stack<Component2>();
				Components.Push(RootComponent);

				while (Components.Count != 0)
				{
					Component2 Component = Components.Pop();

					FConfigurationTree.FComponentTreeNode NewNode = new FConfigurationTree.FComponentTreeNode(Component);
					NewNode.ComponentInfo.ComponentName = new FComponentName(Component);
					InDoc.AddCollectedComponent(NewNode);

					object[] Children = (object[])Component.GetChildren();
					if (Children != null)
					{
						foreach (Component2 Child in Children)
						{
							Components.Push(Child);
						}
					}
				}
			}

			List<FVariantName> DisplayStateConfigurations = new List<FVariantName>();
			
			if (bExportDisplayStates && !ConfigManager.LinkDisplayStatesToConfigurations)
			{
				// Export display states as separate configurations
				string[] DisplayStates = OriginalConfiguration.GetDisplayStates();

				for (int Index = 0; Index < DisplayStates.Length; Index++)
				{
					string DisplayState = DisplayStates[Index];
					if (Index != 0)
					{
						OriginalConfiguration.ApplyDisplayState(DisplayState);
					}
					
					FVariantName DisplayStateConfigName = FVariantName.DisplayStateVariant(DisplayState);
					DisplayStateConfigurations.Add(DisplayStateConfigName);
					LogDebug(
						$"DisplayState '{DisplayState}' Materials for variant '{DisplayStateConfigName}'");

					// Specifying display state doesn't seem to work in the api like this:
					// GetComponentMaterials(InDoc, DisplayState, OriginalConfiguration);
					//   (which becomes Ext.GetRenderMaterials2((int)swDisplayStateOpts_e.swSpecifyDisplayState, InDisplayStateNames))
					// - component materials are empty (Comp.GetRenderMaterialsCount2 gives 0 for assembly components)
					Dictionary<FComponentName, FObjectMaterials> MaterialsMap = GetComponentMaterials(InDoc, null, OriginalConfiguration);

					SetComponentTreeMaterials(CombinedTree, MaterialsMap, DisplayStateConfigName, true);
				}

				// Restore original active display state, if there were more than one display state
				if (DisplayStates.Length > 1)
				{
					OriginalConfiguration.ApplyDisplayState(DisplayStates[0]);  // Active display state is the first in the list returned by GetDisplayStates
				}
			}

			// Remove configuration data when it's the same
			FConfigurationTree.Compress(CombinedTree, this);

			List<FConfigurationData> FlatConfigurationData = new List<FConfigurationData>();

			// Order exported variants by original configurations order
			IEnumerable<FVariantName> ExportedConfigurationNamesOrdered = ExportedVariantNames.OrderBy(V => ConfigurationOrder[V.Key]).SelectMany(V => V.Value);

			// Add roots for configurations
			foreach (FVariantName ConfigName in ExportedConfigurationNamesOrdered)
			{
				FConfigurationData CfgData = new FConfigurationData();
				CfgData.Name = ConfigName.ToString();
				CfgData.bIsDisplayStateConfiguration = false;
				FConfigurationTree.FillConfigurationData(this, CombinedTree, ConfigName, CfgData, false);

				if (!CfgData.IsEmpty())
				{
					FlatConfigurationData.Add(CfgData);
				}
			}
			foreach (FVariantName DisplayStateConfigName in DisplayStateConfigurations)
			{
				FConfigurationData CfgData = new FConfigurationData();
				CfgData.Name = DisplayStateConfigName.ToString();
				CfgData.bIsDisplayStateConfiguration = true;
				FConfigurationTree.FillConfigurationData(this, CombinedTree, DisplayStateConfigName, CfgData, true);

				if (!CfgData.IsEmpty())
				{
					FlatConfigurationData.Add(CfgData);
				}
			}

			return FlatConfigurationData;
		}

		// Collect exploded views which need to be exported and
		// collapse current configuration exploded views in case we are exporting exploded views(we want top configuration to be in un-exploded state)
		private List<string> InitExplodedViews(FDocumentTracker InDoc, string CfgName)
		{
			if (!bExportExplodedViews)
			{
				return null;
			}

			switch (InDoc)
			{
				case FAssemblyDocumentTracker AsmDoc:
				{
					AssemblyDoc SwAsmDoc = AsmDoc.SwAsmDoc;

					if (SwAsmDoc == null)
					{
						return null;
					}

					int ExplodedViewCount = SwAsmDoc.GetExplodedViewCount2(CfgName);
					LogDebug($"Exploded Views({ExplodedViewCount})");

					if (ExplodedViewCount <= 0)
					{
						return null;
					}

					string[] ExplodedViewNames = (string[])SwAsmDoc.GetExplodedViewNames2(CfgName);
					if (ExplodedViewNames == null)
					{
						return null;
					}

					List<string> ExplodedViews = new List<string>();
					ExplodedViews.AddRange(ExplodedViewNames);

					// Whe exporting exploded view collapse exploded view when exporting parent configuration itself
					foreach (string ExplodedViewName in ExplodedViews)
					{
						SwAsmDoc.ShowExploded2(false, ExplodedViewName);
					}

					return ExplodedViews;
				}
				case FPartDocumentTracker PartDoc:
				{
					PartDoc SwPartDoc = PartDoc.SwPartDoc;

					if (SwPartDoc == null)
					{
						return null;
					}

					int ExplodedViewCount = SwPartDoc.GetExplodedViewCount(CfgName);
					LogDebug($"Exploded Views({ExplodedViewCount})");

					if (ExplodedViewCount <= 0)
					{
						return null;
					}

					string[] ExplodedViewNames = (string[])SwPartDoc.GetExplodedViewNames(CfgName);
					if (ExplodedViewNames == null)
					{
						return null;
					}

					List<string> ExplodedViews = new List<string>();
					ExplodedViews.AddRange(ExplodedViewNames);

					// Whe exporting exploded view collapse exploded view when exporting parent configuration itself
					foreach (string ExplodedViewName in ExplodedViews)
					{
						SwPartDoc.ShowExploded(false, ExplodedViewName);
					}

					return ExplodedViews;
				}
			}

			return null;
		}

		// Assign meshes to actors, export materials and assign materials
		public void FinalizeExport(FDocumentTracker InDoc)
		{
			List<FDatasmithExporter.FMeshExportInfo> ExportedMeshes = InDoc.Exporter.ExportMeshes(ExtractedMeshes).ToList();

			foreach (FDatasmithExporter.FMeshExportInfo Info in ExportedMeshes)
			{
				LogDebug($"  AddMesh(DatasmithMeshName: {Info}");

				InDoc.Exporter.AddMesh(Info);

				// Assign to child variant actors of component actors registered 
				if (ActorsForMesh.TryGetValue(Info.MeshName, out List<FActorName> Names))
				{
					foreach (FActorName ActorName in Names)
					{

						InDoc.Exporter.AssignMeshToDatasmithMeshActor(ActorName, Info.MeshElement);
					}
				}
			}

			foreach (KeyValuePair<FComponentName, List<FMeshName>> KVP in MeshesForComponent)
			{
				FComponentName ComponentName = KVP.Key;
				InDoc.ReleaseComponentMeshes(ComponentName);
				// Register that this mesh was used for the component
				foreach (FMeshName MeshName in KVP.Value)
				{
					InDoc.AddMeshForComponent(ComponentName, MeshName);
				}
			}

			InDoc.CleanupComponentMeshes();

			InDoc.ExportMaterials();
			InDoc.AssignMaterialsToDatasmithMeshes(ExportedMeshes);
		}

		private static Dictionary<FComponentName, FObjectMaterials> GetComponentMaterials(FDocumentTracker InDoc, string InDisplayState, IConfiguration InConfiguration)
		{
			Dictionary<FComponentName, FObjectMaterials> MaterialsMap = new Dictionary<FComponentName, FObjectMaterials>();

			swDisplayStateOpts_e Option = InDisplayState != null ? swDisplayStateOpts_e.swSpecifyDisplayState : swDisplayStateOpts_e.swThisDisplayState;
			string[] DisplayStates = InDisplayState != null ? new string[] { InDisplayState } : null;

			if (InDoc is FAssemblyDocumentTracker AsmDoc)
			{
				HashSet<FComponentName> ComponentsSet = new HashSet<FComponentName>(AsmDoc.SyncState.CollectedComponentsMap.Keys);

				Dictionary<FComponentName, FObjectMaterials> ComponentMaterials =
					FObjectMaterials.LoadAssemblyMaterials(AsmDoc, ComponentsSet, Option, DisplayStates);

				if (ComponentMaterials != null)
				{
					foreach (var KVP in ComponentMaterials)
					{
						MaterialsMap.Add(KVP.Key, KVP.Value);
					}
				}
			}
			else
			{
				FPartDocumentTracker PartDocumentTracker = InDoc as FPartDocumentTracker;
				FObjectMaterials PartMaterials = FObjectMaterials.LoadPartMaterials(InDoc, PartDocumentTracker.SwPartDoc, Option, DisplayStates, InDoc.ExportedMaterialsMap);
				Component2 Comp = InConfiguration.GetRootComponent3(true);

				MaterialsMap.Add(FComponentName.FromCustomString(Comp?.Name2 ?? ""), PartMaterials);
			}

			return MaterialsMap;
		}

		private static void SetComponentTreeMaterials(FConfigurationTree.FComponentTreeNode InComponentTree, Dictionary<FComponentName, FObjectMaterials> InComponentMaterialsMap, FVariantName InConfigurationName, bool bIsDisplayState)
		{
			LogDebug($"SetComponentTreeMaterials: '{InComponentTree.ComponentName}'");

			FConfigurationTree.FComponentConfig TargetConfig = null;

			if (InConfigurationName.IsValid())
			{
				TargetConfig = InComponentTree.GetConfiguration(InConfigurationName, bIsDisplayState);
				if (TargetConfig == null)
				{
					TargetConfig = InComponentTree.AddConfiguration(InConfigurationName, bIsDisplayState, bIsActiveConfiguration: false);
				}
			}
			else
			{
				TargetConfig = InComponentTree.CommonConfig;
			}

			if (InComponentMaterialsMap.ContainsKey(InComponentTree.ComponentName))
			{
				FObjectMaterials Materials = InComponentMaterialsMap[InComponentTree.ComponentName];
				TargetConfig.Materials = Materials;
			}

			LogDebug($"  Materials: {TargetConfig.Materials}");

			foreach (FConfigurationTree.FComponentTreeNode Child in InComponentTree.EnumChildren())
			{
				LogIndent();
				SetComponentTreeMaterials(Child, InComponentMaterialsMap, InConfigurationName, bIsDisplayState);
				LogDedent();
			}
		}

		private static void CollectComponentsRecursive(FDocumentTracker InDoc, Component2 InComponent,
			FComponentTreeNode InParentNode, FMeshes.FConfiguration Meshes)
		{
			LogDebug($"'{InComponent.Name2}'");

			FConfigurationTree.FComponentTreeNode NewNode = new FConfigurationTree.FComponentTreeNode(InComponent);
			InParentNode.Children.Add(NewNode);

			// Basic properties
			NewNode.ComponentInfo.ComponentName = new FComponentName(InComponent);
			NewNode.ComponentInfo.ComponentId = InComponent.GetID();

			// ComponentDoc is null if component is suppressed or lightweight
			ModelDoc2 ModelDoc = (ModelDoc2)InComponent.GetModelDoc2();
			NewNode.ComponentInfo.PartPath = (ModelDoc is PartDoc) ? ModelDoc.GetPathName() : null;  // Identify whether the component is a Part component


			NewNode.CommonConfig.bVisible = InComponent.Visible != (int)swComponentVisibilityState_e.swComponentHidden;
			NewNode.CommonConfig.bSuppressed = InComponent.IsSuppressed();

			ComputeNodeTransform(InParentNode.CommonConfig, NewNode, NewNode.CommonConfig);

			// Process children components
			var Children = (Object[])InComponent.GetChildren();
			if (Children != null && Children.Length > 0)  // Children can be null sometimes and sometimes zero-length array
			{
				NewNode.Children = new List<FConfigurationTree.FComponentTreeNode>();
				foreach (object ObjChild in Children)
				{
					Component2 Child = (Component2)ObjChild;
					LogIndent();
					CollectComponentsRecursive(InDoc, Child, NewNode, Meshes);
					LogDedent();
				}

				// todo: sorting ensures that component order is stable in export(e.g. in variant property bindings order)
				// but probably makes sense to to it at the datasmith export, explicitly
				// especially that Children here begs for a dictionary, not list(child looked up by name form Children list)
				NewNode.Children.Sort((InA, InB) => InA.ComponentId - InB.ComponentId);
			}

			if (NewNode.IsPartComponent())
			{
				if (InDoc.NeedExportComponent(NewNode, NewNode.CommonConfig))
				{
					InDoc.AddPartDocument(NewNode);
				}
				Meshes.AddComponentWhichNeedsMesh(NewNode.Component, NewNode.ComponentName);
			}
			InDoc.AddCollectedComponent(NewNode);
		}

		private static void ComputeNodeTransform(FComponentConfig ParentConfig, FComponentTreeNode InNode, FComponentConfig ComponentConfig)
		{
			// Read transform
			MathTransform ComponentTransform = InNode.Component.GetTotalTransform(true) ?? InNode.Component.Transform2;

			if (ComponentTransform != null)
			{
				ComponentConfig.Transform =
					MathUtils.ConvertFromSolidworksTransform(ComponentTransform, 100f /*GeomScale*/);
				LogDebug($" Transform: {ComponentConfig.Transform}");

				if (ParentConfig != null && ParentConfig.Transform.IsValid())
				{
					// Convert transform to parent space (original transform value fetched from Solidworks
					// is in the root component's space). Datasmith wants relative transform for variants.
					FMatrix4 ParentTransform = new FMatrix4(ParentConfig.Transform.Matrix);
					FMatrix4 InverseParentTransform = ParentTransform.Inverse();
					ComponentConfig.RelativeTransform = new FConvertedTransform(
						FMatrix4.FMatrix4x4Multiply(ComponentConfig.Transform.Matrix, InverseParentTransform));
				}
				else
				{
					ComponentConfig.RelativeTransform = ComponentConfig.Transform;
				}

				LogDebug($"   ParentTransform: {ParentConfig?.Transform}");
				LogDebug($"   RelativeTransform: {ComponentConfig.RelativeTransform}");
			}
		}

		public bool DoesComponentHaveVisibleButDifferentMeshesInConfigs(FComponentName ComponentName, FVariantName ConfigNameA, FVariantName ConfigNameB)
		{
			return Meshes.DoesComponentHaveVisibleButDifferentMeshesInConfigs(ComponentName, ConfigNameA.CfgName, ConfigNameB.CfgName);
		}

		// name/label for variant mesh actor 
		public FActorName GetMeshActorName(FVariantName ConfigName, FComponentName ComponentName)
		{
			return FActorName.FromString($"{ComponentName}_{ConfigName.CfgName}");
		}

		public FMeshName GetMeshName(FVariantName ConfigName, FComponentName ComponentName)
		{
			return Meshes.GetMeshName(ConfigName.CfgName, ComponentName);
		}

		// Get mesh for component which has only single mesh configuration
		public FMeshName GetMeshName(FComponentName ComponentName)
		{
			string ConfigName = Meshes.GetConfigurationNameForComponentMesh(ComponentName);
			return Meshes.GetMeshName(ConfigName, ComponentName);
		}

		// Register actor which needs mesh of a component in specific config
		// this is needed for mesh actors for variants
		public void AddActorForMesh(FActorName ActorName, FVariantName ConfigName, FComponentName ComponentName)
		{
			LogDebug($"AddActorForMesh(ActorName='{ActorName}', ConfigName='{ConfigName}', ComponentName='{ComponentName}')");
			AddActorForMesh(ActorName, ComponentName, GetMeshName(ConfigName, ComponentName));
		}

		// Register actor which needs mesh of a component in any config(i.e. single actor of a component which doesn't have different meshes in different configs)
		// this is needed for mesh actors for variants
		public void AddActorForMesh(FActorName ActorName, FComponentName ComponentName)
		{
			LogDebug($"AddActorForMesh(ActorName='{ActorName}', ComponentName='{ComponentName}')");
			AddActorForMesh(ActorName, ComponentName, GetMeshName(ComponentName));
		}
		private void AddActorForMesh(FActorName ActorName, FComponentName ComponentName, FMeshName MeshName)
		{
			LogDebug($"  MeshName='{MeshName}')");
			if (!MeshName.IsValid())
			{
				LogDebug(
					$"  mesh INVALID for this component/configuration(e.g. empty mesh would not be exported)");
				return;
			}

			ActorsForMesh.FindOrAdd(MeshName).Add(ActorName);
			MeshesForComponent.FindOrAdd(ComponentName).Add(MeshName);
		}
	}
}