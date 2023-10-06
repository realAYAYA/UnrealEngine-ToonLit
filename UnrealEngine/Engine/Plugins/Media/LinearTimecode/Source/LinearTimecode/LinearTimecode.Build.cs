// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LinearTimecode : ModuleRules
	{
		public LinearTimecode(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Media",
					"MediaUtils",
					"MediaAssets",
					"TimeManagement",
				}
			);
		}
	}
}
