// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealPak : ModuleRules
{
	public UnrealPak(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");

		PrivateDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"AssetRegistry", 
			"PakFile", 
			"Json", 
			"Projects", 
			"PakFileUtilities", 
			"RSA", 
			"ApplicationCore" 
		});

		if (Target.bBuildWithEditorOnlyData)
		{
			DynamicallyLoadedModuleNames.AddRange(new string[] {
				"PerforceSourceControl"
			});
		}
	}
}
