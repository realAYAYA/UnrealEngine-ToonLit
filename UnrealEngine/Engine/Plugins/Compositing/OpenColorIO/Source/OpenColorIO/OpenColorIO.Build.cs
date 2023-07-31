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
					"RenderCore",
					"RHI",
					"RenderCore",
					"Renderer",
					"Slate",
					"SlateCore",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"DerivedDataCache",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"), //required for FPostProcessMaterialInputs
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
						"OpenColorIOLib",
						"TargetPlatform",
						"EditorFramework",
						"UnrealEd"
					});
			}
		}
	}
}
