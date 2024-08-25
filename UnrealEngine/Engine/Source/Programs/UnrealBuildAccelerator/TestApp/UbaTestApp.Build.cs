// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class UbaTestApp : ModuleRules
{
	public UbaTestApp(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PrivateDefinitions.AddRange(new string[] {
			"_CONSOLE",
		});

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Linux))
		{
			PublicSystemLibraries.Add("dl");
		}
	}
}
