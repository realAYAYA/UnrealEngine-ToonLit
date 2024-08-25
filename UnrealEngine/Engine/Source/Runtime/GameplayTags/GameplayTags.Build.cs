// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayTags : ModuleRules
	{
		public GameplayTags(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"DeveloperSettings"
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Projects",
					"Json",
					"JsonUtilities"
				}
			);

			if (Target.bCompileAgainstEditor)
            {
                PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "SlateCore",
                    "Slate"
				}
                );
            }

			bAllowAutoRTFMInstrumentation = true;

			SetupIrisSupport(Target);
		}
	}
}
