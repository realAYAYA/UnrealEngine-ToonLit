// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class UbaDetours : ModuleRules
{
	public UbaDetours(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;
		StaticAnalyzerDisabledCheckers.Clear();
		bUseUnity = false;

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UbaCore"
		});

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"Detours",
			});

			PublicSystemLibraries.AddRange(new string[] {
				"ntdll.lib",
				"onecore.lib"
			});
		}
		else if (Target.Platform.IsInGroup(UnrealPlatformGroup.Linux))
		{
			PublicSystemLibraries.Add("dl");
		}
	}
}
