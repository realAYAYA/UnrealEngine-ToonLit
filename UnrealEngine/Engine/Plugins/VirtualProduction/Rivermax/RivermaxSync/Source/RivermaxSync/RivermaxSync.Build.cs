// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RivermaxSync : ModuleRules
{
	public RivermaxSync(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"DisplayClusterMedia",
				"RivermaxCore"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"RivermaxMedia"
			});

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
