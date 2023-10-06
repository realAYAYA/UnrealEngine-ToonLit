// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DesktopPlatform : ModuleRules
{
	public DesktopPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DerivedDataCache",
				"SlateFontDialog",
				"SlateFileDialogs",
				"LauncherPlatform"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ApplicationCore",
				"Json"
			}
		);
		

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			if(Target.Type == TargetType.Editor)
			{
				DynamicallyLoadedModuleNames.Add("SlateFontDialog");
			}
			
			DynamicallyLoadedModuleNames.Add("SlateFileDialogs");

			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
