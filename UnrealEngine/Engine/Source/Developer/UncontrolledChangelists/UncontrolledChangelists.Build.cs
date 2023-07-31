// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UncontrolledChangelists : ModuleRules
{
	public UncontrolledChangelists(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add(ModuleDirectory + "/Public");
		PrivateIncludePaths.Add(ModuleDirectory + "/Private");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"SlateCore",
				"InputCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                
				"Json",
				"SourceControl",
				"AssetRegistry",
			}
		);

		if (Target.bBuildEditor)
        {
			PrivateDependencyModuleNames.AddRange(
                new string[]
				{
					"EditorFramework",
					"Engine",
					"UnrealEd",
					"PackagesDialog",
				}
			);
        }
	}
}
