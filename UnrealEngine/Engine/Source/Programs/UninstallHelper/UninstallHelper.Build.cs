// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UninstallHelper : ModuleRules
{
	public UninstallHelper(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");
		PrivateIncludePaths.Add("Runtime/Launch/Private");
		
		PrivateDependencyModuleNames.AddRange(new []
		{
			"Core", "Projects"
		});
	}
}
