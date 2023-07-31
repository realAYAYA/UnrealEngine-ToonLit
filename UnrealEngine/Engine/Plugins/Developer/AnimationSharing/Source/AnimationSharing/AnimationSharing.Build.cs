// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AnimationSharing : ModuleRules
{
	public AnimationSharing(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"EngineSettings",
				"SignificanceManager"
            }
		);

        if (Target.Type == TargetType.Editor)
        {
            PrivateDependencyModuleNames.Add("TargetPlatform");
        }     
    }
}
