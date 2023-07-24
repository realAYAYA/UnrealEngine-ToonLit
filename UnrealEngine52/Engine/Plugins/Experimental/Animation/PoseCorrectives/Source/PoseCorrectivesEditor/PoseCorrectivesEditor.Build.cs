// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class PoseCorrectivesEditor : ModuleRules
{
	public PoseCorrectivesEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new  string[]
			{
				"PoseCorrectives",
				"Core",
				"CoreUObject",
                "Engine",
				"AnimationCore",
				"AnimGraph",
				"AnimGraphRuntime",
				"GraphEditor",
				"ControlRig",
				"ControlRigDeveloper",
				"ControlRigEditor",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"InputCore",
				"BlueprintGraph",
				"EditorFramework",
				"Kismet",
				"UnrealEd",
				"Persona",
				"EditorStyle",
				"AnimationBlueprintLibrary"
			}
			);
	}
}
