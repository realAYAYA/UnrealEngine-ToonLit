// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealPackageTool : ModuleRules
{
	public UnrealPackageTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore", // AssetRegistry requires this 
				"Core",
				"CoreUObject",
				"Projects",
				"AssetRegistry"
			}
		);

		bUseUnity = false;
		PrivateDependencyModuleNames.Add("CLI11");
		
		bEnableExceptions = true;
	}
}
