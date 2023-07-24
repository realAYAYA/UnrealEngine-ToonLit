// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;
using System.Runtime.InteropServices;
using System.Collections.Concurrent;
using System.Linq;
using DatasmithSolidworks.Names;
using static DatasmithSolidworks.FConfigurationTree;
using System.ComponentModel;
using System.Diagnostics;
using static DatasmithSolidworks.FMeshes;

namespace DatasmithSolidworks
{
	public class FConfigurationData
	{
		public string Name;
		public bool bIsDisplayStateConfiguration = false;
		public Dictionary<FComponentName, float[]> ComponentTransform = new Dictionary<FComponentName, float[]>();  // Relative(to parent) transform
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
		private Dictionary<string, FConfiguration> Configurations = new Dictionary<string, FConfiguration>();

		private HashSet<FMesh> Meshes = new HashSet<FMesh>();

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

		public FConfiguration GetMeshesConfiguration(string ConfigurationName)
		{
			if (Configurations.TryGetValue(ConfigurationName, out var Configuration))
			{
				return Configuration;
			}			

			Configuration = new FConfiguration(this, ConfigurationName);
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
				public FActorName ActorName;
				public FMesh Mesh = new FMesh();
			}

			private ConcurrentDictionary<FComponentName, FComponentMesh> MeshForComponent = new ConcurrentDictionary<FComponentName, FComponentMesh>();

			public FConfiguration(FMeshes Meshes, string Name)
			{
			}

			public void AddComponentWhichNeedsMesh(Component2 InComponent, FComponentName InComponentName, FActorName InActorName)
			{
				MeshForComponent.TryAdd(InComponentName, new FComponentMesh() {Component = InComponent, ActorName = InActorName});
			}

			public void AddMesh(Component2 Component, FMeshData MeshData)
			{
				FComponentMesh ComponentMesh;
				if (MeshForComponent.TryGetValue(new FComponentName(Component), out ComponentMesh))
				{
					ComponentMesh.Mesh = new FMesh(MeshData);
				}
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
	}

	public class FConfigurationExporter
	{
		public readonly FMeshes Meshes;
		public readonly string[] ConfigurationNames;
		private readonly string MainConfigurationName;

		public FConfigurationTree.FComponentTreeNode CombinedTree;

		public FConfigurationExporter(FMeshes InMeshes, string[] InConfigurationNames, string InMainConfigurationName)
		{
			Meshes = InMeshes;
			ConfigurationNames = InConfigurationNames;
			MainConfigurationName = InMainConfigurationName;
		}

		public List<FConfigurationData> ExportConfigurations(FDocumentTracker InDoc)
		{
			Addin.Instance.LogDebug($"FConfigurationExporter.ExportConfigurations [{string.Join(",", ConfigurationNames)}]");
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

			Dictionary<string, string> ExportedConfigurationNames = new Dictionary<string, string>();

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
				Addin.Instance.LogDebug($"Configuration '{CfgName}'");
				IConfiguration swConfiguration = InDoc.SwDoc.GetConfigurationByName(CfgName) as IConfiguration;

				if (!bIsActiveConfiguration)
				{
					Addin.Instance.LogDebug($"ShowConfiguration2 '{CfgName}'");
					InDoc.SwDoc.ShowConfiguration2(CfgName);
				}
				Addin.Instance.LogDebug($"ActiveConfiguration '{ConfigManager.ActiveConfiguration.Name}'");

				string ConfigName = CfgName;

				int DisplayStateCount = swConfiguration.GetDisplayStatesCount();
				string[] DisplayStates = null;

				if (ConfigManager.LinkDisplayStatesToConfigurations && DisplayStateCount > 1)
				{
					DisplayStates = swConfiguration.GetDisplayStates();
					ConfigName = $"{CfgName}_{DisplayStates[0]}";
				}

				FConfigurationTree.FComponentTreeNode ConfigNode = new FConfigurationTree.FComponentTreeNode();
				ConfigNode.ComponentInfo.ComponentName = FComponentName.FromCustomString(ConfigName);

				ConfigNode.Children = new List<FConfigurationTree.FComponentTreeNode>();

				FMeshes.FConfiguration MeshesConfiguration = Meshes.GetMeshesConfiguration(ConfigurationName: CfgName);

				// Build the tree and get default materials (which aren't affected by any configuration)
				// Use GetRootComponent3() with Resolve = true to ensure suppressed components will be loaded
				// todo: docs says that Part document SW returns null. Not the case. But probably better to add a guard
				Addin.Instance.LogDebug($"Components:");
				CollectComponentsRecursive(InDoc, swConfiguration.GetRootComponent3(true), ConfigNode, MeshesConfiguration, CfgName);

				ExportedConfigurationNames.Add(CfgName, ConfigName);

				Addin.Instance.LogDebug($"Materials:");
				Dictionary<FComponentName, FObjectMaterials> MaterialsMap = GetComponentMaterials(InDoc, DisplayStates != null ? DisplayStates[0] : null, swConfiguration);
				SetComponentTreeMaterials(ConfigNode, MaterialsMap, null, false);

				if (DisplayStates != null)
				{
					// Export display states here: they are linked to configurations.
					// Skip first as it was already exported above.
					for (int Index = 1; Index < DisplayStateCount; ++Index)
					{
						string DisplayState = DisplayStates[Index];
						string DisplayStateTreeName = $"{CfgName}_DisplayState_{FDatasmithExporter.SanitizeName(DisplayState)}";
						Addin.Instance.LogDebug($"Linked DisplayState '{DisplayState}' Materials for variant '{DisplayStateTreeName}'");

						MaterialsMap = GetComponentMaterials(InDoc, DisplayState, swConfiguration);
						SetComponentTreeMaterials(ConfigNode, MaterialsMap, DisplayStateTreeName, false);
					}
				}

				// Export materials
				InDoc.SetExportStatus($"Component Materials");
				HashSet<FComponentName> ComponentNamesToExportSet = new HashSet<FComponentName>();
				foreach (FComponentName ComponentName in MeshesConfiguration.EnumerateComponentNames())
				{
					if (!ComponentNamesToExportSet.Contains(ComponentName))
					{
						ComponentNamesToExportSet.Add(ComponentName);
					}
				}

				ConcurrentDictionary<FComponentName, FObjectMaterials> ModifiedComponentsMaterials = InDoc.LoadDocumentMaterials(ComponentNamesToExportSet);

				if (ModifiedComponentsMaterials != null)
				{
					foreach (var MatKVP in ModifiedComponentsMaterials)
					{
						InDoc.AddComponentMaterials(MatKVP.Key, MatKVP.Value);
					}
				}
				
				// Export materials before exporting meshes(currently mesh export code is tied to FDatasmithExporter.ExportedMaterialsMap
				// todo: separate material export and mesh export(see comment above)?
				InDoc.Exporter.ExportMaterials(InDoc.ExportedMaterialsMap);

				// Export meshes
				InDoc.SetExportStatus($"Component Meshes");

				string MeshSuffix = bIsActiveConfiguration ? null : CfgName; // Suffix for main configuration is absent(so in default or single configuration export we don't have unnecessary long names)
				InDoc.ProcessConfigurationMeshes(MeshesConfiguration, MeshSuffix);

				// Combine separate scene trees into the single one with configuration-specific data
				FConfigurationTree.Merge(CombinedTree, ConfigNode, CfgName, bIsActiveConfiguration);

				bIsActiveConfiguration = false;  // Next enumerated configurations will be switched to(they weren't active on export start)
			}

			if (OriginalConfiguration != null && ConfigurationNames.Length > 1)
			{
				Addin.Instance.LogDebug($"ShowConfiguration2 '{OriginalConfiguration.Name}'");
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
					InDoc.AddExportedComponent(NewNode);

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

			List<string> DisplayStateConfigurations = new List<string>();

			if (!ConfigManager.LinkDisplayStatesToConfigurations)
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

					string DisplayStateConfigName = $"DisplayState_{FDatasmithExporter.SanitizeName(DisplayState)}";
					DisplayStateConfigurations.Add(DisplayStateConfigName);
					Addin.Instance.LogDebug(
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
			IEnumerable<string> ExportedConfigurationNamesOrdered = ExportedConfigurationNames.OrderBy(V => ConfigurationOrder[V.Key]).Select(V => V.Value);

			// Add roots for configurations
			foreach (string CfgName in ExportedConfigurationNamesOrdered)
			{
				FConfigurationData CfgData = new FConfigurationData();
				CfgData.Name = CfgName;
				CfgData.bIsDisplayStateConfiguration = false;
				FConfigurationTree.FillConfigurationData(this, CombinedTree, CfgName, CfgData, false);

				if (!CfgData.IsEmpty())
				{
					FlatConfigurationData.Add(CfgData);
				}
			}
			foreach (string DisplayStateConfigName in DisplayStateConfigurations)
			{
				FConfigurationData CfgData = new FConfigurationData();
				CfgData.Name = DisplayStateConfigName;
				CfgData.bIsDisplayStateConfiguration = true;
				FConfigurationTree.FillConfigurationData(this, CombinedTree, DisplayStateConfigName, CfgData, true);

				if (!CfgData.IsEmpty())
				{
					FlatConfigurationData.Add(CfgData);
				}
			}

			return FlatConfigurationData;
		}

		private static Dictionary<FComponentName, FObjectMaterials> GetComponentMaterials(FDocumentTracker InDoc, string InDisplayState, IConfiguration InConfiguration)
		{
			Dictionary<FComponentName, FObjectMaterials> MaterialsMap = new Dictionary<FComponentName, FObjectMaterials>();

			swDisplayStateOpts_e Option = InDisplayState != null ? swDisplayStateOpts_e.swSpecifyDisplayState : swDisplayStateOpts_e.swThisDisplayState;
			string[] DisplayStates = InDisplayState != null ? new string[] { InDisplayState } : null;

			if (InDoc is FAssemblyDocumentTracker AsmDoc)
			{
				HashSet<FComponentName> ComponentsSet = new HashSet<FComponentName>();

				foreach (FComponentName CompName in AsmDoc.SyncState.ExportedComponentsMap.Keys)
				{
					ComponentsSet.Add(CompName);
				}

				ConcurrentDictionary<FComponentName, FObjectMaterials> ComponentMaterials =
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
				FObjectMaterials PartMaterials = FObjectMaterials.LoadPartMaterials(InDoc, PartDocumentTracker.SwPartDoc, Option, DisplayStates);
				Component2 Comp = InConfiguration.GetRootComponent3(true);

				MaterialsMap.Add(FComponentName.FromCustomString(Comp?.Name2 ?? ""), PartMaterials);
			}

			return MaterialsMap;
		}

		private static void SetComponentTreeMaterials(FConfigurationTree.FComponentTreeNode InComponentTree, Dictionary<FComponentName, FObjectMaterials> InComponentMaterialsMap, string InConfigurationName, bool bIsDisplayState)
		{
			Addin.Instance.LogDebug($"SetComponentTreeMaterials: '{InComponentTree.ComponentName}'");

			FConfigurationTree.FComponentConfig TargetConfig = null;

			if (InConfigurationName != null)
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

			Addin.Instance.LogDebug($"  Materials: {TargetConfig.Materials}");

			foreach (FConfigurationTree.FComponentTreeNode Child in InComponentTree.EnumChildren())
			{
				Addin.Instance.LogIndent();
				SetComponentTreeMaterials(Child, InComponentMaterialsMap, InConfigurationName, bIsDisplayState);
				Addin.Instance.LogDedent();
			}
		}

		private static void CollectComponentsRecursive(FDocumentTracker InDoc, Component2 InComponent,
			FComponentTreeNode InParentNode, FMeshes.FConfiguration Meshes, string CfgName)
		{
			Addin.Instance.LogDebug($"'{InComponent.Name2}'");

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

			// Read transform
			MathTransform ComponentTransform = InComponent.GetTotalTransform(true);
			if (ComponentTransform == null)
			{
				ComponentTransform = InComponent.Transform2;
			}

			if (ComponentTransform != null)
			{
				NewNode.CommonConfig.Transform = MathUtils.ConvertFromSolidworksTransform(ComponentTransform, 100f /*GeomScale*/);

				if (InParentNode.CommonConfig.Transform != null)
				{
					// Convert transform to parent space (original transform value fetched from Solidworks
					// is in the root component's space). Datasmith wants relative transform for variants.
					FMatrix4 ParentTransform = new FMatrix4(InParentNode.CommonConfig.Transform);
					FMatrix4 InverseParentTransform = ParentTransform.Inverse();
					NewNode.CommonConfig.RelativeTransform = FMatrix4.FMatrix4x4Multiply(InverseParentTransform, NewNode.CommonConfig.Transform);
				}
				else
				{
					NewNode.CommonConfig.RelativeTransform = NewNode.CommonConfig.Transform;
				}
			}

			// Process children components
			var Children = (Object[])InComponent.GetChildren();
			if (Children != null && Children.Length > 0)  // Children can be null sometimes and sometimes zero-length array
			{
				NewNode.Children = new List<FConfigurationTree.FComponentTreeNode>();
				foreach (object ObjChild in Children)
				{
					Component2 Child = (Component2)ObjChild;
					Addin.Instance.LogIndent();
					CollectComponentsRecursive(InDoc, Child, NewNode, Meshes, CfgName);
					Addin.Instance.LogDedent();
				}

				// todo: sorting ensures that component order is stable in export(e.g. in variant property bindings order)
				// but probably makes sense to to it at the datasmith export, explicitly
				// especially that Children here begs for a dictionary, not list(child looked up by name form Children list)
				NewNode.Children.Sort((InA, InB) => InA.ComponentId - InB.ComponentId);
			}

			if (InDoc.NeedExportComponent(NewNode, NewNode.CommonConfig))
			{
				if (NewNode.IsPartComponent())
				{
					// Collect meshes to export when it's a part component
					InDoc.AddPartDocument(NewNode);
					Meshes.AddComponentWhichNeedsMesh(NewNode.Component, NewNode.ComponentName, NewNode.ActorName);
				}
			}
			InDoc.AddExportedComponent(NewNode);
		}

		public bool DoesComponentHaveVisibleButDifferentMeshesInConfigs(FComponentName ComponentName, string ConfigNameA, string ConfigNameB)
		{
			return Meshes.DoesComponentHaveVisibleButDifferentMeshesInConfigs(ComponentName, ConfigNameA, ConfigNameB);
		}

		// name/label for variant mesh actor 
		public FActorName GetMeshActorName(string ConfigName, FComponentName ComponentName)
		{
			return FActorName.FromString($"{ComponentName}_{ConfigName}");
		}

		public FMeshName GetMeshName(string ConfigName, FComponentName ComponentName)
		{
			return FMeshName.FromString(ConfigName == MainConfigurationName ? $"{ComponentName}_Mesh" : $"{ComponentName}_{ConfigName}_Mesh");
		}

		// Get mesh for component which has only single mesh configuration
		public FMeshName GetMeshName(FComponentName ComponentName)
		{
			string ConfigName = Meshes.GetConfigurationNameForComponentMesh(ComponentName);
			return GetMeshName(ConfigName, ComponentName);
		}
	}
}