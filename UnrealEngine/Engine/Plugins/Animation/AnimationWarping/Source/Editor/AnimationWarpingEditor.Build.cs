// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimationWarpingEditor : ModuleRules
	{
		public AnimationWarpingEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AnimationCore",
				"AnimationWarpingRuntime",
                "AnimGraph",
				"AnimGraphRuntime",
				"AnimationModifiers",
				"AnimationBlueprintLibrary",
				"AnimationModifierLibrary",
				"Core",
                "CoreUObject",
                "Engine",
            });

			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "SlateCore",
            });

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.AddRange(
                    new string[]
                    {
						"BlueprintGraph",
						"EditorFramework",
						"Kismet",
                        "UnrealEd",
                    }
                );
            }
        }
	}
}
