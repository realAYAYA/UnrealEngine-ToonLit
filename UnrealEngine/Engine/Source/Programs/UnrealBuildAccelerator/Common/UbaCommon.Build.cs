// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class UbaCommon : ModuleRules
{
	public UbaCommon(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivatePCHHeaderFile = "../Core/Public/UbaCorePch.h";

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
		StaticAnalyzerDisabledCheckers.Clear();

		PublicDependencyModuleNames.AddRange(new string[] {
			"UbaCore",
		});

		// External modules
		PrivateDependencyModuleNames.AddRange(new string[] {
			"UbaMimalloc",
		});

		PublicDependencyModuleNames.AddRange(new string[] {
			"OodleDataCompression",
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"Detours",
			});
		}
		else if (Target.Platform.IsInGroup(UnrealPlatformGroup.Linux))
		{
			PublicSystemLibraries.Add("dl");
		}

		PrivateDefinitions.AddRange(new string[] {
			"_CONSOLE",
		});
	}
}
