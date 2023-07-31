// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class HLMediaEditor : ModuleRules
	{
		public HLMediaEditor(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "UnrealEd",
                    "DesktopWidgets",             
                    "Slate",
                    "SlateCore",
                    "MediaAssets",
                    "HLMedia",
                });

            PrivateIncludePaths.AddRange(
                new string[] {
                    "HLMedia/Private",
                });
        }
    }
}
