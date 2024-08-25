// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class USDClasses : ModuleRules
	{
		public USDClasses(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					"Core",
					"CoreUObject",
					"Engine",
					"Slate",
					"SlateCore"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DeveloperSettings",
					"GeometryCache",
					"GeometryFramework", // For BaseDynamicMeshSceneProxy.h
					"HairStrandsCore",
					"Json", // To read/write plugInfo.json files from UnrealUSDWrapper.cpp
					"MeshDescription",
					"RenderCore", // For UUsdDrawModeComponent implementation
					"RHI",
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"MaterialEditor", // For SetMaterialInstanceParent, when managing the DisplayColor materials
					}
				);
			}
		}
	}
}
