// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AssetDefinition : ModuleRules
{
	public AssetDefinition(ReadOnlyTargetRules Target) : base(Target)
	{		
        PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"AssetRegistry",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
				"EditorSubsystem"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
			}
		);
		
		PrivateIncludePaths.AddRange(
        	new string[] {
        	}
        );
	}
}
