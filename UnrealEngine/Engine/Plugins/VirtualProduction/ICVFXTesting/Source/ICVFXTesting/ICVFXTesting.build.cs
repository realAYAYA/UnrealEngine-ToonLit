// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ICVFXTesting : ModuleRules
{
	public ICVFXTesting(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"CoreUObject",
				"Gauntlet"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CinematicCamera",
            	"DisplayCluster",
				"Engine",
                "LiveLink",
                "LiveLinkComponents",
				"LiveLinkInterface",
				"RenderCore"
			}
        );
	}
}
