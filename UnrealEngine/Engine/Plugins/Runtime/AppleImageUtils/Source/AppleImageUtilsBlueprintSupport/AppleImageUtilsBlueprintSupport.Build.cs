// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AppleImageUtilsBlueprintSupport : ModuleRules
	{
		public AppleImageUtilsBlueprintSupport(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicIncludePaths.AddRange(
                new string[] {
                    // ... add public include paths required here ...
                }
                );


            PrivateIncludePaths.AddRange(
                new string[] {
                    "AppleImageUtils/Public",
                    "AppleImageUtilsBlueprintSupport/Classes"
                    // ... add other private include paths required here ...
                }
                );


            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "Engine",
                    "BlueprintGraph",
                    "AppleImageUtils"
                    // ... add other public dependencies that you statically link with here ...
                }
                );

            PublicFrameworks.AddRange(
                new string[]
                {
                    // ... add other public dependencies that you statically link with here ...
                }
                );

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
    				"CoreUObject"
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
}
