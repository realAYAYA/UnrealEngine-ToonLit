// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveLinkComponents : ModuleRules
	{
		public LiveLinkComponents(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"LiveLinkInterface",
			});

			if (Target.bCompileAgainstEditor)
			{
				PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Slate",
					"UnrealEd",
				});
			}
		}
	}
}
