// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GPULightmass : ModuleRules
	{
		public GPULightmass(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
					"../Shaders/Shared"
				}
				);

			PrivateIncludePaths.AddRange(
				new string[]
				{
					System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
				}
			);
			
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
					"Landscape",
					"RenderCore",
					"Renderer",
					"RHI",
					"Projects",
					"EditorFramework",
					"UnrealEd",
					"SlateCore",
					"Slate",
					"IntelOIDN",
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);
		}
	}
}
