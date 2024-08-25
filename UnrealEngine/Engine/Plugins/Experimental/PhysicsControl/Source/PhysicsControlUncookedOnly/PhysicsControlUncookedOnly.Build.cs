// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
public class PhysicsControlUncookedOnly : ModuleRules
{
	public PhysicsControlUncookedOnly(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AnimationCore",
				"AnimGraphRuntime",
				"Core",
				"CoreUObject",
				"Engine",
				"Persona",
				"PhysicsControl",
				"Slate",
				"SlateCore",
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
	}
}
