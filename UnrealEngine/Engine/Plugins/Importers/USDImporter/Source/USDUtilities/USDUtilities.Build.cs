// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class USDUtilities : ModuleRules
	{
		public USDUtilities(ReadOnlyTargetRules Target) : base(Target)
		{
			// Does not compile with C++20:
			// error C4002: too many arguments for function-like macro invocation 'TF_PP_CAT_IMPL'
			// warning C5103: pasting '"TF_LOG_STACK_TRACE_ON_ERROR"' and '"TF_LOG_STACK_TRACE_ON_WARNING"' does not result in a valid preprocessing token
			CppStandard = CppStandardVersion.Cpp17;

			// Replace with PCHUsageMode.UseExplicitOrSharedPCHs when this plugin can compile with cpp20
			PCHUsage = PCHUsageMode.NoPCHs;

			bUseRTTI = true;

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Boost",
					"Core",
					"CoreUObject",
					"UnrealUSDWrapper",
					"USDClasses", // So that consumers can also include IUsdClassesModule for the new definition of FDisplayColorMaterial
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
					"OpenSubdiv",
					"RenderCore",
					"RHI", // So that we can use GMaxRHIFeatureLevel when force-loading textures before baking materials
					"Slate",
					"SlateCore",
					"StaticMeshDescription",
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
