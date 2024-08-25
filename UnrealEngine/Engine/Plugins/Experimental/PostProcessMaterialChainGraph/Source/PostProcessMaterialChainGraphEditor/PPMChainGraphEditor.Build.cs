// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PPMChainGraphEditor : ModuleRules
	{
		public PPMChainGraphEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AssetDefinition",
					"Core",
					"CoreUObject",
					"EditorFramework",
					"PPMChainGraph",
					"UnrealEd",
					"Slate",
					"SlateCore",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					
				});

			PrivateIncludePaths.AddRange(
				new string[] {
				});
		}
	}
}
