// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EditorTraceUtilities : ModuleRules
{
	public EditorTraceUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"MessageLog",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"TraceAnalysis",
				"TraceInsights",
				"TraceLog",
				"UATHelper",
			}
		);
	}
}
