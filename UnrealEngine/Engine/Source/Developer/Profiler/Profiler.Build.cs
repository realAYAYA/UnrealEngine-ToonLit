// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Profiler : ModuleRules
{
	public Profiler( ReadOnlyTargetRules Target ) : base(Target)
	{
		PublicDependencyModuleNames.AddRange
		(
			new string[] {
				"Core",
				"ApplicationCore",
                "InputCore",
				"RHI",
				"RenderCore",
				"Slate",
				"ProfilerClient",
				"DesktopPlatform",
			}
		);

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
					"Engine",
				}
            );
        }

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"SlateCore",
				"ToolWidgets"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Messaging",
				"SessionServices",
			}
		);
	}
}
