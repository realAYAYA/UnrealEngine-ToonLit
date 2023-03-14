// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class AnimationModifierLibrary : ModuleRules
	{
		public AnimationModifierLibrary(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AnimationModifiers",
					"AnimationBlueprintLibrary"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
				}
			);

		}
	}
}
