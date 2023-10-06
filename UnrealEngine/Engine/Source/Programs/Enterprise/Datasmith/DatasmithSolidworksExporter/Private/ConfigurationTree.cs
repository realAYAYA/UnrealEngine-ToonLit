// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using SolidWorks.Interop.sldworks;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using DatasmithSolidworks.Names;
using static DatasmithSolidworks.Addin;

namespace DatasmithSolidworks
{
	public struct FConvertedTransform
	{
		public float[] Matrix;

		public FConvertedTransform(float[] Floats)
		{
			Matrix = Floats;
		}

		public bool IsValid()
		{
			return Matrix != null;
		}

		public override string ToString()
		{
			return (Matrix==null ? "null" : string.Join(", ", Matrix));
		}

		public static FConvertedTransform Identity()
		{
			return new FConvertedTransform(new[]
			{
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			});
		}
	}

	public class FConfigurationTree
	{
		// Component state for a single configuration
		public class FComponentConfig
		{
			public readonly FVariantName ConfigName;
			public readonly bool bIsMainActive;  // if this Configuration is the active config

			public bool bVisible = true;
			public bool bSuppressed = false;
			
			// Node transform. Null value if transform is not changed in this configuration.
			public FConvertedTransform Transform;
			public FConvertedTransform RelativeTransform;

			// Null value if configuration doesn't override the material.
			public FObjectMaterials Materials = null;

			public List<Body2> Bodies = null;

			public FComponentConfig(FVariantName InConfigName, bool bInIsMainActive)
			{
				ConfigName = InConfigName;
				bIsMainActive = bInIsMainActive;
			}

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
			public readonly Component2 Component;

			public struct FComponentInfo
			{
				public FComponentName ComponentName;
				public int ComponentId;
				public string PartPath;
			}

			public FComponentInfo ComponentInfo = new FComponentInfo();

			public int ComponentId => ComponentInfo.ComponentId;

			public FComponentName ComponentName => ComponentInfo.ComponentName;
			public FActorName ActorName => ComponentInfo.ComponentName.GetActorName();
			public string PartPath => ComponentInfo.PartPath;

			// Common configuration data
			public FComponentConfig CommonConfig = new FComponentConfig(new FVariantName(), true);

			// Per-configuration data
			public List<FComponentConfig> Configurations = null;
			public List<FComponentConfig> DisplayStates = null;

			public bool bVisibilitySame = true;
			public bool bSuppressionSame = true;
			public bool bMaterialSame = true;
			public bool bGeometrySame = true;

			public HashSet<FActorName> Meshes = null;

			public List<FComponentTreeNode> Children;

			// Traverse tree passing result of function computation for each node to its children
			// Function - receives parent's computed value and current node and returns value computed for the node(to pass to children)
			public void Traverse<T>(T ParentValue, Func<T, FComponentTreeNode, T> Function)
			{
				LogDebug($" {ComponentName}");

				T Value = Function(ParentValue, this);

				if (Children == null)
				{
					return;
				}				
				
				foreach (FComponentTreeNode Child in Children)
				{
					using (LogScopedIndent())
					{
						Child.Traverse(Value, Function);
					}
				}
			}

			public IEnumerable<FComponentTreeNode> EnumChildren()
			{
				if (Children == null)
				{
					yield break;
				}

				foreach (FComponentTreeNode Child in Children)
				{
					yield return Child;
				}
			}

			public FComponentTreeNode(Component2 InComponent=null)
			{
				Component = InComponent;
			}

			public FComponentConfig AddConfiguration(FVariantName InConfigurationName, bool bIsDisplayState, bool bIsActiveConfiguration)
			{
				List<FComponentConfig> TargetList = null;

				Debug.Assert(InConfigurationName.IsValid());

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
				FComponentConfig Result = new FComponentConfig(InConfigurationName, bIsActiveConfiguration);
				TargetList.Add(Result);

				return Result;
			}

			public FComponentConfig GetConfiguration(FVariantName InConfigurationName, bool bIsDisplayState)
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

			public FActorName GetActorName()
			{
				return ComponentName.GetActorName();
			}

			public bool IsPartComponent()
			{
				return ComponentInfo.PartPath != null;
			}
		};

		static public void Merge(FComponentTreeNode OutCombined, FComponentTreeNode InTree, FVariantName InConfigurationName,
			bool bIsActiveConfiguration)
		{
			foreach (FComponentTreeNode Child in InTree.EnumChildren())
			{
				if (OutCombined.Children == null)
				{
					OutCombined.Children = new List<FComponentTreeNode>();
				}

				// Find the same node in 'combined', merge parameters
				FComponentTreeNode CombinedChild = OutCombined.Children.FirstOrDefault(X => X.ComponentName == Child.ComponentName);

				if (CombinedChild == null)
				{
					// The node doesn't exist yet, so add it. copying its info from the input node
					CombinedChild = new FComponentTreeNode(Child.Component)
					{
						ComponentInfo = Child.ComponentInfo
					};

					OutCombined.Children.Add(CombinedChild);

					// Copy common materials, which should be "default" for the component. Do not propagate these
					// material to configurations, so configuration will only have material when it is changed.
					OutCombined.CommonConfig.Materials = Child.CommonConfig.Materials;
				}

				if (CombinedChild.PartPath == null)
				{
					CombinedChild.ComponentInfo.PartPath = Child.PartPath;
				}
				else
				{
					// Either child didn't have PartPath resolved - when it's document is not loaded in this configuration(suppressed) or it's not a part
					// Same component is not expected to have different PartPath in different configurations, seems like by SW design
					Debug.Assert(Child.PartPath == null || CombinedChild.PartPath == Child.PartPath);
				}

				// Make a NodeConfig and copy parameter values from 'tree' node
				FComponentConfig NodeConfig = CombinedChild.AddConfiguration(InConfigurationName, false, bIsActiveConfiguration);
				NodeConfig.CopyFrom(Child.CommonConfig);

				// Copy 'extra' configurations created for this actual configuration - linked display states and exploded views
				if (Child.Configurations != null)
				{
					CombinedChild.Configurations.AddRange(Child.Configurations);
				}

				// Recurse to children
				Merge(CombinedChild, Child, InConfigurationName, bIsActiveConfiguration);
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
					FObjectMaterials OtherMaterials = ConfigList[Idx].Materials;
					if (Materials != null && OtherMaterials != null)
					{
						if (!OtherMaterials.EqualMaterials(Materials))
						{
							bAllMaterialsAreSame = false;
							break;
						}
					}
					else
					{
						if (Materials != OtherMaterials)
						{
							// One of the material sets is null(empty) but another isn't
							bAllMaterialsAreSame = false;
							break;
						}
					}
				}

				return bAllMaterialsAreSame;
			}

			if (InNode.Configurations != null && InNode.Configurations.Count > 0)
			{
				// Check transform
				FConvertedTransform Transform = InNode.Configurations[0].Transform;
				FConvertedTransform RelativeTransform = InNode.Configurations[0].RelativeTransform;
				bool bAllTransformsAreSame = true;
				if (RelativeTransform.IsValid())
				{
					// There could be components without a transform, so we're checking if for null
					for (int Idx = 1; Idx < InNode.Configurations.Count; Idx++)
					{
						FConvertedTransform OtherTransform = InNode.Configurations[Idx].RelativeTransform;
						if (OtherTransform.IsValid() && !MathUtils.TransformsAreEqual(OtherTransform, RelativeTransform))
						{
							bAllTransformsAreSame = false;
							break;
						}
					}
					if (bAllTransformsAreSame)
					{
						InNode.CommonConfig.Transform = Transform;
						InNode.CommonConfig.RelativeTransform = RelativeTransform;
						foreach (FComponentConfig Config in InNode.Configurations)
						{
							Config.RelativeTransform = new FConvertedTransform();
							Config.Transform = new FConvertedTransform();
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

					if (ConfigurationExporter.DoesComponentHaveVisibleButDifferentMeshesInConfigs(InNode.ComponentName, InNode.Configurations[0].ConfigName, InNode.Configurations[Idx].ConfigName))
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
						InNode.Meshes.Add(ConfigurationExporter.GetMeshActorName(ComponentConfig.ConfigName, InNode.ComponentName));
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
			foreach (FComponentTreeNode Child in InNode.EnumChildren())
			{
				Compress(Child, ConfigurationExporter);
			}
		}

		static public void FillConfigurationData(FConfigurationExporter ConfigurationExporter, FComponentTreeNode InNode, FVariantName InConfigurationName, FConfigurationData OutData, bool bIsDisplayState)
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
				if (NodeConfig.RelativeTransform.IsValid())
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
							VisibleActor = ConfigurationExporter.GetMeshActorName(InConfigurationName, InNode.ComponentName),
							All = InNode.Meshes.ToList()
						}
					);
				}
				if (!InNode.bMaterialSame)
				{
					// There's no material set for the config, use default one
					// todo: handle missing materials for a config/display set by making a 'default' material
					FObjectMaterials Materials = NodeConfig.Materials ?? InNode.CommonConfig.Materials;
					if (Materials != null)
					{
						OutData.ComponentMaterials.Add(InNode.ComponentName, Materials);
					}
				}
			}

			// Recurse to children
			foreach (FComponentTreeNode Child in InNode.EnumChildren())
			{
				FillConfigurationData(ConfigurationExporter, Child, InConfigurationName, OutData, bIsDisplayState);
			}
		}
	}
}
