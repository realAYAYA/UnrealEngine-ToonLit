// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class detex : ModuleRules
{
	public detex(ReadOnlyTargetRules Target) : base(Target)
	{
		string detexpath = Target.UEThirdPartySourceDirectory + "Android/detex/";
		PublicIncludePaths.Add(detexpath);
		
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
			}
			);
    }
}
