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
				"LauncherPlatform",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ApplicationCore",
				"Json",
			}
		);

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			if(Target.Type == TargetType.Editor)
			{
				PrivateIncludePathModuleNames.Add("SlateFontDialog");
				DynamicallyLoadedModuleNames.Add("SlateFontDialog");
			}

			PrivateIncludePathModuleNames.Add("SlateFileDialogs");
			DynamicallyLoadedModuleNames.Add("SlateFileDialogs");

			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
