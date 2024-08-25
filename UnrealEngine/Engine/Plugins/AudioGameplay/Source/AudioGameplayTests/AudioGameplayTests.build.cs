// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioGameplayTests : ModuleRules
{
	public AudioGameplayTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AudioExtensions",
				"AudioGameplay",
				"Core",
				"GameplayTags",
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}