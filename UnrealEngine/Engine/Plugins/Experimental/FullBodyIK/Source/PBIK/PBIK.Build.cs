// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PBIK : ModuleRules
{
	public PBIK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RigVM",
				"ControlRig"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
			}
			);

		
	}
}
