// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MobileLauncherProfileWizard : ModuleRules
	{
		public MobileLauncherProfileWizard(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
                    "Core",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                    "LauncherServices",
                    "CoreUObject",
                    "TargetPlatform",
                    "DesktopPlatform",
                    "Json",
                    "Slate",
                    "SlateCore",
                    "InputCore",
                    "AppFramework",
                    "Projects",
                    // ... add private dependencies that you statically link with here ...
				}
                );
		}
	}
}
