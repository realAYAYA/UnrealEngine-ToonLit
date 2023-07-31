// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimationLocomotionLibraryEditor : ModuleRules
	{
		public AnimationLocomotionLibraryEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
			new string[]
			{
					"Core",
					"CoreUObject",
					"Engine",
					"AnimGraphRuntime",
					"AnimationModifiers",
					"AnimationBlueprintLibrary"
			});

			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
            });

			PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
			});
		}
	}
}
