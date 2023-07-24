// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class JsonBlueprintUtilities : ModuleRules
{
	public JsonBlueprintUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
                "CoreUObject",
                "Engine",
            });

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Json",
				"JsonUtilities"
			});
	}
}
