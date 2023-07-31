// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LightGizmos : ModuleRules
{
	public LightGizmos(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);


		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add other public dependencies that you statically link with here ...
				"Core"
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"InputCore",
				"UnrealEd",
				"SlateCore",
				"CoreUObject",
				"EditorInteractiveToolsFramework",
				"InteractiveToolsFramework",
				"GizmoEdMode",
				"LevelEditor",
				// ... add private dependencies that you statically link with here ...
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
