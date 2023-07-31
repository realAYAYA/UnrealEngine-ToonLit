// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AppleVision : ModuleRules
	{
		public AppleVision(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "Engine",
                    "AppleImageUtils"
                    // ... add other public dependencies that you statically link with here ...
                }
                );

    		if (Target.Platform == UnrealTargetPlatform.IOS)
    		{
                PublicFrameworks.AddRange(
                    new string[]
                    {
                        "CoreImage",
                        "Vision"
                        // ... add other public dependencies that you statically link with here ...
                    }
                    );
            }

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
    				"CoreUObject"
					// ... add private dependencies that you statically link with here ...
				}
				);
		}
	}
}
