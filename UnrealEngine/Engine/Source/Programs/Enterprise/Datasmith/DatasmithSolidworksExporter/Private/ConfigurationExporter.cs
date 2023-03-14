// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using SolidWorks.Interop.sldworks;
using SolidWorks.Interop.swconst;
using System.Runtime.InteropServices;
using System.Collections.Concurrent;
using System.Linq;

namespace DatasmithSolidworks
{
	[ComVisible(false)]
	public class FConfigurationData
	{
		public string Name;
		public bool bIsDisplayStateConfiguration = false;
		public Dictionary<FComponentName, float[]> ComponentTransform = new Dictionary<FComponentName, float[]>();
		public Dictionary<FComponentName, bool> ComponentVisibility = new Dictionary<FComponentName, bool>();
		public Dictionary<FComponentName, FObjectMaterials> ComponentMaterials = new Dictionary<FComponentName, FObjectMaterials>();

		[ComVisible(false)]
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

	[ComVisible(false)]
	public class FMeshes
	{
		private Dictionary<string, IConfiguration> Configurations = new Dictionary<string, IConfiguration>();

		private HashSet<FMesh> Meshes = new HashSet<FMesh>();

		[ComVisible(false)]
		public struct FMesh
		{
			public FMeshData MeshData;

			private readonly int Hash;

			public FMesh(FMeshData InMeshData)
			{

				MeshData = InMeshData;

				Hash = 0;

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

		[ComVisible(false)]
		public interface IConfiguration
		{
			void AddMesh(Component2 Component, FMeshData Mesh);
			bool GetMeshForComponent(FComponentName ComponentName, out FMesh Result);
		}

		public IConfiguration GetMeshesConfiguration(string ConfigurationName)
		{
			if (Configurations.TryGetValue(ConfigurationName, out var Configuration))
			{
				return Configuration;
			}			

			Configuration = new FConfigurationImpl(this, ConfigurationName);
			Configurations.Add(ConfigurationName, Configuration);
			return Configuration;
		}

		public bool IsSameMesh(FComponentName ComponentName, string ConfigNameA, string ConfigNameB)
		{
			return GetMeshesConfiguration(ConfigNameA).GetMeshForComponent(ComponentName, out FMesh MeshA)
			       && GetMeshesConfiguration(ConfigNameB).GetMeshForComponent(ComponentName, out FMesh MeshB)
			       && MeshA == MeshB;
		}


		public FMeshData GetMeshData(string ConfigName, FComponentName ComponentName)
		{
			return GetMeshesConfiguration(ConfigName).GetMeshForComponent(ComponentName, out FMesh Mesh) ? Mesh.MeshData : null;
		}

		private class FConfigurationImpl : IConfiguration
		{
			private Dictionary<FComponentName, FMesh> MeshForComponent = new Dictionary<FComponentName, FMesh>();

			public FConfigurationImpl(FMeshes Meshes, string Name)
			{
			}

			public void AddMesh(Component2 Component, FMeshData Mesh)
			{
				MeshForComponent.Add(new FComponentName(Component), new FMesh(Mesh));
			}

			public bool GetMeshForComponent(FComponentName ComponentName, out FMesh Result)
			{
				return MeshForComponent.TryGetValue(ComponentName, out Result);
			}

		}
	}

	[ComVisible(false)]
	public class FConfigurationExporter
	{
		private readonly FMeshes Meshes;

		public FConfigurationExporter(FMeshes Meshes)
		{
			this.Meshes = Meshes;
		}

		public List<FConfigurationData> ExportConfigurations(FDocument InDoc)
		{
			string[] CfgNames = InDoc.SwDoc?.GetConfigurationNames();

			if (CfgNames == null || CfgNames.Length <= 1)
			{
				return null;
			}

			FConfigurationTree.FComponentTreeNode CombinedTree = new FConfigurationTree.FComponentTreeNode();
			CombinedTree.ComponentName = FComponentName.FromCustomString("CombinedTree");

			// Ensure recursion will not stop on the root node (this may happen if it is not explicitly marked as visible)
			CombinedTree.bVisibilitySame = true;
			CombinedTree.CommonConfig.bVisible = true;

			ConfigurationManager ConfigManager = InDoc.SwDoc.ConfigurationManager;
			IConfiguration OriginalConfiguration = ConfigManager.ActiveConfiguration;

			List<string> ExportedConfigurationNames = new List<string>();

			foreach (string CfgName in CfgNames)
			{
				IConfiguration swConfiguration = InDoc.SwDoc.GetConfigurationByName(CfgName) as IConfiguration;

				InDoc.SwDoc.ShowConfiguration2(CfgName);

				string ConfigName = CfgName;

				int DisplayStateCount = swConfiguration.GetDisplayStatesCount();
				string[] DisplayStates = null;

				if (ConfigManager.LinkDisplayStatesToConfigurations && DisplayStateCount > 1)
				{
					DisplayStates = swConfiguration.GetDisplayStates();
					ConfigName = $"{CfgName}_{DisplayStates[0]}";
				}

				FConfigurationTree.FComponentTreeNode ConfigNode = new FConfigurationTree.FComponentTreeNode();
				ConfigNode.Children = new List<FConfigurationTree.FComponentTreeNode>();
				ConfigNode.ComponentName = FComponentName.FromCustomString(ConfigName);


				// Build the tree and get default materials (which aren't affected by any configuration)
				// Use GetRootComponent3() with Resolve = true to ensure suppressed components will be loaded
				// todo: docs says that Part document SW returns null. Not the case. But probably better to add a guard
				CollectComponentsRecursive(InDoc, swConfiguration.GetRootComponent3(true), ConfigNode);

				ExportedConfigurationNames.Add(ConfigName);

				Dictionary<FComponentName, FObjectMaterials> MaterialsMap = GetComponentMaterials(InDoc, DisplayStates != null ? DisplayStates[0] : null, swConfiguration);
				SetComponentTreeMaterials(ConfigNode, MaterialsMap, null, false);

				if (DisplayStates != null)
				{
					// Export display states here: they are linked to configurations.
					// Skip first as it was already exported above.
					for (int Index = 1; Index < DisplayStateCount; ++Index)
					{
						string DisplayState = DisplayStates[Index];
						MaterialsMap = GetComponentMaterials(InDoc, DisplayState, swConfiguration);
						string DisplayStateTreeName = $"{CfgName}_DisplayState_{FDatasmithExporter.SanitizeName(DisplayState)}";
						SetComponentTreeMaterials(ConfigNode, MaterialsMap, DisplayStateTreeName, false);
					}
				}

				// Combine separate scene trees into the single one with configuration-specific data
				FConfigurationTree.Merge(CombinedTree, ConfigNode, CfgName);
			}

			if (OriginalConfiguration != null)
			{
				InDoc.SwDoc.ShowConfiguration2(OriginalConfiguration.Name);
			}

			List<string> DisplayStateConfigurations = new List<string>();

			if (!ConfigManager.LinkDisplayStatesToConfigurations)
			{
				// Export display states as separate configurations
				string[] DisplayStates = OriginalConfiguration.GetDisplayStates();

				foreach (string DisplayState in DisplayStates)
				{
					string DisplayStateConfigName = $"DisplayState_{FDatasmithExporter.SanitizeName(DisplayState)}";
					DisplayStateConfigurations.Add(DisplayStateConfigName);
					Dictionary<FComponentName, FObjectMaterials> MaterialsMap = GetComponentMaterials(InDoc, DisplayState, OriginalConfiguration);

					SetComponentTreeMaterials(CombinedTree, MaterialsMap, DisplayStateConfigName, true);
				}
			}

			// Remove configuration data when it's the same
			FConfigurationTree.Compress(CombinedTree, this);

			List<FConfigurationData> FlatConfigurationData = new List<FConfigurationData>();

			// Add roots for configurations
			foreach (string CfgName in ExportedConfigurationNames)
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

		private static Dictionary<FComponentName, FObjectMaterials> GetComponentMaterials(FDocument InDoc, string InDisplayState, IConfiguration InConfiguration)
		{
			Dictionary<FComponentName, FObjectMaterials> MaterialsMap = new Dictionary<FComponentName, FObjectMaterials>();

			swDisplayStateOpts_e Option = InDisplayState != null ? swDisplayStateOpts_e.swSpecifyDisplayState : swDisplayStateOpts_e.swThisDisplayState;
			string[] DisplayStates = InDisplayState != null ? new string[] { InDisplayState } : null;

			if (InDoc is FAssemblyDocument)
			{
				FAssemblyDocument AsmDoc = InDoc as FAssemblyDocument;

				HashSet<FComponentName> InComponentsSet = new HashSet<FComponentName>();

				foreach (FComponentName CompName in AsmDoc.SyncState.ExportedComponentsMap.Keys)
				{
					InComponentsSet.Add(CompName);
				}

				ConcurrentDictionary<FComponentName, FObjectMaterials> ComponentMaterials =
					FObjectMaterials.LoadAssemblyMaterials(AsmDoc, InComponentsSet, Option, DisplayStates);

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
				FPartDocument PartDocument = InDoc as FPartDocument;
				FObjectMaterials PartMaterials = FObjectMaterials.LoadPartMaterials(InDoc, PartDocument.SwPartDoc, Option, DisplayStates);
				Component2 Comp = InConfiguration.GetRootComponent3(true);

				MaterialsMap.Add(FComponentName.FromCustomString(Comp?.Name2 ?? ""), PartMaterials);
			}

			return MaterialsMap;
		}

		private static void SetComponentTreeMaterials(FConfigurationTree.FComponentTreeNode InComponentTree, Dictionary<FComponentName, FObjectMaterials> InComponentMaterialsMap, string InConfigurationName, bool bIsDisplayState)
		{
			FConfigurationTree.FComponentConfig TargetConfig = null;

			if (InConfigurationName != null)
			{
				TargetConfig = InComponentTree.GetConfiguration(InConfigurationName, bIsDisplayState);
				if (TargetConfig == null)
				{
					TargetConfig = InComponentTree.AddConfiguration(InConfigurationName, bIsDisplayState);
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

			if (InComponentTree.Children != null)
			{
				foreach (FConfigurationTree.FComponentTreeNode Child in InComponentTree.Children)
				{
					SetComponentTreeMaterials(Child, InComponentMaterialsMap, InConfigurationName, bIsDisplayState);
				}
			}
		}

		private static void CollectComponentsRecursive(FDocument InDoc, Component2 InComponent, FConfigurationTree.FComponentTreeNode InParentNode)
		{
			FConfigurationTree.FComponentTreeNode NewNode = new FConfigurationTree.FComponentTreeNode();
			InParentNode.Children.Add(NewNode);

			// Basic properties
			NewNode.ComponentName = new FComponentName(InComponent);
			NewNode.ComponentID = InComponent.GetID();
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
					CollectComponentsRecursive(InDoc, Child, NewNode);
				}

				NewNode.Children.Sort(delegate (FConfigurationTree.FComponentTreeNode InA, FConfigurationTree.FComponentTreeNode InB)
				{
					return InA.ComponentID - InB.ComponentID;
				});
			}
		}

		public bool IsSameMesh(FComponentName ComponentName, string ConfigNameA, string ConfigNameB)
		{
			return Meshes.IsSameMesh(ComponentName, ConfigNameA, ConfigNameB);
		}

		public FActorName GetMeshActorName(FComponentName ComponentName, string ConfigName)
		{
			return FActorName.FromString(ComponentName.GetString() + "_" + ConfigName);
		}
	}
}