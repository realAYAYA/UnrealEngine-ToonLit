// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AbilitySystemGameFeatureActions : ModuleRules
	{
        public AbilitySystemGameFeatureActions(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "AbilitySysGFAs";

			PublicDependencyModuleNames.AddRange(
				new string[]
                {
                    "Core",
                    "CoreUObject",
					"DeveloperSettings",
					"Engine",
                    "ModularGameplay",
					"DataRegistry"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"GameFeatures",
					"GameplayAbilities",
				}
			);
		}
	}
}
