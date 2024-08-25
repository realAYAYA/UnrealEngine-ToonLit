// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
public class PhysicsControlEditor : ModuleRules
{
	public PhysicsControlEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] 
			{
				// ... add other private include paths required here ...
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
				"AnimationCore",
				"AnimationEditMode",
				"AnimGraph",
				"AnimGraphRuntime",
				"ApplicationCore",
				"BlueprintGraph",
				"ComponentVisualizers",
				"Core",
				"CoreUObject",
				"EditorFramework",
				"Engine",
				"InputCore",
				"Persona",
				"PhysicsControl",
				"PhysicsControlUncookedOnly",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"ToolWidgets",
				"UnrealEd",
			}
			);

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
						"AnimationEditor",
						"AnimGraph",
						"BlueprintGraph",
						"EditorFramework",
						"Kismet",
						"UnrealEd",
				}
			);
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
