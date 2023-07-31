// Copyright Epic Games, Inc. All Rights Reserved.

using SolidWorks.Interop.sldworks;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;

namespace DatasmithSolidworks
{
	[ComVisible(false)]
	public class FConfigurationTree
	{
		// Component state for a single configuration
		public class FComponentConfig
		{
			public string ConfigName = null;

			public bool bVisible = true;
			public bool bSuppressed = false;
			
			// Node transform. Null value if transform is not changed in this configuration.
			public float[] Transform = null;
			public float[] RelativeTransform = null;

			// Null value if configuration doesn't override the material.
			public FObjectMaterials Materials = null;

			public List<Body2> Bodies = null;

			public void CopyFrom(FComponentConfig InOther)
			{
				bVisible = InOther.bVisible;
				bSuppressed = InOther.bSuppressed;
				Materials = InOther.Materials;
				Transform = InOther.Transform;
				RelativeTransform = InOther.RelativeTransform;
				Bodies = InOther.Bodies;
			}
		};

		// A node contains all the configuration data for a single component
		public class FComponentTreeNode
		{
			public FComponentName ComponentName;
			public int ComponentID;

			// Common configuration data
			public FComponentConfig CommonConfig = new FComponentConfig();

			// Per-configuration data
			public List<FComponentConfig> Configurations = null;
			public List<FComponentConfig> DisplayStates = null;

			public bool bVisibilitySame = true;
			public bool bSuppressionSame = true;
			public bool bMaterialSame = true;
			public bool bGeometrySame = true;

			public HashSet<FActorName> Meshes = null;

			public List<FComponentTreeNode> Children;

			public FComponentConfig AddConfiguration(string InConfigurationName, bool bIsDisplayState)
			{
				List<FComponentConfig> TargetList = null;

				if (bIsDisplayState)
				{
					if (DisplayStates == null)
					{
						DisplayStates = new List<FComponentConfig>();
					}
					TargetList = DisplayStates;
				}
				else
				{
					if (Configurations == null)
					{
						Configurations = new List<FComponentConfig>();
					}
					TargetList = Configurations;
				}
				FComponentConfig Result = new FComponentConfig();
				Result.ConfigName = InConfigurationName;
				TargetList.Add(Result);

				return Result;
			}

			public FComponentConfig GetConfiguration(string InConfigurationName, bool bIsDisplayState)
			{
				List<FComponentConfig> TargetList = null;

				if (bIsDisplayState)
				{
					TargetList = DisplayStates;
				}
				else
				{
					TargetList = Configurations;
				}

				if (TargetList == null)
				{
					return null;
				}

				foreach (FComponentConfig Config in TargetList)
				{
					if (Config.ConfigName == InConfigurationName)
					{
						return Config;
					}
				}
				return null;
			}

			// Add all parameters except those which could be configuration-specific
			public void AddParametersFrom(FComponentTreeNode InInput)
			{
				ComponentName = InInput.ComponentName;
				ComponentID = InInput.ComponentID;
			}
		};

		static public void Merge(FComponentTreeNode OutCombined, FComponentTreeNode InTree, string InConfigurationName)
		{
			if (InTree.Children == null)
			{
				return;
			}

			if (OutCombined.Children == null)
			{
				OutCombined.Children = new List<FComponentTreeNode>();
			}

			foreach (FComponentTreeNode Child in InTree.Children)
			{
				// Find the same node in 'combined', merge parameters
				FComponentTreeNode CombinedChild = OutCombined.Children.FirstOrDefault(X => X.ComponentName == Child.ComponentName);

				if (CombinedChild == null)
				{
					// The node doesn't exist yet, so add it
					CombinedChild = new FComponentTreeNode();
					CombinedChild.AddParametersFrom(Child);
					OutCombined.Children.Add(CombinedChild);

					// Copy common materials, which should be "default" for the component. Do not propagate these
					// material to configurations, so configuration will only have material when it is changed.
					OutCombined.CommonConfig.Materials = Child.CommonConfig.Materials;
				}

				// Make a NodeConfig and copy parameter values from 'tree' node
				FComponentConfig NodeConfig = CombinedChild.AddConfiguration(InConfigurationName, false);
				NodeConfig.CopyFrom(Child.CommonConfig);

				// Recurse to children
				Merge(CombinedChild, Child, InConfigurationName);
			}
		}

		static public void Compress(FComponentTreeNode InNode, FConfigurationExporter ConfigurationExporter)
		{
			bool CheckMaterialsEqual(List<FComponentConfig> ConfigList)
			{
				if (ConfigList == null || ConfigList.Count <= 1)
				{
					return true;
				}

				FObjectMaterials Materials = ConfigList[0].Materials;
				bool bAllMaterialsAreSame = true;
				for (int Idx = 1; Idx < ConfigList.Count; Idx++)
				{
					if (Materials != null && (!ConfigList[Idx].Materials?.EqualMaterials(Materials) ?? false))
					{
						bAllMaterialsAreSame = false;
						break;
					}
				}

				return bAllMaterialsAreSame;
			}

			if (InNode.Configurations != null && InNode.Configurations.Count > 0)
			{
				// Check transform
				float[] Transform = InNode.Configurations[0].RelativeTransform;
				bool bAllTransformsAreSame = true;
				if (Transform != null)
				{
					// There could be components without a transform, so we're checking if for null
					for (int Idx = 1; Idx < InNode.Configurations.Count; Idx++)
					{
						if (!InNode.Configurations[Idx].RelativeTransform.SequenceEqual(Transform))
						{
							bAllTransformsAreSame = false;
							break;
						}
					}
					if (bAllTransformsAreSame)
					{
						InNode.CommonConfig.RelativeTransform = Transform;
						foreach (FComponentConfig Config in InNode.Configurations)
						{
							Config.RelativeTransform = null;
						}
					}
				}

				bool bAllMaterialsAreSame = CheckMaterialsEqual(InNode.Configurations);

				if (bAllMaterialsAreSame && InNode.Configurations[0].Materials != null)
				{
					// We're explicitly checking for 'material != null' to not erase default
					// material when no overrides detected.
					InNode.CommonConfig.Materials = InNode.Configurations[0].Materials;
					foreach (FComponentConfig Config in InNode.Configurations)
					{
						Config.Materials = null;
					}
				}

				bool bVisible = InNode.Configurations[0].bVisible;
				bool bSuppressed = InNode.Configurations[0].bSuppressed;
				bool bVisibilitySame = true;
				bool bSuppressionSame = true;
				bool bGeometrySame = true;

				for (int Idx = 1; Idx < InNode.Configurations.Count; Idx++)
				{
					if (InNode.Configurations[Idx].bVisible != bVisible)
					{
						bVisibilitySame = false;
					}

					if (InNode.Configurations[Idx].bSuppressed != bSuppressed)
					{
						bSuppressionSame = false;
					}

					if (!ConfigurationExporter.IsSameMesh(InNode.ComponentName, InNode.Configurations[0].ConfigName, InNode.Configurations[Idx].ConfigName))
					{
						bGeometrySame = false;
					}
				}

				if (!bGeometrySame)
				{
					Debug.Assert(InNode.Meshes == null);
					InNode.Meshes = new HashSet<FActorName>();

					foreach (FComponentConfig ComponentConfig in InNode.Configurations)
					{
						InNode.Meshes.Add(ConfigurationExporter.GetMeshActorName(InNode.ComponentName,
							ComponentConfig.ConfigName));
					}
				}

				// Propagate common values
				InNode.bMaterialSame = bAllMaterialsAreSame;
				InNode.bVisibilitySame = bVisibilitySame;
				InNode.bSuppressionSame = bSuppressionSame;
				InNode.bGeometrySame = bGeometrySame;

				if (bVisibilitySame)
				{
					InNode.CommonConfig.bVisible = bVisible;
				}
				if (bSuppressionSame)
				{
					InNode.CommonConfig.bSuppressed = bSuppressed;
				}

				//todo: store bAll...Same in ConfigData

				// If EVERYTHING is same, just remove all configurations at all
				if (bAllTransformsAreSame && bAllMaterialsAreSame && bVisibilitySame && bSuppressionSame && bGeometrySame)
				{
					InNode.Configurations = null;
				}
			}

			if (InNode.DisplayStates != null && InNode.DisplayStates.Count > 0)
			{
				bool bAllDisplayStateMaterialsAreSame = CheckMaterialsEqual(InNode.DisplayStates);

				InNode.bMaterialSame = bAllDisplayStateMaterialsAreSame;

				if (bAllDisplayStateMaterialsAreSame)
				{
					InNode.DisplayStates = null;
				}
			}

			// Recurse to children
			if (InNode.Children != null)
			{
				foreach (FComponentTreeNode Child in InNode.Children)
				{
					Compress(Child, ConfigurationExporter);
				}
			}
		}

		static public void FillConfigurationData(FConfigurationExporter ConfigurationExporter, FComponentTreeNode InNode, string InConfigurationName, FConfigurationData OutData, bool bIsDisplayState)
		{
			FComponentConfig NodeConfig = InNode.GetConfiguration(InConfigurationName, bIsDisplayState);

			// Visibility or suppression flags are set per node, and not propagated to children.
			// Also, suppression doesn't mark node as invisible. We should process these separately
			// to exclude any variant information from invisible nodes and their children.
			if ((InNode.bVisibilitySame && !InNode.CommonConfig.bVisible) || (NodeConfig != null && !NodeConfig.bVisible) ||
				(InNode.bSuppressionSame && InNode.CommonConfig.bSuppressed) || (NodeConfig != null && NodeConfig.bSuppressed))
			{
				// This node is not visible
				OutData.ComponentVisibility.Add(InNode.ComponentName, false);
				// Do not process children of this node
				return;
			}

			// Process current configuration
			if (NodeConfig != null)
			{
				// Only add variant information if attribute is not the same in all configurations
				if (NodeConfig.RelativeTransform != null)
				{
					OutData.ComponentTransform.Add(InNode.ComponentName, NodeConfig.RelativeTransform);
				}
				if (!InNode.bVisibilitySame)
				{
					OutData.ComponentVisibility.Add(InNode.ComponentName, NodeConfig.bVisible);
				}
				if (!InNode.bSuppressionSame)
				{
					OutData.ComponentVisibility.Add(InNode.ComponentName, !NodeConfig.bSuppressed);
				}
				if (!InNode.bGeometrySame)
				{
					OutData.ComponentGeometry.Add(InNode.ComponentName, new FConfigurationData.FComponentGeometryVariant
						{
							VisibleActor = ConfigurationExporter.GetMeshActorName(InNode.ComponentName, InConfigurationName),
							All = InNode.Meshes.ToList()
						}
					);
				}
				if (!InNode.bMaterialSame)
				{
					FObjectMaterials Materials = NodeConfig.Materials;
					if (Materials == null)
					{
						// There's no material set for the config, use default one
						Materials = InNode.CommonConfig.Materials;
					}
					if (Materials != null)
					{
						OutData.ComponentMaterials.Add(InNode.ComponentName, Materials);
					}
				}
			}

			// Recurse to children
			if (InNode.Children != null)
			{
				foreach (FComponentTreeNode Child in InNode.Children)
				{
					FillConfigurationData(ConfigurationExporter, Child, InConfigurationName, OutData, bIsDisplayState);
				}
			}
		}
	}
}
