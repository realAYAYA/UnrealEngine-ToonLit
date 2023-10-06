// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VirtualFileCache : ModuleRules
{
	public VirtualFileCache(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);
	}
}
