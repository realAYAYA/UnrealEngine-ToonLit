// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class EnhancedInput : ModuleRules
	{
        public EnhancedInput(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "ApplicationCore",
					"Core",
					"CoreUObject",
                    "Engine",
					"InputCore",
					"Slate",
                    "SlateCore",
                    "DeveloperSettings",
                }
            );
        }
    }
}