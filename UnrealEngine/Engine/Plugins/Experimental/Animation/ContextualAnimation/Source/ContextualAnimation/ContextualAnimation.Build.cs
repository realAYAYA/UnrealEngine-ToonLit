// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ContextualAnimation : ModuleRules
{
	public ContextualAnimation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
                "Engine",
				"GameplayTags",
				"MotionWarping",
				"IKRig"
			}
			);			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore"
            }
			);
	}
}
