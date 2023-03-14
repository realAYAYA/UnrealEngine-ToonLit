// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InputCore : ModuleRules
{
	public InputCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject" });

		if(Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
		{
			PrivateIncludePathModuleNames.Add("ApplicationCore");
		}
		if(Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			PrivateIncludePathModuleNames.Add("SDL2");
		}
	}
}
