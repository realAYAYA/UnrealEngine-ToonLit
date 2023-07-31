// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class USDUtilities : ModuleRules
	{
		public USDUtilities(ReadOnlyTargetRules Target) : base(Target)
		{
			bUseRTTI = true;

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Boost",
					"Core",
					"CoreUObject",
					"UnrealUSDWrapper",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CinematicCamera",
					"ControlRig",
					"Engine",
					"Foliage",
					"GeometryCache", // Just so that we can fetch its AssetImportData
					"HairStrandsCore",
					"IntelTBB",
					"Landscape", // So that GetSchemaNameForComponent knows what to do with landscape proxies
					"LiveLinkComponents", // For converting LiveLinkComponentController properties to USD
					"MeshDescription",
					"MovieScene",
					"MovieSceneTracks",
					"RenderCore",
					"RHI", // So that we can use GMaxRHIFeatureLevel when force-loading textures before baking materials
					"Slate",
					"SlateCore",
					"StaticMeshDescription",
					"USDClasses",
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"DesktopPlatform", // For OpenFileDialog/SaveFileDialog
						"MaterialBaking", // For the BakeMaterials function
						"MaterialEditor",
						"MeshUtilities",
						"MessageLog",
						"PropertyEditor",
						"UnrealEd",
					}
				);
			}
		}
	}
}
