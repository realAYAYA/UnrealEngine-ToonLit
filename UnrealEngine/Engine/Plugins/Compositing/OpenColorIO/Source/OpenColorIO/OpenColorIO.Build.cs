// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	using System.IO;

	public class OpenColorIO : ModuleRules
	{
		public OpenColorIO(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Projects",
					"RHI",
					"RenderCore",
					"Renderer",
					"ImageCore",
					"Slate",
					"SlateCore",
					"ColorManagement"
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"DerivedDataCache",
					"OpenColorIOWrapper" //for OpenColorIOWrapperDefines.h
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"DeveloperSettings"
				});

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"TargetPlatform",
						"EditorFramework",
						"UnrealEd",
						"OpenColorIOWrapper"
					});
			}
		}
	}
}
