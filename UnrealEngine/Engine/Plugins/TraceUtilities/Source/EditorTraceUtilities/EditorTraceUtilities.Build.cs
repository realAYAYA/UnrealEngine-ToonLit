// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EditorTraceUtilities : ModuleRules
{
    public EditorTraceUtilities(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"MessageLog",
			}
		);

		PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "ToolWidgets",
                "EditorFramework",
                "ApplicationCore",
                "UATHelper",
                "TraceInsights",
                "TraceAnalysis"
            }
        );
    }
}