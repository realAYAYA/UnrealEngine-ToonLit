// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RemoteSessionEditor : ModuleRules
{
	public RemoteSessionEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "../RemoteSession/")
				// ... add public include paths required here ...
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"RemoteSession",
				"UMG",
				"UMGEditor",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"CoreUObject",
				"EditorFramework",			
				"Engine",
				"LevelEditor",
				"MediaIOCore",
				"RenderCore",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"WorkspaceMenuStructure",
			}
		);
	}
}
