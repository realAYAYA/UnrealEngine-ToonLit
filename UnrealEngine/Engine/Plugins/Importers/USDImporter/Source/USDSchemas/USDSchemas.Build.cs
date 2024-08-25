// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class USDSchemas : ModuleRules
	{
		public USDSchemas(ReadOnlyTargetRules Target) : base(Target)
		{
			// Does not compile with C++20:
			// error C4002: too many arguments for function-like macro invocation 'TF_PP_CAT_IMPL'
			// warning C5103: pasting '"TF_LOG_STACK_TRACE_ON_ERROR"' and '"TF_LOG_STACK_TRACE_ON_WARNING"' does not result in a valid preprocessing token
			CppStandard = CppStandardVersion.Cpp17;

			// Replace with PCHUsageMode.UseExplicitOrSharedPCHs when this plugin can compile with cpp20
			PCHUsage = PCHUsageMode.NoPCHs;

			bUseRTTI = true;

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Boost",
					"CinematicCamera",
					"Core",
					"CoreUObject",
					"Engine",
					"GeometryCache",
					"GeometryCore", // for FitKDOP
					"InterchangeCore",
					"InterchangeEngine",
					"InterchangeFactoryNodes",
					"InterchangeNodes",
					"InterchangePipelines",
					"LiveLinkAnimationCore",
					"LiveLinkComponents",
					"LiveLinkInterface",
					"MeshDescription",
					"RenderCore",
					"RHI", // For FMaterialUpdateContext and the right way of updating material instance constants
					"Slate",
					"SlateCore",
					"StaticMeshDescription",
					"UnrealUSDWrapper",
					"USDClasses",
					"USDUtilities",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"HairStrandsCore",
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"BlueprintGraph",
						"GeometryCacheUSD",
						"HairStrandsEditor",
						"Kismet",
						"LiveLinkGraphNode",
						"MaterialEditor",
						"MDLImporter",
						"MeshUtilities",
						"PhysicsUtilities", // For generating UPhysicsAssets for SkeletalMeshes and ConvexDecompTool
						"PropertyEditor",
						"SparseVolumeTexture",
						"UnrealEd",
					}
				);

				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					PrivateDependencyModuleNames.AddRange(
						new string[]
						{
							"MaterialX"
						}
					);
				}
			}
		}
	}
}
